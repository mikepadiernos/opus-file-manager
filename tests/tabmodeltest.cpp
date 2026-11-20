// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabmodel.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
QUrl url(const char *path)
{
    return QUrl::fromLocalFile(QString::fromLatin1(path));
}
}

class TabModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsEmpty()
    {
        TabModel model;
        QCOMPARE(model.count(), 0);
        QCOMPARE(model.currentIndex(), -1);
        QVERIFY(!model.currentUrl().isValid());
        QVERIFY(!model.canGoBack());
        QVERIFY(!model.closeTab(0));
    }

    void addTabActivatesAndSignals()
    {
        TabModel model;
        QSignalSpy inserted(&model, &TabModel::tabInserted);
        QSignalSpy current(&model, &TabModel::currentIndexChanged);
        QSignalSpy urlChanged(&model, &TabModel::currentUrlChanged);

        QCOMPARE(model.addTab(url("/a")), 0);
        QCOMPARE(model.addTab(url("/b"), false), 1);
        QCOMPARE(model.addTab(url("/c")), 2);
        QCOMPARE(model.addTab(QUrl()), -1);

        QCOMPARE(model.count(), 3);
        QCOMPARE(model.currentIndex(), 2);
        QCOMPARE(inserted.count(), 3);
        QCOMPARE(current.count(), 2);
        QCOMPARE(urlChanged.count(), 2);
        QCOMPARE(model.urls(), (QList<QUrl>{url("/a"), url("/b"), url("/c")}));
    }

    void insertBeforeCurrentShiftsIndex()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        QCOMPARE(model.insertTab(0, url("/x"), false), 0);
        QCOMPARE(model.currentIndex(), 2);
        QCOMPARE(model.currentUrl(), url("/b"));
    }

    void closeTabKeepsOneAndFixesCurrent()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        QSignalSpy removed(&model, &TabModel::tabRemoved);

        model.setCurrentIndex(2);
        QVERIFY(model.closeTab(2));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.currentUrl(), url("/b"));

        model.setCurrentIndex(1);
        QVERIFY(model.closeTab(0));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.currentUrl(), url("/b"));

        QVERIFY(!model.closeTab(0));
        QCOMPARE(model.count(), 1);
        QCOMPARE(removed.count(), 2);
    }

    void reopenClosedTabRestoresHistory()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.setCurrentUrl(url("/b/sub"));
        QVERIFY(model.closeTab(1));
        QCOMPARE(model.reopenClosedTab(), 1);
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.currentUrl(), url("/b/sub"));
        QVERIFY(model.canGoBack());
        QCOMPARE(model.reopenClosedTab(), -1);
    }

    void closedTabsAreCapped()
    {
        TabModel model;
        model.addTab(url("/keep"));
        for (int i = 0; i < TabModel::MaxClosedTabs + 5; ++i) {
            model.addTab(url("/t"));
            model.closeTab(model.currentIndex());
        }
        int reopened = 0;
        while (model.reopenClosedTab() >= 0) {
            ++reopened;
        }
        QCOMPARE(reopened, TabModel::MaxClosedTabs);
    }

    void moveTabTracksCurrent()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        model.setCurrentIndex(0);
        QSignalSpy moved(&model, &TabModel::tabMoved);

        QVERIFY(model.moveTab(0, 2));
        QCOMPARE(model.urls(), (QList<QUrl>{url("/b"), url("/c"), url("/a")}));
        QCOMPARE(model.currentIndex(), 2);

        QVERIFY(model.moveTab(0, 1));
        QCOMPARE(model.currentIndex(), 2);
        QVERIFY(model.moveTab(0, 2));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.currentUrl(), url("/a"));

        QVERIFY(!model.moveTab(1, 1));
        QVERIFY(!model.moveTab(5, 0));
        QCOMPARE(moved.count(), 3);
    }

    void swappingCurrentTabsPreservesHistoryAndProtections()
    {
        TabModel left;
        TabModel right;
        left.addTab(url("/a"));
        left.addTab(url("/pinned"));
        left.setCurrentUrl(url("/pinned/sub"));
        left.setName(left.currentIndex(), QStringLiteral("Pinned"));
        NavigationHistory::ViewState viewState;
        viewState.valid = true;
        viewState.verticalScroll = 42;
        left.setCurrentViewState(viewState);
        left.setPinned(left.currentIndex(), true);
        left.setLocked(left.currentIndex(), true);
        right.addTab(url("/b"));
        right.addTab(url("/other"));
        right.setCurrentUrl(url("/other/sub"));
        right.setName(right.currentIndex(), QStringLiteral("Other"));

        QVERIFY(left.swapCurrent(right));
        QCOMPARE(left.currentUrl(), url("/other/sub"));
        QCOMPARE(left.nameAt(left.currentIndex()), QStringLiteral("Other"));
        QVERIFY(!left.isPinned(left.currentIndex()));
        QVERIFY(!left.isLocked(left.currentIndex()));
        QVERIFY(left.canGoBack());
        left.goBack();
        QCOMPARE(left.currentUrl(), url("/other"));
        QCOMPARE(right.currentUrl(), url("/pinned/sub"));
        QCOMPARE(right.nameAt(right.currentIndex()), QStringLiteral("Pinned"));
        QVERIFY(right.isPinned(right.currentIndex()));
        QVERIFY(right.isLocked(right.currentIndex()));
        QCOMPARE(right.historyAt(right.currentIndex()).currentViewState().verticalScroll, 42);
        QVERIFY(!right.closeTab(right.currentIndex()));
        right.setLocked(right.currentIndex(), false);
        QVERIFY(right.canGoBack());
        right.goBack();
        QCOMPARE(right.currentUrl(), url("/pinned"));
        QVERIFY(left.swapCurrent(right));
        QVERIFY(left.isPinned(left.currentIndex()));
        QVERIFY(!right.isPinned(right.currentIndex()));
        QCOMPARE(left.currentUrl(), url("/pinned"));
        QCOMPARE(right.currentUrl(), url("/other"));
        QVERIFY(!left.swapCurrent(left));
        TabModel empty;
        QVERIFY(!left.swapCurrent(empty));
    }

    void nextAndPreviousWrap()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        model.activateNext();
        QCOMPARE(model.currentIndex(), 0);
        model.activatePrevious();
        QCOMPARE(model.currentIndex(), 2);
    }

    void navigationUsesCurrentTabHistory()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        QSignalSpy tabUrl(&model, &TabModel::tabUrlChanged);
        QSignalSpy currentUrl(&model, &TabModel::currentUrlChanged);

        model.setCurrentUrl(url("/b/1"));
        model.setCurrentUrl(url("/b/2"));
        QCOMPARE(model.urlAt(1), url("/b/2"));
        QCOMPARE(model.urlAt(0), url("/a"));
        QVERIFY(model.canGoBack());
        QVERIFY(!model.canGoForward());

        model.goBack();
        QCOMPARE(model.currentUrl(), url("/b/1"));
        QVERIFY(model.canGoForward());
        model.goForward();
        QCOMPARE(model.currentUrl(), url("/b/2"));

        QCOMPARE(tabUrl.count(), 4);
        QCOMPARE(tabUrl.last().at(0).toInt(), 1);
        QCOMPARE(currentUrl.count(), 4);

        model.setCurrentIndex(0);
        QVERIFY(!model.canGoBack());
    }

    void setCurrentUrlOnEmptyModelCreatesTab()
    {
        TabModel model;
        model.setCurrentUrl(url("/a"));
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.currentUrl(), url("/a"));
    }

    void replaceCurrentUrlDoesNotAddHistory()
    {
        TabModel model;
        model.addTab(url("/a"));
        QSignalSpy tabUrl(&model, &TabModel::tabUrlChanged);
        model.replaceCurrentUrl(url("/redirected"));
        QCOMPARE(model.currentUrl(), url("/redirected"));
        QVERIFY(!model.canGoBack());
        QCOMPARE(tabUrl.count(), 1);
        model.replaceCurrentUrl(url("/redirected"));
        QCOMPARE(tabUrl.count(), 1);
    }

    void pinningMovesTabsToThePinnedStart()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        QSignalSpy state(&model, &TabModel::tabStateChanged);
        model.setPinned(2, true);
        QCOMPARE(model.urls(), (QList<QUrl>{url("/c"), url("/a"), url("/b")}));
        QCOMPARE(model.pinnedCount(), 1);
        QVERIFY(model.isPinned(0));
        QCOMPARE(model.currentUrl(), url("/c"));
        QVERIFY(state.count() >= 1);

        model.setPinned(2, true);
        QCOMPARE(model.urls(), (QList<QUrl>{url("/c"), url("/b"), url("/a")}));
        QCOMPARE(model.pinnedCount(), 2);

        model.setPinned(0, false);
        QCOMPARE(model.urls(), (QList<QUrl>{url("/b"), url("/c"), url("/a")}));
        QCOMPARE(model.pinnedCount(), 1);
        QVERIFY(!model.isPinned(1));
    }

    void pinnedAndLockedTabsCannotBeClosed()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        model.setPinned(0, true);
        model.setLocked(1, true);
        QVERIFY(!model.closeTab(0));
        QVERIFY(!model.closeTab(1));
        QVERIFY(model.closeTab(2));
        QCOMPARE(model.count(), 2);
    }

    void movesStayOnTheirSideOfThePinnedBoundary()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        model.setPinned(0, true);
        QVERIFY(!model.moveTab(0, 1));
        QVERIFY(!model.moveTab(2, 0));
        QVERIFY(model.moveTab(2, 1));
        QCOMPARE(model.urls(), (QList<QUrl>{url("/a"), url("/c"), url("/b")}));
    }

    void lockedTabOpensNavigationInANewTab()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.setCurrentUrl(url("/a/sub"));
        model.addTab(url("/b"));
        model.setCurrentIndex(0);
        model.setLocked(0, true);
        QVERIFY(model.isLocked(0));
        QVERIFY(!model.canGoBack());

        model.setCurrentUrl(url("/x"));
        QCOMPARE(model.urls(), (QList<QUrl>{url("/a/sub"), url("/x"), url("/b")}));
        QCOMPARE(model.currentIndex(), 1);
        QVERIFY(!model.isLocked(1));
    }

    void stateFollowsMovedAndClosedTabs()
    {
        TabModel model;
        model.addTab(url("/a"));
        model.addTab(url("/b"));
        model.addTab(url("/c"));
        model.setLocked(2, true);
        model.setName(2, QStringLiteral("  Work  "));
        model.moveTab(2, 1);
        QVERIFY(model.isLocked(1));
        QCOMPARE(model.nameAt(1), QStringLiteral("Work"));
        model.closeTab(0);
        QVERIFY(model.isLocked(0));
        QCOMPARE(model.nameAt(0), QStringLiteral("Work"));
        model.setName(0, QStringLiteral(" "));
        QVERIFY(model.nameAt(0).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TabModelTest)

#include "tabmodeltest.moc"
