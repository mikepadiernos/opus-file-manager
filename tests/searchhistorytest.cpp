// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchhistory.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SearchHistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void keepsRecentDistinctQueriesFirst()
    {
        QStringList history = SearchHistory::added({}, QStringLiteral("  report  "));
        history = SearchHistory::added(history, QStringLiteral("invoice"));
        history = SearchHistory::added(history, QStringLiteral("report"));
        QCOMPARE(history, (QStringList{QStringLiteral("report"), QStringLiteral("invoice")}));
        QCOMPARE(SearchHistory::added(history, QStringLiteral("   ")), history);
        for (int i = 0; i < SearchHistory::MaximumEntries + 5; ++i) {
            history = SearchHistory::added(history, QString::number(i));
        }
        QCOMPARE(history.size(), SearchHistory::MaximumEntries);
        QCOMPARE(history.constFirst(), QString::number(SearchHistory::MaximumEntries + 4));
    }

    void persistsAndClearsQueries()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("SearchHistory"));
        QVERIFY(SearchHistory::load(group).isEmpty());
        // Commas and quotes survive KConfig's list encoding.
        const QStringList queries{QStringLiteral("name:\"a, b\""), QStringLiteral("type:pdf")};
        SearchHistory::save(group, queries);
        config->sync();
        config->reparseConfiguration();
        QCOMPARE(SearchHistory::load(group), queries);
        group.writeEntry("Queries", QStringList{QStringLiteral("x"), QStringLiteral(" "), QStringLiteral("x ")});
        QCOMPARE(SearchHistory::load(group), QStringList{QStringLiteral("x")});
        SearchHistory::save(group, {});
        QVERIFY(!group.hasKey("Queries"));

        auto search = config->group(QStringLiteral("Search"));
        QVERIFY(SearchHistory::retained(search));
        search.writeEntry("RetainSearchHistory", false);
        QVERIFY(!SearchHistory::retained(search));
    }
};

QTEST_GUILESS_MAIN(SearchHistoryTest)
#include "searchhistorytest.moc"
