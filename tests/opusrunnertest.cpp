// SPDX-License-Identifier: GPL-3.0-or-later

#include "opusrunner.h"
#include "favoritesmodel.h"
#include "settings.h"
#include "workspacecatalog.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusMetaType>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class OpusRunnerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        const KSharedConfig::Ptr config = Settings::self()->sharedConfig();
        for (const QString &group : config->groupList()) {
            config->deleteGroup(group);
        }
        config->sync();

        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Photo Edits"), QStringLiteral("PE"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        m_workspaceId = catalog.workspaces.first().id;
        FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
        QVERIFY(favorites.add(QUrl::fromLocalFile(m_dir.path())));
        config->sync();
        m_folderName = QUrl::fromLocalFile(m_dir.path()).fileName();
    }

    void usesKRunnerSignatures()
    {
        OpusRunner::registerTypes();
        QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<RemoteMatches>())), QStringLiteral("a(sssida{sv})"));
        QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<RemoteActions>())), QStringLiteral("a(sss)"));
    }

    void matchesWorkspacesAndFavorites()
    {
        OpusRunner runner;
        const RemoteMatches workspaces = runner.Match(QStringLiteral("photo"));
        QCOMPARE(workspaces.size(), 1);
        QCOMPARE(workspaces.first().text, QStringLiteral("Photo Edits"));
        QVERIFY(!workspaces.first().properties.value(QStringLiteral("subtext")).toString().isEmpty());
        QVERIFY(workspaces.first().properties.contains(QStringLiteral("actions")));

        const RemoteMatches folders = runner.Match(m_folderName);
        QCOMPARE(folders.size(), 1);
        QCOMPARE(folders.first().properties.value(QStringLiteral("urls")).toStringList(), QStringList{QUrl::fromLocalFile(m_dir.path()).toString()});
        QVERIFY(runner.Actions().isEmpty());
    }

    void respectsTheSettingsAfterTeardown()
    {
        OpusRunner runner;
        QCOMPARE(runner.Match(QStringLiteral("photo")).size(), 1);
        Settings::setRunnerWorkspaces(false);
        Settings::self()->save();
        QCOMPARE(runner.Match(QStringLiteral("photo")).size(), 1);
        runner.Teardown();
        QVERIFY(runner.Match(QStringLiteral("photo")).isEmpty());
        Settings::setRunnerWorkspaces(true);
        Settings::setRunnerFavorites(false);
        Settings::self()->save();
        runner.Teardown();
        QVERIFY(runner.Match(m_folderName).isEmpty());
        Settings::setRunnerFavorites(true);
        Settings::self()->save();
    }

    void runsOnlyTargetsThatStillExist()
    {
        OpusRunner runner;
        QList<std::pair<QStringList, QString>> launched;
        runner.setLauncher([&launched](const QStringList &arguments, const QString &token) {
            launched.append({arguments, token});
        });
        const QString id = runner.Match(QStringLiteral("photo")).first().id;
        runner.SetActivationToken(QStringLiteral("token-1"));
        runner.Run(id, QString());
        QCOMPARE(launched.size(), 1);
        QCOMPARE(launched.first().first, (QStringList{QStringLiteral("--workspace"), m_workspaceId}));
        QCOMPARE(launched.first().second, QStringLiteral("token-1"));

        runner.Run(QStringLiteral("workspace:missing"), QString());
        QCOMPARE(launched.size(), 1);

        WorkspaceCatalog::Catalog catalog;
        QString error;
        const KSharedConfig::Ptr config = Settings::self()->sharedConfig();
        QVERIFY(WorkspaceCatalog::load(config->group(QStringLiteral("Workspaces")), catalog, error));
        QVERIFY(WorkspaceCatalog::remove(catalog, m_workspaceId, error));
        QVERIFY(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error));
        config->sync();
        runner.Run(id, QString());
        QCOMPARE(launched.size(), 1);
    }

private:
    QTemporaryDir m_dir;
    QString m_workspaceId;
    QString m_folderName;
};

QTEST_GUILESS_MAIN(OpusRunnerTest)
#include "opusrunnertest.moc"
