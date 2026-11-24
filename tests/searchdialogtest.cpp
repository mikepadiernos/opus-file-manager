// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdialog.h"

#include "favoritesmodel.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "savedpathfilters.h"
#include "searchcollection.h"
#include "settings.h"
#include "workspacelocations.h"

#include <KActionCollection>
#include <KFilePlacesModel>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class SearchDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void usesSavedSearchDefaults()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        auto defaults = config->group(QStringLiteral("Search"));
        defaults.writeEntry("SearchIndexedByDefault", true);
        defaults.writeEntry("SearchContentsByDefault", true);
        QVERIFY(defaults.sync());
        SearchDialog indexed(QUrl::fromLocalFile(root.path()), nullptr, config);
        auto *scope = indexed.findChild<QComboBox *>(QStringLiteral("searchScope"));
        auto *field = indexed.findChild<QComboBox *>(QStringLiteral("searchField"));
        QVERIFY(scope && field);
        QCOMPARE(scope->currentIndex(), 1);
        QCOMPARE(field->currentIndex(), 1);
        QVERIFY(field->currentText().contains(QStringLiteral("Indexed")));
        QVERIFY(indexed.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("Baloo")));
        defaults.writeEntry("SearchIndexedByDefault", false);
        defaults.writeEntry("SearchContentsByDefault", false);
        QVERIFY(defaults.sync());
        SearchDialog live(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(live.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 0);
        QCOMPARE(live.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 0);
    }

    void usesApplicationSettings()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        Settings::setSearchIndexedByDefault(true);
        Settings::setSearchContentsByDefault(true);
        Settings::self()->save();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()));
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 1);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 1);
        Settings::setSearchIndexedByDefault(false);
        Settings::setSearchContentsByDefault(false);
        Settings::self()->save();
    }

    void loadsSharedPathFilter()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent/excluded")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent/deep/nested")));
        for (const QString &name : {QStringLiteral("parent/report.txt"),
                                    QStringLiteral("parent/excluded/report.txt"),
                                    QStringLiteral("parent/deep/nested/report.txt"),
                                    QStringLiteral("parent/other.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        QString error;
        const PathFilter filter{QStringLiteral("report"), QStringLiteral("parent"), {root.filePath(QStringLiteral("parent/excluded"))}, 1};
        QVERIFY(SavedPathFilters::save(config->group(QStringLiteral("SavedPathFilters")), {{QStringLiteral("Work"), filter}}, &error));
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchFilterPresets"))->currentText(), QStringLiteral("Work"));
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->click();
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->text(), filter.pathText);
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->text(), filter.parentText);
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->toPlainText(), filter.excludedFolders.first());
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->value(), 1);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral(".txt"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), root.filePath(QStringLiteral("parent/report.txt")));
        QVERIFY(SavedPathFilters::save(config->group(QStringLiteral("SavedPathFilters")), {}, &error));
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchFilterPresetsStatus"))->text().contains(QStringLiteral("no longer exists")));
        dialog.findChild<QPushButton *>(QStringLiteral("searchRefreshFilterPresets"))->click();
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->isEnabled());
    }

    void searchesAndReportsFailures()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("example.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("matching content"), 16);
        file.close();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()));
        auto *query = dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"));
        auto *field = dialog.findChild<QComboBox *>(QStringLiteral("searchField"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("searchStatus"));
        QVERIFY(query && field && start && results && status);
        start->click();
        QVERIFY(status->text().contains(QStringLiteral("Enter text")));
        query->setText(QStringLiteral("example"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QVERIFY(dialog.findChild<QProgressBar *>(QStringLiteral("searchProgress"))->isHidden());
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
        results->setCurrentItem(results->topLevelItem(0));
        dialog.findChild<QPushButton *>(QStringLiteral("searchCopy"))->click();
        QCOMPARE(QApplication::clipboard()->text(), QUrl::fromLocalFile(file.fileName()).toString(QUrl::FullyEncoded));
        field->setCurrentIndex(1);
        query->setText(QStringLiteral("content"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
    }

    void searchesWorkspaceLocationsOrExplicitlyOutside()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QStringList folders{QStringLiteral("work"), QStringLiteral("unassigned"), QStringLiteral("other")};
        for (const QString &folder : folders) {
            QVERIFY(QDir(root.path()).mkpath(folder));
            QFile file(root.filePath(folder + QStringLiteral("/needle.txt")));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        for (const QString &folder : folders) {
            QVERIFY(favorites.add(QUrl::fromLocalFile(root.filePath(folder))));
        }
        WorkspaceLocations locations(&favorites, &places);
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Other"), QStringLiteral("O"), error));
        QString active = catalog.workspaces.first().id;
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(QUrl::fromLocalFile(root.filePath(folders[0]))), active, error));
        QVERIFY(WorkspaceCatalog::assignLocation(catalog,
                                                 WorkspaceCatalog::locationId(QUrl::fromLocalFile(root.filePath(folders[2]))),
                                                 catalog.workspaces.last().id,
                                                 error));
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config, [&] {
            SearchDialog::WorkspaceScope scope;
            scope.active = !active.isEmpty();
            for (const auto &location : locations.locations()) {
                if (!active.isEmpty() && catalog.locationAssignments.value(location.id) == active) {
                    scope.locations.append(location.url);
                }
            }
            return scope;
        });
        auto *workspaceOnly = dialog.findChild<QCheckBox *>(QStringLiteral("searchWorkspaceOnly"));
        QVERIFY(workspaceOnly);
        QVERIFY(workspaceOnly->isChecked());
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QStringList paths;
        for (int i = 0; i < results->topLevelItemCount(); ++i) {
            paths.append(results->topLevelItem(i)->text(0));
        }
        paths.sort();
        QCOMPARE(paths, (QStringList{root.filePath(QStringLiteral("work/needle.txt"))}));
        workspaceOnly->setChecked(false);
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 3);
        workspaceOnly->setChecked(true);
        active = catalog.workspaces.last().id;
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        paths.clear();
        for (int i = 0; i < results->topLevelItemCount(); ++i) {
            paths.append(results->topLevelItem(i)->text(0));
        }
        QVERIFY(paths.contains(root.filePath(QStringLiteral("other/needle.txt"))));
        QVERIFY(!paths.contains(root.filePath(QStringLiteral("work/needle.txt"))));
        QVERIFY(!paths.contains(root.filePath(QStringLiteral("unassigned/needle.txt"))));
        active.clear();
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 3);
    }

    void workspaceSearchDefaultFollowsPreference()
    {
        QTemporaryDir root;
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        const auto activeScope = [] {
            SearchDialog::WorkspaceScope scope;
            scope.active = true;
            return scope;
        };
        {
            SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config, activeScope);
            auto *workspaceOnly = dialog.findChild<QCheckBox *>(QStringLiteral("searchWorkspaceOnly"));
            QVERIFY(workspaceOnly);
            QVERIFY(workspaceOnly->isChecked());
        }
        config->group(QStringLiteral("Search")).writeEntry("WorkspaceSearchByDefault", false);
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config, activeScope);
        auto *workspaceOnly = dialog.findChild<QCheckBox *>(QStringLiteral("searchWorkspaceOnly"));
        auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("searchScope"));
        QVERIFY(workspaceOnly && scope);
        QVERIFY(!workspaceOnly->isChecked());
        QCOMPARE(scope->itemText(0), QStringLiteral("Current folder (live)"));
        workspaceOnly->setChecked(true);
        QCOMPARE(scope->itemText(0), QStringLiteral("Workspace locations (live)"));
    }

    void workspaceSearchReportsUnsupportedLocations()
    {
        QTemporaryDir root;
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        QFile file(root.filePath(QStringLiteral("needle.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QList<QUrl> locations{QUrl(QStringLiteral("sftp://user:secret@example.invalid/path")), QUrl::fromLocalFile(root.path())};
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config, [&] {
            return SearchDialog::WorkspaceScope{true, locations};
        });
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("incomplete")));
        auto *errors = dialog.findChild<QTreeWidget *>(QStringLiteral("searchErrors"));
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(!errors->topLevelItem(0)->text(0).contains(QStringLiteral("secret")));
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"))->topLevelItemCount(), 1);
        locations.clear();
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"))->topLevelItemCount(), 0);
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("no searchable")));
        locations = {QUrl::fromLocalFile(root.filePath(QStringLiteral("missing")))};
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("existing local folder")));
    }

    void mainWindowScopesSearchToWorkspace()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("work")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("other")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("unassigned")));
        for (const QString &folder : {QStringLiteral("work"), QStringLiteral("other"), QStringLiteral("unassigned")}) {
            QFile file(root.filePath(folder + QStringLiteral("/needle.txt")));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto config = KSharedConfig::openConfig();
        const auto backup = KSharedConfig::openConfig(root.filePath(QStringLiteral("backup")), KConfig::SimpleConfig);
        auto favorites = config->group(QStringLiteral("Favorites"));
        auto workspaces = config->group(QStringLiteral("Workspaces"));
        auto savedFavorites = backup->group(QStringLiteral("Favorites"));
        auto savedWorkspaces = backup->group(QStringLiteral("Workspaces"));
        favorites.copyTo(&savedFavorites);
        workspaces.copyTo(&savedWorkspaces);
        favorites.deleteGroup();
        workspaces.deleteGroup();
        const auto restore = qScopeGuard([config, savedFavorites, savedWorkspaces]() mutable {
            config->reparseConfiguration();
            auto favorites = config->group(QStringLiteral("Favorites"));
            auto workspaces = config->group(QStringLiteral("Workspaces"));
            favorites.deleteGroup();
            workspaces.deleteGroup();
            savedFavorites.copyTo(&favorites);
            savedWorkspaces.copyTo(&workspaces);
            config->sync();
        });
        FavoritesModel model(favorites);
        QVERIFY(model.add(QUrl::fromLocalFile(root.filePath(QStringLiteral("work")))));
        QVERIFY(model.add(QUrl::fromLocalFile(root.filePath(QStringLiteral("other")))));
        QVERIFY(model.add(QUrl::fromLocalFile(root.filePath(QStringLiteral("unassigned")))));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Other"), QStringLiteral("O"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Empty"), QStringLiteral("E"), error));
        QVERIFY(WorkspaceCatalog::activate(catalog, catalog.workspaces.first().id, error));
        QVERIFY(WorkspaceCatalog::assignLocation(catalog,
                                                 WorkspaceCatalog::locationId(QUrl::fromLocalFile(root.filePath(QStringLiteral("work")))),
                                                 catalog.workspaces.first().id,
                                                 error));
        QVERIFY(WorkspaceCatalog::assignLocation(catalog,
                                                 WorkspaceCatalog::locationId(QUrl::fromLocalFile(root.filePath(QStringLiteral("other")))),
                                                 catalog.workspaces.at(1).id,
                                                 error));
        QVERIFY(WorkspaceCatalog::save(workspaces, catalog, error));
        QVERIFY(config->sync());
        for (int workspace = 0; workspace < 4; ++workspace) {
            if (workspace == 3) {
                catalog.activeId.clear();
            } else {
                QVERIFY(WorkspaceCatalog::activate(catalog, catalog.workspaces.at(workspace).id, error));
            }
            QVERIFY(WorkspaceCatalog::save(workspaces, catalog, error));
            QVERIFY(config->sync());
            MainWindow window;
            window.openUrls({QUrl::fromLocalFile(root.path())});
            Pane *activePane = window.activePane();
            Pane *inactivePane = window.inactivePane();
            inactivePane->setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("other"))));
            QTRY_COMPARE(inactivePane->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("other"))));
            const QList<QAction *> entries{window.actionCollection()->action(QStringLiteral("search_folder")),
                                           activePane->toolbarAction(QStringLiteral("paneSearch")),
                                           inactivePane->toolbarAction(QStringLiteral("paneSearch"))};
            for (int entry = 0; entry < entries.size(); ++entry) {
                QVERIFY(entries[entry]);
                entries[entry]->trigger();
                auto *dialog = window.findChild<SearchDialog *>(QStringLiteral("searchDialog"));
                QVERIFY(dialog);
                auto *workspaceOnly = dialog->findChild<QCheckBox *>(QStringLiteral("searchWorkspaceOnly"));
                QVERIFY(workspaceOnly && workspaceOnly->isChecked());
                QCOMPARE(workspaceOnly->isHidden(), workspace == 3);
                dialog->findChild<QComboBox *>(QStringLiteral("searchScope"))->setCurrentIndex(0);
                dialog->findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
                auto *start = dialog->findChild<QPushButton *>(QStringLiteral("searchStart"));
                start->click();
                QTRY_VERIFY(start->isEnabled());
                auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("searchResults"));
                const int broaderCount = entry == 2 ? 1 : 3;
                QCOMPARE(results->topLevelItemCount(), workspace == 3 ? broaderCount : workspace == 2 ? 0 : 1);
                if (workspace < 2) {
                    const QString folder = workspace == 0 ? QStringLiteral("work") : QStringLiteral("other");
                    QCOMPARE(results->topLevelItem(0)->text(0), root.filePath(folder + QStringLiteral("/needle.txt")));
                } else if (workspace == 2) {
                    auto *errors = dialog->findChild<QTreeWidget *>(QStringLiteral("searchErrors"));
                    QCOMPARE(errors->topLevelItemCount(), 1);
                    QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("no searchable")));
                }
                if (workspace != 3) {
                    workspaceOnly->setChecked(false);
                    start->click();
                    QTRY_VERIFY(start->isEnabled());
                    QCOMPARE(results->topLevelItemCount(), broaderCount);
                }
                delete dialog;
            }
        }
    }

    void refusesRemoteFolders()
    {
        SearchDialog dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("x"));
        dialog.findChild<QPushButton *>(QStringLiteral("searchStart"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("local folders only")));
    }

    void indexedScopeReportsInvalidQueries()
    {
        SearchDialog dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
        auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("searchScope"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *errors = dialog.findChild<QTreeWidget *>(QStringLiteral("searchErrors"));
        QVERIFY(scope && start && errors);
        scope->setCurrentIndex(1);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("a\"b"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("double quotes")));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("incomplete")));
        dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->setValue(0);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("term"));
        start->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("local current folder")));
    }

    void mainWindowOpensSearch()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        QAction *action = window.actionCollection()->action(QStringLiteral("search_folder"));
        QVERIFY(action);
        action->trigger();
        QVERIFY(window.findChild<SearchDialog *>(QStringLiteral("searchDialog")));
    }

    void revealsResultInActivePaneAndReportsMissingItems()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("sub")));
        const QString filePath = root.filePath(QStringLiteral("sub/find-me.txt"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        window.actionCollection()->action(QStringLiteral("search_folder"))->trigger();
        auto *dialog = window.findChild<SearchDialog *>(QStringLiteral("searchDialog"));
        QVERIFY(dialog);
        auto *query = dialog->findChild<QLineEdit *>(QStringLiteral("searchQuery"));
        auto *start = dialog->findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *reveal = dialog->findChild<QPushButton *>(QStringLiteral("searchReveal"));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QVERIFY(query && start && reveal && results);
        QVERIFY(!reveal->isEnabled());
        query->setText(QStringLiteral("find-me"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        results->setCurrentItem(results->topLevelItem(0));
        QVERIFY(reveal->isEnabled());
        Pane *pane = window.activePane();
        reveal->click();
        QTRY_COMPARE(pane->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("sub"))));
        QTRY_COMPARE(pane->view()->selectedItems().size(), 1);
        QCOMPARE(pane->view()->selectedItems().first().url(), QUrl::fromLocalFile(filePath));
        QSignalSpy status(pane, &Pane::statusMessage);
        pane->view()->setNameFilter(NameFilter(QStringLiteral("not-present"), NameFilter::Scope::FilesAndFolders));
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.takeFirst().first().toString().contains(QStringLiteral("filtered")));
        pane->view()->setNameFilter(NameFilter());
        pane->view()->setFlat(true);
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.takeFirst().first().toString().contains(QStringLiteral("flat view")));
        pane->view()->setFlat(false);
        QVERIFY(QFile::remove(filePath));
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.first().first().toString().contains(QStringLiteral("no longer available")));
    }

    void savesAndRestoresSearchCriteria()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QTemporaryDir configDir;
        QVERIFY(configDir.isValid());
        QFile file(root.filePath(QStringLiteral("result.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("needle"), 6);
        file.close();
        const auto config = KSharedConfig::openConfig(configDir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        {
            SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
            dialog.findChild<QComboBox *>(QStringLiteral("searchField"))->setCurrentIndex(1);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->setText(QStringLiteral("result"));
            dialog.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->setText(root.path());
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->setPlainText(root.filePath(QStringLiteral("excluded")));
            dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->setValue(0);
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Documents"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("searchSave"))->click();
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        }
        auto defaults = config->group(QStringLiteral("Search"));
        defaults.writeEntry("SearchIndexedByDefault", true);
        defaults.writeEntry("SearchContentsByDefault", false);
        QVERIFY(defaults.sync());
        config->reparseConfiguration();
        SearchDialog restored(QUrl::fromLocalFile(QStringLiteral("/")), nullptr, config);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 0);
        restored.findChild<QPushButton *>(QStringLiteral("searchLoad"))->click();
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->text(), QStringLiteral("needle"));
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 0);
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->text(), QStringLiteral("result"));
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->text(), root.path());
        QCOMPARE(restored.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->toPlainText(), root.filePath(QStringLiteral("excluded")));
        QCOMPARE(restored.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->value(), 0);
        auto *start = restored.findChild<QPushButton *>(QStringLiteral("searchStart"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        auto *results = restored.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
        auto *deleteSaved = restored.findChild<QPushButton *>(QStringLiteral("searchDelete"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        deleteSaved->click();
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        deleteSaved->click();
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 0);
        QVERIFY(QFile::exists(file.fileName()));
    }

    void savingFromTwoDialogsPreservesBothSearches()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        SearchDialog first(QUrl::fromLocalFile(root.path()), nullptr, config);
        SearchDialog second(QUrl::fromLocalFile(root.path()), nullptr, config);
        const auto save = [](SearchDialog &dialog, const QString &name) {
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("term"));
            QTimer::singleShot(0, [name] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(name);
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("searchSave"))->click();
        };
        save(first, QStringLiteral("One"));
        save(second, QStringLiteral("Two"));
        SearchDialog restored(QUrl::fromLocalFile(root.path()), nullptr, config);
        auto *saved = restored.findChild<QComboBox *>(QStringLiteral("searchSaved"));
        QCOMPARE(saved->count(), 2);
        QCOMPARE(saved->itemText(0), QStringLiteral("One"));
        QCOMPARE(saved->itemText(1), QStringLiteral("Two"));
    }

    void updatesSavedSearchAndDetectsStaleEdits()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const auto group = config->group(QStringLiteral("SavedSearches"));
        QString error;
        QVERIFY(
            SavedSearches::save(group,
                                {{QStringLiteral("Work"), QStringLiteral("old"), QUrl::fromLocalFile(root.path()), LiveSearch::Field::FileName, false, {}, {}}},
                                &error));
        const QString stableId = SavedSearches::load(group, &error).first().id;
        SearchDialog first(QUrl::fromLocalFile(root.path()), nullptr, config);
        SearchDialog second(QUrl::fromLocalFile(root.path()), nullptr, config);
        first.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("new"));
        first.findChild<QPushButton *>(QStringLiteral("searchUpdate"))->click();
        auto saved = SavedSearches::load(group, &error);
        QCOMPARE(saved.first().text, QStringLiteral("new"));
        QCOMPARE(saved.first().id, stableId);
        second.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("stale"));
        second.findChild<QPushButton *>(QStringLiteral("searchUpdate"))->click();
        QVERIFY(second.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("changed elsewhere")));
        QCOMPARE(SavedSearches::load(group, &error).first().text, QStringLiteral("new"));
        second.findChild<QPushButton *>(QStringLiteral("searchLoad"))->click();
        QCOMPARE(second.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->text(), QStringLiteral("new"));
        second.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("latest"));
        second.findChild<QPushButton *>(QStringLiteral("searchUpdate"))->click();
        saved = SavedSearches::load(group, &error);
        QCOMPARE(saved.first().text, QStringLiteral("latest"));
        QCOMPARE(saved.first().id, stableId);
    }

    void savesLoadsAndUpdatesNamedCollections()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const auto group = config->group(QStringLiteral("SearchCollection"));
        const QUrl a = QUrl::fromLocalFile(root.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(root.filePath(QStringLiteral("b")));
        QString error;
        QVERIFY(SearchCollection::save(group, {a}, &error));
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        dialog.findChild<QTabWidget *>(QStringLiteral("searchResultTabs"))->setCurrentIndex(1);
        QTimer::singleShot(0, [] {
            auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(input);
            input->setTextValue(QStringLiteral("First"));
            input->accept();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchSaveCollection"))->click();
        QCOMPARE(SearchCollection::loadSaved(group, &error).first().urls, (QList<QUrl>{a}));
        QVERIFY(SearchCollection::save(group, {b}, &error));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadCollection"))->click();
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{b}));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadCollection"))->click();
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a}));
        QVERIFY(SearchCollection::save(group, {b}, &error));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchUpdateCollection"))->click();
        QCOMPARE(SearchCollection::loadSaved(group, &error).first().urls, (QList<QUrl>{b}));
        QVERIFY(SearchCollection::save(group, {a}, &error));
        QTimer::singleShot(0, [&] {
            QString updateError;
            QVERIFY(SearchCollection::saveSaved(group, {{QStringLiteral("First"), {a, b}}}, &updateError));
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadCollection"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("changed elsewhere")));
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a}));
        config->reparseConfiguration();
        SearchDialog restored(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSavedCollections"))->currentText(), QStringLiteral("First"));
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a}));
    }

    void collectsReferencesAcrossSearchesWithoutMovingFiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("first")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("second")));
        const QString first = root.filePath(QStringLiteral("first/match-a"));
        const QString second = root.filePath(QStringLiteral("second/match-b"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        for (const QString &folder : {root.filePath(QStringLiteral("first")), root.filePath(QStringLiteral("second"))}) {
            SearchDialog dialog(QUrl::fromLocalFile(folder), nullptr, config);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("match"));
            auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
            start->click();
            QTRY_VERIFY(start->isEnabled());
            auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
            QCOMPARE(results->topLevelItemCount(), 1);
            results->setCurrentItem(results->topLevelItem(0));
            dialog.findChild<QPushButton *>(QStringLiteral("searchCollect"))->click();
        }
        config->reparseConfiguration();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        dialog.findChild<QTabWidget *>(QStringLiteral("searchResultTabs"))->setCurrentIndex(1);
        auto *collection = dialog.findChild<QTreeWidget *>(QStringLiteral("searchCollection"));
        QCOMPARE(collection->topLevelItemCount(), 2);
        QCOMPARE(collection->topLevelItem(0)->text(0), first);
        QCOMPARE(collection->topLevelItem(1)->text(0), second);
        collection->setCurrentItem(collection->topLevelItem(0));
        QSignalSpy reveal(&dialog, &SearchDialog::revealRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("searchReveal"))->click();
        QCOMPARE(reveal.size(), 1);
        QCOMPARE(reveal.first().first().toUrl(), QUrl::fromLocalFile(first));
        dialog.findChild<QPushButton *>(QStringLiteral("searchUncollect"))->click();
        QCOMPARE(collection->topLevelItemCount(), 1);
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }
};

QTEST_MAIN(SearchDialogTest)
#include "searchdialogtest.moc"
