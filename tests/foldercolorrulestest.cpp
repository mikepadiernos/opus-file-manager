// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercolorrules.h"

#include <QDir>
#include <QHash>
#include <QStandardPaths>
#include <QTest>

using Rule = FolderColorRules::Rule;
using Match = FolderColorRules::Match;

namespace
{
QString entry(Match match, const QString &pattern, const QColor &color, bool subfolders = false)
{
    return FolderColorRules::entry({match, pattern, color, subfolders});
}
}

class FolderColorRulesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void roundTripsAndRejectsInvalidRules()
    {
        const Rule rule{Match::Path, QStringLiteral("~/Work/"), QColor(0xda, 0x44, 0x53), true};
        const std::optional<Rule> parsed = FolderColorRules::parse(FolderColorRules::entry(rule));
        QVERIFY(parsed);
        QCOMPARE(parsed->match, Match::Path);
        QCOMPARE(parsed->pattern, QStringLiteral("~/Work"));
        QCOMPARE(parsed->color, rule.color);
        QVERIFY(parsed->subfolders);

        QVERIFY(!FolderColorRules::parse(QStringLiteral("not json")));
        QVERIFY(!FolderColorRules::parse(entry(Match::Path, QStringLiteral("relative/path"), Qt::red)));
        QVERIFY(!FolderColorRules::parse(entry(Match::Name, QStringLiteral("a/b"), Qt::red)));
        QVERIFY(!FolderColorRules::parse(entry(Match::Tag, QStringLiteral("  "), Qt::red)));
        QVERIFY(!FolderColorRules::parse(entry(Match::Tag, QStringLiteral("work"), QColor())));
        QVERIFY(!FolderColorRules::parse(QStringLiteral(R"({"match":"size","pattern":"x","color":"#ff0000"})")));
    }

    void dropsDuplicatesAndLimitsRules()
    {
        QStringList entries{entry(Match::Name, QStringLiteral("a"), Qt::red), entry(Match::Name, QStringLiteral("a"), Qt::red), QStringLiteral("{}")};
        QCOMPARE(FolderColorRules(entries).rules().size(), 1);
        entries.clear();
        for (int i = 0; i < FolderColorRules::MaximumRules + 5; ++i) {
            entries.append(entry(Match::Name, QString::number(i), Qt::red));
        }
        QCOMPARE(FolderColorRules(entries).rules().size(), FolderColorRules::MaximumRules);
        QCOMPARE(FolderColorRules(FolderColorRules(entries).entries()).rules(), FolderColorRules(entries).rules());
    }

    void matchesPathsWithOptionalSubfolders()
    {
        const QColor red(0xda, 0x44, 0x53);
        const FolderColorRules exact({entry(Match::Path, QStringLiteral("/data/work"), red)});
        QCOMPARE(exact.colorFor(QStringLiteral("/data/work")), red);
        QCOMPARE(exact.colorFor(QStringLiteral("/data/work/")), red);
        QVERIFY(!exact.colorFor(QStringLiteral("/data/work/a")).isValid());
        QVERIFY(!exact.colorFor(QStringLiteral("/data/workshop")).isValid());

        const FolderColorRules inherited({entry(Match::Path, QStringLiteral("/data/work"), red, true)});
        QCOMPARE(inherited.colorFor(QStringLiteral("/data/work/a/b")), red);
        QVERIFY(!inherited.colorFor(QStringLiteral("/data/workshop")).isValid());
        QVERIFY(!inherited.colorFor(QStringLiteral("/data")).isValid());

        const FolderColorRules home({entry(Match::Path, QStringLiteral("~/Projects"), red)});
        QCOMPARE(home.colorFor(QDir::homePath() + QStringLiteral("/Projects")), red);
    }

    void matchesNamePatternsCaseInsensitively()
    {
        const QColor green(0x27, 0xae, 0x60);
        const FolderColorRules rules({entry(Match::Name, QStringLiteral("*-archive"), green)});
        QCOMPARE(rules.colorFor(QStringLiteral("/data/2024-Archive")), green);
        QVERIFY(!rules.colorFor(QStringLiteral("/data/2024-archive/inside")).isValid());
        QVERIFY(!rules.colorFor(QStringLiteral("/data/archive")).isValid());

        const FolderColorRules inherited({entry(Match::Name, QStringLiteral("node_modules"), green, true)});
        QCOMPARE(inherited.colorFor(QStringLiteral("/src/app/node_modules/pkg/lib")), green);
        QVERIFY(!inherited.colorFor(QStringLiteral("/src/app")).isValid());
    }

    void matchesTagsWithOptionalSubfolders()
    {
        const QColor blue(0x3d, 0xae, 0xe9);
        const QHash<QString, QStringList> tags{{QStringLiteral("/data/client"), {QStringLiteral("Clients"), QStringLiteral("2024")}}};
        int reads = 0;
        const FolderColorRules::TagReader reader = [&tags, &reads](const QString &path) {
            ++reads;
            return tags.value(path);
        };
        const FolderColorRules exact({entry(Match::Tag, QStringLiteral("clients"), blue)});
        QCOMPARE(exact.colorFor(QStringLiteral("/data/client"), reader), blue);
        QVERIFY(!exact.colorFor(QStringLiteral("/data/client/a"), reader).isValid());
        QCOMPARE(reads, 2);

        const FolderColorRules inherited({entry(Match::Tag, QStringLiteral("Clients"), blue, true)});
        QCOMPARE(inherited.colorFor(QStringLiteral("/data/client/a/b"), reader), blue);
    }

    void firstMatchingRuleWins()
    {
        const QColor red(0xda, 0x44, 0x53);
        const QColor green(0x27, 0xae, 0x60);
        const QStringList entries{entry(Match::Name, QStringLiteral("docs"), red), entry(Match::Path, QStringLiteral("/data"), green, true)};
        QCOMPARE(FolderColorRules(entries).colorFor(QStringLiteral("/data/docs")), red);
        QCOMPARE(FolderColorRules(entries).colorFor(QStringLiteral("/data/other")), green);
        const QStringList reversed{entries.at(1), entries.at(0)};
        QCOMPARE(FolderColorRules(reversed).colorFor(QStringLiteral("/data/docs")), green);
        QVERIFY(!FolderColorRules().colorFor(QStringLiteral("/data/docs")).isValid());
    }
};

QTEST_GUILESS_MAIN(FolderColorRulesTest)
#include "foldercolorrulestest.moc"
