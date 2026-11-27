// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchranking.h"

#include <QStandardPaths>
#include <QTest>

class SearchRankingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void scoresTiers()
    {
        using SearchRanking::score;
        QCOMPARE(score(QStringLiteral("Report.pdf"), QStringLiteral("report")), 1000);
        QCOMPARE(score(QStringLiteral("report.pdf"), QStringLiteral("report.pdf")), 1000);
        QCOMPARE(score(QStringLiteral("archive.tar.gz"), QStringLiteral("archive")), 1000);
        const int prefix = score(QStringLiteral("reports-2026.pdf"), QStringLiteral("report"));
        const int word = score(QStringLiteral("annual-report.pdf"), QStringLiteral("report"));
        const int camel = score(QStringLiteral("annualReport.pdf"), QStringLiteral("report"));
        const int inside = score(QStringLiteral("misreported.pdf"), QStringLiteral("report"));
        const int typo = score(QStringLiteral("annual-reprot.pdf"), QStringLiteral("report"));
        const int scattered = score(QStringLiteral("r-e-p-o-r-t.pdf"), QStringLiteral("report"));
        QVERIFY(prefix >= 800 && prefix < 1000);
        QVERIFY(word >= 600 && word < 800);
        QVERIFY(camel >= 600 && camel < 800);
        QVERIFY(inside >= 400 && inside < 600);
        QVERIFY(typo > 200 && typo <= 300);
        QVERIFY(scattered >= 100 && scattered < 200);
        QCOMPARE(score(QStringLiteral("notes.txt"), QStringLiteral("report")), 0);
        QCOMPARE(score(QStringLiteral("notes.txt"), QStringLiteral(" ")), 0);
        QCOMPARE(score(QString(), QStringLiteral("report")), 0);
        // Short terms are not matched by spelling, and a closer cover ranks higher.
        QCOMPARE(score(QStringLiteral("cat.txt"), QStringLiteral("cot")), 300);
        QCOMPARE(score(QStringLiteral("ab.txt"), QStringLiteral("xb")), 0);
        QVERIFY(score(QStringLiteral("report-a.pdf"), QStringLiteral("report")) > score(QStringLiteral("report-long-name.pdf"), QStringLiteral("report")));
    }

    void derivesTerms()
    {
        QCOMPARE(SearchRanking::terms(QStringLiteral(" report "), LiveSearch::Field::FileName), QStringList{QStringLiteral("report")});
        QCOMPARE(SearchRanking::terms(QStringLiteral("invoice"), LiveSearch::Field::Contents), QStringList{QStringLiteral("invoice")});
        QVERIFY(SearchRanking::terms(QStringLiteral("  "), LiveSearch::Field::FileName).isEmpty());
        QCOMPARE(SearchRanking::terms(QStringLiteral("report content:invoice meta:Canon type:pdf -old REPORT"), LiveSearch::Field::Query),
                 (QStringList{QStringLiteral("report"), QStringLiteral("invoice"), QStringLiteral("Canon")}));
        QVERIFY(SearchRanking::terms(QStringLiteral("a OR b"), LiveSearch::Field::Query).isEmpty());
        QVERIFY(SearchRanking::terms(QStringLiteral("report AND"), LiveSearch::Field::Query).isEmpty());
    }

    void ranksStably()
    {
        const QStringList paths{
            QStringLiteral("/a/notes.txt"),
            QStringLiteral("/a/misreported.pdf"),
            QStringLiteral("/b/report.pdf"),
            QStringLiteral("/a/other.txt"),
            QStringLiteral("/a/reports/"),
            QStringLiteral("/c/report.pdf"),
        };
        QCOMPARE(SearchRanking::rank(paths, {QStringLiteral("report")}),
                 (QStringList{QStringLiteral("/b/report.pdf"),
                              QStringLiteral("/c/report.pdf"),
                              QStringLiteral("/a/reports/"),
                              QStringLiteral("/a/misreported.pdf"),
                              QStringLiteral("/a/notes.txt"),
                              QStringLiteral("/a/other.txt")}));
        QCOMPARE(SearchRanking::rank(paths, {}), paths);
        QCOMPARE(SearchRanking::rank(paths, {QStringLiteral("zzz"), QStringLiteral("notes")}).first(), QStringLiteral("/a/notes.txt"));
    }
};

QTEST_GUILESS_MAIN(SearchRankingTest)
#include "searchrankingtest.moc"
