// SPDX-License-Identifier: GPL-3.0-or-later

#include "combinedpanel.h"
#include "favoritesmodel.h"
#include "foldertree.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KDirModel>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KSharedConfig>

#include <QContextMenuEvent>
#include <QDir>
#include <QDockWidget>
#include <QDropEvent>
#include <QFile>
#include <QListView>
#include <QMimeData>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
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
        Settings::setConfirmClosingMultipleTabs(false);
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Favorites"));
        KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", false);
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
        tree.reload();
        QTRY_COMPARE(tree.expandedUrls(), QList<QUrl>({m_a, url(QStringLiteral("a/sub"))}));
    }

    void revealExpandsToAndSelectsTheFolder()
    {
        FolderTree tree({QUrl::fromLocalFile(m_root.path())});
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        QSignalSpy revealed(&tree, &FolderTree::revealed);
        const QUrl deeper = url(QStringLiteral("a/sub/deeper"));
        tree.reveal(deeper);

        auto *view = tree.findChild<QTreeView *>();
        QTRY_COMPARE(view->currentIndex().data(KDirModel::FileItemRole).value<KFileItem>().url(), deeper);
        QVERIFY(tree.expandedUrls().contains(url(QStringLiteral("a/sub"))));
        QTRY_VERIFY(!revealed.isEmpty());
    }

    void revealUsesTheInnermostRoot()
    {
        FolderTree tree({QUrl::fromLocalFile(m_root.path()), m_a});
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        tree.reveal(url(QStringLiteral("a/sub")));

        const auto views = tree.findChildren<QTreeView *>();
        QTRY_COMPARE(views.at(1)->currentIndex().data(KDirModel::FileItemRole).value<KFileItem>().url(), url(QStringLiteral("a/sub")));
        QVERIFY(!views.at(0)->currentIndex().isValid());
    }

    void hiddenFoldersFollowTheSetting()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b/.secret")));
        FolderTree tree({m_b});
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        auto *view = tree.findChild<QTreeView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        const QModelIndex root = view->model()->index(0, 0);
        view->expand(root);
        QTest::qWait(200);
        QVERIFY(!childNamed(view->model(), root, QStringLiteral(".secret")).isValid());

        tree.setShowHiddenFolders(true);
        QVERIFY(tree.showHiddenFolders());
        QTRY_VERIFY(childNamed(view->model(), view->model()->index(0, 0), QStringLiteral(".secret")).isValid());
    }

    void treeFoldersOfferAMenuAndTakeDrops()
    {
        FolderTree tree({m_a});
        tree.resize(300, 300);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        auto *view = tree.findChild<QTreeView *>();
        QTRY_COMPARE(view->model()->rowCount(), 1);
        const QPoint onRoot = view->visualRect(view->model()->index(0, 0)).center();

        QSignalSpy menu(&tree, &FolderTree::contextMenuRequested);
        QContextMenuEvent rightClick(QContextMenuEvent::Mouse, onRoot, view->viewport()->mapToGlobal(onRoot));
        QApplication::sendEvent(view->viewport(), &rightClick);
        QCOMPARE(menu.size(), 1);
        QCOMPARE(menu.first().first().toUrl(), m_a);

        QSignalSpy dropped(&tree, &FolderTree::urlsDropped);
        QMimeData mime;
        mime.setUrls({url(QStringLiteral("a/file.txt"))});
        QDragEnterEvent enter(onRoot, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &enter);
        QDragMoveEvent move(onRoot, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QVERIFY(move.isAccepted());
        QDropEvent drop(onRoot, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &drop);
        QCOMPARE(dropped.size(), 1);
        QCOMPARE(dropped.first().first().toUrl(), m_a);
    }

    void collapsedSectionsCanBeSetAndRead()
    {
        KFilePlacesModel places;
        FavoritesModel favorites(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);
        panel.setCollapsedSections({QStringLiteral("tree")});
        QCOMPARE(panel.collapsedSections(), QStringList({QStringLiteral("tree")}));
        QVERIFY(panel.folderTree()->isHidden());
        panel.setCollapsedSections({});
        QVERIFY(!panel.folderTree()->isHidden());
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

    void accordionHeadersAreLeftAligned()
    {
        KFilePlacesModel places;
        FavoritesModel favorites(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);
        panel.resize(400, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        const auto buttons = panel.findChildren<QToolButton *>();
        for (const QString &title : {QStringLiteral("Favorites"), QStringLiteral("Tree")}) {
            const auto it = std::find_if(buttons.cbegin(), buttons.cend(), [&title](const QToolButton *button) {
                return button->text() == title;
            });
            QVERIFY(it != buttons.cend());
            const QToolButton *button = *it;
            QCOMPARE(button->geometry().left(), 0);
            QCOMPARE(button->width(), button->parentWidget()->width());
        }
    }

    void placesGroupsCollapseIndependentlyAndKeepNativeActions()
    {
        KFilePlacesModel places;
        const bool placesHidden = places.isGroupHidden(KFilePlacesModel::PlacesType);
        const bool remoteHidden = places.isGroupHidden(KFilePlacesModel::RemoteType);
        const QUrl remote(QStringLiteral("sftp://example.invalid/opus-test"));
        const QUrl local = url(QStringLiteral("a"));
        const auto cleanup = qScopeGuard([&] {
            for (int row = places.rowCount() - 1; row >= 0; --row) {
                if (places.text(places.index(row, 0)) == QLatin1String("Opus test remote")
                    || places.text(places.index(row, 0)) == QLatin1String("Opus test local")) {
                    places.removePlace(places.index(row, 0));
                }
            }
            places.setGroupHidden(KFilePlacesModel::PlacesType, placesHidden);
            places.setGroupHidden(KFilePlacesModel::RemoteType, remoteHidden);
        });
        places.setGroupHidden(KFilePlacesModel::PlacesType, false);
        places.setGroupHidden(KFilePlacesModel::RemoteType, false);
        FavoritesModel favorites(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);
        panel.resize(400, 800);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        places.addPlace(QStringLiteral("Opus test remote"), remote);
        places.addPlace(QStringLiteral("Opus test local"), local);
        auto *placesHeader = panel.findChild<QToolButton *>(QStringLiteral("places:PlacesType"));
        auto *remoteHeader = panel.findChild<QToolButton *>(QStringLiteral("places:RemoteType"));
        QVERIFY(placesHeader);
        QVERIFY(remoteHeader);
        QTRY_VERIFY(placesHeader->isVisible());
        QTRY_VERIFY(remoteHeader->isVisible());
        QCOMPARE(placesHeader->parentWidget()->childAt(placesHeader->geometry().center()), placesHeader);
        QCOMPARE(remoteHeader->parentWidget()->childAt(remoteHeader->geometry().center()), remoteHeader);
        auto *placesView = placesHeader->parentWidget()->findChild<KFilePlacesView *>();
        auto *remoteView = remoteHeader->parentWidget()->findChild<KFilePlacesView *>();
        QVERIFY(placesView);
        QVERIFY(remoteView);
        QVERIFY(placesView->isDropOnPlaceEnabled());
        QTRY_VERIFY(!places.groupIndexes(KFilePlacesModel::RemoteType).isEmpty());
        const QModelIndexList remoteIndexes = places.groupIndexes(KFilePlacesModel::RemoteType);
        QTRY_VERIFY(!remoteView->isRowHidden(remoteIndexes.first().row()));
        for (const QModelIndex &index : remoteIndexes) {
            QTRY_VERIFY(placesView->isRowHidden(index.row()));
        }
        QTest::qWait(700);
        for (const QModelIndex &index : places.groupIndexes(KFilePlacesModel::RemoteType)) {
            QVERIFY(placesView->isRowHidden(index.row()));
        }
        QTest::mouseClick(remoteHeader, Qt::LeftButton);
        QVERIFY(remoteView->isHidden());
        QVERIFY(!placesView->isHidden());
        QVERIFY(panel.collapsedSections().contains(QStringLiteral("places:RemoteType")));
        QVERIFY(!places.isGroupHidden(KFilePlacesModel::RemoteType));
        places.setGroupHidden(KFilePlacesModel::RemoteType, true);
        QTRY_VERIFY(!remoteHeader->isVisible());
        places.setGroupHidden(KFilePlacesModel::RemoteType, false);
        QTRY_VERIFY(remoteHeader->isVisible());
        QVERIFY(remoteView->isHidden());
        panel.setSectionVisibility(true, false, true);
        panel.setSectionVisibility(true, true, true);
        QVERIFY(remoteView->isHidden());
        QTest::mouseClick(remoteHeader, Qt::LeftButton);
        QVERIFY(!remoteView->isHidden());
        QVERIFY(!panel.collapsedSections().contains(QStringLiteral("places:RemoteType")));
        QModelIndex localIndex;
        for (const QModelIndex &index : places.groupIndexes(KFilePlacesModel::PlacesType)) {
            if (places.text(index) == QLatin1String("Opus test local")) {
                localIndex = index;
            }
        }
        QVERIFY(localIndex.isValid());
        const QModelIndexList placesIndexes = places.groupIndexes(KFilePlacesModel::PlacesType);
        QVERIFY(placesIndexes.size() >= 2);
        const int nativeHeaderHeight = placesView->sizeHintForIndex(placesIndexes.at(0)).height() - placesView->sizeHintForIndex(placesIndexes.at(1)).height();
        QCOMPARE(placesHeader->height(), nativeHeaderHeight);
        QSignalSpy activated(&panel, &CombinedPanel::urlActivated);
        QTRY_VERIFY(!placesView->visualRect(localIndex).isEmpty());
        clickRow(placesView, localIndex);
        QTRY_COMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toUrl(), local);
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
        QCOMPARE(dock(window, QStringLiteral("combinedDock"))->windowTitle(), QStringLiteral("Explorer Panel"));
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

    void treeFollowsTheActivePane()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto views = window.findChild<CombinedPanel *>()->folderTree()->findChildren<QTreeView *>();
        // m_a is below Root, not Home, so the Root tree reveals it.
        QTRY_COMPARE(views.at(1)->currentIndex().data(KDirModel::FileItemRole).value<KFileItem>().url(), m_a);
    }

    void treeShowsHiddenFoldersWithThePane()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.actionCollection()->action(QStringLiteral("show_hidden_files"))->trigger();
        QVERIFY(window.findChild<CombinedPanel *>()->folderTree()->showHiddenFolders());
    }

    void collapsedSectionsAreRemembered()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            window.findChild<CombinedPanel *>()->setCollapsedSections({QStringLiteral("favorites"), QStringLiteral("places:PlacesType")});
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(window.close());
        }
        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.findChild<CombinedPanel *>()->collapsedSections(), (QStringList{QStringLiteral("favorites"), QStringLiteral("places:PlacesType")}));
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
