// SPDX-License-Identifier: GPL-3.0-or-later

#include "appfonts.h"

#include <QTest>

class AppFontsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripsCustomFonts()
    {
        QFont font(QStringLiteral("Noto Sans"), 13);
        font.setBold(true);
        font.setItalic(true);
        const QString setting = AppFonts::toSetting(font);
        QVERIFY(!setting.isEmpty());
        const QFont restored = AppFonts::fromSetting(setting);
        QVERIFY(AppFonts::isOverride(restored));
        QCOMPARE(restored.family(), font.family());
        QCOMPARE(restored.pointSizeF(), 13.0);
        QVERIFY(restored.bold());
        QVERIFY(restored.italic());
    }

    void emptyOrInvalidSettingsFollowPlasma()
    {
        for (const QString &setting : {QString(), QStringLiteral("   "), QStringLiteral(",,,")}) {
            const QFont font = AppFonts::fromSetting(setting);
            QVERIFY2(!AppFonts::isOverride(font), qPrintable(setting));
        }
        QVERIFY(AppFonts::toSetting(QFont()).isEmpty());
    }

    void pixelSizesAreStoredAsPoints()
    {
        QFont font(QStringLiteral("Noto Sans"));
        font.setPixelSize(20);
        const QFont restored = AppFonts::fromSetting(AppFonts::toSetting(font));
        QCOMPARE(restored.pixelSize(), -1);
        QVERIFY(restored.pointSizeF() > 0);
    }
};

QTEST_MAIN(AppFontsTest)

#include "appfontstest.moc"
