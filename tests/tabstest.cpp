// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "labelcolor.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"
#include "tabcolors.h"
#include "tabmodel.h"

#include <KSharedConfig>

#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

class TabsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_c;

    /// The active pane, showing exactly @p urls as tabs.
    static Pane *paneWith(MainWindow &window, const QList<QUrl> &urls)
    {
        window.openUrls({urls.first()});
        Pane *pane = window.activePane();
        while (pane->tabs()->count() > 1) {
            pane->tabs()->closeTab(0);
        }
        for (int i = 1; i < urls.size(); ++i) {
            pane->openInNewTab(urls.at(i));
        }
        return pane;
    }

    static QTabBar *tabBarOf(Pane *pane)
    {
        return pane->findChild<QTabBar *>();
    }

    static QWidget *closeButton(QTabBar *bar, int index)
    {
        const auto side = static_cast<QTabBar::ButtonPosition>(bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
        return bar->tabButton(index, side);
    }

    static QStringList tabTexts(QTabBar *bar)
    {
        QStringList texts;
        for (int i = 0; i < bar->count(); ++i) {
            texts.append(bar->tabText(i).remove(u'&'));
        }
        return texts;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        for (const char *name : {"a", "b", "c"}) {
            QVERIFY(QDir(m_root.path()).mkpath(QLatin1String(name)));
        }
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_c = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("c")));
    }

    void init()
    {
        Settings::setConfirmClosingMultipleTabs(false);
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("TabColors"));
        KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", false);
        KSharedConfig::openConfig()->sync();
    }

    void pinnedAndLockedTabsShowStateAndNoCloseButton()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a, m_b, m_c});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        pane->tabs()->setPinned(0, true);
        pane->tabs()->setPinned(1, true);
        pane->tabs()->setLocked(1, true);

        QTabBar *bar = tabBarOf(pane);
        QCOMPARE(bar->tabIcon(0).name(), QStringLiteral("pin"));
        QCOMPARE(bar->tabIcon(1).name(), QStringLiteral("pin"));
        QVERIFY(bar->tabToolTip(1).contains(QStringLiteral("Pinned")));
        QVERIFY(bar->tabToolTip(1).contains(QStringLiteral("Locked")));
        QVERIFY(!closeButton(bar, 0)->isVisible());
        QVERIFY(!closeButton(bar, 1)->isVisible());
        QVERIFY(closeButton(bar, 2)->isVisible());

        pane->tabs()->setLocked(1, false);
        pane->tabs()->setPinned(1, false);
        QVERIFY(closeButton(bar, 1)->isVisible());
    }

    void lockedTabOpensFoldersInNewTabs()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a});
        pane->tabs()->setLocked(0, true);
        pane->setUrl(m_b);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_b}));
        QCOMPARE(pane->url(), m_b);
    }

    void customNameReplacesTheFolderName()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a});
        pane->tabs()->setName(0, QStringLiteral("Work"));
        QCOMPARE(tabBarOf(pane)->tabText(0), QStringLiteral("Work"));
        pane->tabs()->setName(0, QString());
        QCOMPARE(tabBarOf(pane)->tabText(0), QStringLiteral("a"));
    }

    void colourAppliesToEveryTabOfTheFolder()
    {
        MainWindow window;
        window.openUrls({m_a, m_a});
        const int active = window.activePane()->tabs()->urls().indexOf(m_a);
        const int inactive = window.inactivePane()->tabs()->urls().indexOf(m_a);
        window.tabColors()->setColor(m_a, LabelColor::Green);
        QCOMPARE(tabBarOf(window.activePane())->tabTextColor(active), LabelColors::rgb(LabelColor::Green));
        QCOMPARE(tabBarOf(window.inactivePane())->tabTextColor(inactive), LabelColors::rgb(LabelColor::Green));
        Settings::setShowTabColors(false);
        window.applySettings();
        QVERIFY(!tabBarOf(window.activePane())->tabTextColor(active).isValid());
        QCOMPARE(window.tabColors()->colorFor(m_a), LabelColor::Green);
        Settings::setShowTabColors(true);
        window.applySettings();
        QCOMPARE(tabBarOf(window.activePane())->tabTextColor(active), LabelColors::rgb(LabelColor::Green));
        window.tabColors()->setColor(m_a, LabelColor::None);
        QVERIFY(!tabBarOf(window.activePane())->tabTextColor(active).isValid());
    }

    void draggingAcrossThePinnedBoundaryIsUndone()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        pane->tabs()->setPinned(0, true);
        QTabBar *bar = tabBarOf(pane);
        bar->moveTab(1, 0);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_b}));
        QCOMPARE(tabTexts(bar), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    }

    void sessionKeepsPinsLocksAndNames()
    {
        {
            MainWindow window;
            Pane *pane = paneWith(window, {m_a, m_b, m_c});
            pane->tabs()->setPinned(2, true);
            pane->tabs()->setLocked(1, true);
            pane->tabs()->setName(2, QStringLiteral("Bee"));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(window.close());
        }
        MainWindow window;
        window.openUrls({});
        TabModel *tabs = window.activePane()->tabs();
        QCOMPARE(tabs->urls(), (QList<QUrl>{m_c, m_a, m_b}));
        QCOMPARE(tabs->pinnedCount(), 1);
        QVERIFY(tabs->isLocked(1));
        QCOMPARE(tabs->nameAt(2), QStringLiteral("Bee"));
    }

    void historyRestoresSelectionAndScroll()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("a")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("b")));
        for (int i = 0; i < 80; ++i) {
            QFile file(root.filePath(QStringLiteral("a/%1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QFile other(root.filePath(QStringLiteral("b/other.txt")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        const QUrl a = QUrl::fromLocalFile(root.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(root.filePath(QStringLiteral("b")));
        const QUrl first = QUrl::fromLocalFile(root.filePath(QStringLiteral("a/005.txt")));
        const QUrl missing = QUrl::fromLocalFile(root.filePath(QStringLiteral("a/006.txt")));
        const QUrl otherUrl = QUrl::fromLocalFile(other.fileName());
        MainWindow window;
        Pane *pane = paneWith(window, {a});
        window.resize(600, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = pane->view();
        QTRY_COMPARE(view->itemCount(), 80);
        view->selectUrls({first, missing});
        auto *scroll = view->currentView()->verticalScrollBar();
        QTRY_VERIFY(scroll->maximum() > 5);
        scroll->setValue(5);
        QCOMPARE(scroll->value(), 5);
        pane->setUrl(b);
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({otherUrl});
        QVERIFY(QFile::remove(missing.toLocalFile()));
        pane->goBack();
        QTRY_COMPARE(view->itemCount(), 79);
        QTRY_COMPARE(view->selectedItems().size(), 1);
        QTRY_COMPARE(view->selectedItems().first().url(), first);
        QTRY_COMPARE(scroll->value(), 5);
        pane->goForward();
        QTRY_COMPARE(view->itemCount(), 1);
        QTRY_COMPARE(view->selectedItems().size(), 1);
        QTRY_COMPARE(view->selectedItems().first().url(), otherUrl);
    }

    void historyRestorePreferences_data()
    {
        QTest::addColumn<bool>("selection");
        QTest::addColumn<bool>("scrollPosition");
        QTest::newRow("selection only") << true << false;
        QTest::newRow("scroll only") << false << true;
        QTest::newRow("neither") << false << false;
    }

    void historyRestorePreferences()
    {
        QFETCH(bool, selection);
        QFETCH(bool, scrollPosition);
        Settings::setRestoreHistorySelection(selection);
        Settings::setRestoreHistoryScroll(scrollPosition);
        const auto restore = qScopeGuard([] {
            Settings::setRestoreHistorySelection(true);
            Settings::setRestoreHistoryScroll(true);
        });
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("a")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("b")));
        for (int i = 0; i < 80; ++i) {
            QFile file(root.filePath(QStringLiteral("a/%1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        Pane *pane = paneWith(window, {QUrl::fromLocalFile(root.filePath(QStringLiteral("a")))});
        window.resize(600, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = pane->view();
        QTRY_COMPARE(view->itemCount(), 80);
        const QUrl selected = QUrl::fromLocalFile(root.filePath(QStringLiteral("a/005.txt")));
        view->selectUrls({selected});
        auto *scroll = view->currentView()->verticalScrollBar();
        QTRY_VERIFY(scroll->maximum() > 5);
        scroll->setValue(5);
        pane->setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("b"))));
        QTRY_COMPARE(view->itemCount(), 0);
        pane->goBack();
        QTRY_COMPARE(view->itemCount(), 80);
        QTRY_COMPARE(view->selectedItems().size(), selection ? 1 : 0);
        QTRY_COMPARE(scroll->value(), scrollPosition ? 5 : 0);
        if (selection) {
            QCOMPARE(view->selectedItems().first().url(), selected);
        }
    }

    void sameFolderTabsKeepIndependentSelections()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("first"), QStringLiteral("second")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QUrl folder = QUrl::fromLocalFile(root.path());
        const QUrl first = QUrl::fromLocalFile(root.filePath(QStringLiteral("first")));
        const QUrl second = QUrl::fromLocalFile(root.filePath(QStringLiteral("second")));
        MainWindow window;
        Pane *pane = paneWith(window, {folder});
        FolderView *view = pane->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({first});
        pane->openInNewTab(folder, true);
        QCOMPARE(pane->tabs()->historyAt(0).currentViewState().selectedUrls, QList<QUrl>{first});
        QVERIFY(view->selectedItems().isEmpty());
        view->selectUrls({second});
        pane->tabs()->setCurrentIndex(0);
        QVERIFY(pane->tabs()->historyAt(0).currentViewState().valid);
        QTRY_VERIFY(!view->isLoading());
        QTRY_COMPARE(view->selectedItems().size(), 1);
        QTRY_COMPARE(view->selectedItems().first().url(), first);
        pane->tabs()->setCurrentIndex(1);
        QTRY_COMPARE(view->selectedItems().size(), 1);
        QTRY_COMPARE(view->selectedItems().first().url(), second);
    }

    void historyRestoresSelectionInsideExpandedFolders()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("a/one/two")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("a/three")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("b")));
        QFile file(root.filePath(QStringLiteral("a/one/two/item.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QFile other(root.filePath(QStringLiteral("a/three/other.txt")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        QFile missing(root.filePath(QStringLiteral("a/one/two/missing.txt")));
        QVERIFY(missing.open(QIODevice::WriteOnly));
        missing.close();
        const QUrl a = QUrl::fromLocalFile(root.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(root.filePath(QStringLiteral("b")));
        const QUrl item = QUrl::fromLocalFile(file.fileName());
        const QUrl otherItem = QUrl::fromLocalFile(other.fileName());
        const QUrl missingItem = QUrl::fromLocalFile(missing.fileName());
        MainWindow window;
        Pane *pane = paneWith(window, {a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = pane->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *tree = qobject_cast<QTreeView *>(view->currentView());
        QVERIFY(tree);
        QModelIndex one = tree->model()->match(tree->model()->index(0, 0), Qt::DisplayRole, QStringLiteral("one")).first();
        tree->expand(one);
        QTRY_COMPARE(tree->model()->rowCount(one), 1);
        QModelIndex two = tree->model()->index(0, 0, one);
        tree->expand(two);
        QTRY_COMPARE(tree->model()->rowCount(two), 2);
        QModelIndex three = tree->model()->match(tree->model()->index(0, 0), Qt::DisplayRole, QStringLiteral("three")).first();
        tree->expand(three);
        QTRY_COMPARE(tree->model()->rowCount(three), 1);
        view->selectUrls({item, otherItem, missingItem});
        QCOMPARE(view->selectedItems().size(), 3);
        pane->setUrl(b);
        QTRY_COMPARE(view->itemCount(), 0);
        QVERIFY(QFile::remove(missing.fileName()));
        pane->goBack();
        QTRY_COMPARE(view->selectedItems().size(), 2);
        QList<QUrl> selected;
        for (const KFileItem &selectedItem : view->selectedItems()) {
            selected.append(selectedItem.url());
        }
        QVERIFY(selected.contains(item));
        QVERIFY(selected.contains(otherItem));
    }
};

QTEST_MAIN(TabsTest)

#include "tabstest.moc"
