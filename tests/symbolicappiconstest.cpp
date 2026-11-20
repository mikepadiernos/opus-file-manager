// SPDX-License-Identifier: GPL-3.0-or-later

#include "favoritesmodel.h"
#include "filterbar.h"
#include "foldertree.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "symbolicactionicons.h"
#include "symbolicicon.h"
#include "testicontheme.h"

#include <KActionCollection>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KSharedConfig>
#include <KToolBar>

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeView>

using namespace TestIconTheme;

class SymbolicAppIconsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_themeDir.isValid());
        QVERIFY(install(m_themeDir.path(),
                        {},
                        {QStringLiteral("dialog-close"),
                         QStringLiteral("dialog-ok"),
                         QStringLiteral("edit-copy"),
                         QStringLiteral("edit-cut"),
                         QStringLiteral("foo"),
                         QStringLiteral("folder"),
                         QStringLiteral("inode-directory"),
                         QStringLiteral("user-home"),
                         QStringLiteral("user-trash"),
                         QStringLiteral("view-filter"),
                         QStringLiteral("view-list-details"),
                         QStringLiteral("view-list-icons"),
                         QStringLiteral("view-pim-notes"),
                         QStringLiteral("view-sort-ascending")},
                        {QStringLiteral("emblem-symbolic-link")}));
        qApp->installEventFilter(new SymbolicActionIcons(qApp));
    }

    void menuActionsUseSymbolicIcons()
    {
        QMenu menu;
        QAction *action = menu.addAction(QIcon::fromTheme(QStringLiteral("foo")), QStringLiteral("Foo"));
        QCOMPARE(colorAt(action->icon(), 16), SymbolicColor);
        QCOMPARE(colorAt(action->icon(), 48), RegularColor);
    }

    void changedActionIconsUseSymbolicIcons()
    {
        QMenu menu;
        QAction *action = menu.addAction(QStringLiteral("Foo"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("foo")));
        QCOMPARE(colorAt(action->icon(), 16), SymbolicColor);
    }

    void folderViewDecorationsUseSymbolicIcons()
    {
        QTemporaryDir root;
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("sub")));

        FolderView view;
        view.setPreviewsEnabled(false);
        view.setUrl(QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(view.itemCount(), 1);

        const QAbstractItemModel *model = view.currentView()->model();
        const QIcon icon = model->data(model->index(0, 0), Qt::DecorationRole).value<QIcon>();
        QCOMPARE(colorAt(icon, 16), SymbolicColor);
        QVERIFY(colorAt(icon, 64) != SymbolicColor);
    }

    void folderTreeDecorationsUseSymbolicIcons()
    {
        QTemporaryDir root;
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("sub")));

        FolderTree tree({QUrl::fromLocalFile(root.path())});
        auto *view = tree.findChild<QTreeView *>();
        QVERIFY(view);
        QTRY_COMPARE(view->model()->rowCount(), 1);

        const QIcon icon = view->model()->data(view->model()->index(0, 0), Qt::DecorationRole).value<QIcon>();
        QCOMPARE(colorAt(icon, 16), SymbolicColor);
        QVERIFY(colorAt(icon, 64) != SymbolicColor);
    }

    void symbolicFolderIconsKeepSymlinkEmblems()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("target")));
        QVERIFY(QFile::link(root.filePath(QStringLiteral("target")), root.filePath(QStringLiteral("link"))));
        FolderView view;
        view.setPreviewsEnabled(false);
        view.setUrl(QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(view.itemCount(), 2);
        const QAbstractItemModel *model = view.currentView()->model();
        const QModelIndexList links = model->match(model->index(0, 0), Qt::DisplayRole, QStringLiteral("link"));
        QCOMPARE(links.size(), 1);
        const QIcon icon = model->data(links.first(), Qt::DecorationRole).value<QIcon>();
        QVERIFY(containsColor(icon, 16, SymbolicColor));
        QVERIFY(containsColor(icon, 16, EmblemColor));

        FolderTree tree({QUrl::fromLocalFile(root.path())});
        auto *treeView = tree.findChild<QTreeView *>();
        QVERIFY(treeView);
        const QAbstractItemModel *treeModel = treeView->model();
        QTRY_COMPARE(treeModel->rowCount(), 1);
        const QModelIndex rootIndex = treeModel->index(0, 0);
        treeView->expand(rootIndex);
        QTRY_COMPARE(treeModel->rowCount(rootIndex), 2);
        const QModelIndexList treeLinks = treeModel->match(treeModel->index(0, 0, rootIndex), Qt::DisplayRole, QStringLiteral("link"));
        QCOMPARE(treeLinks.size(), 1);
        const QIcon treeIcon = treeModel->data(treeLinks.first(), Qt::DecorationRole).value<QIcon>();
        QVERIFY(containsColor(treeIcon, 16, SymbolicColor));
        QVERIFY(containsColor(treeIcon, 16, EmblemColor));
    }

    void favoritesDecorationsUseSymbolicIcons()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString groupName = QStringLiteral("SymbolicIconsTestFavorites");
        KSharedConfig::openConfig()->deleteGroup(groupName);
        FavoritesModel favorites(KSharedConfig::openConfig()->group(groupName));
        QVERIFY(favorites.add(QUrl::fromLocalFile(folder.path())));

        const QModelIndex index = favorites.index(0, 0);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        QCOMPARE(colorAt(icon, 16), SymbolicColor);
        QVERIFY(colorAt(icon, 64) != SymbolicColor);
    }

    void customWidgetIconsUseSymbolicIcons()
    {
        FilterBar filterBar;
        for (const QString &name : {QStringLiteral("filterScope"), QStringLiteral("filterClose")}) {
            const auto *button = filterBar.findChild<QToolButton *>(name);
            QVERIFY(button);
            QCOMPARE(colorAt(button->icon(), 16), SymbolicColor);
        }

        FolderView view;
        view.setSelectionMode(true);
        for (const QString &name :
             {QStringLiteral("selectionCopy"), QStringLiteral("selectionCut"), QStringLiteral("selectionTrash"), QStringLiteral("selectionDone")}) {
            const auto *button = view.findChild<QToolButton *>(name);
            QVERIFY(button);
            QCOMPARE(colorAt(button->icon(), 16), SymbolicColor);
        }
    }

    void placesUseSymbolicIcons()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        auto *places = window.findChild<KFilePlacesView *>();
        QVERIFY(places);
        const auto *model = qobject_cast<KFilePlacesModel *>(places->model());
        QVERIFY(model);

        const QModelIndex home = model->closestItem(QUrl::fromLocalFile(QDir::homePath()));
        QVERIFY(home.isValid());
        QCOMPARE(model->icon(home).name(), QStringLiteral("user-home"));
        QCOMPARE(colorAt(model->icon(home), 16), SymbolicColor);
    }

    void toolbarViewAndEditorIconsUseThemeVariants()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            for (const QString &name : {QStringLiteral("paneViewModeButton"), QStringLiteral("paneSortingButton")}) {
                QAction *action = pane->toolbarAction(name);
                auto *button = qobject_cast<QToolButton *>(pane->toolBar()->widgetForAction(action));
                QVERIFY(action && button);
                QCOMPARE(colorAt(button->icon(), 16), SymbolicColor);
                QCOMPARE(colorAt(button->icon(), 48), RegularColor);
            }
        }

        QAction *editor = window.actionCollection()->action(QStringLiteral("open_editor"));
        QAction *viewMode = window.actionCollection()->action(QStringLiteral("toggle_view_mode"));
        QVERIFY(editor && viewMode);
        QCOMPARE(editor->icon().name(), QStringLiteral("view-pim-notes"));
        QCOMPARE(colorAt(editor->icon(), 16), SymbolicColor);
        QCOMPARE(colorAt(editor->icon(), 48), RegularColor);
        QCOMPARE(colorAt(viewMode->icon(), 16), SymbolicColor);
        viewMode->trigger();
        QCOMPARE(colorAt(viewMode->icon(), 16), SymbolicColor);
        QCOMPARE(colorAt(viewMode->icon(), 48), RegularColor);
    }

private:
    QTemporaryDir m_themeDir;
};

QTEST_MAIN(SymbolicAppIconsTest)

#include "symbolicappiconstest.moc"
