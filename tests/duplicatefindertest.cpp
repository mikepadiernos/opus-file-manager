// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicatefinder.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace
{
bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class DuplicateFinderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void groupsBySizeAndHashWithoutModifyingFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath(QStringLiteral("a"));
        const QString b = dir.filePath(QStringLiteral("b"));
        const QString c = dir.filePath(QStringLiteral("c"));
        const QString d = dir.filePath(QStringLiteral("d"));
        QVERIFY(write(a, "data"));
        QVERIFY(write(b, "data"));
        QVERIFY(write(c, "else"));
        QVERIFY(write(d, "different"));
        int lastProgress = -1;
        const auto result = DuplicateFinder::find({a, b, c, d, a}, {}, [&lastProgress](int current, int, const QString &) {
            lastProgress = current;
        });
        QVERIFY(result.errors.isEmpty());
        QVERIFY(!result.canceled);
        QCOMPARE(result.groups.size(), 1);
        QCOMPARE(result.groups.first().paths, (QStringList{a, b}));
        QCOMPARE(result.groups.first().size, 4);
        QCOMPARE(result.groups.first().digest.size(), 64);
        QCOMPARE(lastProgress, 3);
        QVERIFY(QFile::exists(a) && QFile::exists(b) && QFile::exists(c) && QFile::exists(d));
    }

    void reportsInvalidFilesAndCancellation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath(QStringLiteral("a"));
        const QString b = dir.filePath(QStringLiteral("b"));
        const QString link = dir.filePath(QStringLiteral("link"));
        QVERIFY(write(a, "same"));
        QVERIFY(write(b, "same"));
        QVERIFY(QFile::link(a, link));
        const auto invalid = DuplicateFinder::find({a, b, link, dir.filePath(QStringLiteral("missing"))});
        QCOMPARE(invalid.errors.size(), 2);
        QCOMPARE(invalid.groups.size(), 1);
        int checks = 0;
        const auto canceled = DuplicateFinder::find({a, b}, [&checks] {
            return ++checks >= 4;
        });
        QVERIFY(canceled.canceled);
        QVERIFY(canceled.groups.isEmpty());
    }

    void optionallyRequiresExactFileNamesAndRevalidatesSeparateGroups()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &folder : {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")}) {
            QVERIFY(QDir(dir.path()).mkdir(folder));
        }
        const QString a = dir.filePath(QStringLiteral("one/a"));
        const QString b = dir.filePath(QStringLiteral("two/a"));
        const QString c = dir.filePath(QStringLiteral("one/b"));
        const QString d = dir.filePath(QStringLiteral("two/b"));
        const QString e = dir.filePath(QStringLiteral("three/a"));
        for (const QString &path : {a, b, c, d}) {
            QVERIFY(write(path, "same"));
        }
        QVERIFY(write(e, "else"));
        const QStringList files{a, b, c, d, e};
        const auto all = DuplicateFinder::find(files);
        QCOMPARE(all.groups.size(), 1);
        QCOMPARE(all.groups.first().paths.size(), 4);
        const auto named = DuplicateFinder::find(files, {}, {}, true);
        QVERIFY(named.errors.isEmpty());
        QCOMPARE(named.groups.size(), 2);
        QCOMPARE(named.groups.at(0).paths, (QStringList{a, b}));
        QCOMPARE(named.groups.at(1).paths, (QStringList{c, d}));
        QVERIFY(DuplicateFinder::revalidate(named.groups, {}, true).isEmpty());
        QVERIFY(write(b, "else"));
        QVERIFY(!DuplicateFinder::revalidate(named.groups, {}, true).isEmpty());
    }

    void automaticSelectionAlwaysKeepsAFilePerGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("inside")));
        const QString a = dir.filePath(QStringLiteral("inside/a"));
        const QString b = dir.filePath(QStringLiteral("inside/b"));
        const QString c = dir.filePath(QStringLiteral("outside"));
        QVERIFY(write(a, "data"));
        QVERIFY(write(b, "data"));
        QVERIFY(write(c, "data"));
        QFile first(a);
        QVERIFY(first.open(QIODevice::ReadOnly));
        QVERIFY(first.setFileTime(QDateTime::fromSecsSinceEpoch(1000), QFileDevice::FileModificationTime));
        first.close();
        QFile second(b);
        QVERIFY(second.open(QIODevice::ReadOnly));
        QVERIFY(second.setFileTime(QDateTime::fromSecsSinceEpoch(2000), QFileDevice::FileModificationTime));
        second.close();
        const QList<DuplicateFinder::Group> groups{{4, {}, {a, b}}, {4, {}, {b, c}}};
        const auto newest = DuplicateFinder::select({groups.first()}, DuplicateFinder::SelectionPolicy::KeepNewest);
        QVERIFY(newest.errors.isEmpty());
        QCOMPARE(newest.paths, QStringList{a});
        const auto oldest = DuplicateFinder::select({groups.first()}, DuplicateFinder::SelectionPolicy::KeepOldest);
        QCOMPARE(oldest.paths, QStringList{b});
        QFile tied(c);
        QVERIFY(tied.open(QIODevice::ReadOnly));
        QVERIFY(tied.setFileTime(QDateTime::fromSecsSinceEpoch(2000), QFileDevice::FileModificationTime));
        tied.close();
        const auto ties = DuplicateFinder::select({groups.last()}, DuplicateFinder::SelectionPolicy::KeepNewest);
        QCOMPARE(ties.paths, QStringList{c});
        const auto inFolder = DuplicateFinder::select(groups, DuplicateFinder::SelectionPolicy::InFolder, dir.filePath(QStringLiteral("inside")));
        QVERIFY(inFolder.errors.isEmpty());
        QCOMPARE(inFolder.paths, (QStringList{b, b}));
        QVERIFY(!DuplicateFinder::select(groups, DuplicateFinder::SelectionPolicy::InFolder, dir.filePath(QStringLiteral("missing"))).errors.isEmpty());
        QVERIFY(QFile::remove(a));
        const auto missing = DuplicateFinder::select({groups.first()}, DuplicateFinder::SelectionPolicy::KeepNewest);
        QVERIFY(!missing.errors.isEmpty());
        QVERIFY(missing.paths.isEmpty());
    }

    void revalidatesEveryMemberBeforeRemoval()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath(QStringLiteral("a"));
        const QString b = dir.filePath(QStringLiteral("b"));
        QVERIFY(write(a, "data"));
        QVERIFY(write(b, "data"));
        const auto initial = DuplicateFinder::find({a, b});
        QCOMPARE(initial.groups.size(), 1);
        QVERIFY(DuplicateFinder::revalidate(initial.groups).isEmpty());
        QVERIFY(write(b, "else"));
        QVERIFY(!DuplicateFinder::revalidate(initial.groups).isEmpty());
        QVERIFY(QFile::remove(b));
        QVERIFY(!DuplicateFinder::revalidate(initial.groups).isEmpty());
        QVERIFY(!DuplicateFinder::revalidate(initial.groups, [] {
                     return true;
                 }).isEmpty());
    }
};

QTEST_GUILESS_MAIN(DuplicateFinderTest)
#include "duplicatefindertest.moc"
