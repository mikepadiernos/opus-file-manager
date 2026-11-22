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

        group.writeEntry("Catalog", QByteArray(R"({"version":4,"workspaces":[],"activeId":""})"));
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
        const Layout layout{QByteArray::fromHex("0001ff"), {200, 300, 0}, {400, 100}, false, true, std::nullopt};
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
                                           {QStringLiteral("mainToolBar"), QStringLiteral("editorDock")}};
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
        QCOMPARE(QJsonDocument::fromJson(group.readEntry("Catalog", QByteArray())).object().value(QStringLiteral("version")).toInt(), 3);
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
