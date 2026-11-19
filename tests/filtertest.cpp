// SPDX-License-Identifier: GPL-3.0-or-later

#include "filterbar.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

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
};

QTEST_MAIN(FilterTest)

#include "filtertest.moc"
