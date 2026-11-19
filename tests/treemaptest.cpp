// SPDX-License-Identifier: GPL-3.0-or-later

#include "treemap.h"

#include <QStandardPaths>
#include <QTest>

#include <limits>

class TreemapTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void tilesAreProportionalAndDoNotOverlap()
    {
        const QList<quint64> weights{1, 4, 0, 3, 2};
        const QRectF bounds(10, 20, 200, 100);
        const auto tiles = Treemap::layout(weights, bounds);
        QCOMPARE(tiles.size(), weights.size());
        for (qsizetype i = 0; i < tiles.size(); ++i) {
            if (!weights[i]) {
                QVERIFY(tiles[i].isEmpty());
                continue;
            }
            QVERIFY(bounds.contains(tiles[i]));
            QVERIFY(qAbs(tiles[i].width() * tiles[i].height() - 20000.0 * weights[i] / 10.0) < 0.001);
            for (qsizetype j = 0; j < i; ++j) {
                QVERIFY(tiles[i].intersected(tiles[j]).isEmpty());
            }
        }
    }

    void handlesEmptyAndLargeWeights()
    {
        QVERIFY(Treemap::layout({}, QRectF(0, 0, 100, 100)).isEmpty());
        const auto zeros = Treemap::layout({0, 0}, QRectF(0, 0, 100, 100));
        QVERIFY(zeros[0].isEmpty() && zeros[1].isEmpty());
        const auto equal = Treemap::layout({std::numeric_limits<quint64>::max(), std::numeric_limits<quint64>::max()}, QRectF(0, 0, 100, 100));
        QCOMPARE(equal[0].width() * equal[0].height(), 5000.0);
        QCOMPARE(equal[1].width() * equal[1].height(), 5000.0);
    }

    void coversBoundsForManyItems()
    {
        QList<quint64> weights;
        quint64 total = 0;
        for (quint64 i = 1; i <= 200; ++i) {
            const quint64 weight = (i * 47) % 103 + 1;
            weights.append(weight);
            total += weight;
        }
        const QRectF bounds(0, 0, 1200, 800);
        const auto tiles = Treemap::layout(weights, bounds);
        double area = 0;
        for (qsizetype i = 0; i < tiles.size(); ++i) {
            const double tileArea = tiles[i].width() * tiles[i].height();
            QVERIFY(qAbs(tileArea - bounds.width() * bounds.height() * weights[i] / total) < 0.001);
            area += tileArea;
            for (qsizetype j = 0; j < i; ++j) {
                const QRectF overlap = tiles[i].intersected(tiles[j]);
                QVERIFY(overlap.isEmpty() || overlap.width() * overlap.height() < 1e-8);
            }
        }
        QVERIFY(qAbs(area - bounds.width() * bounds.height()) < 0.001);
    }
};

QTEST_GUILESS_MAIN(TreemapTest)
#include "treemaptest.moc"
