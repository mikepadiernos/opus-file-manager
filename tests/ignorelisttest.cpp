// SPDX-License-Identifier: GPL-3.0-or-later

#include "ignorelist.h"

#include <KConfig>

#include <QTemporaryDir>
#include <QTest>

class IgnoreListTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exactNamesIgnoreCase()
    {
        const IgnoreList list({QStringLiteral("desktop.ini")});
        QVERIFY(list.ignores(QStringLiteral("Desktop.INI")));
        QVERIFY(!list.ignores(QStringLiteral("desktop.ini.bak")));
    }

    void globsMatchWholeNames()
    {
        const IgnoreList list({QStringLiteral("*.tmp"), QStringLiteral("~$*")});
        QVERIFY(list.ignores(QStringLiteral("build.TMP")));
        QVERIFY(list.ignores(QStringLiteral("~$report.docx")));
        QVERIFY(!list.ignores(QStringLiteral("build.tmp.txt")));
    }

    void invalidEntriesAreDropped()
    {
        QStringList entries{QStringLiteral("a/b"), QString(81, u'x'), QStringLiteral("  "), QStringLiteral(" keep ")};
        for (int i = 0; i < IgnoreList::MaxEntries; ++i) {
            entries.append(QStringLiteral("n%1").arg(i));
        }
        const IgnoreList list(entries);
        QCOMPARE(list.entries().size(), IgnoreList::MaxEntries);
        QCOMPARE(list.entries().first(), QStringLiteral("keep"));
        QVERIFY(!list.ignores(QStringLiteral("a/b")));
    }

    void configDefaultsAndStoredList()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Files"));
        QCOMPARE(IgnoreList::fromConfig(group).entries(), QStringList({QStringLiteral(".directory"), QStringLiteral("desktop.ini")}));
        group.writeEntry("IgnoreList", QStringList{QStringLiteral("Thumbs.db")});
        QCOMPARE(IgnoreList::fromConfig(group).entries(), QStringList({QStringLiteral("Thumbs.db")}));
        group.writeEntry("IgnoreList", QStringList());
        QVERIFY(IgnoreList::fromConfig(group).entries().isEmpty());
    }
};

QTEST_GUILESS_MAIN(IgnoreListTest)

#include "ignorelisttest.moc"
