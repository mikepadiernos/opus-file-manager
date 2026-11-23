// SPDX-License-Identifier: GPL-3.0-or-later

#include "livesearch.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class LiveSearchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void searchesNamesAndContentsSeparately()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("sub")));
        QFile first(root.filePath(QStringLiteral("name-Needle.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        QCOMPARE(first.write("other"), 5);
        first.close();
        QFile second(root.filePath(QStringLiteral("sub/content.txt")));
        QVERIFY(second.open(QIODevice::WriteOnly));
        QCOMPARE(second.write("a NEEDLE in text"), 16);
        second.close();
        QVERIFY(QFile::link(root.filePath(QStringLiteral("sub")), root.filePath(QStringLiteral("link"))));

        const auto names = LiveSearch::find(root.path(), QStringLiteral("needle"), LiveSearch::Field::FileName);
        QCOMPARE(names.paths, QStringList{first.fileName()});
        QVERIFY(names.errors.isEmpty());
        const auto contents = LiveSearch::find(root.path(), QStringLiteral("needle"), LiveSearch::Field::Contents);
        QCOMPARE(contents.paths, QStringList{second.fileName()});
        QVERIFY(contents.errors.isEmpty());
    }

    void skipsBinaryAndOversizedContent()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile binary(root.filePath(QStringLiteral("binary")));
        QVERIFY(binary.open(QIODevice::WriteOnly));
        QCOMPARE(binary.write(QByteArray("needle\0data", 11)), 11);
        binary.close();
        QFile oversized(root.filePath(QStringLiteral("large")));
        QVERIFY(oversized.open(QIODevice::WriteOnly));
        QVERIFY(oversized.resize(2 * 1024 * 1024 + 1));
        oversized.close();
        QFile invalid(root.filePath(QStringLiteral("invalid")));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        QCOMPARE(invalid.write(QByteArray("needle\xff", 7)), 7);
        invalid.close();
        QFile atLimit(root.filePath(QStringLiteral("at-limit")));
        QVERIFY(atLimit.open(QIODevice::WriteOnly));
        QCOMPARE(atLimit.write("needle"), 6);
        QCOMPARE(atLimit.write(QByteArray(2 * 1024 * 1024 - 6, 'a')), 2 * 1024 * 1024 - 6);
        atLimit.close();
        const auto result = LiveSearch::find(root.path(), QStringLiteral("needle"), LiveSearch::Field::Contents);
        QCOMPARE(result.paths, QStringList{atLimit.fileName()});
        QCOMPARE(result.skippedFiles, 3);
    }

    void respectsRemainingEntryBudget()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("needle1"), QStringLiteral("needle2")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto result = LiveSearch::find(root.path(), QStringLiteral("needle"), LiveSearch::Field::FileName, {}, {}, {}, 1);
        QCOMPARE(result.scannedEntries, 1);
        QCOMPARE(result.paths.size(), 1);
        QVERIFY(result.limited);
    }

    void rejectsInvalidInputAndCancels()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(!LiveSearch::find(root.filePath(QStringLiteral("missing")), QStringLiteral("x"), LiveSearch::Field::FileName).errors.isEmpty());
        QVERIFY(!LiveSearch::find(root.path(), QStringLiteral(" "), LiveSearch::Field::FileName).errors.isEmpty());
        const auto result = LiveSearch::find(root.path(), QStringLiteral("x"), LiveSearch::Field::FileName, [] {
            return true;
        });
        QVERIFY(result.canceled);
        QFile file(root.filePath(QStringLiteral("one")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        bool cancel = false;
        const auto interrupted = LiveSearch::find(
            root.path(),
            QStringLiteral("one"),
            LiveSearch::Field::FileName,
            [&cancel] {
                return cancel;
            },
            [&cancel](int, const QString &) {
                cancel = true;
            });
        QVERIFY(interrupted.canceled);
    }

    void filtersPathsAndPrunesExcludedSubtrees()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("include/deep")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("excluded/nested")));
        const QString shallow = root.filePath(QStringLiteral("match-top"));
        const QString child = root.filePath(QStringLiteral("include/match-child"));
        const QString deep = root.filePath(QStringLiteral("include/deep/match-deep"));
        const QString excluded = root.filePath(QStringLiteral("excluded/nested/match-excluded"));
        for (const QString &path : {shallow, child, deep, excluded}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        PathFilter filter;
        filter.excludedFolders = {root.filePath(QStringLiteral("excluded"))};
        filter.parentText = QStringLiteral("include");
        filter.pathText = QStringLiteral("match-");
        filter.maxDepth = 1;
        const auto result = LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(result.paths, QStringList{child});
        QVERIFY(result.errors.isEmpty());
        filter.maxDepth = -1;
        const auto nested = LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(nested.paths, (QStringList{child, deep}));
        filter.parentText.clear();
        filter.maxDepth = 0;
        const auto topLevel = LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(topLevel.paths, QStringList{shallow});
        filter.maxDepth = -1;
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("excluded-other")));
        QFile boundary(root.filePath(QStringLiteral("excluded-other/match-boundary")));
        QVERIFY(boundary.open(QIODevice::WriteOnly));
        boundary.close();
        const auto boundaryResult = LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter);
        QVERIFY(boundaryResult.paths.contains(boundary.fileName()));
        QVERIFY(!boundaryResult.paths.contains(excluded));
        filter.excludedFolders = {root.path()};
        QVERIFY(LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter).paths.isEmpty());
        filter.excludedFolders = {QStringLiteral("relative")};
        QVERIFY(!LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName, {}, {}, filter).errors.isEmpty());
    }

    void capsResults()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (int i = 0; i < 5001; ++i) {
            QFile file(root.filePath(QStringLiteral("match-%1").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto result = LiveSearch::find(root.path(), QStringLiteral("match"), LiveSearch::Field::FileName);
        QCOMPARE(result.paths.size(), 5000);
        QVERIFY(result.limited);
    }
};

QTEST_GUILESS_MAIN(LiveSearchTest)
#include "livesearchtest.moc"
