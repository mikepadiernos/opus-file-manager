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

#include <QAccessible>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QKeySequence>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
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

    void mixedSectionsStayAdjacent()
    {
        QTemporaryDir configDir;
        QVERIFY(configDir.isValid());
        const auto config = KSharedConfig::openConfig(configDir.filePath(QStringLiteral("favorites")));
        KFilePlacesModel places;
        const bool remoteHidden = places.isGroupHidden(KFilePlacesModel::RemoteType);
        places.setGroupHidden(KFilePlacesModel::RemoteType, false);
        const QUrl remote(QStringLiteral("sftp://example.invalid/spacing-test"));
        places.addPlace(QStringLiteral("Spacing test remote"), remote);
        const auto cleanup = qScopeGuard([&] {
            for (int row = places.rowCount() - 1; row >= 0; --row) {
                if (places.url(places.index(row, 0)) == remote) {
                    places.removePlace(places.index(row, 0));
                }
            }
            places.setGroupHidden(KFilePlacesModel::RemoteType, remoteHidden);
        });
        FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
        favorites.add(m_a);
        CombinedPanel panel(&places, &favorites);
        panel.setCollapsedSections(
            {QStringLiteral("tree"), QStringLiteral("places:PlacesType"), QStringLiteral("places:DevicesType"), QStringLiteral("places:RemovableDevicesType")});
        for (const int height : {300, 800, 1600}) {
            panel.resize(400, height);
            panel.show();
            QVERIFY(QTest::qWaitForWindowExposed(&panel));
            auto *scroll = panel.findChild<QScrollArea *>();
            QVERIFY(scroll && scroll->widget());
            auto *column = scroll->widget()->layout();
            QVERIFY(column);
            QTRY_VERIFY(panel.findChild<QToolButton *>(QStringLiteral("places:RemoteType"))->isVisible());
            QTest::qWait(100);
            QWidget *previous = nullptr;
            for (int i = 0; i < column->count(); ++i) {
                QWidget *section = column->itemAt(i)->widget();
                if (!section || section->isHidden()) {
                    continue;
                }
                if (previous) {
                    QCOMPARE(section->geometry().top(), previous->geometry().bottom() + 1);
                }
                if (auto *header = section->findChild<QToolButton *>()) {
                    QCOMPARE(header->geometry().top(), 0);
                    QCOMPARE(section->height(), section->layout()->sizeHint().height());
                    if (header->arrowType() == Qt::RightArrow) {
                        QCOMPARE(section->height(), header->height());
                    }
                }
                previous = section;
            }
        }
        auto *remoteHeader = panel.findChild<QToolButton *>(QStringLiteral("places:RemoteType"));
        QVERIFY(remoteHeader);
        QTest::mouseClick(remoteHeader, Qt::LeftButton);
        QTRY_COMPARE(remoteHeader->parentWidget()->height(), remoteHeader->height());
        const QStringList collapsed = panel.collapsedSections();
        panel.setSectionVisibility(true, false, true);
        panel.setSectionVisibility(true, true, true);
        QVERIFY(panel.collapsedSections().contains(QStringLiteral("places:RemoteType")));
        panel.setCollapsedSections(collapsed);
        QTRY_COMPARE(remoteHeader->parentWidget()->height(), remoteHeader->height());
        QTest::mouseClick(remoteHeader, Qt::LeftButton);
        QTRY_COMPARE(remoteHeader->parentWidget()->height(), remoteHeader->parentWidget()->layout()->sizeHint().height());
        auto *treeHeader = panel.findChild<QToolButton *>(QStringLiteral("tree"));
        QVERIFY(treeHeader);
        QTest::mouseClick(treeHeader, Qt::LeftButton);
        QTRY_COMPARE(treeHeader->parentWidget()->height(), treeHeader->parentWidget()->layout()->sizeHint().height());
        QCOMPARE(treeHeader->geometry().top(), 0);
        for (int i = 0; i < 30; ++i) {
            QVERIFY(favorites.add(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("favorite-%1").arg(i)))));
        }
        panel.resize(400, 300);
        auto *scroll = panel.findChild<QScrollArea *>();
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        scroll->ensureWidgetVisible(treeHeader);
        const int headerTop = treeHeader->mapTo(scroll->viewport(), QPoint()).y();
        QVERIFY(headerTop >= 0);
        QVERIFY(headerTop + treeHeader->height() <= scroll->viewport()->height());
    }

    void sectionTitlesRenderMnemonics()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto config = KSharedConfig::openConfig();
        KConfigGroup sections = config->group(QStringLiteral("CombinedPanelSections"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("settings")));
        KConfigGroup saved = backup->group(QStringLiteral("CombinedPanelSections"));
        sections.copyTo(&saved);
        const auto cleanup = qScopeGuard([&] {
            sections.deleteGroup();
            saved.copyTo(&sections);
            config->sync();
        });
        sections.deleteGroup();
        sections.group(QStringLiteral("Titles")).writeEntry("favorites", QStringLiteral("R&&D &Zebra"));
        KFilePlacesModel places;
        FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);
        panel.resize(400, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto *header = panel.findChild<QToolButton *>(QStringLiteral("favorites"));
        QVERIFY(header);
        QCOMPARE(header->text(), QStringLiteral("R&&D &Zebra"));
        QCOMPARE(header->accessibleName(), QStringLiteral("R&D Zebra"));
        QVERIFY(QAccessible::queryAccessibleInterface(header));
        QCOMPARE(QAccessible::queryAccessibleInterface(header)->text(QAccessible::Name), QStringLiteral("R&D Zebra"));
        QCOMPARE(header->shortcut(), QKeySequence::mnemonic(header->text()));

        panel.activateWindow();
        QTRY_VERIFY(panel.isActiveWindow());
        const bool initiallyExpanded = header->isChecked();
        QTest::keyClick(header, Qt::Key_Z, Qt::AltModifier);
        QTRY_COMPARE(header->isChecked(), !initiallyExpanded);

        QPalette colors = header->palette();
        colors.setColor(QPalette::Accent, QColor(255, 0, 255));
        header->setPalette(colors);
        QImage withMnemonic(header->size(), QImage::Format_ARGB32_Premultiplied);
        withMnemonic.fill(Qt::transparent);
        header->render(&withMnemonic);
        header->setText(QStringLiteral("R&&D Zebra"));
        QImage withoutMnemonic(header->size(), QImage::Format_ARGB32_Premultiplied);
        withoutMnemonic.fill(Qt::transparent);
        header->render(&withoutMnemonic);
        int accentPixels = 0;
        for (int y = 0; y < withMnemonic.height(); ++y) {
            for (int x = 0; x < withMnemonic.width(); ++x) {
                if (withMnemonic.pixelColor(x, y) != withoutMnemonic.pixelColor(x, y)) {
                    const QColor color = withMnemonic.pixelColor(x, y);
                    QVERIFY(color.red() > color.green() && color.blue() > color.green());
                    ++accentPixels;
                }
            }
        }
        QVERIFY(accentPixels > 0);

        colors.setColor(QPalette::Accent, QColor(0, 255, 0));
        header->setPalette(colors);
        header->setText(QStringLiteral("R&&D &Zebra"));
        QImage changedAccent(header->size(), QImage::Format_ARGB32_Premultiplied);
        changedAccent.fill(Qt::transparent);
        header->render(&changedAccent);
        int greenPixels = 0;
        for (int y = 0; y < changedAccent.height(); ++y) {
            for (int x = 0; x < changedAccent.width(); ++x) {
                if (changedAccent.pixelColor(x, y) != withoutMnemonic.pixelColor(x, y)) {
                    const QColor color = changedAccent.pixelColor(x, y);
                    QVERIFY(color.green() > color.red() && color.green() > color.blue());
                    ++greenPixels;
                }
            }
        }
        QVERIFY(greenPixels > 0);

        for (const Qt::LayoutDirection direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            header->setLayoutDirection(direction);
            header->resize(60, header->height());
            header->setText(direction == Qt::LeftToRight ? QStringLiteral("&Z with a very long title") : QStringLiteral("A very long title with &Z"));
            QImage visibleMnemonic(header->size(), QImage::Format_ARGB32_Premultiplied);
            visibleMnemonic.fill(Qt::transparent);
            header->render(&visibleMnemonic);
            header->setText(direction == Qt::LeftToRight ? QStringLiteral("Z with a very long title") : QStringLiteral("A very long title with Z"));
            QImage visibleWithoutMnemonic(header->size(), QImage::Format_ARGB32_Premultiplied);
            visibleWithoutMnemonic.fill(Qt::transparent);
            header->render(&visibleWithoutMnemonic);
            QVERIFY(visibleMnemonic != visibleWithoutMnemonic);

            header->setText(direction == Qt::LeftToRight ? QStringLiteral("A very long title with &Z") : QStringLiteral("&Z with a very long title"));
            QImage clipped(header->size(), QImage::Format_ARGB32_Premultiplied);
            clipped.fill(Qt::transparent);
            header->render(&clipped);
            header->setText(direction == Qt::LeftToRight ? QStringLiteral("A very long title with Z") : QStringLiteral("Z with a very long title"));
            QImage clippedWithoutMnemonic(header->size(), QImage::Format_ARGB32_Premultiplied);
            clippedWithoutMnemonic.fill(Qt::transparent);
            header->render(&clippedWithoutMnemonic);
            QCOMPARE(clipped, clippedWithoutMnemonic);
        }

        header->setFocus();
        const bool expanded = header->isChecked();
        QTest::keyClick(header, Qt::Key_Space);
        QCOMPARE(header->isChecked(), !expanded);
    }

    void combinedSectionsCanBeCustomizedAndRestored()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto config = KSharedConfig::openConfig();
        KConfigGroup group = config->group(QStringLiteral("CombinedPanelSections"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("settings")));
        KConfigGroup saved = backup->group(QStringLiteral("CombinedPanelSections"));
        group.copyTo(&saved);
        const bool favoritesVisible = Settings::favoritesVisible();
        const bool placesVisible = Settings::placesVisible();
        const bool foldersVisible = Settings::foldersVisible();
        const auto restore = qScopeGuard([&] {
            group.deleteGroup();
            saved.copyTo(&group);
            Settings::setFavoritesVisible(favoritesVisible);
            Settings::setPlacesVisible(placesVisible);
            Settings::setFoldersVisible(foldersVisible);
            Settings::self()->save();
            config->sync();
        });
        group.deleteGroup();
        Settings::setFavoritesVisible(true);
        Settings::setPlacesVisible(true);
        Settings::setFoldersVisible(true);

        KFilePlacesModel places;
        const QUrl remoteUrl(QStringLiteral("sftp://example.invalid/combined-customization"));
        const QUrl extraUrl(QStringLiteral("sftp://example.invalid/combined-customization-extra"));
        places.addPlace(QStringLiteral("Combined customization remote"), remoteUrl);
        const auto removePlace = qScopeGuard([&] {
            for (int row = places.rowCount() - 1; row >= 0; --row) {
                if (places.url(places.index(row, 0)) == remoteUrl || places.url(places.index(row, 0)) == extraUrl) {
                    places.removePlace(places.index(row, 0));
                }
            }
        });
        FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
        CombinedPanel panel(&places, &favorites);
        panel.resize(400, 800);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = panel.findChild<QDialog *>(QStringLiteral("combinedPanelSectionsDialog"));
            QVERIFY(dialog);
            auto *list = dialog->findChild<QListWidget *>(QStringLiteral("combinedSectionsList"));
            QVERIFY(list);
            list->item(0)->setCheckState(Qt::Unchecked);
            dialog->reject();
        });
        panel.configureSections();
        QVERIFY(Settings::favoritesVisible());
        QVERIFY(!group.hasKey("Order"));
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = panel.findChild<QDialog *>(QStringLiteral("combinedPanelSectionsDialog"));
            QVERIFY(dialog);
            const auto close = qScopeGuard([dialog] {
                if (dialog->isVisible()) {
                    dialog->reject();
                }
            });
            auto *list = dialog->findChild<QListWidget *>(QStringLiteral("combinedSectionsList"));
            auto *title = dialog->findChild<QLineEdit *>(QStringLiteral("combinedSectionTitle"));
            auto *up = dialog->findChild<QPushButton *>(QStringLiteral("combinedSectionUp"));
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(list && title && up && buttons);
            const auto find = [list](const QString &key) {
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->data(Qt::UserRole).toString() == key) {
                        return i;
                    }
                }
                return -1;
            };
            const int tree = find(QStringLiteral("tree"));
            const int favorite = find(QStringLiteral("favorites"));
            const int remote = find(QStringLiteral("places:RemoteType"));
            QVERIFY(tree >= 0 && favorite >= 0 && remote >= 0);
            list->item(favorite)->setCheckState(Qt::Unchecked);
            list->item(remote)->setCheckState(Qt::Unchecked);
            list->setCurrentRow(remote);
            title->setText(QStringLiteral("Cloud"));
            list->setCurrentRow(tree);
            title->setText(QStringLiteral("My folders"));
            while (list->currentRow() > 0) {
                up->click();
            }
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        panel.configureSections();
        QVERIFY(!Settings::favoritesVisible());
        QCOMPARE(group.readEntry("Order", QStringList()).first(), QStringLiteral("tree"));
        QVERIFY(group.readEntry("HiddenSections", QStringList()).contains(QStringLiteral("places:RemoteType")));
        auto *tree = panel.findChild<QToolButton *>(QStringLiteral("tree"));
        auto *favorite = panel.findChild<QToolButton *>(QStringLiteral("favorites"));
        auto *remote = panel.findChild<QToolButton *>(QStringLiteral("places:RemoteType"));
        QVERIFY(tree && favorite && remote);
        QCOMPARE(tree->text(), QStringLiteral("My folders"));
        QCOMPARE(remote->text(), QStringLiteral("Cloud"));
        QCOMPARE(tree->parentWidget()->parentWidget()->layout()->itemAt(0)->widget(), tree->parentWidget());
        QVERIFY(favorite->parentWidget()->isHidden());
        QVERIFY(remote->parentWidget()->isHidden());
        places.addPlace(QStringLiteral("Another customization remote"), extraUrl);
        QCoreApplication::processEvents();
        QVERIFY(remote->parentWidget()->isHidden());
        QCOMPARE(remote->text(), QStringLiteral("Cloud"));

        CombinedPanel restoredPanel(&places, &favorites);
        restoredPanel.setSectionVisibility(Settings::favoritesVisible(), Settings::placesVisible(), Settings::foldersVisible());
        auto *restoredTree = restoredPanel.findChild<QToolButton *>(QStringLiteral("tree"));
        QVERIFY(restoredTree);
        QCOMPARE(restoredTree->text(), QStringLiteral("My folders"));
        QCOMPARE(restoredTree->parentWidget()->parentWidget()->layout()->itemAt(0)->widget(), restoredTree->parentWidget());
        QVERIFY(restoredPanel.findChild<QToolButton *>(QStringLiteral("favorites"))->parentWidget()->isHidden());
        restoredPanel.setSectionVisibility(true, false, true);
        QVERIFY(!restoredPanel.findChild<QToolButton *>(QStringLiteral("favorites"))->parentWidget()->isHidden());
        restoredPanel.setSectionVisibility(true, true, true);
        QVERIFY(restoredPanel.findChild<QToolButton *>(QStringLiteral("places:RemoteType"))->parentWidget()->isHidden());
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
        QCOMPARE(dock(window, QStringLiteral("combinedDock"))->windowTitle(), QStringLiteral("Explorer"));
        QVERIFY(dock(window, QStringLiteral("combinedDock"))->isVisible());
        QVERIFY(!dock(window, QStringLiteral("placesDock"))->isVisible());
        QVERIFY(!dock(window, QStringLiteral("foldersDock"))->isVisible());
    }

    void separateLayoutShowsPlacesAndFoldersAndIsRemembered()
    {
        Settings::setShowPlaces(true);
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
