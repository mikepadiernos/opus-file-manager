// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchwidget.h"

#include "favoritesmodel.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "savedpathfilters.h"
#include "searchcollection.h"
#include "searchpanel.h"
#include "settings.h"
#include "workspacelocations.h"

#include <KActionCollection>
#include <KFilePlacesModel>
#include <KSharedConfig>
#include <KToolBar>

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
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidgetAction>

class SearchWidgetTest : public QObject
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
        SearchWidget indexed(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget live(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()));
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()));
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config, [&] {
            SearchWidget::WorkspaceScope scope;
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
            SearchWidget::WorkspaceScope scope;
            scope.active = true;
            return scope;
        };
        {
            SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config, activeScope);
            auto *workspaceOnly = dialog.findChild<QCheckBox *>(QStringLiteral("searchWorkspaceOnly"));
            QVERIFY(workspaceOnly);
            QVERIFY(workspaceOnly->isChecked());
        }
        config->group(QStringLiteral("Search")).writeEntry("WorkspaceSearchByDefault", false);
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config, activeScope);
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config, [&] {
            return SearchWidget::WorkspaceScope{true, locations};
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

    void searchesSearchFoldersAndSkipsExclusions()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &folder : {QStringLiteral("docs/cache"), QStringLiteral("music"), QStringLiteral("elsewhere")}) {
            QVERIFY(QDir(root.path()).mkpath(folder));
            QFile file(root.filePath(folder + QStringLiteral("/needle.txt")));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QFile docs(root.filePath(QStringLiteral("docs/needle.txt")));
        QVERIFY(docs.open(QIODevice::WriteOnly));
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        SearchWidget dialog(QUrl::fromLocalFile(root.filePath(QStringLiteral("elsewhere"))), nullptr, config);
        auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("searchScope"));
        QCOMPARE(scope->count(), 3);
        scope->setCurrentIndex(2);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        auto *errors = dialog.findChild<QTreeWidget *>(QStringLiteral("searchErrors"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 0);
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("No search folders")));

        auto search = config->group(QStringLiteral("Search"));
        search.writeEntry("SearchFolders", QStringList{root.filePath(QStringLiteral("docs")), root.filePath(QStringLiteral("music"))});
        search.writeEntry("SearchExclusions", QStringList{root.filePath(QStringLiteral("docs/cache"))});
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QStringList paths;
        for (int i = 0; i < results->topLevelItemCount(); ++i) {
            paths.append(results->topLevelItem(i)->text(0));
        }
        paths.sort();
        QCOMPARE(paths, (QStringList{root.filePath(QStringLiteral("docs/needle.txt")), root.filePath(QStringLiteral("music/needle.txt"))}));
        QCOMPARE(errors->topLevelItemCount(), 0);

        // Exclusions apply to the current folder scope too.
        search.writeEntry("SearchExclusions", QStringList{root.filePath(QStringLiteral("elsewhere"))});
        scope->setCurrentIndex(0);
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 0);

        scope->setCurrentIndex(2);
        QTimer::singleShot(0, [] {
            auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(input);
            input->setTextValue(QStringLiteral("Folders"));
            input->accept();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("searchSave"))->click();
        SearchWidget restored(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 0);
        restored.findChild<QPushButton *>(QStringLiteral("searchLoad"))->click();
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 2);
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
                auto *dialog = window.findChild<SearchWidget *>(QStringLiteral("searchWidget"));
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
                    workspaceOnly->setChecked(true);
                }
            }
        }
    }

    void refusesRemoteFolders()
    {
        SearchWidget dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("x"));
        dialog.findChild<QPushButton *>(QStringLiteral("searchStart"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("local folders only")));
    }

    void indexedScopeReportsInvalidQueries()
    {
        SearchWidget dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
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

    void mainWindowOpensSearchPanel()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        MainWindow window;
        window.resize(1200, 900);
        window.openUrls({QUrl::fromLocalFile(root.path()), QUrl::fromLocalFile(root.path())});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        QAction *action = window.actionCollection()->action(QStringLiteral("search_folder"));
        auto *toggle = window.actionCollection()->action(QStringLiteral("show_search_panel"));
        auto *panel = window.findChild<SearchPanel *>(QStringLiteral("searchPanel"));
        auto *workspace = window.findChild<QSplitter *>(QStringLiteral("workspaceSplitter"));
        QVERIFY(action && toggle && toggle->isCheckable() && panel && workspace);
        QVERIFY(panel->isHidden());
        QCOMPARE(workspace->indexOf(panel), 2);
        QCOMPARE(window.findChildren<SearchWidget *>().size(), 1);

        action->trigger();
        QVERIFY(toggle->isChecked());
        QVERIFY(panel->isVisible());
        QVERIFY(window.activePane()->isVisible());
        QVERIFY(window.inactivePane()->isVisible());
        auto *search = panel->searchWidget();
        QCOMPARE(search->folder(), QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(QApplication::focusWidget(), search->findChild<QLineEdit *>(QStringLiteral("searchQuery")));
        QVERIFY2(workspace->sizes().at(0) > workspace->sizes().at(2),
                 qPrintable(QDebug::toString(workspace->sizes()) + QDebug::toString(panel->minimumSizeHint())));

        // Showing the preview in a pane keeps both panes while an embedded panel is visible.
        auto *preview = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        QVERIFY(preview);
        preview->setChecked(true);
        QVERIFY(window.activePane()->isVisible());
        QVERIFY(window.inactivePane()->isVisible());
        preview->setChecked(false);

        panel->findChild<QToolButton *>(QStringLiteral("searchPanelClose"))->click();
        QVERIFY(!toggle->isChecked());
        QVERIFY(panel->isHidden());
        QTRY_VERIFY(window.activePane()->isAncestorOf(QApplication::focusWidget()));
        toggle->setChecked(true);
        QVERIFY(panel->isVisible());
        QCOMPARE(window.findChildren<SearchWidget *>().size(), 1);

        window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->setChecked(true);
        QCOMPARE(workspace->indexOf(window.findChild<QWidget *>(QStringLiteral("terminalPanel"))), 1);
        QVERIFY(panel->isVisible());
        const QList<int> sizes = workspace->sizes();
        QVERIFY2(sizes.at(0) * 10 >= (sizes.at(0) + sizes.at(1) + sizes.at(2)) * 5, qPrintable(QDebug::toString(sizes)));
        window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->setChecked(false);
    }

    void searchPanelScopeFollowsRequestingPane()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        const QUrl left = QUrl::fromLocalFile(root.filePath(QStringLiteral("left")));
        const QUrl right = QUrl::fromLocalFile(root.filePath(QStringLiteral("right")));
        MainWindow window;
        window.openUrls({left, right});
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        Pane *inactive = window.inactivePane();
        QTRY_COMPARE(inactive->url(), right);
        inactive->toolbarAction(QStringLiteral("paneSearch"))->trigger();
        auto *search = window.findChild<SearchWidget *>(QStringLiteral("searchWidget"));
        QVERIFY(search);
        QCOMPARE(window.activePane(), inactive);
        QCOMPARE(search->folder(), right);
        QCOMPARE(search->findChild<QLabel *>(QStringLiteral("searchFolder"))->text(), right.toDisplayString());
    }

    void tabOrderFollowsLayout()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        SearchWidget widget(QUrl::fromLocalFile(root.path()));
        widget.resize(500, 900);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.focusQuery();
        QTRY_COMPARE(QApplication::focusWidget(), widget.findChild<QLineEdit *>(QStringLiteral("searchQuery")));
        QList<QWidget *> visited;
        for (int i = 0; i < 40; ++i) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QWidget *focused = QApplication::focusWidget();
            QVERIFY(focused);
            if (focused->objectName() == QLatin1String("searchQuery")) {
                break;
            }
            QVERIFY2(!visited.contains(focused), qPrintable(QStringLiteral("Tab stayed on ") + focused->objectName() + QStringLiteral(" after ") + [&visited] {
                QStringList names;
                for (QWidget *w : visited) {
                    names.append(w->objectName());
                }
                return names.join(QLatin1Char(','));
            }()));
            QVERIFY2(!qobject_cast<QScrollArea *>(focused), qPrintable(focused->objectName()));
            visited.append(focused);
        }
        QCOMPARE(QApplication::focusWidget()->objectName(), QStringLiteral("searchQuery"));
        const auto position = [&visited, &widget](const QString &name) {
            return visited.indexOf(widget.findChild<QWidget *>(name));
        };
        QCOMPARE(position(QStringLiteral("searchField")), 0);
        QVERIFY(position(QStringLiteral("searchExcludedFolders")) >= 0);
        QVERIFY(position(QStringLiteral("searchExcludedFolders")) < position(QStringLiteral("searchMaxDepth")));
        QVERIFY(position(QStringLiteral("searchSaved")) < position(QStringLiteral("searchSave")));
        auto *tabs = widget.findChild<QTabWidget *>(QStringLiteral("searchResultTabs"));
        QVERIFY(tabs);
        QVERIFY(visited.indexOf(tabs->tabBar()) >= 0);
        QVERIFY(position(QStringLiteral("searchSave")) < visited.indexOf(tabs->tabBar()));
        QVERIFY(visited.indexOf(tabs->tabBar()) < position(QStringLiteral("searchResults")));
        QVERIFY(position(QStringLiteral("searchResults")) < position(QStringLiteral("searchStart")));
    }

    void toolbarSearchFieldSearchesActivePane()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QFile needle(root.filePath(QStringLiteral("left/needle.txt")));
        QVERIFY(needle.open(QIODevice::WriteOnly));
        needle.close();
        const QUrl left = QUrl::fromLocalFile(root.filePath(QStringLiteral("left")));
        const QUrl right = QUrl::fromLocalFile(root.filePath(QStringLiteral("right")));
        const auto config = KSharedConfig::openConfig();
        KConfigGroup customization = config->group(QStringLiteral("ToolbarCustomization"));
        const auto backup = KSharedConfig::openConfig(root.filePath(QStringLiteral("toolbar-backup")));
        KConfigGroup savedCustomization = backup->group(QStringLiteral("ToolbarCustomization"));
        customization.copyTo(&savedCustomization);
        customization.deleteGroup();
        const auto restoreCustomization = qScopeGuard([&] {
            customization.deleteGroup();
            savedCustomization.copyTo(&customization);
            config->sync();
        });
        MainWindow window;
        window.resize(1200, 900);
        window.openUrls({left, right});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(window.activePane()->url(), left);

        auto *action = qobject_cast<QWidgetAction *>(window.actionCollection()->action(QStringLiteral("search_toolbar")));
        QVERIFY(action);
        QVERIFY(!action->text().isEmpty());
        QVERIFY(!action->icon().isNull());
        QToolBar *toolbar = window.toolBar();
        QVERIFY(toolbar);
        QVERIFY(!toolbar->actions().contains(action));
        window.actionCollection()->action(QStringLiteral("customize_main_toolbar"))->trigger();
        auto *actionList = window.findChild<QListWidget *>(QStringLiteral("toolbarActionList"));
        QVERIFY(actionList);
        const auto listed = [actionList] {
            for (int row = 0; row < actionList->count(); ++row) {
                if (actionList->item(row)->data(Qt::UserRole + 1).toString() == QLatin1String("search_toolbar")) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(listed());
        toolbar->addAction(action);
        QTRY_VERIFY(!listed());
        QTRY_VERIFY(customization.readEntry("Actions", QStringList()).contains(QStringLiteral("action:search_toolbar")));
        window.actionCollection()->action(QStringLiteral("customize_main_toolbar"))->trigger();
        auto *field = toolbar->findChild<QLineEdit *>(QStringLiteral("toolbarSearch"));
        QVERIFY(field);
        QTRY_VERIFY(field->isVisible());

        auto *panel = window.findChild<SearchPanel *>(QStringLiteral("searchPanel"));
        auto *toggle = window.actionCollection()->action(QStringLiteral("show_search_panel"));
        QVERIFY(panel && toggle);
        SearchWidget *search = panel->searchWidget();
        QVERIFY(search->setFolder(right));

        // Empty text only opens the panel and focuses its query.
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Return);
        QVERIFY(toggle->isChecked());
        QCOMPARE(search->folder(), left);
        QTRY_COMPARE(QApplication::focusWidget(), search->findChild<QLineEdit *>(QStringLiteral("searchQuery")));

        toggle->setChecked(false);
        QVERIFY(search->setFolder(right));
        field->setFocus();
        QTest::keyClicks(field, QStringLiteral("needle"));
        QTest::keyClick(field, Qt::Key_Return);
        QVERIFY(toggle->isChecked());
        QVERIFY(panel->isVisible());
        QCOMPARE(search->folder(), left);
        QCOMPARE(search->findChild<QLineEdit *>(QStringLiteral("searchQuery"))->text(), QStringLiteral("needle"));
        auto *results = search->findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QVERIFY(results);
        QTRY_COMPARE(results->topLevelItemCount(), 1);
        QVERIFY(!search->isRunning());
        QCOMPARE(QApplication::focusWidget(), field);

        // Down moves from the field to the results, Escape returns to the active pane.
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget(), results);
        QVERIFY(results->currentItem());
        QTest::keyClick(results, Qt::Key_Escape);
        QVERIFY(panel->isVisible());
        QVERIFY(window.activePane()->isAncestorOf(QApplication::focusWidget()));
        auto *query = search->findChild<QLineEdit *>(QStringLiteral("searchQuery"));
        search->focusQuery();
        QTest::keyClick(query, Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget(), results);
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Escape);
        QVERIFY(window.activePane()->isAncestorOf(QApplication::focusWidget()));

        // Without results, Down moves to the panel's query; with the panel hidden it stays put.
        results->clear();
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget(), query);
        toggle->setChecked(false);
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget(), field);
    }

    void opensResultsAndRevealsWithKeyboard()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("match-folder")));
        const QString filePath = root.filePath(QStringLiteral("match-folder/match-file.txt"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        window.actionCollection()->action(QStringLiteral("search_folder"))->trigger();
        auto *search = window.findChild<SearchWidget *>(QStringLiteral("searchWidget"));
        QVERIFY(search);
        auto *open = search->findChild<QPushButton *>(QStringLiteral("searchOpen"));
        auto *start = search->findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *results = search->findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        auto *status = search->findChild<QLabel *>(QStringLiteral("searchStatus"));
        QVERIFY(open && start && results && status);
        QVERIFY(!open->isEnabled());
        QSignalSpy opened(search, &SearchWidget::openRequested);
        QSignalSpy revealed(search, &SearchWidget::revealRequested);
        search->search(QStringLiteral("match"));
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 2);
        QTreeWidgetItem *folderItem = results->findItems(root.filePath(QStringLiteral("match-folder")), Qt::MatchExactly).value(0);
        QTreeWidgetItem *fileItem = results->findItems(filePath, Qt::MatchExactly).value(0);
        QVERIFY(folderItem && fileItem);

        results->setCurrentItem(fileItem);
        QTest::keyClick(results, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(revealed.size(), 1);
        QCOMPARE(revealed.takeFirst().first().toUrl(), QUrl::fromLocalFile(filePath));
        QTest::keyClick(results, Qt::Key_Return);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.takeFirst().first().toUrl(), QUrl::fromLocalFile(filePath));

        Pane *pane = window.activePane();
        results->setCurrentItem(folderItem);
        QVERIFY(open->isEnabled());
        open->click();
        QCOMPARE(opened.size(), 1);
        QTRY_COMPARE(pane->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("match-folder"))));
        opened.clear();

        QVERIFY(QFile::remove(filePath));
        results->setCurrentItem(fileItem);
        open->click();
        QVERIFY(opened.isEmpty());
        QVERIFY(status->text().contains(QStringLiteral("no longer exists")));
    }

    void setFolderIsIgnoredWhileSearching()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        SearchWidget search(QUrl::fromLocalFile(root.path()));
        QVERIFY(search.setFolder(QUrl::fromLocalFile(QDir::rootPath())));
        QCOMPARE(search.folder(), QUrl::fromLocalFile(QDir::rootPath()));
        search.search(QStringLiteral("a-name-that-should-not-exist-anywhere"));
        QVERIFY(search.isRunning());
        QVERIFY(!search.setFolder(QUrl::fromLocalFile(root.path())));
        QCOMPARE(search.folder(), QUrl::fromLocalFile(QDir::rootPath()));
        search.findChild<QPushButton *>(QStringLiteral("searchCancel"))->click();
        QTRY_VERIFY(!search.isRunning());
        QVERIFY(search.setFolder(QUrl::fromLocalFile(root.path())));
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
        auto *dialog = window.findChild<SearchWidget *>(QStringLiteral("searchWidget"));
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
            SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget restored(QUrl::fromLocalFile(QStringLiteral("/")), nullptr, config);
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
        SearchWidget first(QUrl::fromLocalFile(root.path()), nullptr, config);
        SearchWidget second(QUrl::fromLocalFile(root.path()), nullptr, config);
        const auto save = [](SearchWidget &dialog, const QString &name) {
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
        SearchWidget restored(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget first(QUrl::fromLocalFile(root.path()), nullptr, config);
        SearchWidget second(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
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
        SearchWidget restored(QUrl::fromLocalFile(root.path()), nullptr, config);
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
            SearchWidget dialog(QUrl::fromLocalFile(folder), nullptr, config);
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
        SearchWidget dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        dialog.findChild<QTabWidget *>(QStringLiteral("searchResultTabs"))->setCurrentIndex(1);
        auto *collection = dialog.findChild<QTreeWidget *>(QStringLiteral("searchCollection"));
        QCOMPARE(collection->topLevelItemCount(), 2);
        QCOMPARE(collection->topLevelItem(0)->text(0), first);
        QCOMPARE(collection->topLevelItem(1)->text(0), second);
        collection->setCurrentItem(collection->topLevelItem(0));
        QSignalSpy reveal(&dialog, &SearchWidget::revealRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("searchReveal"))->click();
        QCOMPARE(reveal.size(), 1);
        QCOMPARE(reveal.first().first().toUrl(), QUrl::fromLocalFile(first));
        dialog.findChild<QPushButton *>(QStringLiteral("searchUncollect"))->click();
        QCOMPARE(collection->topLevelItemCount(), 1);
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }
};

QTEST_MAIN(SearchWidgetTest)
#include "searchwidgettest.moc"
