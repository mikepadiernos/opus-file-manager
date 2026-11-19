// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentchartdialog.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
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
};

QTEST_MAIN(ContentChartTest)
#include "contentcharttest.moc"
