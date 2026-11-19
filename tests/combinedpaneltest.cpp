// SPDX-License-Identifier: GPL-3.0-or-later

#include "combinedpanel.h"
#include "favoritesmodel.h"
#include "foldertree.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KSharedConfig>

#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QListView>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

class CombinedPanelTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;

    QUrl url(const QString &relative) const
    {
        return QUrl::fromLocalFile(m_root.filePath(relative));
    }

    /// Index of the child named @p name below @p parent, once listed.
    static QModelIndex childNamed(const QAbstractItemModel *model, const QModelIndex &parent, const QString &name)
    {
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const QModelIndex child = model->index(row, 0, parent);
            if (child.data().toString() == name) {
                return child;
            }
        }
        return {};
    }

    static QDockWidget *dock(MainWindow &window, const QString &name)
    {
        return window.findChild<QDockWidget *>(name);
    }

    static void clickRow(QAbstractItemView *view, const QModelIndex &index, Qt::MouseButton button = Qt::LeftButton)
    {
        view->scrollTo(index);
        QTest::mouseClick(view->viewport(), button, Qt::NoModifier, view->visualRect(index).center());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a/sub/deeper")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        m_a = url(QStringLiteral("a"));
        m_b = url(QStringLiteral("b"));
    }

    void init()
    {
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Favorites"));
        KSharedConfig::openConfig()->sync();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Panels"));
        KSharedConfig::openStateConfig()->sync();
    }

    void folderTreeShowsEachRootAndOnlyFolders()
    {
        FolderTree tree({m_a, m_b});
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        QCOMPARE(tree.roots(), QList<QUrl>({m_a, m_b}));
        const auto views = tree.findChildren<QTreeView *>();
        QCOMPARE(views.size(), 2);

        const QAbstractItemModel *model = views.first()->model();
        QTRY_COMPARE(model->rowCount(), 1);
        const QModelIndex root = model->index(0, 0);
        QCOMPARE(root.data().toString(), QStringLiteral("a"));

        views.first()->expand(root);
        QTRY_VERIFY(childNamed(model, root, QStringLiteral("sub")).isValid());
        QCOMPARE(model->rowCount(root), 1);
    }

    void folderTreeClickActivatesFolder()
    {
        FolderTree tree({m_a});
        tree.resize(300, 300);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        auto *view = tree.findChild<QTreeView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);

        QSignalSpy activated(&tree, &FolderTree::urlActivated);
        QSignalSpy tab(&tree, &FolderTree::tabRequested);
        clickRow(view, view->model()->index(0, 0));
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toUrl(), m_a);
        clickRow(view, view->model()->index(0, 0), Qt::MiddleButton);
        QCOMPARE(tab.size(), 1);
    }

    void folderTreeRestoresExpandedFolders()
    {
        FolderTree tree({m_a});
        tree.setExpandedUrls({m_a, url(QStringLiteral("a/sub")), QUrl::fromLocalFile(QStringLiteral("/elsewhere"))});
        QTRY_COMPARE(tree.expandedUrls(), QList<QUrl>({m_a, url(QStringLiteral("a/sub"))}));
    }

    void combinedPanelHasFavoritesPlacesAndTree()
    {
        KFilePlacesModel places;
        FavoritesModel favorites(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);

        QVERIFY(panel.findChild<KFilePlacesView *>());
        const auto lists = panel.findChildren<QListView *>();
        QVERIFY(std::any_of(lists.cbegin(), lists.cend(), [&favorites](QListView *list) {
            return list->model() == &favorites;
        }));
        QCOMPARE(panel.folderTree()->roots(), QList<QUrl>({QUrl::fromLocalFile(QDir::homePath()), QUrl::fromLocalFile(QStringLiteral("/"))}));
    }

    void combinedPanelShowsEveryPlaceWithoutScrolling()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.resize(1000, 650);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *view = window.findChild<CombinedPanel *>()->findChild<KFilePlacesView *>();
        const QAbstractItemModel *places = view->model();
        const auto lastVisibleRow = [view, places] {
            int last = -1;
            for (int row = 0; row < places->rowCount(); ++row) {
                if (!view->isRowHidden(row)) {
                    last = row;
                }
            }
            return last;
        };
        QTest::qWait(500);
        QVERIFY(lastVisibleRow() >= 0);
        QVERIFY(view->visualRect(places->index(lastVisibleRow(), 0)).bottom() < view->viewport()->height());
    }

    void combinedPanelActivatesFavorites()
    {
        KFilePlacesModel places;
        FavoritesModel favorites(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")));
        favorites.add(m_b);
        CombinedPanel panel(&places, &favorites);
        panel.resize(300, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        QListView *list = nullptr;
        for (QListView *candidate : panel.findChildren<QListView *>()) {
            if (candidate->model() == &favorites) {
                list = candidate;
            }
        }
        QVERIFY(list);
        QSignalSpy activated(&panel, &CombinedPanel::urlActivated);
        clickRow(list, favorites.index(0));
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toUrl(), m_b);
    }

    void windowDefaultsToCombinedPanel()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.panelLayout(), MainWindow::PanelLayout::Combined);
        QVERIFY(dock(window, QStringLiteral("combinedDock"))->isVisible());
        QVERIFY(!dock(window, QStringLiteral("placesDock"))->isVisible());
        QVERIFY(!dock(window, QStringLiteral("foldersDock"))->isVisible());
    }

    void separateLayoutShowsPlacesAndFoldersAndIsRemembered()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.setPanelLayout(MainWindow::PanelLayout::Separate);
            QVERIFY(!dock(window, QStringLiteral("combinedDock"))->isVisible());
            QVERIFY(dock(window, QStringLiteral("placesDock"))->isVisible());
            QVERIFY(dock(window, QStringLiteral("foldersDock"))->isVisible());
        }
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.panelLayout(), MainWindow::PanelLayout::Separate);
        QVERIFY(dock(window, QStringLiteral("foldersDock"))->isVisible());
    }

    void addToFavoritesAddsCurrentFolder()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.actionCollection()->action(QStringLiteral("add_to_favorites"))->trigger();
        QVERIFY(window.favorites()->contains(m_a));
    }

    void favoriteNavigatesActivePane()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.favorites()->add(m_b);

        auto *panel = window.findChild<CombinedPanel *>();
        QVERIFY(panel);
        Q_EMIT panel->urlActivated(m_b);
        QCOMPARE(window.activePane()->url(), m_b);
    }
};

QTEST_MAIN(CombinedPanelTest)

#include "combinedpaneltest.moc"
