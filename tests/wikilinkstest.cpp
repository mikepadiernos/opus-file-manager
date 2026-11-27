// SPDX-License-Identifier: GPL-3.0-or-later

#include "wikilinks.h"

#include <QTest>

using namespace WikiLinks;

namespace
{
QStringList targetsOf(const QList<Link> &links)
{
    QStringList targets;
    for (const Link &link : links) {
        targets.append(link.target);
    }
    return targets;
}
}

class WikiLinksTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesTargetsAliasesAndHeadings()
    {
        const QString text = QStringLiteral("See [[Project plan]], [[Ideas|my ideas]] and [[Recipes#Bread | bread]].");
        const QList<Link> links = parse(text);
        QCOMPARE(links.size(), 3);

        QCOMPARE(links.at(0).target, QStringLiteral("Project plan"));
        QVERIFY(links.at(0).alias.isEmpty());
        QVERIFY(!links.at(0).embed);
        QCOMPARE(links.at(0).offset, text.indexOf(QStringLiteral("[[Project")));
        QCOMPARE(links.at(0).length, qsizetype(QStringLiteral("[[Project plan]]").size()));
        QCOMPARE(links.at(0).displayText(), QStringLiteral("Project plan"));

        QCOMPARE(links.at(1).target, QStringLiteral("Ideas"));
        QCOMPARE(links.at(1).alias, QStringLiteral("my ideas"));
        QCOMPARE(links.at(1).displayText(), QStringLiteral("my ideas"));

        QCOMPARE(links.at(2).target, QStringLiteral("Recipes"));
        QCOMPARE(links.at(2).heading, QStringLiteral("Bread"));
        QCOMPARE(links.at(2).alias, QStringLiteral("bread"));
        QCOMPARE(links.at(2).context, text);
    }

    void parsesEmbedsLocalHeadingsAndTablePipes()
    {
        const QList<Link> links = parse(QStringLiteral("![[diagram.png]]\n[[#Summary]]\n| [[Budget\\|the budget]] | 3 |\n"));
        QCOMPARE(links.size(), 3);
        QVERIFY(links.at(0).embed);
        QCOMPARE(links.at(0).target, QStringLiteral("diagram.png"));
        QCOMPARE(links.at(0).offset, qsizetype(0));
        QCOMPARE(links.at(0).length, qsizetype(16));

        QVERIFY(links.at(1).target.isEmpty());
        QCOMPARE(links.at(1).heading, QStringLiteral("Summary"));
        QCOMPARE(links.at(1).displayText(), QStringLiteral("Summary"));
        QCOMPARE(links.at(1).line, 1);

        QCOMPARE(links.at(2).target, QStringLiteral("Budget"));
        QCOMPARE(links.at(2).alias, QStringLiteral("the budget"));

        Link headed;
        headed.target = QStringLiteral("Recipes");
        headed.heading = QStringLiteral("Bread");
        QCOMPARE(headed.displayText(), QStringLiteral("Recipes › Bread"));
    }

    void ignoresCodeCommentsAndEscapes()
    {
        const QString text = QStringLiteral(
            "[[Before]]\n"
            "```markdown\n"
            "[[In fence]]\n"
            "```\n"
            "~~~~\n"
            "```\n"
            "[[In tilde fence]]\n"
            "~~~~\n"
            "Inline `[[span]]` and ``a ` [[double span]]`` and \\[[escaped]].\n"
            "<!-- [[comment]]\n"
            "[[still comment]] -->[[After comment]]\n"
            "\n"
            "    [[indented code]]\n"
            "\n"
            "- item\n"
            "    [[Nested list text]]\n"
            "[[unclosed\n"
            "]] [[ ]] [[a [[Inner]]\n");
        QCOMPARE(targetsOf(parse(text)),
                 (QStringList{QStringLiteral("Before"), QStringLiteral("After comment"), QStringLiteral("Nested list text"), QStringLiteral("Inner")}));
    }

    void reportsLinesAndContextWithWindowsLineEndings()
    {
        const QString text = QStringLiteral("# Title\r\n\r\n  Read [[Notes]] first.  \r\n[[Last]]");
        const QList<Link> links = parse(text);
        QCOMPARE(links.size(), 2);
        QCOMPARE(links.at(0).line, 2);
        QCOMPARE(links.at(0).context, QStringLiteral("Read [[Notes]] first."));
        QCOMPARE(links.at(1).line, 3);
        QCOMPARE(links.at(1).context, QStringLiteral("[[Last]]"));
        QCOMPARE(text.mid(links.at(1).offset, links.at(1).length), QStringLiteral("[[Last]]"));

        const QList<Link> long_ = parse(QString(500, QLatin1Char('x')) + QStringLiteral(" [[Far]]"));
        QCOMPARE(long_.size(), 1);
        QCOMPARE(long_.at(0).context.size(), qsizetype(160));
    }

    void rewritesLinksForAPreview()
    {
        const QString text = QStringLiteral("Go to [[My Note|*bold* alias]], [[Missing]] or `[[code]]`.");
        const QString out = toMarkdownLinks(text, [](const Link &link) {
            return link.target == QLatin1String("My Note") ? QUrl(QStringLiteral("opus-note:My Note")) : QUrl();
        });
        QCOMPARE(out, QStringLiteral("Go to [\\*bold\\* alias](<opus-note:My%20Note>), Missing or `[[code]]`."));
        QCOMPARE(toMarkdownLinks(QStringLiteral("No links."),
                                 [](const Link &) {
                                     return QUrl();
                                 }),
                 QStringLiteral("No links."));
    }
};

QTEST_GUILESS_MAIN(WikiLinksTest)

#include "wikilinkstest.moc"
