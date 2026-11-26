// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacecatalog.h"

#include <KConfig>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace WorkspaceCatalog;

class WorkspaceCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void managesStableIdentities()
    {
        Catalog catalog;
        QString error;
        QVERIFY2(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error), qPrintable(error));
        QVERIFY2(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error), qPrintable(error));
        const QString workId = catalog.workspaces.at(0).id;
        const QString homeId = catalog.workspaces.at(1).id;
        QVERIFY(!workId.isEmpty());
        QVERIFY(workId != homeId);
        QVERIFY2(activate(catalog, workId, error), qPrintable(error));
        QVERIFY2(rename(catalog, workId, QStringLiteral("Office"), QStringLiteral("O"), error), qPrintable(error));
        QCOMPARE(catalog.activeId, workId);
        QCOMPARE(catalog.workspaces.at(0).id, workId);
        QVERIFY2(remove(catalog, homeId, error), qPrintable(error));
        QCOMPARE(catalog.activeId, workId);
        QVERIFY2(remove(catalog, workId, error), qPrintable(error));
        QVERIFY(catalog.activeId.isEmpty());
        QVERIFY(catalog.workspaces.isEmpty());
    }

    void rejectsInvalidChangesWithoutMutation()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const Catalog original = catalog;
        QVERIFY(!create(catalog, QStringLiteral("  work "), QStringLiteral("X"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!rename(catalog, original.workspaces.first().id, QStringLiteral("Other"), QStringLiteral(" "), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!activate(catalog, QStringLiteral("missing"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!remove(catalog, QStringLiteral("missing"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
    }

    void persistsCatalog()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(activate(catalog, catalog.workspaces.first().id, error));
        catalog.workspaces.first().behaviorSelection = {BehaviorMode::Selected, false, true, false, true};
        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(save(config.group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        }
        KConfig config(path, KConfig::SimpleConfig);
        Catalog loaded;
        QVERIFY2(load(config.group(QStringLiteral("Workspaces")), loaded, error), qPrintable(error));
        QCOMPARE(loaded, catalog);
    }

    void preventsStaleCatalogUpdates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        const KConfigGroup firstGroup = first.group(QStringLiteral("Workspaces"));
        const KConfigGroup secondGroup = second.group(QStringLiteral("Workspaces"));
        Catalog original;
        QString error;
        QVERIFY(load(firstGroup, original, error));
        Catalog updated = original;
        QVERIFY(create(updated, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY2(saveIfUnchanged(firstGroup, original, updated, error), qPrintable(error));

        Catalog staleUpdate = original;
        QVERIFY(create(staleUpdate, QStringLiteral("Home"), QStringLiteral("H"), error));
        QVERIFY(!saveIfUnchanged(secondGroup, original, staleUpdate, error));
        QVERIFY(!error.isEmpty());
        second.reparseConfiguration();
        Catalog stored;
        QVERIFY(load(secondGroup, stored, error));
        QCOMPARE(stored, updated);
    }

    void reportsLockedCatalogWithoutChangingIt()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig config(path, KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        Catalog original;
        Catalog updated;
        QString error;
        QVERIFY(create(updated, QStringLiteral("Work"), QStringLiteral("W"), error));
        QLockFile lock(path + QStringLiteral(".workspaces.lock"));
        QVERIFY(lock.tryLock());
        QVERIFY(!saveIfUnchanged(group, original, updated, error));
        QVERIFY(!error.isEmpty());
        Catalog stored;
        QVERIFY(load(group, stored, error));
        QCOMPARE(stored, original);
    }

    void leavesNoPendingWriteAfterSaveFailure()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        QVERIFY(QDir().mkdir(path));
        KConfig config(path, KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        Catalog original;
        Catalog updated;
        QString error;
        QVERIFY(create(updated, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(!saveIfUnchanged(group, original, updated, error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!config.isDirty());
    }

    void rejectsCorruptDataWithoutOverwriting()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry("Catalog", QByteArray("{broken"));
        Catalog catalog;
        catalog.activeId = QStringLiteral("unchanged");
        const Catalog original = catalog;
        QString error;
        QVERIFY(!load(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QCOMPARE(group.readEntry("Catalog", QByteArray()), QByteArray("{broken"));
    }

    void rejectsInvalidCatalogBeforeSaving()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(save(group, catalog, error));
        const QByteArray previous = group.readEntry("Catalog", QByteArray());

        catalog.activeId = QStringLiteral("missing");
        QVERIFY(!save(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(group.readEntry("Catalog", QByteArray()), previous);

        group.writeEntry("Catalog", QByteArray(R"({"version":9,"workspaces":[],"activeId":""})"));
        const Catalog original = catalog;
        QVERIFY(!load(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
    }

    void duplicatesAndPersistsLayout()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString originalId = catalog.workspaces.first().id;
        QVERIFY(activate(catalog, originalId, error));
        const Layout layout{QByteArray::fromHex("0001ff"), {200, 300, 0}, {400, 100}, false, true, std::nullopt, {}};
        QVERIFY2(setLayout(catalog, originalId, layout, error), qPrintable(error));
        QVERIFY2(duplicate(catalog, originalId, QStringLiteral("Copy"), QStringLiteral("C"), error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 2);
        QVERIFY(catalog.workspaces.last().id != originalId);
        QCOMPARE(catalog.workspaces.last().layout, std::optional<Layout>(layout));
        QCOMPARE(catalog.activeId, originalId);

        QVERIFY(save(group, catalog, error));
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded, catalog);

        Layout changed = layout;
        changed.dual = true;
        QVERIFY(setLayout(loaded, loaded.workspaces.last().id, changed, error));
        QCOMPARE(loaded.workspaces.first().layout, std::optional<Layout>(layout));
    }

    void persistsPaneAndTabSnapshots()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        Layout layout{QByteArray::fromHex("0001ff"), {200, 300}, {400, 100}, true, false, std::nullopt, {}};
        WindowSession session;
        session.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/left")), QUrl::fromLocalFile(QStringLiteral("/tmp/left-tab"))};
        session.left.currentTab = 1;
        session.left.pinnedCount = 1;
        session.left.lockedTabs = {1};
        session.left.names = {QStringLiteral("Pinned"), QStringLiteral("Current")};
        session.left.locationPresentation = 1;
        session.left.viewMode = 3;
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/right"))};
        session.right.filterText = QStringLiteral("*.txt");
        session.right.filterScope = 2;
        session.right.yaziMode = true;
        session.activePane = 1;
        session.dual = layout.dual;
        session.stacked = layout.stacked;
        layout.session = session;
        QVERIFY2(setLayout(catalog, catalog.workspaces.first().id, layout, error), qPrintable(error));
        QVERIFY2(save(group, catalog, error), qPrintable(error));
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout, std::optional<Layout>(layout));
    }

    void readsYaziModeOnlyAsAnOptionalBoolean()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        // Leaves the panes as the only objects with a yaziMode entry.
        catalog.workspaces.first().behaviorSelection.yaziMode = std::nullopt;
        Layout layout;
        WindowSession session;
        session.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/left"))};
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/right"))};
        session.right.yaziMode = true;
        layout.session = session;
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, layout, error));
        QVERIFY(save(group, catalog, error));
        const QByteArray stored = group.readEntry("Catalog", QByteArray());
        QCOMPARE(stored.count(",\"yaziMode\":false"), 1);
        QCOMPARE(stored.count(",\"yaziMode\":true"), 1);

        QByteArray legacy = stored;
        legacy.replace(",\"yaziMode\":false", "");
        legacy.replace(",\"yaziMode\":true", "");
        group.writeEntry("Catalog", legacy);
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QVERIFY(!loaded.workspaces.first().layout->session->left.yaziMode);
        QVERIFY(!loaded.workspaces.first().layout->session->right.yaziMode);

        QByteArray invalid = stored;
        invalid.replace(",\"yaziMode\":true", ",\"yaziMode\":1");
        group.writeEntry("Catalog", invalid);
        Catalog rejected;
        QVERIFY(!load(group, rejected, error));
    }

    void persistsYaziModeBehaviorAsAnOptionalBoolean()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        QVERIFY(create(catalog, QStringLiteral("Old"), QStringLiteral("O"), error));
        QCOMPARE(catalog.workspaces.first().behaviorSelection.yaziMode, std::optional<bool>(true));
        catalog.workspaces[1].behaviorSelection.yaziMode = false;
        catalog.workspaces[2].behaviorSelection.yaziMode = std::nullopt;
        QVERIFY2(save(group, catalog, error), qPrintable(error));

        const QByteArray stored = group.readEntry("Catalog", QByteArray());
        const QJsonArray workspaces = QJsonDocument::fromJson(stored).object().value(QStringLiteral("workspaces")).toArray();
        QCOMPARE(workspaces.size(), 3);
        const auto selection = [&workspaces](int index) {
            return workspaces.at(index).toObject().value(QStringLiteral("behaviorSelection")).toObject();
        };
        QCOMPARE(selection(0).value(QStringLiteral("yaziMode")), QJsonValue(true));
        QCOMPARE(selection(1).value(QStringLiteral("yaziMode")), QJsonValue(false));
        QVERIFY(!selection(2).contains(QStringLiteral("yaziMode")));

        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.at(0).behaviorSelection.yaziMode, std::optional<bool>(true));
        QCOMPARE(loaded.workspaces.at(1).behaviorSelection.yaziMode, std::optional<bool>(false));
        QCOMPARE(loaded.workspaces.at(2).behaviorSelection.yaziMode, std::optional<bool>());
        QCOMPARE(loaded.workspaces, catalog.workspaces);

        QJsonObject root = QJsonDocument::fromJson(stored).object();
        QJsonArray invalidWorkspaces = workspaces;
        QJsonObject workspace = invalidWorkspaces.at(1).toObject();
        QJsonObject invalidSelection = selection(1);
        invalidSelection.insert(QStringLiteral("yaziMode"), 0);
        workspace.insert(QStringLiteral("behaviorSelection"), invalidSelection);
        invalidWorkspaces.replace(1, workspace);
        root.insert(QStringLiteral("workspaces"), invalidWorkspaces);
        group.writeEntry("Catalog", QJsonDocument(root).toJson(QJsonDocument::Compact));
        Catalog rejected;
        QVERIFY(!load(group, rejected, error));
    }

    void persistsYaziParentColumnAndColumnRatios()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        Layout layout;
        WindowSession session;
        session.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/left"))};
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/right"))};
        session.left.yaziParentColumn = 0;
        session.left.yaziColumnRatios = {2, 5, 3};
        layout.session = session;
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, layout, error));
        QVERIFY(save(group, catalog, error));
        const QByteArray stored = group.readEntry("Catalog", QByteArray());
        QCOMPARE(stored.count("\"yaziParentColumn\""), 1);
        QCOMPARE(stored.count("\"yaziParentColumn\":false"), 1);
        QCOMPARE(stored.count("\"yaziColumnRatios\""), 1);
        QCOMPARE(stored.count("\"yaziColumnRatios\":[2,5,3]"), 1);
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziParentColumn, 0);
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziColumnRatios, QList<int>({2, 5, 3}));
        QCOMPARE(loaded.workspaces.first().layout->session->right.yaziParentColumn, -1);
        QVERIFY(loaded.workspaces.first().layout->session->right.yaziColumnRatios.isEmpty());

        QByteArray legacy = stored;
        legacy.replace(",\"yaziColumnRatios\":[2,5,3]", "");
        legacy.replace(",\"yaziParentColumn\":false", "");
        QCOMPARE(legacy.count("yaziColumnRatios") + legacy.count("yaziParentColumn"), 0);
        group.writeEntry("Catalog", legacy);
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziParentColumn, -1);
        QVERIFY(loaded.workspaces.first().layout->session->left.yaziColumnRatios.isEmpty());

        for (const QByteArray &ratios :
             {QByteArray("[2,5]"), QByteArray("[2,0,3]"), QByteArray("[2,-5,3]"), QByteArray("[2,5.5,3]"), QByteArray("\"2,5,3\"")}) {
            QByteArray invalid = stored;
            invalid.replace("[2,5,3]", ratios);
            group.writeEntry("Catalog", invalid);
            Catalog rejected;
            QVERIFY2(!load(group, rejected, error), ratios.constData());
        }
        QByteArray invalidParent = stored;
        invalidParent.replace("\"yaziParentColumn\":false", "\"yaziParentColumn\":0");
        group.writeEntry("Catalog", invalidParent);
        Catalog rejected;
        QVERIFY(!load(group, rejected, error));

        layout.session->left.yaziColumnRatios = {2, 0, 3};
        QVERIFY(!setLayout(catalog, catalog.workspaces.first().id, layout, error));
        layout.session->left.yaziColumnRatios = {};
        layout.session->left.yaziParentColumn = 2;
        QVERIFY(!setLayout(catalog, catalog.workspaces.first().id, layout, error));
    }

    void readsYaziPreviewOnlyAsAnOptionalBoolean()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        Layout layout;
        WindowSession session;
        session.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/left"))};
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp/right"))};
        session.left.yaziPreview = -1;
        session.right.yaziPreview = 0;
        session.left.yaziPreviewContent = 1;
        layout.session = session;
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, layout, error));
        QVERIFY(save(group, catalog, error));
        const QByteArray stored = group.readEntry("Catalog", QByteArray());
        QCOMPARE(stored.count("\"yaziPreview\""), 1);
        QCOMPARE(stored.count(",\"yaziPreview\":false"), 1);
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziPreview, -1);
        QCOMPARE(loaded.workspaces.first().layout->session->right.yaziPreview, 0);
        QCOMPARE(stored.count("\"yaziPreviewContent\":\"information\""), 1);
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziPreviewContent, 1);
        QCOMPARE(loaded.workspaces.first().layout->session->right.yaziPreviewContent, -1);

        QByteArray preview = stored;
        preview.replace("\"yaziPreviewContent\":\"information\"", "\"yaziPreviewContent\":\"preview\"");
        group.writeEntry("Catalog", preview);
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->left.yaziPreviewContent, 0);

        QByteArray unknown = stored;
        unknown.replace("\"yaziPreviewContent\":\"information\"", "\"yaziPreviewContent\":1");
        group.writeEntry("Catalog", unknown);
        Catalog rejectedContent;
        QVERIFY(!load(group, rejectedContent, error));

        QByteArray shown = stored;
        shown.replace(",\"yaziPreview\":false", ",\"yaziPreview\":true");
        group.writeEntry("Catalog", shown);
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->right.yaziPreview, 1);

        QByteArray invalid = stored;
        invalid.replace(",\"yaziPreview\":false", ",\"yaziPreview\":0");
        group.writeEntry("Catalog", invalid);
        Catalog rejected;
        QVERIFY(!load(group, rejected, error));
    }

    void removesCredentialDataFromStoredPaneLocations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        Layout layout;
        WindowSession session;
        session.left.tabs = {QUrl(QStringLiteral("sftp://example.invalid/folder"))};
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp"))};
        layout.session = session;
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, layout, error));
        QVERIFY(save(group, catalog, error));
        QByteArray stored = group.readEntry("Catalog", QByteArray());
        QVERIFY(stored.contains("sftp://example.invalid/folder"));
        stored.replace("sftp://example.invalid/folder", "sftp://user:password@example.invalid/folder?token=secret");
        group.writeEntry("Catalog", stored);
        QVERIFY(group.sync());

        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().layout->session->left.tabs.first().toString(), QStringLiteral("sftp://example.invalid/folder"));
        stored = group.readEntry("Catalog", QByteArray());
        QVERIFY(!stored.contains("user:password"));
        QVERIFY(!stored.contains("token=secret"));
    }

    void persistsReusableProfileReferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString searchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString profileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        catalog.workspaces.first().savedSearchIds.append(searchId);
        catalog.workspaces.first().folderSyncProfileIds.append(profileId);
        QVERIFY2(save(group, catalog, error), qPrintable(error));
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded.workspaces.first().savedSearchIds, QStringList{searchId});
        QCOMPARE(loaded.workspaces.first().folderSyncProfileIds, QStringList{profileId});
        QVERIFY2(duplicate(loaded, loaded.workspaces.first().id, QStringLiteral("Copy"), QStringLiteral("C"), error), qPrintable(error));
        QCOMPARE(loaded.workspaces.last().savedSearchIds, QStringList{searchId});
        QCOMPARE(loaded.workspaces.last().folderSyncProfileIds, QStringList{profileId});
    }

    void rejectsInvalidLayoutAndDuplication()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const Catalog original = catalog;
        Layout invalid;
        invalid.paneSizes = {10, -1};
        QVERIFY(!setLayout(catalog, catalog.workspaces.first().id, invalid, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!duplicate(catalog, QStringLiteral("missing"), QStringLiteral("Other"), QStringLiteral("O"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!duplicate(catalog, original.workspaces.first().id, QStringLiteral("work"), QStringLiteral("O"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
    }

    void rejectsMalformedStoredLayouts()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, Layout{}, error));
        QVERIFY(save(group, catalog, error));

        const QJsonObject root = QJsonDocument::fromJson(group.readEntry("Catalog", QByteArray())).object();
        const QJsonObject entry = root.value(QStringLiteral("workspaces")).toArray().first().toObject();
        const QJsonObject layout = entry.value(QStringLiteral("layout")).toObject();
        for (const auto &invalid :
             {QPair{QStringLiteral("paneSizes"), QJsonValue(QJsonArray{-1})}, QPair{QStringLiteral("windowState"), QJsonValue(QStringLiteral("%%%"))}}) {
            QJsonObject corruptLayout = layout;
            corruptLayout.insert(invalid.first, invalid.second);
            QJsonObject corruptEntry = entry;
            corruptEntry.insert(QStringLiteral("layout"), corruptLayout);
            QJsonObject corruptRoot = root;
            corruptRoot.insert(QStringLiteral("workspaces"), QJsonArray{corruptEntry});
            group.writeEntry("Catalog", QJsonDocument(corruptRoot).toJson(QJsonDocument::Compact));
            const Catalog original = catalog;
            QVERIFY(!load(group, catalog, error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(catalog, original);
        }
    }

    void persistsPresentationAndRejectsInvalidValues()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        Layout layout;
        layout.presentation = Presentation{PanePlacement::Tab,
                                           PanelLayout::Separate,
                                           PreviewPlacement::Right,
                                           EditorPlacement::Window,
                                           true,
                                           true,
                                           true,
                                           true,
                                           {QStringLiteral("mainToolBar"), QStringLiteral("editorDock")},
                                           TerminalPlacement::Below,
                                           {},
                                           std::nullopt,
                                           {}};
        layout.presentation->terminalPlacement = TerminalPlacement::Right;
        layout.presentation->detachedPaneGeometry = QRect(50, 60, 700, 500);
        layout.presentation->toolbarActions = QStringList{QStringLiteral("action:open"), QStringLiteral("separator")};
        layout.presentation->paneToolbars.insert(
            QStringLiteral("Left"),
            {QStringList{QStringLiteral("open"), QStringLiteral("filter")}, QStringList{QStringLiteral("open")}, QStringList{QStringLiteral("filter")}});
        QVERIFY(setLayout(catalog, catalog.workspaces.first().id, layout, error));
        QVERIFY(save(group, catalog, error));
        Catalog restored;
        QVERIFY2(load(group, restored, error), qPrintable(error));
        QCOMPARE(restored, catalog);

        Layout invalid = layout;
        invalid.presentation->panePlacement = static_cast<PanePlacement>(99);
        QVERIFY(!setLayout(catalog, catalog.workspaces.first().id, invalid, error));
        QVERIFY(!error.isEmpty());
    }

    void loadsLegacyCatalogWithoutLayout()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry(
            "Catalog",
            QByteArray(R"({"version":1,"workspaces":[{"id":"12f42f08-3054-4da6-8a3c-6ba332887f5c","name":"Work","shortLabel":"W"}],"activeId":""})"));
        Catalog catalog;
        QString error;
        QVERIFY2(load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 1);
        QVERIFY(!catalog.workspaces.first().layout.has_value());
        QVERIFY(save(group, catalog, error));
        QCOMPARE(QJsonDocument::fromJson(group.readEntry("Catalog", QByteArray())).object().value(QStringLiteral("version")).toInt(), 8);
    }

    void managesExclusiveLocationAssignments()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.at(0).id;
        const QString homeId = catalog.workspaces.at(1).id;
        const QString locationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY2(assignLocation(catalog, locationId, workId, error), qPrintable(error));
        QCOMPARE(catalog.locationAssignments.value(locationId), workId);
        const Catalog original = catalog;
        QVERIFY(!assignLocation(catalog, locationId, homeId, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!assignLocation(catalog, QStringLiteral("not-a-uuid"), workId, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!assignLocation(catalog, QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("missing"), error));
        QCOMPARE(catalog, original);
        QVERIFY(unassignLocation(catalog, locationId, error));
        QVERIFY(!catalog.locationAssignments.contains(locationId));
        QVERIFY(!unassignLocation(catalog, locationId, error));
        QVERIFY(!error.isEmpty());
        QVERIFY(assignLocation(catalog, locationId, homeId, error));
        QCOMPARE(catalog.locationAssignments.value(locationId), homeId);
    }

    void identifiesLocationsWithoutCredentials()
    {
        const QUrl first(QStringLiteral("sftp://alice:secret@example.invalid/folder/?token=one"));
        const QUrl second(QStringLiteral("sftp://bob:other@example.invalid/folder?token=two"));
        const QString id = locationId(first);
        QVERIFY(!QUuid(id).isNull());
        QCOMPARE(locationId(second), id);
        QVERIFY(id != locationId(QUrl(QStringLiteral("sftp://example.invalid/elsewhere"))));
        QVERIFY(locationId(QUrl()).isEmpty());
        QVERIFY(locationId(QUrl(QStringLiteral("relative/path"))).isEmpty());
    }

    void assignmentsPersistAndDoNotFollowDuplicateOrDeletion()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString locationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(assignLocation(catalog, locationId, workId, error));
        QVERIFY(duplicate(catalog, workId, QStringLiteral("Copy"), QStringLiteral("C"), error));
        const QString copyId = catalog.workspaces.last().id;
        QCOMPARE(catalog.locationAssignments.size(), 1);
        QCOMPARE(catalog.locationAssignments.value(locationId), workId);
        const QString secondLocationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(assignLocation(catalog, secondLocationId, copyId, error));
        QVERIFY(save(group, catalog, error));
        Catalog loaded;
        QVERIFY2(load(group, loaded, error), qPrintable(error));
        QCOMPARE(loaded, catalog);
        QVERIFY(remove(loaded, workId, error));
        QVERIFY(!loaded.locationAssignments.contains(locationId));
        QCOMPARE(loaded.locationAssignments.value(secondLocationId), copyId);
        QCOMPARE(loaded.workspaces.first().id, copyId);
        QVERIFY(save(group, loaded, error));
        Catalog restored;
        QVERIFY(load(group, restored, error));
        QCOMPARE(restored, loaded);
    }

    void loadsVersionThreeCatalogWithoutAssignments()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        auto group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry(
            "Catalog",
            QByteArray(R"({"version":3,"workspaces":[{"id":"12f42f08-3054-4da6-8a3c-6ba332887f5c","name":"Work","shortLabel":"W"}],"activeId":""})"));
        Catalog catalog;
        QString error;
        QVERIFY2(load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 1);
        QVERIFY(catalog.locationAssignments.isEmpty());
        QCOMPARE(catalog.workspaces.first().behaviorSelection.yaziMode, std::optional<bool>());
    }

    void rejectsStaleAndMalformedAssignments()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        auto firstGroup = first.group(QStringLiteral("Workspaces"));
        const auto secondGroup = second.group(QStringLiteral("Workspaces"));
        Catalog original;
        QString error;
        QVERIFY(create(original, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(save(firstGroup, original, error));
        Catalog assigned = original;
        const QString locationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(assignLocation(assigned, locationId, original.workspaces.first().id, error));
        QVERIFY(saveIfUnchanged(firstGroup, original, assigned, error));
        Catalog stale = original;
        QVERIFY(rename(stale, original.workspaces.first().id, QStringLiteral("Office"), QStringLiteral("O"), error));
        QVERIFY(!saveIfUnchanged(secondGroup, original, stale, error));
        QVERIFY(!error.isEmpty());
        second.reparseConfiguration();
        Catalog stored;
        QVERIFY(load(secondGroup, stored, error));
        QCOMPARE(stored, assigned);

        const QJsonObject root = QJsonDocument::fromJson(firstGroup.readEntry("Catalog", QByteArray())).object();
        for (const QJsonValue &invalid : {QJsonValue(QJsonObject{{QStringLiteral("https://user:password@example.org"), original.workspaces.first().id}}),
                                          QJsonValue(QJsonObject{{locationId, QStringLiteral("missing")}}),
                                          QJsonValue(QJsonArray{})}) {
            QJsonObject corrupt = root;
            corrupt.insert(QStringLiteral("locationAssignments"), invalid);
            firstGroup.writeEntry("Catalog", QJsonDocument(corrupt).toJson(QJsonDocument::Compact));
            const Catalog before = stored;
            QVERIFY(!load(firstGroup, stored, error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(stored, before);
        }
    }

    void loadsVersionTwoLayouts()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry(
            "Catalog",
            QByteArray(
                R"({"version":2,"workspaces":[{"id":"12f42f08-3054-4da6-8a3c-6ba332887f5c","name":"Work","shortLabel":"W","layout":{"windowState":"d2luZG93","paneSizes":[1,2],"workspaceSizes":[3,4],"dual":true,"stacked":false}}],"activeId":""})"));
        Catalog catalog;
        QString error;
        QVERIFY2(load(group, catalog, error), qPrintable(error));
        QVERIFY(catalog.workspaces.first().layout.has_value());
        QCOMPARE(catalog.workspaces.first().layout->windowState, QByteArray("window"));
        QVERIFY(!catalog.workspaces.first().layout->presentation.has_value());
    }
};

QTEST_GUILESS_MAIN(WorkspaceCatalogTest)

#include "workspacecatalogtest.moc"
