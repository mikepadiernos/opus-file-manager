// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabcolors.h"
#include "labelcolor.h"

#include <KConfig>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TabColorsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void labelColorKeysRoundTrip()
    {
        QCOMPARE(LabelColors::all().size(), 8);
        for (LabelColor color : LabelColors::all()) {
            QCOMPARE(LabelColors::fromKey(LabelColors::key(color)), color);
            QVERIFY(LabelColors::rgb(color).isValid());
        }
        QCOMPARE(LabelColors::fromKey(QStringLiteral("nonsense")), LabelColor::None);
        QVERIFY(!LabelColors::rgb(LabelColor::None).isValid());
    }

    void colorsAreKeptPerFolderAndSaved()
    {
        QTemporaryDir dir;
        const QUrl folder = QUrl::fromLocalFile(QStringLiteral("/data/work"));
        {
            KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
            TabColors colors(config.group(QStringLiteral("TabColors")));
            QSignalSpy changed(&colors, &TabColors::colorChanged);
            colors.setColor(folder, LabelColor::Green);
            QCOMPARE(changed.count(), 1);
            QCOMPARE(colors.colorFor(QUrl::fromLocalFile(QStringLiteral("/data/work/"))), LabelColor::Green);
            QCOMPARE(colors.colorFor(QUrl::fromLocalFile(QStringLiteral("/data"))), LabelColor::None);
        }
        KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        TabColors colors(config.group(QStringLiteral("TabColors")));
        QCOMPARE(colors.colorFor(folder), LabelColor::Green);
        colors.setColor(folder, LabelColor::None);
        QCOMPARE(colors.colorFor(folder), LabelColor::None);
        KConfig reread(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        QVERIFY(reread.group(QStringLiteral("TabColors")).keyList().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TabColorsTest)

#include "tabcolorstest.moc"
