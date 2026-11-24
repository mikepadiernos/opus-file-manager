// SPDX-License-Identifier: GPL-3.0-or-later

#include "carddelegate.h"
#include "colorschemes.h"
#include "filterbar.h"
#include "foldercolors.h"
#include "foldermetadata.h"
#include "folderview.h"
#include "labelcolor.h"
#include "mainwindow.h"
#include "pane.h"
#include "session.h"
#include "settings.h"
#include "tabcolors.h"

#include <KActionCollection>
#include <KCategorizedSortFilterProxyModel>
#include <KCategorizedView>
#include <KDirModel>
#include <KFilePlacesModel>
#include <KIO/Global>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

class FilterTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;

    static QStringList visibleNames(FolderView *view)
    {
        QStringList names;
        const QAbstractItemModel *model = view->currentView()->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            names.append(model->index(row, 0).data().toString());
        }
        names.sort();
        return names;
    }

    void touch(const QString &relative)
    {
        QFile file(m_root.filePath(relative));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a/Pictures")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        touch(QStringLiteral("a/photo.jpg"));
        touch(QStringLiteral("a/notes.txt"));
        touch(QStringLiteral("b/scan.jpg"));
        touch(QStringLiteral("b/todo.txt"));
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        QFile::remove(m_root.filePath(QStringLiteral("a/.mfm.json")));
        QFile::remove(m_root.filePath(QStringLiteral("b/.mfm.json")));
    }

    void viewFilterHidesFilesButKeepsFolders()
    {
        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        view.setNameFilter(NameFilter(QStringLiteral("*.jpg")));
        QCOMPARE(visibleNames(&view), QStringList({QStringLiteral("Pictures"), QStringLiteral("photo.jpg")}));
        view.setNameFilter(NameFilter());
        QCOMPARE(view.itemCount(), 3);
    }

    void detailsCanExpandFoldersInline()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent/nested")));
        QFile child(root.filePath(QStringLiteral("parent/nested/file.txt")));
        QVERIFY(child.open(QIODevice::WriteOnly));
        child.close();
        FolderView view;
        view.setPreviewsEnabled(false);
        const QUrl folder = QUrl::fromLocalFile(root.path());
        const QUrl fileUrl = QUrl::fromLocalFile(child.fileName());
        view.setUrl(folder);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_COMPARE(view.itemCount(), 1);
        auto *tree = qobject_cast<QTreeView *>(view.currentView());
        QVERIFY(tree);
        QVERIFY(tree->rootIsDecorated());
        const QModelIndex parent = tree->model()->index(0, 0);
        const QRect row = tree->visualRect(parent);
        const QPoint branch(row.left() - tree->indentation() / 2, row.center().y());
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, branch);
        QVERIFY(tree->isExpanded(parent));
        QTRY_COMPARE(tree->model()->rowCount(parent), 1);
        const QModelIndex nested = tree->model()->index(0, 0, parent);
        tree->expand(nested);
        QTRY_COMPARE(tree->model()->rowCount(nested), 1);
        QCOMPARE(view.url(), folder);
        view.selectUrls({fileUrl});
        QCOMPARE(view.selectedItems().size(), 1);
        QCOMPARE(view.selectedItems().first().url(), fileUrl);
        QCOMPARE(view.itemsInViewOrder().size(), 3);
        view.setViewMode(FolderView::ViewMode::Icons);
        QVERIFY(view.selectedItems().isEmpty());
        QVERIFY(!view.currentView()->currentIndex().parent().isValid());
        view.setViewMode(FolderView::ViewMode::Details);
        view.selectUrls({fileUrl});
        QCOMPARE(view.selectedItems().size(), 1);
        const QModelIndex childIndex = tree->model()->index(0, 0, nested);
        tree->selectionModel()->setCurrentIndex(childIndex, QItemSelectionModel::NoUpdate);
        tree->collapse(parent);
        QVERIFY(view.selectedItems().isEmpty());
        QCOMPARE(tree->currentIndex(), parent);
        QCOMPARE(view.itemsInViewOrder().size(), 1);
        view.setFlat(true);
        QVERIFY(!tree->itemsExpandable());
        view.setFlat(false);
        QVERIFY(tree->itemsExpandable());
    }

    void inheritedFormatAndPaneLock()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("child/grandchild")));
        const QString child = root.filePath(QStringLiteral("child"));
        const QString grandchild = root.filePath(QStringLiteral("child/grandchild"));
        FolderMetadata::Format format{QStringLiteral("compact"), {QStringLiteral("size")}, QStringLiteral("size"), false, QStringLiteral("type"), true, false};
        QVERIFY(FolderMetadata::setFormat(root.path(), format));
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(child));
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Compact);
        QCOMPARE(view.groupBy(), QStringLiteral("type"));
        QVERIFY(view.formatRuleDescription().contains(root.path()));
        view.setFormatLocked(true);
        view.setViewMode(FolderView::ViewMode::Icons);
        view.setUrl(QUrl::fromLocalFile(grandchild));
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(view.groupBy(), QStringLiteral("type"));
        QVERIFY(!QFile::exists(QDir(grandchild).filePath(QStringLiteral(".mfm.json"))));
        view.setFormatLocked(false);
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Compact);
        view.setFormatIncludesDescendants(true);
        QVERIFY(view.formatIncludesDescendants());
        FolderMetadata::Format saved;
        QVERIFY(FolderMetadata::format(FolderMetadata::read(grandchild), &saved));
        QVERIFY(saved.includeDescendants);
    }

    void inheritedAndLockedFormatsRestoreColumnOrder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("child")));
        FolderMetadata::Format format{QStringLiteral("list"),
                                      {QStringLiteral("owner"), QStringLiteral("size"), QStringLiteral("type")},
                                      QStringLiteral("name"),
                                      true,
                                      QStringLiteral("none"),
                                      true};
        QVERIFY(FolderMetadata::setFormat(root.path(), format));
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("child"))));
        auto *tree = qobject_cast<QTreeView *>(view.currentView());
        QVERIFY(tree);
        const auto verifyOrder = [tree] {
            QCOMPARE(tree->header()->visualIndex(KDirModel::Name), 0);
            QCOMPARE(tree->header()->visualIndex(KDirModel::Owner), 1);
            QCOMPARE(tree->header()->visualIndex(KDirModel::Size), 2);
            QCOMPARE(tree->header()->visualIndex(KDirModel::Type), 3);
        };
        verifyOrder();
        view.setFormatLocked(true);
        view.setUrl(QUrl::fromLocalFile(root.path()));
        verifyOrder();
        view.setFormatLocked(false);
        verifyOrder();
        QTemporaryDir other;
        QVERIFY(other.isValid());
        view.setFormatLocked(true);
        view.setUrl(QUrl::fromLocalFile(other.path()));
        verifyOrder();
        view.setFormatLocked(false);
        for (int column = 0; column < tree->header()->count(); ++column) {
            QCOMPARE(tree->header()->visualIndex(column), column);
        }
    }

    void removingLocalFormatRestoresTheInheritedFormat()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("child")));
        const QString child = root.filePath(QStringLiteral("child"));
        FolderMetadata::Format parent{QStringLiteral("grid"), {}, QStringLiteral("name"), true, QStringLiteral("none"), true};
        FolderMetadata::Format local{QStringLiteral("list"), {}, QStringLiteral("name"), true, QStringLiteral("none")};
        QVERIFY(FolderMetadata::setFormat(root.path(), parent));
        QVERIFY(FolderMetadata::setFormat(child, local));
        QVERIFY(FolderMetadata::setNote(child, QStringLiteral("file"), QStringLiteral("keep")));
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(child));
        QVERIFY(view.hasLocalFolderFormat());
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Details);
        view.setFormatLocked(true);
        view.clearLocalFolderFormat();
        QVERIFY(view.hasLocalFolderFormat());
        view.setFormatLocked(false);
        view.clearLocalFolderFormat();
        QVERIFY(!view.hasLocalFolderFormat());
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(view.formatRuleSource(), root.path());
        QCOMPARE(FolderMetadata::read(child).value(QStringLiteral("notes")).toObject().value(QStringLiteral("file")).toString(), QStringLiteral("keep"));
    }

    void folderSizesAppearOnDemandAndFromInheritedRule()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("child/small")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("child/large")));
        QFile small(root.filePath(QStringLiteral("child/small/file")));
        QVERIFY(small.open(QIODevice::WriteOnly));
        QCOMPARE(small.write(QByteArray(12, 'a')), 12);
        small.close();
        QFile large(root.filePath(QStringLiteral("child/large/file")));
        QVERIFY(large.open(QIODevice::WriteOnly));
        QCOMPARE(large.write(QByteArray(80, 'b')), 80);
        large.close();
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("child"))));
        view.setShowHiddenFiles(false);
        QTRY_COMPARE(view.itemCount(), 2);
        view.setGroupBy(QStringLiteral("none"));
        view.setSorting(KDirModel::Size, Qt::AscendingOrder);
        QCOMPARE(view.sortColumn(), int(KDirModel::Size));
        QCOMPARE(view.sortOrder(), Qt::AscendingOrder);
        view.calculateFolderSizes();
        QAbstractItemModel *model = view.currentView()->model();
        QTRY_COMPARE(model->index(0, KDirModel::Size).data().toString(), KIO::convertSize(12));
        QTRY_COMPARE(model->index(1, KDirModel::Size).data().toString(), KIO::convertSize(80));
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("small"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("large"));
        auto *tree = qobject_cast<QTreeView *>(view.currentView());
        QVERIFY(tree);
        tree->expand(model->index(0, 0));
        QTRY_COMPARE(model->rowCount(model->index(0, 0)), 1);
        QVERIFY(small.open(QIODevice::Append));
        QCOMPARE(small.write(QByteArray(20, 'a')), 20);
        small.close();
        QTRY_COMPARE(model->index(0, KDirModel::Size).data().toString(), KIO::convertSize(32));

        view.setUrl(QUrl::fromLocalFile(root.path()));
        QVERIFY(QFile::remove(root.filePath(QStringLiteral("child/.mfm.json"))));
        view.setGroupBy(QStringLiteral("none"));
        view.setSorting(KDirModel::Size, Qt::AscendingOrder);
        view.setAutomaticFolderSizes(true);
        view.setFormatIncludesDescendants(true);
        view.setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("child"))));
        view.setShowHiddenFiles(false);
        QTRY_COMPARE(view.itemCount(), 2);
        model = view.currentView()->model();
        QTRY_COMPARE(model->index(0, KDirModel::Size).data().toString(), KIO::convertSize(32));
        QTRY_COMPARE(model->index(1, KDirModel::Size).data().toString(), KIO::convertSize(80));
    }

    void periodicallyRefreshesCalculatedFolderSizes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("selected/nested")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("other/nested")));
        QFile file(root.filePath(QStringLiteral("selected/nested/file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray(12, 'a')), 12);
        file.close();
        const int originalInterval = Settings::folderSizeRefreshInterval();
        const auto restoreInterval = qScopeGuard([originalInterval] {
            Settings::setFolderSizeRefreshInterval(originalInterval);
        });
        Settings::setFolderSizeRefreshInterval(0);
        FolderView view;
        view.show();
        view.setUrl(QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(view.itemCount(), 2);
        view.selectUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("selected")))});
        QCOMPARE(view.selectedItems().size(), 1);
        view.calculateFolderSizes();
        QAbstractItemModel *model = view.currentView()->model();
        QModelIndex selected = model->index(1, KDirModel::Size);
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("selected"));
        QTRY_COMPARE(selected.data().toString(), KIO::convertSize(12));
        const QString otherSize = model->index(0, KDirModel::Size).data().toString();

        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write(QByteArray(20, 'b')), 20);
        file.close();
        QTest::qWait(1200);
        QCOMPARE(selected.data().toString(), KIO::convertSize(12));
        Settings::setFolderSizeRefreshInterval(1);
        view.reloadListingSettings();
        QTRY_COMPARE(selected.data().toString(), KIO::convertSize(32));
        QCOMPARE(model->index(0, KDirModel::Size).data().toString(), otherSize);

        Settings::setFolderSizeRefreshInterval(0);
        view.reloadListingSettings();
        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write(QByteArray(10, 'c')), 10);
        file.close();
        QTest::qWait(1200);
        QCOMPARE(selected.data().toString(), KIO::convertSize(32));
    }

    void viewFilterStaysSetInAnotherFolder()
    {
        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        view.setNameFilter(NameFilter(QStringLiteral("*.txt")));
        view.setUrl(m_b);
        QTRY_COMPARE(visibleNames(&view), QStringList({QStringLiteral("todo.txt")}));
        QCOMPARE(view.nameFilter(), NameFilter(QStringLiteral("*.txt")));
    }

    void activateFirstItemOpensFirstVisible()
    {
        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        view.setNameFilter(NameFilter(QStringLiteral("notes")));
        QSignalSpy activated(&view, &FolderView::itemActivated);
        view.activateFirstItem();
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().value<KFileItem>().name(), QStringLiteral("notes.txt"));
    }

    void ignoredNamesAreNeverListed()
    {
        touch(QStringLiteral("b/.directory"));
        touch(QStringLiteral("b/Desktop.ini"));
        FolderView view;
        view.setShowHiddenFiles(true);
        view.setUrl(m_b);
        QTRY_COMPARE(visibleNames(&view), QStringList({QStringLiteral("scan.jpg"), QStringLiteral("todo.txt")}));
    }

    void ctrlFOpensTheFilterBar()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *bar = window.activePane()->findChild<FilterBar *>();
        QVERIFY(bar);
        QVERIFY(!bar->isVisible());
        window.actionCollection()->action(QStringLiteral("show_filter_bar"))->trigger();
        QVERIFY(bar->isVisible());
        QVERIFY(bar->findChild<QLineEdit *>()->hasFocus());
    }

    void typingInTheViewFilters()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        view->currentView()->setFocus();
        QTest::keyClicks(view->currentView(), QStringLiteral("not"));

        auto *bar = window.activePane()->findChild<FilterBar *>();
        QVERIFY(bar->isVisible());
        QCOMPARE(bar->filter().text(), QStringLiteral("not"));
        QCOMPARE(visibleNames(view), QStringList({QStringLiteral("Pictures"), QStringLiteral("notes.txt")}));
        for (int row = 0; row < view->currentView()->model()->rowCount(); ++row) {
            const QModelIndex index = view->currentView()->model()->index(row, 0);
            if (index.data().toString() == QLatin1String("notes.txt")) {
                QVERIFY(index.data(Qt::FontRole).value<QFont>().bold());
            }
        }
    }

    void escapeClearsAndClosesTheBar()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        window.activePane()->showFilterBar();
        auto *bar = window.activePane()->findChild<FilterBar *>();
        QTest::keyClicks(bar->findChild<QLineEdit *>(), QStringLiteral("*.jpg"));
        QCOMPARE(view->itemCount(), 2);

        QTest::keyClick(bar->findChild<QLineEdit *>(), Qt::Key_Escape);
        QVERIFY(!bar->isVisible());
        QVERIFY(view->nameFilter().isEmpty());
        QCOMPARE(view->itemCount(), 3);
        QVERIFY(view->currentView()->hasFocus());
    }

    void enterOpensTheFirstMatch()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 3);
        window.activePane()->showFilterBar();
        auto *edit = window.activePane()->findChild<FilterBar *>()->findChild<QLineEdit *>();
        QTest::keyClicks(edit, QStringLiteral("Pic"));
        QTest::keyClick(edit, Qt::Key_Return);
        QTRY_COMPARE(window.activePane()->url(), QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/Pictures"))));
    }

    void scopeButtonFiltersFoldersToo()
    {
        FilterBar bar;
        bar.open(QStringLiteral("x"));
        QCOMPARE(bar.filter().scope(), NameFilter::Scope::Files);
        auto *scope = bar.findChild<QToolButton *>(QStringLiteral("filterScope"));
        QVERIFY(scope && scope->menu());
        scope->menu()->actions().at(2)->trigger();
        QCOMPARE(bar.filter().scope(), NameFilter::Scope::FilesAndFolders);
    }

    void filterPreferencesApplyToPanes()
    {
        const auto restore = qScopeGuard([] {
            Settings::setDefaultFilterScope(Settings::EnumDefaultFilterScope::Files);
            Settings::setFilterTypeAhead(true);
            Settings::setRetainFilterOnNavigation(true);
            Settings::setHighlightFilterMatches(true);
        });
        Settings::setDefaultFilterScope(Settings::EnumDefaultFilterScope::FilesAndFolders);
        Settings::setFilterTypeAhead(false);
        Settings::setRetainFilterOnNavigation(false);
        Settings::setHighlightFilterMatches(false);
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QTRY_COMPARE(view->itemCount(), 3);
        auto *bar = pane->findChild<FilterBar *>();
        QCOMPARE(bar->filter().scope(), NameFilter::Scope::FilesAndFolders);
        view->currentView()->setFocus();
        QTest::keyClicks(view->currentView(), QStringLiteral("not"));
        QVERIFY(!bar->isVisible());
        QVERIFY(view->nameFilter().isEmpty());
        bar->setFilter(NameFilter(QStringLiteral("not"), NameFilter::Scope::FilesAndFolders));
        QTRY_COMPARE(view->itemCount(), 1);
        QModelIndex index = view->currentView()->model()->index(0, 0);
        QVERIFY(!index.data(Qt::BackgroundRole).isValid());
        Settings::setHighlightFilterMatches(true);
        view->reloadListingSettings();
        QVERIFY(index.data(Qt::BackgroundRole).isValid());
        Settings::setDefaultFilterScope(Settings::EnumDefaultFilterScope::Folders);
        window.applySettings();
        QCOMPARE(bar->filter().scope(), NameFilter::Scope::Folders);
        QCOMPARE(bar->filter().text(), QStringLiteral("not"));
        pane->setUrl(m_b);
        QTRY_COMPARE(pane->url(), m_b);
        QVERIFY(bar->filter().isEmpty());
        QCOMPARE(bar->filter().scope(), NameFilter::Scope::Folders);
    }

    void restoredEmptyFilterKeepsItsSavedScope()
    {
        const auto restore = qScopeGuard([] {
            Settings::setDefaultFilterScope(Settings::EnumDefaultFilterScope::Files);
        });
        Settings::setDefaultFilterScope(Settings::EnumDefaultFilterScope::Folders);
        KFilePlacesModel places;
        TabColors colors(KSharedConfig::openConfig()->group(QStringLiteral("FilterTestTabColors")));
        PaneSession state;
        state.tabs = {m_a};
        Pane fresh(&places, &colors);
        fresh.restoreState(state);
        QCOMPARE(fresh.findChild<FilterBar *>()->filter().scope(), NameFilter::Scope::Folders);
        state.filterScope = static_cast<int>(NameFilter::Scope::FilesAndFolders);
        Pane restored(&places, &colors);
        restored.restoreState(state);
        QCOMPARE(restored.findChild<FilterBar *>()->filter().scope(), NameFilter::Scope::FilesAndFolders);
    }

    void metadataFiltersMatchLabelsAndRatings()
    {
        QVERIFY(FolderMetadata::setLabel(m_root.filePath(QStringLiteral("a")), QStringLiteral("photo.jpg"), QStringLiteral("blue")));
        QVERIFY(FolderMetadata::setRating(m_root.filePath(QStringLiteral("a")), QStringLiteral("photo.jpg"), 5));
        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);

        view.setNameFilter(NameFilter(QStringLiteral("#blu")));
        QCOMPARE(visibleNames(&view), QStringList({QStringLiteral("Pictures"), QStringLiteral("photo.jpg")}));

        view.setNameFilter(NameFilter(QStringLiteral("*4")));
        QCOMPARE(visibleNames(&view), QStringList({QStringLiteral("Pictures"), QStringLiteral("photo.jpg")}));
    }

    void metadataPresentationDoesNotChangeStoredValues()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(FolderMetadata::setLabel(root.path(), QStringLiteral("b.txt"), QStringLiteral("blue")));
        QVERIFY(FolderMetadata::setColor(root.path(), QStringLiteral("b.txt"), QStringLiteral("#123456")));
        QVERIFY(FolderMetadata::setRating(root.path(), QStringLiteral("b.txt"), 5));
        QVERIFY(FolderMetadata::togglePin(root.path(), QStringLiteral("b.txt")));
        const QJsonObject original = FolderMetadata::read(root.path());
        const auto restore = qScopeGuard([] {
            Settings::setShowFileLabels(true);
            Settings::setShowFileColors(true);
            Settings::setShowFileRatings(true);
            Settings::setShowFilePins(true);
            Settings::setPinnedItemsFirst(true);
        });
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(root.path()));
        view.setRatingColumn(true);
        QTRY_COMPARE(view.itemCount(), 2);
        QAbstractItemModel *model = view.currentView()->model();
        QTRY_COMPARE(model->index(0, 0).data().toString(), QStringLiteral("b.txt"));
        QCOMPARE(model->index(0, 0).data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        QCOMPARE(model->index(0, KDirModel::Group).data().toInt(), 5);
        QVERIFY(model->index(0, 0).data(Qt::ToolTipRole).toString().contains(QStringLiteral("Pinned")));
        Settings::setShowFileColors(false);
        view.reloadListingSettings();
        QCOMPARE(model->index(0, 0).data(Qt::ForegroundRole).value<QColor>(), LabelColors::rgb(LabelColor::Blue));
        Settings::setShowFileLabels(false);
        Settings::setShowFileRatings(false);
        Settings::setShowFilePins(false);
        view.reloadListingSettings();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("b.txt"));
        QVERIFY(!model->index(0, 0).data(Qt::ToolTipRole).toString().contains(QStringLiteral("Pinned")));
        Settings::setPinnedItemsFirst(false);
        view.reloadListingSettings();
        QTRY_COMPARE(model->index(0, 0).data().toString(), QStringLiteral("a.txt"));
        const QModelIndex pinned = model->index(1, 0);
        QVERIFY(pinned.data(Qt::ForegroundRole).value<QColor>() != LabelColors::rgb(LabelColor::Blue));
        QVERIFY(!model->index(1, KDirModel::Group).data().isValid());
        QVERIFY(!pinned.data(Qt::ToolTipRole).toString().contains(QStringLiteral("Pinned")));
        view.setNameFilter(NameFilter(QStringLiteral("#blue")));
        QCOMPARE(view.itemCount(), 1);
        QCOMPARE(FolderMetadata::read(root.path()), original);
    }

    void folderColoursShowOnIconsOrNames()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("Projects")));
        const QColor color(0x12, 0x34, 0x56);
        QVERIFY(FolderMetadata::setFolderColor(root.filePath(QStringLiteral("Projects")), color.name()));
        const auto restore = qScopeGuard([] {
            Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Icon);
            FolderColors::clearCache();
        });
        FolderColors::clearCache();
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(view.itemCount(), 1);
        const QModelIndex folder = view.currentView()->model()->index(0, 0);
        QCOMPARE(folder.data().toString(), QStringLiteral("Projects"));
        const auto badgePixel = [&folder] {
            return folder.data(Qt::DecorationRole).value<QIcon>().pixmap(48).toImage().pixelColor(38, 38);
        };

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Badge);
        QCOMPARE(badgePixel(), color);
        QVERIFY(!folder.data(Qt::ForegroundRole).value<QColor>().isValid());

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Hidden);
        QVERIFY(badgePixel() != color);
        QVERIFY(!folder.data(Qt::ForegroundRole).value<QColor>().isValid());

        Settings::setFolderColorStyle(Settings::EnumFolderColorStyle::Name);
        QVERIFY(badgePixel() != color);
        QCOMPARE(folder.data(Qt::ForegroundRole).value<QColor>(), ColorSchemes::readable(color, QApplication::palette().color(QPalette::Base), 4.5));
    }

    void selectionPatternsSelectAndDeselectInBothViews()
    {
        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);

        QCOMPARE(view.applySelectionPattern(QStringLiteral("*.jpg"), true), 1);
        QCOMPARE(view.selectedItems().size(), 1);
        QCOMPARE(view.selectedItems().first().name(), QStringLiteral("photo.jpg"));

        view.setViewMode(FolderView::ViewMode::Icons);
        view.clearSelection();
        QCOMPARE(view.applySelectionPattern(QStringLiteral("*.txt"), true), 1);
        QCOMPARE(view.selectedItems().size(), 1);
        QCOMPARE(view.selectedItems().first().name(), QStringLiteral("notes.txt"));
        QCOMPARE(view.applySelectionPattern(QStringLiteral("*.txt"), false), 1);
        QVERIFY(view.selectedItems().isEmpty());
    }

    void compactViewUsesColumnsAndRemembersFormat()
    {
        const QString path = m_root.filePath(QStringLiteral("compact"));
        QVERIFY(QDir().mkpath(path));
        for (int i = 0; i < 12; ++i) {
            QFile file(QDir(path).filePath(QStringLiteral("file%1.txt").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderView view;
        view.resize(600, 150);
        view.setUrl(QUrl::fromLocalFile(path));
        view.setViewMode(FolderView::ViewMode::Compact);
        view.show();
        QTRY_COMPARE(view.itemCount(), 12);
        auto *list = qobject_cast<QListView *>(view.currentView());
        QVERIFY(list);
        QCOMPARE(list->flow(), QListView::TopToBottom);
        QVERIFY(list->isWrapping());
        QTRY_VERIFY(list->visualRect(list->model()->index(11, 0)).left() > list->visualRect(list->model()->index(0, 0)).left());
        view.selectUrls({QUrl::fromLocalFile(QDir(path).filePath(QStringLiteral("file0.txt")))});
        QCOMPARE(view.selectedItems().size(), 1);
        view.setViewMode(FolderView::ViewMode::Details);
        QCOMPARE(view.selectedItems().size(), 1);
        view.setViewMode(FolderView::ViewMode::Compact);
        view.setUrl(m_b);
        view.setUrl(QUrl::fromLocalFile(path));
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Compact);
    }

    void cardViewLaysOutCardsAndRemembersFormat()
    {
        const QString path = m_root.filePath(QStringLiteral("cards"));
        QVERIFY(QDir().mkpath(path));
        for (int i = 0; i < 6; ++i) {
            QFile file(QDir(path).filePath(QStringLiteral("card%1.txt").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderView view;
        view.resize(800, 500);
        view.setUrl(QUrl::fromLocalFile(path));
        view.setViewMode(FolderView::ViewMode::Cards);
        view.show();
        QTRY_COMPARE(view.itemCount(), 6);
        auto *cards = qobject_cast<QListView *>(view.currentView());
        QVERIFY(cards);
        QCOMPARE(cards->objectName(), QStringLiteral("cardView"));
        QCOMPARE(cards->viewMode(), QListView::IconMode);
        QVERIFY(qobject_cast<CardDelegate *>(cards->itemDelegate()));
        QCOMPARE(cards->iconSize(), QSize(Settings::cardPreviewSize(), Settings::cardPreviewSize()));
        QVERIFY(cards->gridSize().height() > Settings::cardPreviewSize());
        const QModelIndex first = cards->model()->index(0, 0);
        const QModelIndex second = cards->model()->index(1, 0);
        QTRY_VERIFY(!cards->visualRect(second).isEmpty());
        QCOMPARE(cards->visualRect(second).top(), cards->visualRect(first).top());
        QVERIFY(cards->visualRect(second).left() > cards->visualRect(first).left());

        const auto restoreColumns = qScopeGuard([] {
            Settings::self()->setCardColumns(0);
        });
        Settings::self()->setCardColumns(2);
        view.reloadListingSettings();
        QTRY_VERIFY(cards->visualRect(cards->model()->index(2, 0)).top() > cards->visualRect(first).top());
        QVERIFY(cards->gridSize().width() * 2 <= cards->viewport()->width());
        QVERIFY(cards->gridSize().width() * 3 > cards->viewport()->width());

        view.selectUrls({QUrl::fromLocalFile(QDir(path).filePath(QStringLiteral("card0.txt")))});
        QCOMPARE(view.selectedItems().size(), 1);
        view.setViewMode(FolderView::ViewMode::Details);
        QCOMPARE(view.selectedItems().size(), 1);
        view.setViewMode(FolderView::ViewMode::Cards);
        QCOMPARE(view.selectedItems().size(), 1);
        view.setUrl(m_b);
        view.setUrl(QUrl::fromLocalFile(path));
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Cards);
        FolderMetadata::Format format;
        QVERIFY(FolderMetadata::format(FolderMetadata::read(path), &format));
        QCOMPARE(format.view, QStringLiteral("cards"));
    }

    void cardInformationShowsRatingLabelAndTags()
    {
        const QString path = m_root.filePath(QStringLiteral("cardinfo"));
        QVERIFY(QDir().mkpath(path));
        QFile file(QDir(path).filePath(QStringLiteral("rated.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        QVERIFY(FolderMetadata::setRating(path, QStringLiteral("rated.txt"), 3));
        QVERIFY(FolderMetadata::setLabel(path, QStringLiteral("rated.txt"), QStringLiteral("red")));
        FolderView view;
        view.setUrl(QUrl::fromLocalFile(path));
        view.setViewMode(FolderView::ViewMode::Cards);
        QTRY_COMPARE(view.itemCount(), 1);
        const QModelIndex index = view.currentView()->model()->index(0, 0);
        QCOMPARE(index.data(CardRoles::Rating).toInt(), 3);
        QCOMPARE(index.data(CardRoles::Label).toString(), QStringLiteral("red"));
        QVERIFY(index.data(CardRoles::Tags).canConvert<QStringList>());
        QCOMPARE(CardDelegate::informationText(index, QStringLiteral("rating")), QStringLiteral("★★★☆☆"));
        QCOMPARE(CardDelegate::informationText(index, QStringLiteral("label")), QStringLiteral("Red"));
        QVERIFY(!CardDelegate::informationText(index, QStringLiteral("size")).isEmpty());
        QVERIFY(CardDelegate::informationText(index, QStringLiteral("unknown")).isEmpty());
    }

    void cardViewRenamesInPlace()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("before.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FolderView view;
        view.resize(600, 400);
        view.setUrl(QUrl::fromLocalFile(dir.path()));
        view.setViewMode(FolderView::ViewMode::Details);
        view.show();
        QTRY_COMPARE(view.itemCount(), 1);
        view.selectUrls({QUrl::fromLocalFile(file.fileName())});
        QVERIFY(!view.renameInline());
        view.setViewMode(FolderView::ViewMode::Cards);
        QVERIFY(view.renameInline());
        auto *editor = view.findChild<QLineEdit *>(QStringLiteral("cardNameEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->text(), QStringLiteral("before.txt"));
        QTRY_COMPARE(editor->selectedText(), QStringLiteral("before"));
        editor->setText(QStringLiteral("after.txt"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_VERIFY(QFile::exists(dir.filePath(QStringLiteral("after.txt"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("before.txt"))));
    }

    void columnMenuFitsVisibleColumns()
    {
        FolderView view;
        view.setUrl(m_a);
        view.resize(900, 400);
        view.show();
        QTRY_COMPARE(view.itemCount(), 3);
        auto *details = qobject_cast<QTreeView *>(view.currentView());
        QVERIFY(details);
        auto *header = details->header();
        header->setSectionResizeMode(KDirModel::Name, QHeaderView::Interactive);
        header->resizeSection(KDirModel::Name, 500);
        const int before = header->sectionSize(KDirModel::Name);
        bool found = false;
        QTimer::singleShot(0, &view, [&found] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            for (QAction *action : menu->actions()) {
                if (action->text() == QLatin1String("Fit All Columns to Contents")) {
                    found = true;
                    action->trigger();
                    break;
                }
            }
            menu->close();
        });
        QVERIFY(QMetaObject::invokeMethod(header, "customContextMenuRequested", Qt::DirectConnection, Q_ARG(QPoint, QPoint(10, 5))));
        QVERIFY(found);
        QVERIFY(header->sectionSize(KDirModel::Name) < before);
        QVERIFY(details->isColumnHidden(KDirModel::Owner));
    }

    void selectionMarkersAndModeToggleWithoutActivation()
    {
        FolderView view;
        view.resize(600, 400);
        view.setUrl(m_a);
        view.show();
        QTRY_COMPARE(view.itemCount(), 3);
        QSignalSpy activated(&view, &FolderView::itemActivated);
        view.setSelectionMarkersVisible(true);

        for (const auto mode : {FolderView::ViewMode::Details, FolderView::ViewMode::Icons, FolderView::ViewMode::Compact, FolderView::ViewMode::Cards}) {
            view.setViewMode(mode);
            view.clearSelection();
            auto *listing = view.currentView();
            const QModelIndex index = listing->model()->index(0, 0);
            QTRY_VERIFY(!listing->visualRect(index).isEmpty());
            const QRect rect = listing->visualRect(index);
            // Cards put their marker in the top-left corner; other views in the bottom-left.
            const QPoint marker = mode == FolderView::ViewMode::Cards ? rect.topLeft() + QPoint(10, 10) : QPoint(rect.left() + 8, rect.bottom() - 8);
            QTest::mouseClick(listing->viewport(), Qt::LeftButton, Qt::NoModifier, marker);
            QCOMPARE(view.selectedItems().size(), 1);
            QTest::mouseClick(listing->viewport(), Qt::LeftButton, Qt::NoModifier, marker);
            QVERIFY(view.selectedItems().isEmpty());
        }
        QCOMPARE(activated.size(), 0);
        view.setSelectionMarkersVisible(false);
        view.setSelectionMode(true);
        QVERIFY(view.findChild<QWidget *>(QStringLiteral("selectionBar"))->isVisible());
        auto *listing = view.currentView();
        const QModelIndex first = listing->model()->index(0, 0);
        const QModelIndex second = listing->model()->index(1, 0);
        QTest::mouseClick(listing->viewport(), Qt::LeftButton, Qt::NoModifier, listing->visualRect(first).center());
        QTest::mouseClick(listing->viewport(), Qt::LeftButton, Qt::NoModifier, listing->visualRect(second).center());
        QCOMPARE(view.selectedItems().size(), 2);
        QCOMPARE(activated.size(), 0);
        auto *done = view.findChild<QToolButton *>(QStringLiteral("selectionDone"));
        QVERIFY(done);
        done->click();
        QVERIFY(!view.selectionMode());
        QCOMPARE(view.selectedItems().size(), 2);
    }

    void viewFormatIsRememberedPerFolder()
    {
        FolderMetadata::Format otherFormat;
        otherFormat.view = QStringLiteral("list");
        otherFormat.columns = {QStringLiteral("mode")};
        otherFormat.sortKey = QStringLiteral("size");
        otherFormat.sortAscending = false;
        otherFormat.group = QStringLiteral("date");
        QVERIFY(FolderMetadata::setFormat(m_root.filePath(QStringLiteral("b")), otherFormat));

        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        view.setViewMode(FolderView::ViewMode::Icons);
        view.setGroupBy(QStringLiteral("type"));
        view.setUrl(m_b);
        QTRY_COMPARE(view.itemCount(), 2);
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Details);
        QCOMPARE(view.groupBy(), QStringLiteral("date"));
        auto *details = view.findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(!details->isColumnHidden(KDirModel::Permissions));
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QCOMPARE(details->header()->sortIndicatorSection(), KDirModel::Size);
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        QCOMPARE(view.viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(view.groupBy(), QStringLiteral("type"));
    }

    void ratingColumnShowsMetadataAndKeepsSelectionUrls()
    {
        const QString directory = m_root.filePath(QStringLiteral("a"));
        QVERIFY(FolderMetadata::setRating(directory, QStringLiteral("photo.jpg"), 5));
        QVERIFY(FolderMetadata::setRating(directory, QStringLiteral("notes.txt"), 2));
        FolderMetadata::Format format;
        format.view = QStringLiteral("list");
        format.columns = {QStringLiteral("rating")};
        format.sortKey = QStringLiteral("rating");
        format.sortAscending = false;
        format.group = QStringLiteral("none");
        QVERIFY(FolderMetadata::setFormat(directory, format));

        FolderView view;
        view.setUrl(m_a);
        QTRY_COMPARE(view.itemCount(), 3);
        auto *details = view.findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(!details->isColumnHidden(KDirModel::Group));
        QCOMPARE(details->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));

        int photoRow = -1;
        for (int row = 0; row < details->model()->rowCount(); ++row) {
            if (details->model()->index(row, KDirModel::Name).data().toString() == QLatin1String("photo.jpg")) {
                photoRow = row;
                break;
            }
        }
        int notesRow = -1;
        for (int row = 0; row < details->model()->rowCount(); ++row) {
            if (details->model()->index(row, KDirModel::Name).data().toString() == QLatin1String("notes.txt")) {
                notesRow = row;
                break;
            }
        }
        QVERIFY(photoRow >= 0);
        QVERIFY(notesRow >= 0);
        QVERIFY(photoRow < notesRow);
        QCOMPARE(details->model()->index(photoRow, KDirModel::Group).data().toInt(), 5);
        const QUrl photoUrl = QUrl::fromLocalFile(QDir(directory).filePath(QStringLiteral("photo.jpg")));
        view.selectUrls({photoUrl});
        QCOMPARE(view.selectedItems().size(), 1);
        QCOMPARE(view.selectedItems().first().url(), photoUrl);
    }

    void groupingKeepsExtensionCategoriesTogether()
    {
        const QString path = m_root.filePath(QStringLiteral("grouped"));
        QVERIFY(QDir().mkpath(path));
        for (const QString &name : {QStringLiteral("alpha.txt"), QStringLiteral("beta.txt"), QStringLiteral("photo.jpg")}) {
            QFile file(QDir(path).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderMetadata::Format format;
        format.view = QStringLiteral("list");
        format.columns = {QStringLiteral("type")};
        format.sortKey = QStringLiteral("name");
        format.sortAscending = true;
        format.group = QStringLiteral("ext");
        QVERIFY(FolderMetadata::setFormat(path, format));

        FolderView view;
        view.setUrl(QUrl::fromLocalFile(path));
        QTRY_COMPARE(view.itemCount(), 3);
        const QAbstractItemModel *model = view.currentView()->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("photo.jpg"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("alpha.txt"));
        QCOMPARE(model->index(2, 0).data().toString(), QStringLiteral("beta.txt"));

        view.setViewMode(FolderView::ViewMode::Icons);
        auto *icons = qobject_cast<KCategorizedView *>(view.currentView());
        QVERIFY(icons);
        QVERIFY(icons->categoryDrawer());
        auto *categorizedModel = qobject_cast<KCategorizedSortFilterProxyModel *>(icons->model());
        QVERIFY(categorizedModel && categorizedModel->isCategorizedModel());
        QStringList categories;
        for (int row = 0; row < icons->model()->rowCount(); ++row) {
            categories.append(icons->model()->index(row, 0).data(KCategorizedSortFilterProxyModel::CategoryDisplayRole).toString());
        }
        QCOMPARE(categories, QStringList({QStringLiteral("JPG"), QStringLiteral("TXT"), QStringLiteral("TXT")}));
    }
};

QTEST_MAIN(FilterTest)

#include "filtertest.moc"
