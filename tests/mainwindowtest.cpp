// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "combinedpanel.h"
#include "fileoperations.h"
#include "filterbar.h"
#include "foldertree.h"
#include "folderview.h"
#include "pane.h"
#include "settingsdialog.h"
#include "tabmodel.h"
#include "viewsettings.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirModel>
#include <KFileItemDelegate>
#include <KFilePlacesView>
#include <KSharedConfig>

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a/sub")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_c = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("c")));
    }

    void startupFoldersAndPaneDefaultsApply()
    {
        KConfigGroup startup = KSharedConfig::openConfig()->group(QStringLiteral("Startup"));
        startup.writeEntry("StartupMode", QStringLiteral("FixedFolders"));
        startup.writeEntry("LeftStartUrl", m_a.toString());
        startup.writeEntry("RightStartUrl", m_b.toString());
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("DualPaneAtStartup", QStringLiteral("Never"));
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("NewTabsAfterCurrent", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Navigation")).writeEntry("FollowSymlinks", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("DefaultViewMode", QStringLiteral("Icons"));
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("ShowHiddenFiles", true);
        KSharedConfig::openConfig()->sync();

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QVERIFY(!window.actionCollection()->action(QStringLiteral("dual_layout"))->isChecked());
        QCOMPARE(window.activePane()->view()->viewMode(), FolderView::ViewMode::Icons);
        QVERIFY(window.activePane()->view()->showHiddenFiles());

        Pane *pane = window.activePane();
        pane->openInNewTab(m_b, true);
        pane->tabs()->setCurrentIndex(0);
        pane->openInNewTab(m_c, true);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_c, m_b}));

        const QString link = m_root.filePath(QStringLiteral("link-to-b"));
        QVERIFY(QFile::link(m_b.toLocalFile(), link));
        pane->setUrl(QUrl::fromLocalFile(link));
        QCOMPARE(pane->url(), m_b);
    }

    void init()
    {
        // Start from a clean session so the first window does not inherit
        // tabs or layout from an earlier test.
        QFile::remove(m_root.filePath(QStringLiteral("a/.mfm.json")));
        QFile::remove(m_root.filePath(QStringLiteral("b/.mfm.json")));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PopularFolders"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SelectionFilters"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("View"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PanelSettings"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Archives"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Tabs"));
        for (const QString &group : {QStringLiteral("Startup"),
                                     QStringLiteral("Panes"),
                                     QStringLiteral("Navigation"),
                                     QStringLiteral("Previews"),
                                     QStringLiteral("PreviewSettings"),
                                     QStringLiteral("Confirmations"),
                                     QStringLiteral("StatusBar"),
                                     QStringLiteral("Views"),
                                     QStringLiteral("ContextMenu")}) {
            KSharedConfig::openConfig()->deleteGroup(group);
        }
        KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", false);
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("WindowLayout"));
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
    }

    void opensCommandLinePathsInBothPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 0);
    }

    void navigationUpdatesHistory()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *pane = window.activePane();
        QTRY_COMPARE(pane->view()->itemCount(), 2);

        pane->setUrl(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))));
        QVERIFY(pane->tabs()->canGoBack());
        pane->goUp();
        QCOMPARE(pane->url(), m_a);
        pane->goBack();
        QCOMPARE(pane->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
    }

    void displayStyleIsIndependentPerPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *active = window.activePane();
        Pane *other = window.inactivePane();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);

        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("compact_view"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Compact);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
    }

    void selectionModeActionBarCopiesFromItsPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("selection_mode"));
        QVERIFY(action);
        action->trigger();
        QVERIFY(view->selectionMode());
        auto *copy = view->findChild<QToolButton *>(QStringLiteral("selectionCopy"));
        QVERIFY(copy);
        QVERIFY(!copy->isEnabled());
        const QUrl file = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        view->selectUrls({file});
        QVERIFY(copy->isEnabled());
        copy->click();
        QCOMPARE(QApplication::clipboard()->mimeData()->urls(), QList<QUrl>{file});
        view->findChild<QToolButton *>(QStringLiteral("selectionDone"))->click();
        QVERIFY(!action->isChecked());
        QVERIFY(!view->selectionMode());
    }

    void tabAppearanceSettingsApplyToBothPanes()
    {
        KConfigGroup tabs = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
        tabs.writeEntry("ShowIcons", false);
        tabs.writeEntry("Expand", true);
        tabs.sync();

        MainWindow window;
        window.openUrls({m_a});
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            QTabBar *tabBar = pane->findChild<QTabBar *>();
            QVERIFY(tabBar);
            QVERIFY(tabBar->expanding());
            QVERIFY(tabBar->tabIcon(0).isNull());
        }
    }

    void panelSettingsApplyOnStartup()
    {
        KConfigGroup panels = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
        panels.writeEntry("Visible", false);
        panels.writeEntry("Position", QStringLiteral("right"));
        panels.writeEntry("TitlesVisible", false);
        panels.writeEntry("FavoritesVisible", false);
        panels.writeEntry("PlacesVisible", false);
        panels.writeEntry("FoldersVisible", false);
        panels.sync();

        MainWindow window;
        auto *combined = window.findChild<QDockWidget *>(QStringLiteral("combinedDock"));
        auto *places = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(combined);
        QVERIFY(places);
        QVERIFY(combined->isHidden());
        QCOMPARE(window.dockWidgetArea(places), Qt::RightDockWidgetArea);
        QVERIFY(places->titleBarWidget());

        auto *panel = combined->findChild<CombinedPanel *>();
        QVERIFY(panel);
        auto *favorites = panel->findChild<QListView *>();
        auto *panelPlaces = panel->findChild<KFilePlacesView *>();
        auto *tree = panel->findChild<FolderTree *>();
        QVERIFY(favorites);
        QVERIFY(panelPlaces);
        QVERIFY(tree);
        QVERIFY(favorites->isHidden());
        QVERIFY(panelPlaces->isHidden());
        QVERIFY(tree->isHidden());
    }

    void jumpBackTogglesBetweenFolders()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        pane->setUrl(m_b);
        QAction *jumpBack = window.actionCollection()->action(QStringLiteral("jump_back"));
        QVERIFY(jumpBack && jumpBack->isEnabled());
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_a);
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_b);
    }

    void popularFoldersMenuListsVisitedLocations()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.activePane()->setUrl(m_b);

        QAction *popularAction = window.actionCollection()->action(QStringLiteral("popular_folders"));
        QVERIFY(popularAction && popularAction->menu());
        QMenu *menu = popularAction->menu();
        QMetaObject::invokeMethod(menu, "aboutToShow");
        QStringList names;
        for (QAction *action : menu->actions()) {
            names.append(action->text());
        }
        QVERIFY(names.contains(QStringLiteral("a")));
        QVERIFY(names.contains(QStringLiteral("b")));
    }

    void savedSelectionFilterSelectsMatchingFiles()
    {
        KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters")).writeEntry("Patterns", QStringList{QStringLiteral("*.txt")});
        MainWindow window;
        window.openUrls({m_a});
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);

        QAction *selectAction = window.actionCollection()->action(QStringLiteral("select_menu"));
        QVERIFY(selectAction && selectAction->menu());
        QMenu *saved = nullptr;
        for (QAction *action : selectAction->menu()->actions()) {
            if (action->menu() && action->text() == QLatin1String("Saved Selection Filters")) {
                saved = action->menu();
                break;
            }
        }
        QVERIFY(saved);
        QMetaObject::invokeMethod(saved, "aboutToShow");
        QCOMPARE(saved->actions().size(), 1);
        saved->actions().first()->trigger();
        QTRY_COMPARE(window.activePane()->view()->selectedItems().size(), 1);
        QCOMPARE(window.activePane()->view()->selectedItems().first().name(), QStringLiteral("file.txt"));
    }

    void occupiedSpaceActionUsesKioDirectorySize()
    {
        QFile nested(m_root.filePath(QStringLiteral("a/sub/nested.txt")));
        QVERIFY(nested.open(QIODevice::WriteOnly));
        QCOMPARE(nested.write("hi"), 2);
        nested.close();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")))});

        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("occupied in")));
        QVERIFY2(window.statusBar()->currentMessage().contains(QStringLiteral("5 B")), qPrintable(window.statusBar()->currentMessage()));

        view->clearSelection();
        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("folder")));
    }

    void filterPersistsWithThePaneSession()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            auto *filter = window.activePane()->findChild<FilterBar *>();
            QVERIFY(filter);
            filter->setFilter(NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QCOMPARE(restored.activePane()->view()->nameFilter(), NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
        QTRY_COMPARE(restored.activePane()->view()->itemCount(), 2);
    }

    void viewSettingsApplyToCurrentListing()
    {
        ViewSettings settings;
        settings.visibleColumns = {QStringLiteral("size")};
        settings.groupBy = QStringLiteral("ext");
        settings.sizeFormat = QStringLiteral("bytes");
        settings.dateFormat = QStringLiteral("iso");
        settings.iconNameLines = 4;
        settings.iconInformation = {QStringLiteral("size")};
        KConfigGroup viewConfig = KSharedConfig::openConfig()->group(QStringLiteral("View"));
        settings.writeConfig(viewConfig);
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.writeEntry("IgnoreList", QStringList{QStringLiteral("sub")});
        files.writeEntry("ExtensionColors", QStringList{QStringLiteral("txt=#123456")});
        KSharedConfig::openConfig()->sync();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QCOMPARE(view->groupBy(), QStringLiteral("ext"));

        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QVERIFY(!details->isColumnHidden(KDirModel::Size));
        QVERIFY(details->isColumnHidden(KDirModel::ModifiedTime));
        const QModelIndex nameIndex = details->model()->index(0, KDirModel::Name);
        QCOMPARE(details->model()->data(nameIndex, Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        QCOMPARE(details->model()->data(details->model()->index(0, KDirModel::Size)).toString(), QStringLiteral("5 B"));
        QVERIFY(QRegularExpression(QStringLiteral(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$)"))
                    .match(details->model()->data(details->model()->index(0, KDirModel::ModifiedTime)).toString())
                    .hasMatch());

        view->setViewMode(FolderView::ViewMode::Icons);
        auto *delegate = qobject_cast<KFileItemDelegate *>(view->currentView()->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), KFileItemDelegate::InformationList{KFileItemDelegate::Size});
        auto *icons = qobject_cast<QListView *>(view->currentView());
        QVERIFY(icons);
        QVERIFY(icons->gridSize().height() > 112);
    }

    void preferencesActionAppliesSettingsToOpenPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        QAction *preferences = window.actionCollection()->action(QStringLiteral("options_configure"));
        QVERIFY(preferences);
        QTimer::singleShot(0, &window, [] {
            auto *dialog = dynamic_cast<SettingsDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto *groupBy = dialog->findChild<QComboBox *>(QStringLiteral("default_group_by"));
            QVERIFY(groupBy);
            groupBy->setCurrentIndex(groupBy->findData(QStringLiteral("ext")));
            auto *typeColumn = dialog->findChild<QCheckBox *>(QStringLiteral("column_type"));
            QVERIFY(typeColumn);
            typeColumn->setChecked(false);
            QPushButton *ok = dialog->button(QDialogButtonBox::Ok);
            QVERIFY(ok);
            ok->click();
        });
        preferences->trigger();

        QCOMPARE(view->groupBy(), QStringLiteral("ext"));
        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QCOMPARE(window.inactivePane()->view()->groupBy(), QStringLiteral("ext"));
    }

    void copyToOtherPaneUsesKio()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        const QUrl source = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        FileOperations::copy({source}, window.inactivePane()->url(), &window);
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("b/file.txt"))));
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 1);
    }

    void stackedLayoutMeetsTabBarsInTheMiddle()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.inactivePane()->openInNewTab(m_a);
        window.resize(800, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.setStackedLayout(true);
        QVERIFY(window.isStackedLayout());
        Pane *top = window.activePane();
        Pane *bottom = window.inactivePane();
        auto *splitter = window.findChild<QSplitter *>();
        QCOMPARE(splitter->orientation(), Qt::Vertical);
        QCOMPARE(splitter->widget(0), top);
        QCOMPARE(splitter->widget(1), bottom);

        QTRY_VERIFY(tabBarOf(top)->isVisible() && tabBarOf(bottom)->isVisible());
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedSouth);
        QCOMPARE(tabBarOf(bottom)->shape(), QTabBar::RoundedNorth);
        // Top pane: view above tabs. Bottom pane: tabs above view.
        QTRY_VERIFY(globalTop(tabBarOf(top)) > globalTop(top->view()));
        QTRY_VERIFY(globalTop(tabBarOf(bottom)) < globalTop(bottom->view()));
        // Nothing but the splitter handle separates the two tab bars.
        const int gap = globalTop(tabBarOf(bottom)) - (globalTop(tabBarOf(top)) + tabBarOf(top)->height());
        QVERIFY2(gap >= 0 && gap <= splitter->handleWidth(), qPrintable(QString::number(gap)));

        window.setStackedLayout(false);
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedNorth);
        QTRY_VERIFY(globalTop(tabBarOf(top)) < globalTop(top->view()));
    }

    void stackedLayoutSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.setStackedLayout(true);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QVERIFY(restored.isStackedLayout());
    }

    void sessionSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.activePane()->openInNewTab(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))), true);
            QCOMPARE(window.activePane()->tabs()->count(), 3);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        // The first window restored an empty session (home), then added a and a/sub.
        const QList<QUrl> left = restored.activePane()->tabs()->urls();
        QVERIFY(left.contains(m_a));
        QVERIFY(left.contains(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")))));
        QCOMPARE(restored.activePane()->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
        QVERIFY(restored.inactivePane()->tabs()->urls().contains(m_b));
    }

private:
    static QTabBar *tabBarOf(Pane *pane)
    {
        return pane->findChild<QTabBar *>();
    }

    static int globalTop(QWidget *widget)
    {
        return widget->mapToGlobal(QPoint(0, 0)).y();
    }

    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_c;
};

QTEST_MAIN(MainWindowTest)

#include "mainwindowtest.moc"
