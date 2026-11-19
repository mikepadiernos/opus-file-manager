// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationhistory.h"

#include <QTest>

class NavigationHistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyByDefault()
    {
        NavigationHistory history;
        QVERIFY(history.isEmpty());
        QVERIFY(!history.current().isValid());
        QVERIFY(!history.canGoBack());
        QVERIFY(!history.canGoForward());
        QCOMPARE(history.goBack(), QUrl());
    }

    void pushAndNavigate()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/b")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/c")));
        QCOMPARE(history.size(), 3);
        QCOMPARE(history.current().toLocalFile(), QStringLiteral("/c"));
        QVERIFY(history.canGoBack());
        QVERIFY(!history.canGoForward());

        QCOMPARE(history.goBack().toLocalFile(), QStringLiteral("/b"));
        QVERIFY(history.canGoForward());
        QCOMPARE(history.goBack().toLocalFile(), QStringLiteral("/a"));
        QVERIFY(!history.canGoBack());
        QCOMPARE(history.goBack().toLocalFile(), QStringLiteral("/a"));
        QCOMPARE(history.goForward().toLocalFile(), QStringLiteral("/b"));
    }

    void pushDiscardsForwardEntries()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/b")));
        history.goBack();
        history.push(QUrl::fromLocalFile(QStringLiteral("/c")));
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.entries().last().toLocalFile(), QStringLiteral("/c"));
        QVERIFY(!history.canGoForward());
    }

    void pushingCurrentIsNoOp()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/a/")));
        history.push(QUrl());
        QCOMPARE(history.size(), 1);
    }

    void storesUrlsWithoutTrailingSlash()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a/")));
        QCOMPARE(history.current(), QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/b/")));
        QCOMPARE(history.current(), QUrl::fromLocalFile(QStringLiteral("/b")));
        history.replaceCurrent(QUrl::fromLocalFile(QStringLiteral("/c/")));
        QCOMPARE(history.current(), QUrl::fromLocalFile(QStringLiteral("/c")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/")));
        QCOMPARE(history.current(), QUrl::fromLocalFile(QStringLiteral("/")));
    }

    void replaceCurrentKeepsForward()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a")));
        history.push(QUrl::fromLocalFile(QStringLiteral("/b")));
        history.goBack();
        history.replaceCurrent(QUrl::fromLocalFile(QStringLiteral("/x")));
        QCOMPARE(history.current().toLocalFile(), QStringLiteral("/x"));
        QVERIFY(history.canGoForward());
        QCOMPARE(history.size(), 2);
    }

    void viewStateFollowsHistoryEntries()
    {
        NavigationHistory history(QUrl::fromLocalFile(QStringLiteral("/a")));
        const QUrl selected = QUrl::fromLocalFile(QStringLiteral("/a/file"));
        history.setCurrentViewState({{selected}, 12, 4, true});
        history.push(QUrl::fromLocalFile(QStringLiteral("/b")));
        QVERIFY(!history.currentViewState().valid);
        history.setCurrentViewState({{}, 7, 0, true});
        history.goBack();
        QCOMPARE(history.currentViewState().selectedUrls, QList<QUrl>{selected});
        QCOMPARE(history.currentViewState().verticalScroll, 12);
        history.push(QUrl::fromLocalFile(QStringLiteral("/c")));
        QVERIFY(!history.currentViewState().valid);
        QVERIFY(!history.canGoForward());
    }

    void capsEntries()
    {
        NavigationHistory history;
        for (int i = 0; i < NavigationHistory::MaxEntries + 20; ++i) {
            history.push(QUrl::fromLocalFile(QStringLiteral("/%1").arg(i)));
        }
        QCOMPARE(history.size(), NavigationHistory::MaxEntries);
        QCOMPARE(history.currentIndex(), NavigationHistory::MaxEntries - 1);
        QCOMPARE(history.current().toLocalFile(), QStringLiteral("/%1").arg(NavigationHistory::MaxEntries + 19));
    }
};

QTEST_GUILESS_MAIN(NavigationHistoryTest)

#include "navigationhistorytest.moc"
