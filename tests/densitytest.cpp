// SPDX-License-Identifier: GPL-3.0-or-later

#include "density.h"

#include <QTest>

class DensityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void itemSpaces()
    {
        QCOMPARE(Density::itemSpace(Density::Items::Normal, 7), 2);
        QCOMPARE(Density::itemSpace(Density::Items::Compact, 7), 1);
        QCOMPARE(Density::itemSpace(Density::Items::VeryCompact, 7), 0);
        QCOMPARE(Density::itemSpace(Density::Items::Custom, 7), 7);
        QCOMPARE(Density::itemSpace(Density::Items::Custom, 42), Density::MaxCustomItemSpace);
        QCOMPARE(Density::itemSpace(Density::Items::Custom, -3), 0);
    }

    void barMargins()
    {
        QCOMPARE(Density::barMargin(Density::Bars::Normal), -1);
        QCOMPARE(Density::barMargin(Density::Bars::Compact), 2);
        QCOMPARE(Density::barMargin(Density::Bars::VeryCompact), 0);
    }
};

QTEST_GUILESS_MAIN(DensityTest)

#include "densitytest.moc"
