// SPDX-License-Identifier: GPL-3.0-or-later

#include "filterbar.h"
#include "foldermetadata.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KDirModel>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
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
    }
};

QTEST_MAIN(FilterTest)

#include "filtertest.moc"
