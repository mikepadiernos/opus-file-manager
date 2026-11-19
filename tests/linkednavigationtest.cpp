// SPDX-License-Identifier: GPL-3.0-or-later

#include "linkednavigation.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class LinkedNavigationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void mapsDescendantsAndRoots()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/sub/child")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("right/sub/child")));
        const QUrl left = QUrl::fromLocalFile(dir.filePath(QStringLiteral("left")));
        const QUrl right = QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")));
        const auto mapped = LinkedNavigation::target(left, right, QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub/child"))));
        QVERIFY2(mapped.error.isEmpty(), qPrintable(mapped.error));
        QCOMPARE(mapped.target, QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub/child"))));
        QCOMPARE(LinkedNavigation::target(left, right, left).target, right);
    }

    void rejectsMissingRemoteAndEscapes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("right")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("outside")));
        const QUrl left = QUrl::fromLocalFile(dir.filePath(QStringLiteral("left")));
        const QUrl right = QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")));
        QVERIFY(!LinkedNavigation::target(left, right, QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub")))).error.isEmpty());
        QVERIFY(!LinkedNavigation::target(left, right, QUrl::fromLocalFile(dir.path())).error.isEmpty());
        QVERIFY(!LinkedNavigation::target(left, right, QUrl(QStringLiteral("sftp://host/folder"))).error.isEmpty());
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside")), dir.filePath(QStringLiteral("left/link"))));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside")), dir.filePath(QStringLiteral("right/sub"))));
        QVERIFY(!LinkedNavigation::target(left, right, QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/link")))).error.isEmpty());
        QVERIFY(!LinkedNavigation::target(left, right, QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub")))).error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(LinkedNavigationTest)
#include "linkednavigationtest.moc"
