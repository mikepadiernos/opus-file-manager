// SPDX-License-Identifier: GPL-3.0-or-later

#include "runnermatches.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace RunnerMatches;

namespace
{
WorkspaceCatalog::Catalog catalog()
{
    WorkspaceCatalog::Catalog catalog;
    WorkspaceCatalog::Workspace photos;
    photos.id = QStringLiteral("w1");
    photos.name = QStringLiteral("Photos");
    WorkspaceCatalog::Workspace work;
    work.id = QStringLiteral("w2");
    work.name = QStringLiteral("Client Projects");
    catalog.workspaces = {photos, work};
    return catalog;
}
}

class RunnerMatchesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void offersOnlyWhatIsEnabled()
    {
        const QList<Favorite> favorites{{QUrl::fromLocalFile(m_dir.path()), QStringLiteral("Scratch")}};
        QCOMPARE(targets(catalog(), favorites, {true, true}).size(), 3);
        const QList<Target> workspacesOnly = targets(catalog(), favorites, {true, false});
        QCOMPARE(workspacesOnly.size(), 2);
        QVERIFY(std::all_of(workspacesOnly.cbegin(), workspacesOnly.cend(), [](const Target &target) {
            return target.kind == Kind::Workspace;
        }));
        const QList<Target> favoritesOnly = targets(catalog(), favorites, {false, true});
        QCOMPARE(favoritesOnly.size(), 1);
        QCOMPARE(favoritesOnly.first().kind, Kind::Location);
        QVERIFY(targets(catalog(), favorites, {false, false}).isEmpty());
    }

    void ranksExactPrefixAndWordMatches()
    {
        const QList<Target> all = targets(catalog(), {{QUrl::fromLocalFile(m_dir.path()), QStringLiteral("Project Notes")}}, {});
        QVERIFY(match(all, QStringLiteral("p")).isEmpty());
        QVERIFY(match(all, QStringLiteral("zz")).isEmpty());

        const QList<Match> photos = match(all, QStringLiteral("photos"));
        QCOMPARE(photos.size(), 1);
        QCOMPARE(photos.first().categoryRelevance, int(Highest));

        const QList<Match> project = match(all, QStringLiteral("proj"));
        QCOMPARE(project.size(), 2);
        QCOMPARE(project.at(0).target.text, QStringLiteral("Project Notes"));
        QCOMPARE(project.at(0).categoryRelevance, int(High));
        QCOMPARE(project.at(1).target.text, QStringLiteral("Client Projects"));
        QCOMPARE(project.at(1).categoryRelevance, int(Moderate));
        QVERIFY(project.at(0).relevance > project.at(1).relevance);
    }

    void matchesAFavoriteByItsFolderName()
    {
        const QString folder = m_dir.filePath(QStringLiteral("Invoices"));
        QVERIFY(QDir().mkpath(folder));
        const QList<Target> all = targets({}, {{QUrl::fromLocalFile(folder), QStringLiteral("Money")}}, {});
        QCOMPARE(match(all, QStringLiteral("invo")).size(), 1);
    }

    void leavesOutMissingLocalFolders()
    {
        const QUrl missing = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("gone")));
        const QUrl remote(QStringLiteral("sftp://user:secret@host/srv/gone"));
        const QList<Target> all = targets({}, {{missing, QStringLiteral("Gone Local")}, {remote, QStringLiteral("Gone Remote")}}, {});
        const QList<Match> matches = match(all, QStringLiteral("gone"));
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().target.url, remote);
        QVERIFY(!resolve(all, all.first().id));
    }

    void idsAndTextNeverExposeCredentials()
    {
        const QUrl remote(QStringLiteral("sftp://user:secret@host/srv/data"));
        const QList<Target> all = targets({}, {{remote, QStringLiteral("Data")}}, {});
        const Target &target = all.first();
        QVERIFY(!target.id.contains(QStringLiteral("secret")));
        QVERIFY(!target.id.contains(QStringLiteral("host")));
        QVERIFY(!subtext(target).contains(QStringLiteral("secret")));
        QVERIFY(!subtext(target).contains(QStringLiteral("user")));
        QCOMPARE(resolve(all, target.id), std::optional<Target>(target));
        QVERIFY(!resolve(all, QStringLiteral("location:unknown")));
    }

    void argumentsOpenTheTarget()
    {
        const QList<Target> all = targets(catalog(), {{QUrl::fromLocalFile(m_dir.path()), QStringLiteral("Scratch")}}, {});
        QCOMPARE(arguments(all.at(0)), (QStringList{QStringLiteral("--workspace"), QStringLiteral("w1")}));
        QCOMPARE(arguments(all.at(2)), QStringList{QUrl::fromLocalFile(m_dir.path()).toString()});
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(RunnerMatchesTest)
#include "runnermatchestest.moc"
