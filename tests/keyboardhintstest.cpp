// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardhints.h"

#include <QSet>
#include <QStandardPaths>
#include <QTest>

#include <algorithm>
#include <numeric>

using KeyboardHints::Kind;
using KeyboardHints::Layout;
using KeyboardHints::Matcher;

namespace
{

bool prefixFree(const QStringList &labels)
{
    for (int i = 0; i < labels.size(); ++i) {
        for (int j = 0; j < labels.size(); ++j) {
            if (i != j && labels.at(j).startsWith(labels.at(i))) {
                return false;
            }
        }
    }
    return true;
}

int totalLength(const QStringList &labels)
{
    int total = 0;
    for (const QString &label : labels) {
        total += label.size();
    }
    return total;
}

// The fewest keys @p count prefix-free labels over @p base letters can use.
int minimalTotalLength(int count, int base)
{
    if (count <= base) {
        return count;
    }
    // Repeatedly expand the shortest leaf of a complete tree.
    QList<int> leaves(base, 1);
    while (leaves.size() < count) {
        const auto shortest = std::min_element(leaves.begin(), leaves.end());
        const int depth = *shortest;
        leaves.erase(shortest);
        const int added = std::min<int>(base, count - leaves.size());
        for (int i = 0; i < added; ++i) {
            leaves.append(depth + 1);
        }
    }
    return std::accumulate(leaves.begin(), leaves.end(), 0);
}

} // namespace

class KeyboardHintsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void alphabetsUseTheHomeRow()
    {
        QCOMPARE(KeyboardHints::alphabet(Layout::Qwerty), QStringLiteral("fjdkslagh"));
        QCOMPARE(KeyboardHints::alphabet(Layout::Azerty), QStringLiteral("fjdkslqmgh"));
        QCOMPARE(KeyboardHints::alphabet(Layout::Dvorak), QStringLiteral("uhetonasid"));
        QCOMPARE(KeyboardHints::alphabet(Layout::Colemak), QStringLiteral("tnseriaodh"));
        for (const Layout layout : {Layout::Qwerty, Layout::Azerty, Layout::Dvorak, Layout::Colemak}) {
            const QString letters = KeyboardHints::alphabet(layout);
            QCOMPARE(QSet<QChar>(letters.begin(), letters.end()).size(), letters.size());
        }
    }

    void fewTargetsGetSingleKeys()
    {
        QCOMPARE(KeyboardHints::labels(0, QStringLiteral("asd")), QStringList());
        QCOMPARE(KeyboardHints::labels(-1, QStringLiteral("asd")), QStringList());
        QCOMPARE(KeyboardHints::labels(2, QStringLiteral("asd")), (QStringList{QStringLiteral("a"), QStringLiteral("s")}));
        QCOMPARE(KeyboardHints::labels(3, QStringLiteral("asd")), (QStringList{QStringLiteral("a"), QStringLiteral("s"), QStringLiteral("d")}));
    }

    void moreTargetsKeepEarlySingleKeys()
    {
        // Four labels from three keys: only the last key starts longer labels.
        QCOMPARE(KeyboardHints::labels(4, QStringLiteral("asd")),
                 (QStringList{QStringLiteral("a"), QStringLiteral("s"), QStringLiteral("da"), QStringLiteral("ds")}));
        QCOMPARE(KeyboardHints::labels(5, QStringLiteral("asd")),
                 (QStringList{QStringLiteral("a"), QStringLiteral("s"), QStringLiteral("da"), QStringLiteral("ds"), QStringLiteral("dd")}));
        QCOMPARE(
            KeyboardHints::labels(6, QStringLiteral("asd")),
            (QStringList{QStringLiteral("a"), QStringLiteral("sa"), QStringLiteral("ss"), QStringLiteral("sd"), QStringLiteral("da"), QStringLiteral("ds")}));
        QCOMPARE(KeyboardHints::labels(9, QStringLiteral("asd")).first(), QStringLiteral("aa"));
        QCOMPARE(KeyboardHints::labels(10, QStringLiteral("asd")).first(), QStringLiteral("aa"));
        QCOMPARE(KeyboardHints::labels(10, QStringLiteral("asd")).last(), QStringLiteral("dds"));
    }

    void labelsArePrefixFreeAndShortest_data()
    {
        QTest::addColumn<QString>("alphabet");
        QTest::addColumn<int>("count");
        for (const QString &alphabet : {QStringLiteral("ab"), QStringLiteral("asd"), KeyboardHints::alphabet(Layout::Qwerty)}) {
            for (int count : {1, 2, 3, 8, 9, 10, 17, 64, 80, 81, 82, 100, 730}) {
                QTest::addRow("%s-%d", qPrintable(alphabet), count) << alphabet << count;
            }
        }
    }

    void labelsArePrefixFreeAndShortest()
    {
        QFETCH(QString, alphabet);
        QFETCH(int, count);
        const QStringList labels = KeyboardHints::labels(count, alphabet);
        QCOMPARE(labels.size(), count);
        QCOMPARE(QSet<QString>(labels.begin(), labels.end()).size(), count);
        QVERIFY(prefixFree(labels));
        QCOMPARE(totalLength(labels), minimalTotalLength(count, alphabet.size()));
        for (int i = 1; i < labels.size(); ++i) {
            QVERIFY(labels.at(i - 1).size() <= labels.at(i).size());
        }
        for (const QString &label : labels) {
            for (const QChar key : label) {
                QVERIFY(alphabet.contains(key));
            }
        }
        QCOMPARE(KeyboardHints::labels(count, alphabet), labels);
    }

    void alphabetsAreNormalized()
    {
        QCOMPARE(KeyboardHints::labels(3, QStringLiteral("A aSb")), (QStringList{QStringLiteral("a"), QStringLiteral("s"), QStringLiteral("b")}));
        QCOMPARE(KeyboardHints::labels(2, QStringLiteral("a")), QStringList());
        QCOMPARE(KeyboardHints::labels(1, QStringLiteral("a")), QStringList{QStringLiteral("a")});
        QCOMPARE(KeyboardHints::labels(1, QString()), QStringList());
    }

    void itemsGetTheShortestLabels()
    {
        const QList<Kind> kinds{Kind::Place, Kind::Tab, Kind::Item, Kind::Location, Kind::Item};
        // Items first, then locations, tabs and places.
        QCOMPARE(KeyboardHints::assign(kinds, QStringLiteral("asd")),
                 (QStringList{QStringLiteral("dd"), QStringLiteral("ds"), QStringLiteral("a"), QStringLiteral("da"), QStringLiteral("s")}));
        QCOMPARE(KeyboardHints::assign({}, QStringLiteral("asd")), QStringList());
    }

    void matcherNarrowsAndCompletes()
    {
        Matcher matcher(KeyboardHints::labels(4, QStringLiteral("asd")));
        QCOMPARE(matcher.candidateCount(), 4);
        QCOMPARE(matcher.feed(QLatin1Char('D')), Matcher::Result::Pending);
        QCOMPARE(matcher.typed(), QStringLiteral("d"));
        QCOMPARE(matcher.matched(), -1);
        QVERIFY(!matcher.isCandidate(0));
        QVERIFY(matcher.isCandidate(2));
        QVERIFY(!matcher.isCandidate(4));
        QCOMPARE(matcher.candidateCount(), 2);

        QCOMPARE(matcher.feed(QLatin1Char('x')), Matcher::Result::NoMatch);
        QCOMPARE(matcher.typed(), QStringLiteral("d"));
        QCOMPARE(matcher.feed(QLatin1Char('s')), Matcher::Result::Matched);
        QCOMPARE(matcher.matched(), 3);
        QCOMPARE(matcher.feed(QLatin1Char('a')), Matcher::Result::NoMatch);

        QVERIFY(matcher.backspace());
        QCOMPARE(matcher.matched(), -1);
        QCOMPARE(matcher.typed(), QStringLiteral("d"));
        QVERIFY(matcher.backspace());
        QVERIFY(!matcher.backspace());
        QCOMPARE(matcher.feed(QLatin1Char('a')), Matcher::Result::Matched);
        QCOMPARE(matcher.matched(), 0);
        matcher.reset();
        QCOMPARE(matcher.typed(), QString());
        QCOMPARE(matcher.candidateCount(), 4);
    }

    void matcherSkipsEmptyLabels()
    {
        Matcher matcher({QString(), QStringLiteral("a")});
        QVERIFY(!matcher.isCandidate(0));
        QCOMPARE(matcher.candidateCount(), 1);
        QCOMPARE(matcher.feed(QLatin1Char('a')), Matcher::Result::Matched);
        QCOMPARE(matcher.matched(), 1);
    }
};

QTEST_GUILESS_MAIN(KeyboardHintsTest)

#include "keyboardhintstest.moc"
