// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersync.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}

class FolderSyncTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void comparesFilesAndFilters()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        const QString left = root.filePath(QStringLiteral("left"));
        const QString right = root.filePath(QStringLiteral("right"));
        QVERIFY(writeFile(left + QStringLiteral("/same.txt"), "same"));
        QVERIFY(writeFile(right + QStringLiteral("/same.txt"), "same"));
        QVERIFY(writeFile(left + QStringLiteral("/sub/different.txt"), "aaaa"));
        QVERIFY(writeFile(right + QStringLiteral("/sub/different.txt"), "bbbb"));
        QVERIFY(writeFile(left + QStringLiteral("/only.bin"), "binary"));
        QVERIFY(writeFile(right + QStringLiteral("/sub/extra.txt"), "extra"));
        FolderSync::Options options;
        options.compareContents = true;
        options.includePatterns = {QStringLiteral("*.txt")};
        const auto result = FolderSync::compare(left, right, options);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QMap<QString, FolderSync::Difference> rows;
        for (const auto &row : result.rows) {
            rows.insert(row.relativePath, row.difference);
        }
        QCOMPARE(rows.value(QStringLiteral("same.txt")), FolderSync::Difference::ContentEqual);
        QCOMPARE(rows.value(QStringLiteral("sub/different.txt")), FolderSync::Difference::ContentDifferent);
        QCOMPARE(rows.value(QStringLiteral("sub/extra.txt")), FolderSync::Difference::RightOnly);
        QVERIFY(!rows.contains(QStringLiteral("only.bin")));
        options.excludePatterns = {QStringLiteral("sub")};
        const auto excluded = FolderSync::compare(left, right, options);
        QVERIFY2(excluded.error.isEmpty(), qPrintable(excluded.error));
        QCOMPARE(excluded.rows.size(), 1);
        QCOMPARE(excluded.rows.first().relativePath, QStringLiteral("same.txt"));
        options.excludePatterns.clear();
        options.recursive = false;
        const auto shallow = FolderSync::compare(left, right, options);
        QVERIFY2(shallow.error.isEmpty(), qPrintable(shallow.error));
        for (const auto &row : shallow.rows) {
            QVERIFY(!row.relativePath.contains(QLatin1Char('/')));
        }
    }

    void skipsOrComparesLinks()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/link")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/link/child")), "data"));
        const QString link = root.filePath(QStringLiteral("left/link"));
        QCOMPARE(::symlink("target", QFile::encodeName(link).constData()), 0);
        FolderSync::Options options;
        auto result = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), options);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.rows.isEmpty());
        options.compareSymlinks = true;
        result = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), options);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows.first().difference, FolderSync::Difference::Type);
    }

    void rejectsOverlappingFoldersAndCancellation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("nested")));
        QVERIFY(!FolderSync::compare(root.path(), root.filePath(QStringLiteral("nested")), {}).error.isEmpty());
        QVERIFY(!FolderSync::compare(QStringLiteral("/"), root.path(), {}).error.isEmpty());
        QTemporaryDir other;
        QVERIFY(other.isValid());
        const auto canceled = FolderSync::compare(root.path(), other.path(), {}, [] {
            return true;
        });
        QVERIFY(!canceled.error.isEmpty());
        QVERIFY(canceled.rows.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FolderSyncTest)
#include "foldersynctest.moc"
