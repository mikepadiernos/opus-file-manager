// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentchartdialog.h"
#include "cushiontreemap.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

class ContentChartTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void scansFilesFoldersAndSkipsLinks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder")));
        QFile first(dir.filePath(QStringLiteral(".hidden")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        QCOMPARE(first.write("ab"), 2);
        first.close();
        QFile second(dir.filePath(QStringLiteral("folder/data")));
        QVERIFY(second.open(QIODevice::WriteOnly));
        QCOMPARE(second.write("12345678"), 8);
        second.close();
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("folder")), dir.filePath(QStringLiteral("alias"))));
        ContentChartDialog chart(QUrl::fromLocalFile(dir.path()));
        chart.show();
        auto *status = chart.findChild<QLabel *>(QStringLiteral("contentChartStatus"));
        auto *tree = chart.findChild<QTreeWidget *>(QStringLiteral("contentChartEntries"));
        auto *cancel = chart.findChild<QPushButton *>(QStringLiteral("contentChartCancel"));
        QVERIFY(status && tree && cancel);
        QTRY_VERIFY(!cancel->isEnabled());
        QCOMPARE(tree->topLevelItemCount(), 3);
        QVERIFY(status->text().contains(QStringLiteral("Skipped: 1")));
        const auto row = [tree](const QString &name) {
            const auto matches = tree->findItems(name, Qt::MatchExactly, 0);
            return matches.isEmpty() ? nullptr : matches.first();
        };
        QVERIFY(row(QStringLiteral(".hidden")));
        QVERIFY(row(QStringLiteral("folder")));
        QVERIFY(row(QStringLiteral("alias")));
        QCOMPARE(row(QStringLiteral(".hidden"))->data(2, Qt::UserRole).toInt(), 200);
        QCOMPARE(row(QStringLiteral("folder"))->data(2, Qt::UserRole).toInt(), 800);
        QVERIFY(!row(QStringLiteral("alias"))->data(2, Qt::UserRole).isValid());
        auto *treemap = chart.findChild<CushionTreemap *>(QStringLiteral("contentTreemap"));
        QVERIFY(treemap);
        const auto rectangles = treemap->tiles();
        QCOMPARE(rectangles.size(), 3);
        int drawable = 0;
        for (const QRectF &tile : rectangles) {
            if (!tile.isEmpty()) {
                ++drawable;
                QTest::mouseClick(treemap, Qt::LeftButton, Qt::NoModifier, tile.center().toPoint());
                QVERIFY(tree->currentItem());
                QVERIFY(tree->currentItem()->text(0) == QLatin1String(".hidden") || tree->currentItem()->text(0) == QLatin1String("folder"));
            }
        }
        QCOMPARE(drawable, 2);
        QVERIFY(!treemap->grab().isNull());
        QVERIFY(QMetaObject::invokeMethod(tree, "itemActivated", Qt::DirectConnection, Q_ARG(QTreeWidgetItem *, row(QStringLiteral("folder"))), Q_ARG(int, 0)));
        auto *child = chart.findChild<ContentChartDialog *>(QStringLiteral("contentChartDialog"));
        QVERIFY(child);
        QTRY_VERIFY(!child->findChild<QPushButton *>(QStringLiteral("contentChartCancel"))->isEnabled());
        QCOMPARE(child->findChild<QTreeWidget *>(QStringLiteral("contentChartEntries"))->topLevelItemCount(), 1);
        QCOMPARE(child->findChild<QLabel *>(QStringLiteral("contentChartFolder"))->text(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder"))).toDisplayString());
    }

    void handlesEmptyMissingAndCancellation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ContentChartDialog empty(QUrl::fromLocalFile(dir.path()));
        QTRY_VERIFY(!empty.findChild<QPushButton *>(QStringLiteral("contentChartCancel"))->isEnabled());
        QCOMPARE(empty.findChild<QTreeWidget *>(QStringLiteral("contentChartEntries"))->topLevelItemCount(), 0);
        QVERIFY(empty.findChild<QLabel *>(QStringLiteral("contentChartStatus"))->text().startsWith(QStringLiteral("Total:")));
        ContentChartDialog missing(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing"))));
        QTRY_VERIFY(!missing.findChild<QPushButton *>(QStringLiteral("contentChartCancel"))->isEnabled());
        QVERIFY(missing.findChild<QLabel *>(QStringLiteral("contentChartStatus"))->text().startsWith(QStringLiteral("Could not list")));
        ContentChartDialog canceled(QUrl::fromLocalFile(dir.path()));
        canceled.findChild<QPushButton *>(QStringLiteral("contentChartCancel"))->click();
        QCOMPARE(canceled.findChild<QLabel *>(QStringLiteral("contentChartStatus"))->text(), QStringLiteral("Scan canceled; the chart is incomplete."));
    }

    void appliesChartPreferencesAndKeepsSkippedEntriesVisible()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile small(dir.filePath(QStringLiteral("small")));
        QVERIFY(small.open(QIODevice::WriteOnly));
        QCOMPARE(small.write("x"), 1);
        small.close();
        QFile large(dir.filePath(QStringLiteral("large")));
        QVERIFY(large.open(QIODevice::WriteOnly));
        QCOMPARE(large.write(QByteArray(20, 'x')), 20);
        large.close();
        QVERIFY(QFile::link(small.fileName(), dir.filePath(QStringLiteral("link"))));
        const auto oldShow = Settings::contentChartShowTreemap();
        const auto oldSort = Settings::contentChartSort();
        const auto oldDescending = Settings::contentChartSortDescending();
        const auto restore = qScopeGuard([oldShow, oldSort, oldDescending] {
            Settings::setContentChartShowTreemap(oldShow);
            Settings::setContentChartSort(oldSort);
            Settings::setContentChartSortDescending(oldDescending);
        });
        Settings::setContentChartShowTreemap(false);
        Settings::setContentChartSort(Settings::EnumContentChartSort::Size);
        Settings::setContentChartSortDescending(true);
        ContentChartDialog chart(QUrl::fromLocalFile(dir.path()));
        chart.show();
        QTRY_VERIFY(!chart.findChild<QPushButton *>(QStringLiteral("contentChartCancel"))->isEnabled());
        auto *tree = chart.findChild<QTreeWidget *>(QStringLiteral("contentChartEntries"));
        auto *map = chart.findChild<CushionTreemap *>(QStringLiteral("contentTreemap"));
        QVERIFY(tree && map);
        QVERIFY(map->isHidden());
        QCOMPARE(tree->topLevelItemCount(), 3);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("large"));
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("small"));
        QCOMPARE(tree->topLevelItem(2)->text(0), QStringLiteral("link"));
        QVERIFY(chart.findChild<QLabel *>(QStringLiteral("contentChartStatus"))->text().contains(QStringLiteral("Skipped: 1")));
        chart.findChild<QCheckBox *>(QStringLiteral("contentChartShowTreemap"))->setChecked(true);
        QVERIFY(!map->isHidden());
        chart.findChild<QComboBox *>(QStringLiteral("contentChartSort"))->setCurrentIndex(Settings::EnumContentChartSort::Name);
        QTRY_COMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("small"));
        chart.findChild<QComboBox *>(QStringLiteral("contentChartSort"))->setCurrentIndex(Settings::EnumContentChartSort::AsListed);
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QCOMPARE(tree->topLevelItem(i)->data(0, Qt::UserRole).toInt(), i);
        }
    }

    void treemapRecalculatesAfterResize()
    {
        CushionTreemap treemap;
        treemap.setItems({{QStringLiteral("a"), 1}, {QStringLiteral("b"), 3}});
        treemap.resize(200, 200);
        treemap.show();
        QTRY_COMPARE(treemap.tiles().size(), 2);
        QCOMPARE(treemap.tiles().at(0).width() * treemap.tiles().at(0).height(), 10000.0);
        treemap.resize(400, 200);
        QTRY_COMPARE(treemap.tiles().at(0).width() * treemap.tiles().at(0).height(), 20000.0);
    }
};

QTEST_MAIN(ContentChartTest)
#include "contentcharttest.moc"
