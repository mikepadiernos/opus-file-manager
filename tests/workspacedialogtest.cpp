// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacedialog.h"
#include "favoritesmodel.h"
#include "foldersyncprofiles.h"
#include "savedsearches.h"
#include "settings.h"
#include "workspacelocations.h"

#include <KConfig>
#include <KFilePlacesModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class WorkspaceDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void createsRenamesAndDuplicates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        const WorkspaceCatalog::Layout layout{QByteArray("window"), {200, 300}, {500, 100}, true, false, std::nullopt, {}};
        WorkspaceDialog dialog(
            group,
            [layout](WorkspaceCatalog::Layout &captured, QString &) {
                captured = layout;
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"));
        auto *label = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *create = dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"));
        auto *rename = dialog.findChild<QPushButton *>(QStringLiteral("workspaceRename"));
        auto *duplicate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceDuplicate"));
        auto *behaviorMode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceBehaviorMode"));
        auto *locationPresentation = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceLocationPresentation"));
        auto *saveBehaviors = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveBehaviors"));
        QVERIFY(name && label && list && create && rename && duplicate && behaviorMode && locationPresentation && saveBehaviors);
        name->setText(QStringLiteral("Work"));
        label->setText(QStringLiteral("W"));
        create->click();
        QCOMPARE(list->count(), 1);
        behaviorMode->setCurrentIndex(1);
        locationPresentation->setChecked(false);
        saveBehaviors->click();
        list->setCurrentRow(0);
        name->setText(QStringLiteral("Office"));
        label->setText(QStringLiteral("O"));
        rename->click();
        name->setText(QStringLiteral("Copy"));
        label->setText(QStringLiteral("C"));
        duplicate->click();

        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 2);
        QCOMPARE(catalog.workspaces.at(0).name, QStringLiteral("Office"));
        QCOMPARE(catalog.workspaces.at(1).name, QStringLiteral("Copy"));
        QCOMPARE(catalog.workspaces.at(1).layout, std::optional<WorkspaceCatalog::Layout>(layout));
        QVERIFY(catalog.workspaces.at(0).id != catalog.workspaces.at(1).id);
        QCOMPARE(catalog.workspaces.at(0).behaviorSelection.mode, WorkspaceCatalog::BehaviorMode::Selected);
        QVERIFY(!catalog.workspaces.at(0).behaviorSelection.locationPresentation);
        QVERIFY(!catalog.workspaces.at(1).behaviorSelection.locationPresentation);
    }

    void switchesAndDeletesWithoutChangingFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("note.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("unchanged"), qint64(9));
        file.close();
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString homeId = catalog.workspaces.last().id;
        const WorkspaceCatalog::Layout work{QByteArray("work"), {100, 200}, {300, 400}, true, false, std::nullopt, {}};
        const WorkspaceCatalog::Layout home{QByteArray("home"), {200, 300}, {400, 500}, false, true, std::nullopt, {}};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workId, work, error));
        QVERIFY(WorkspaceCatalog::setLayout(catalog, homeId, home, error));
        QVERIFY(WorkspaceCatalog::activate(catalog, workId, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceCatalog::Layout current = work;
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&current](const WorkspaceCatalog::Layout &layout, QString &) {
                current = layout;
                return true;
            },
            workId);
        QSignalSpy changed(&dialog, &WorkspaceDialog::currentWorkspaceChanged);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("workspaceDelete"));
        QVERIFY(list && activate && remove);
        list->setCurrentRow(1);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        activate->click();
        QCOMPARE(current, work);
        QCOMPARE(changed.size(), 0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        activate->click();
        QCOMPARE(current, home);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().first().toString(), homeId);
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveLayout"));
        QVERIFY(save);
        current.dual = true;
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.last().layout, std::optional<WorkspaceCatalog::Layout>(home));
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.last().layout, std::optional<WorkspaceCatalog::Layout>(current));
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        remove->click();
        QCOMPARE(list->count(), 2);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        remove->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.size(), 1);
        QVERIFY(catalog.activeId.isEmpty());
        QCOMPARE(changed.size(), 2);
        QVERIFY(changed.last().first().toString().isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("unchanged"));
    }

    void selectedWorkspaceBehaviorsCanSkipLayoutChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        auto &workspace = catalog.workspaces.first();
        workspace.layout = WorkspaceCatalog::Layout{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, {}};
        workspace.behaviorSelection = {WorkspaceCatalog::BehaviorMode::Selected, false, true, false, false};
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        bool captured = false;
        bool applied = false;
        WorkspaceDialog dialog(
            group,
            [&captured](WorkspaceCatalog::Layout &, QString &) {
                captured = true;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &, QString &) {
                applied = true;
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        QVERIFY(list && activate);
        list->setCurrentRow(0);
        activate->click();

        QVERIFY(!captured);
        QVERIFY(!applied);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        WorkspaceCatalog::Catalog stored;
        QVERIFY(WorkspaceCatalog::load(group, stored, error));
        QCOMPARE(stored.activeId, workspace.id);
    }

    void selectedWorkspaceLayoutKeepsUncheckedPaneSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workspaceId = catalog.workspaces.first().id;
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workspaceId, saved, error));
        catalog.workspaces.first().behaviorSelection = {WorkspaceCatalog::BehaviorMode::Selected, false, false, true, false};
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, currentSession};
        WorkspaceCatalog::Layout applied;
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                applied = layout;
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();

        QCOMPARE(applied.windowState, saved.windowState);
        QVERIFY(applied.session);
        QCOMPARE(applied.session->left.tabs, currentSession.left.tabs);
        QCOMPARE(applied.session->right.tabs, currentSession.right.tabs);
        QCOMPARE(applied.session->activePane, currentSession.activePane);
        QCOMPARE(applied.paneSizes, saved.paneSizes);
        QCOMPARE(applied.workspaceSizes, saved.workspaceSizes);
    }

    void layoutRestoreOptionsKeepCurrentParts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.left.viewMode = 2;
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        WorkspaceCatalog::Presentation savedPresentation;
        savedPresentation.toolbarActions = QStringList{QStringLiteral("saved_action")};
        savedPresentation.previewVisible = true;
        const WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, savedPresentation, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, catalog.workspaces.first().id, saved, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        WorkspaceCatalog::Presentation currentPresentation;
        currentPresentation.toolbarActions = QStringList{QStringLiteral("current_action")};
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, currentPresentation, currentSession};
        WorkspaceCatalog::Layout applied;
        Settings::setWorkspaceRestoreViews(false);
        Settings::setWorkspaceRestoreToolbars(false);
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                applied = layout;
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
        Settings::self()->setDefaults();

        QCOMPARE(applied.windowState, saved.windowState);
        QCOMPARE(applied.paneSizes, saved.paneSizes);
        QVERIFY(applied.presentation && applied.session);
        QCOMPARE(applied.presentation->toolbarActions, currentPresentation.toolbarActions);
        QVERIFY(applied.presentation->previewVisible);
        QCOMPARE(applied.session->left.tabs, savedSession.left.tabs);
        QCOMPARE(applied.session->left.viewMode, 0);
    }

    void layoutOverwriteConfirmationCanBeSkipped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, std::nullopt};
        Settings::setConfirmWorkspaceLayoutOverwrite(false);
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveLayout"))->click();
        Settings::self()->setDefaults();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().layout, std::optional<WorkspaceCatalog::Layout>(current));
    }

    void restoresPreviousLayoutWhenCatalogChangesDuringSwitch()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        const auto firstGroup = first.group(QStringLiteral("Workspaces"));
        const auto secondGroup = second.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog saved;
        QString error;
        QVERIFY(WorkspaceCatalog::create(saved, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString id = saved.workspaces.first().id;
        WorkspaceCatalog::Layout previous;
        WorkspaceCatalog::Layout target;
        target.stacked = false;
        QVERIFY(WorkspaceCatalog::setLayout(saved, id, target, error));
        QVERIFY(WorkspaceCatalog::save(firstGroup, saved, error));
        WorkspaceCatalog::Layout current = previous;
        int applications = 0;
        WorkspaceDialog dialog(
            firstGroup,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&](const WorkspaceCatalog::Layout &layout, QString &failure) {
                current = layout;
                if (++applications == 1) {
                    second.reparseConfiguration();
                    WorkspaceCatalog::Catalog external;
                    if (!WorkspaceCatalog::load(secondGroup, external, failure)
                        || !WorkspaceCatalog::rename(external, id, QStringLiteral("Changed elsewhere"), QStringLiteral("C"), failure)
                        || !WorkspaceCatalog::save(secondGroup, external, failure)) {
                        return false;
                    }
                }
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        list->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
        QCOMPARE(applications, 2);
        QCOMPARE(current, previous);
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        WorkspaceCatalog::Catalog stored;
        QVERIFY(WorkspaceCatalog::load(firstGroup, stored, error));
        QCOMPARE(stored.workspaces.first().name, QStringLiteral("Changed elsewhere"));
        QVERIFY(stored.activeId.isEmpty());
    }

    void reportsConflictsAndRetainsOtherChanges()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        const auto firstGroup = first.group(QStringLiteral("Workspaces"));
        const auto secondGroup = second.group(QStringLiteral("Workspaces"));
        WorkspaceDialog dialog(
            firstGroup,
            [](WorkspaceCatalog::Layout &layout, QString &) {
                layout = {};
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        WorkspaceCatalog::Catalog external;
        QString error;
        QVERIFY(WorkspaceCatalog::create(external, QStringLiteral("Other"), QStringLiteral("O"), error));
        QVERIFY(WorkspaceCatalog::save(secondGroup, external, error));
        dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"))->setText(QStringLiteral("Work"));
        dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"))->setText(QStringLiteral("W"));
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"))->click();
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        QVERIFY(WorkspaceCatalog::load(firstGroup, external, error));
        QCOMPARE(external.workspaces.size(), 1);
        QCOMPARE(external.workspaces.first().name, QStringLiteral("Other"));
    }

    void linksProfilesAndRetainsMissingReferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        QString error;
        SavedSearches::Search search{QStringLiteral("PDFs"), QStringLiteral("*.pdf"), {}, LiveSearch::Field::FileName, true, {}, {}};
        QVERIFY(SavedSearches::save(config.group(QStringLiteral("SavedSearches")), {search}, &error));
        search = SavedSearches::load(config.group(QStringLiteral("SavedSearches")), &error).first();
        FolderSyncProfiles::Profile profile;
        profile.name = QStringLiteral("Photos");
        QVERIFY(FolderSyncProfiles::save(config.group(QStringLiteral("FolderSyncProfiles")), {profile}, &error));
        profile = FolderSyncProfiles::load(config.group(QStringLiteral("FolderSyncProfiles")), &error).first();

        WorkspaceCatalog::Catalog catalog;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::activate(catalog, catalog.workspaces.first().id, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            catalog.activeId);
        auto *workspaces = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *searches = dialog.findChild<QListWidget *>(QStringLiteral("workspaceSavedSearches"));
        auto *profiles = dialog.findChild<QListWidget *>(QStringLiteral("workspaceFolderSyncProfiles"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveProfileLinks"));
        QVERIFY(workspaces && searches && profiles && save);
        workspaces->setCurrentRow(0);
        QCOMPARE(searches->count(), 1);
        QCOMPARE(profiles->count(), 1);
        searches->item(0)->setCheckState(Qt::Checked);
        profiles->item(0)->setCheckState(Qt::Checked);
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().savedSearchIds, QStringList{search.id});
        QCOMPARE(catalog.workspaces.first().folderSyncProfileIds, QStringList{profile.id});

        QVERIFY(SavedSearches::save(config.group(QStringLiteral("SavedSearches")), {}, &error));
        QVERIFY(FolderSyncProfiles::save(config.group(QStringLiteral("FolderSyncProfiles")), {}, &error));
        dialog.refreshCatalog();
        QCOMPARE(searches->count(), 1);
        QCOMPARE(profiles->count(), 1);
        QVERIFY(searches->item(0)->text().contains(QStringLiteral("Missing saved search")));
        QVERIFY(profiles->item(0)->text().contains(QStringLiteral("Missing profile")));
        QCOMPARE(searches->item(0)->data(Qt::UserRole).toString(), search.id);
        QCOMPARE(profiles->item(0)->data(Qt::UserRole).toString(), profile.id);
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().savedSearchIds, QStringList{search.id});
        QCOMPARE(catalog.workspaces.first().folderSyncProfileIds, QStringList{profile.id});
    }

    void assignsReassignsAndUnassignsSavedLocations()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        FavoritesModel favorites(config.group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        const QUrl url = QUrl::fromLocalFile(directory.filePath(QStringLiteral("folder")));
        QVERIFY(favorites.add(url));
        WorkspaceLocations locations(&favorites, &places);
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString homeId = catalog.workspaces.last().id;
        const QString locationId = WorkspaceCatalog::locationId(url);
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            workId,
            nullptr,
            &locations);
        QSignalSpy changed(&dialog, &WorkspaceDialog::catalogChanged);
        auto *workspaces = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *savedLocations = dialog.findChild<QListWidget *>(QStringLiteral("workspaceLocationList"));
        auto *assign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceAssignLocation"));
        auto *unassign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceUnassignLocation"));
        QVERIFY(workspaces && savedLocations && assign && unassign);
        auto selectLocation = [&] {
            for (int row = 0; row < savedLocations->count(); ++row) {
                if (savedLocations->item(row)->data(Qt::UserRole).toString() == locationId) {
                    savedLocations->setCurrentRow(row);
                    return true;
                }
            }
            return false;
        };
        workspaces->setCurrentRow(0);
        QVERIFY(selectLocation());
        assign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.locationAssignments.value(locationId), workId);
        QCOMPARE(changed.size(), 1);
        workspaces->setCurrentRow(1);
        QVERIFY(selectLocation());
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        assign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.locationAssignments.value(locationId), homeId);
        QVERIFY(selectLocation());
        unassign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QVERIFY(!catalog.locationAssignments.contains(locationId));
        QCOMPARE(favorites.urls(), QList<QUrl>({url}));
    }
};

QTEST_MAIN(WorkspaceDialogTest)

#include "workspacedialogtest.moc"
