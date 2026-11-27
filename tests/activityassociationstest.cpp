// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociations.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using ActivityAssociations::Association;

class ActivityAssociationsTest : public QObject
{
    Q_OBJECT

    static WorkspaceCatalog::Catalog catalog()
    {
        WorkspaceCatalog::Catalog catalog;
        WorkspaceCatalog::Workspace work;
        work.id = QStringLiteral("{11111111-1111-1111-1111-111111111111}");
        work.name = QStringLiteral("Work");
        WorkspaceCatalog::Workspace play;
        play.id = QStringLiteral("{22222222-2222-2222-2222-222222222222}");
        play.name = QStringLiteral("Play");
        catalog.workspaces = {work, play};
        catalog.activeId = work.id;
        return catalog;
    }

    static Association association(const QString &activityId, const QString &workspaceId)
    {
        Association association;
        association.activityId = activityId;
        association.activityName = QStringLiteral("Activity ") + activityId;
        association.workspaceId = workspaceId;
        return association;
    }

    static bool always(const QUrl &)
    {
        return true;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void roundTripsAssociations()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        Association first = association(QStringLiteral("a"), catalog().workspaces.at(1).id);
        first.inheritBehaviors = false;
        first.behaviors.mode = WorkspaceCatalog::BehaviorMode::Selected;
        first.behaviors.layout = false;
        first.behaviors.appearance = false;
        first.behaviors.yaziMode = false;
        first.primaryFolder = QUrl::fromLocalFile(dir.path());
        first.secondaryFolder = QUrl(QStringLiteral("sftp://host/srv/share"));
        Association second = association(QStringLiteral("b"), QString());
        second.primaryFolder = QUrl::fromLocalFile(QDir::homePath());
        second.removed = true;
        ActivityAssociations::save(config.group(QLatin1String(ActivityAssociations::Group)), {second, first});
        config.sync();

        KConfig reread(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        const QList<Association> loaded = ActivityAssociations::load(reread.group(QLatin1String(ActivityAssociations::Group)));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(0), first);
        QCOMPARE(loaded.at(1), second);
    }

    void skipsEmptyAndMalformedEntries()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QLatin1String(ActivityAssociations::Group));
        group.group(QStringLiteral("empty")).writeEntry("Name", QStringLiteral("Nothing"));
        KConfigGroup bad = group.group(QStringLiteral("bad"));
        bad.writeEntry("PrimaryFolder", QStringLiteral("no scheme"));
        bad.writeEntry("BehaviorMode", 42);
        bad.writeEntry("Workspace", QStringLiteral("w"));
        const QList<Association> loaded = ActivityAssociations::load(group);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.at(0).activityId, QStringLiteral("bad"));
        QVERIFY(loaded.at(0).primaryFolder.isEmpty());
        QCOMPARE(loaded.at(0).behaviors.mode, WorkspaceCatalog::BehaviorMode::All);
    }

    void setReplacesAndRemovesEmptyAssociations()
    {
        QList<Association> associations;
        QString error;
        QVERIFY(ActivityAssociations::set(associations, association(QStringLiteral("a"), QStringLiteral("w1")), error));
        QVERIFY(ActivityAssociations::set(associations, association(QStringLiteral("b"), QStringLiteral("w2")), error));
        QVERIFY(ActivityAssociations::set(associations, association(QStringLiteral("a"), QStringLiteral("w3")), error));
        QCOMPARE(associations.size(), 2);
        QCOMPARE(ActivityAssociations::find(associations, QStringLiteral("a"))->workspaceId, QStringLiteral("w3"));

        QVERIFY(ActivityAssociations::set(associations, association(QStringLiteral("a"), QString()), error));
        QVERIFY(!ActivityAssociations::find(associations, QStringLiteral("a")));
        QCOMPARE(associations.size(), 1);

        QVERIFY(!ActivityAssociations::set(associations, association(QString(), QStringLiteral("w1")), error));
        QVERIFY(!error.isEmpty());
        Association badFolder = association(QStringLiteral("c"), QString());
        badFolder.primaryFolder = QUrl(QStringLiteral("relative/path"));
        QVERIFY(!ActivityAssociations::set(associations, badFolder, error));

        QVERIFY(ActivityAssociations::remove(associations, QStringLiteral("b")));
        QVERIFY(!ActivityAssociations::remove(associations, QStringLiteral("b")));
        QVERIFY(associations.isEmpty());
    }

    void reconcileKeepsRemovedActivities()
    {
        QList<Association> associations{association(QStringLiteral("a"), QStringLiteral("w1")), association(QStringLiteral("b"), QStringLiteral("w2"))};
        QVERIFY(ActivityAssociations::reconcile(associations, {{QStringLiteral("a"), QStringLiteral("Renamed")}}));
        QCOMPARE(associations.size(), 2);
        QCOMPARE(associations.at(0).activityName, QStringLiteral("Renamed"));
        QVERIFY(!associations.at(0).removed);
        QVERIFY(associations.at(1).removed);
        QCOMPARE(associations.at(1).workspaceId, QStringLiteral("w2"));
        QVERIFY(!ActivityAssociations::reconcile(associations, {{QStringLiteral("a"), QStringLiteral("Renamed")}}));

        // A restored Activity gets its association back.
        QVERIFY(ActivityAssociations::reconcile(associations, {{QStringLiteral("a"), QString()}, {QStringLiteral("b"), QStringLiteral("Back")}}));
        QVERIFY(!associations.at(1).removed);
        QCOMPARE(associations.at(1).activityName, QStringLiteral("Back"));
        QCOMPARE(associations.at(0).activityName, QStringLiteral("Renamed"));
    }

    void plansTheWorkspaceAndFolders()
    {
        const auto workspaces = catalog();
        Association entry = association(QStringLiteral("a"), workspaces.workspaces.at(1).id);
        entry.primaryFolder = QUrl::fromLocalFile(QStringLiteral("/srv/one"));
        ActivityAssociations::Plan plan = ActivityAssociations::plan(entry, workspaces, workspaces.activeId, always);
        QCOMPARE(plan.activityId, QStringLiteral("a"));
        QCOMPARE(plan.workspaceId, workspaces.workspaces.at(1).id);
        QCOMPARE(plan.workspaceName, QStringLiteral("Play"));
        QVERIFY(!plan.behaviors);
        QCOMPARE(plan.primaryFolder, entry.primaryFolder);
        QVERIFY(plan.secondaryFolder.isEmpty());
        QVERIFY(plan.problems.isEmpty());

        entry.inheritBehaviors = false;
        entry.behaviors.mode = WorkspaceCatalog::BehaviorMode::None;
        plan = ActivityAssociations::plan(entry, workspaces, workspaces.activeId, always);
        QVERIFY(plan.behaviors);
        QCOMPARE(plan.behaviors->mode, WorkspaceCatalog::BehaviorMode::None);
    }

    void leavesOutTheActiveWorkspaceUnlessForced()
    {
        const auto workspaces = catalog();
        const Association entry = association(QStringLiteral("a"), workspaces.activeId);
        ActivityAssociations::Plan plan = ActivityAssociations::plan(entry, workspaces, workspaces.activeId, always);
        QVERIFY(plan.isEmpty());
        QVERIFY(plan.problems.isEmpty());
        plan = ActivityAssociations::plan(entry, workspaces, QString(), always);
        QCOMPARE(plan.workspaceId, workspaces.activeId);
    }

    void reportsMissingWorkspacesAndFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto workspaces = catalog();
        Association entry = association(QStringLiteral("a"), QStringLiteral("{33333333-3333-3333-3333-333333333333}"));
        entry.primaryFolder = QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing")));
        entry.secondaryFolder = QUrl::fromLocalFile(dir.path());
        const ActivityAssociations::Plan plan = ActivityAssociations::plan(entry, workspaces, workspaces.activeId);
        QVERIFY(plan.workspaceId.isEmpty());
        QVERIFY(plan.primaryFolder.isEmpty());
        QCOMPARE(plan.secondaryFolder, entry.secondaryFolder);
        QCOMPARE(plan.problems.size(), 2);
        QVERIFY(plan.problems.at(1).contains(QStringLiteral("missing")));
    }

    void checksOnlyLocalFolders()
    {
        QVERIFY(ActivityAssociations::folderAvailable(QUrl(QStringLiteral("smb://server/share"))));
        QVERIFY(ActivityAssociations::folderAvailable(QUrl::fromLocalFile(QDir::tempPath())));
        QVERIFY(!ActivityAssociations::folderAvailable(QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/opus-no-such-folder"))));
    }
};

QTEST_GUILESS_MAIN(ActivityAssociationsTest)

#include "activityassociationstest.moc"
