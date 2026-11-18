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
};

QTEST_GUILESS_MAIN(TabModelTest)

#include "tabmodeltest.moc"
