// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "fileoperations.h"
#include "filterbar.h"
#include "folderview.h"
#include "pane.h"
#include "tabmodel.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QMenu>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>

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
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
    }

    void init()
    {
        // Start from a clean session so the first window does not inherit
        // tabs or layout from an earlier test.
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PopularFolders"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SelectionFilters"));
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
};

QTEST_MAIN(MainWindowTest)

#include "mainwindowtest.moc"
