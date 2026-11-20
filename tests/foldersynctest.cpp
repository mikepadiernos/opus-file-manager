// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersync.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
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

    void rejectsDirectoryReplacedByLinkDuringScan()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("outside")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("outside/secret")), "secret"));
        int checks = 0;
        bool replaced = false;
        const auto result = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {}, [&] {
            if (++checks == 3) {
                replaced = QDir().rename(root.filePath(QStringLiteral("left/sub")), root.filePath(QStringLiteral("moved")))
                    && ::symlink(QFile::encodeName(root.filePath(QStringLiteral("outside"))).constData(),
                                 QFile::encodeName(root.filePath(QStringLiteral("left/sub"))).constData())
                        == 0;
            }
            return false;
        });
        QVERIFY(replaced);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.rows.isEmpty());
    }

    void rejectsFileReplacedByFifoDuringContentComparison()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        const QString left = root.filePath(QStringLiteral("left/item"));
        QVERIFY(writeFile(left, "same"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/item")), "same"));
        FolderSync::Options options;
        options.compareContents = true;
        int checks = 0;
        bool replaced = false;
        const auto result = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), options, [&] {
            if (++checks == 5) {
                replaced = QFile::rename(left, root.filePath(QStringLiteral("original"))) && ::mkfifo(QFile::encodeName(left).constData(), 0600) == 0;
            }
            return false;
        });
        QVERIFY(replaced);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.rows.isEmpty());
    }

    void appliesOnlyConfirmedOperations()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/a.txt")), "new"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/a.txt")), "old"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/sub/nested.txt")), "nested"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/delete.txt")), "remove"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QList<FolderSync::Action> actions(comparison.rows.size(), FolderSync::Action::Skip);
        for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
            const QString path = comparison.rows.at(i).relativePath;
            if (path == QLatin1String("a.txt") || path == QLatin1String("sub") || path == QLatin1String("sub/nested.txt")) {
                actions[i] = FolderSync::Action::CopyToRight;
            } else if (path == QLatin1String("delete.txt")) {
                actions[i] = FolderSync::Action::DeleteRight;
            }
        }
        QVERIFY(!FolderSync::apply(comparison, actions, false, false).error.isEmpty());
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("right/delete.txt"))));
        const auto result = FolderSync::apply(comparison, actions, true, true);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.completed.size(), 4);
        QFile updated(root.filePath(QStringLiteral("right/a.txt")));
        QVERIFY(updated.open(QIODevice::ReadOnly));
        QCOMPARE(updated.readAll(), QByteArray("new"));
        QFile nested(root.filePath(QStringLiteral("right/sub/nested.txt")));
        QVERIFY(nested.open(QIODevice::ReadOnly));
        QCOMPARE(nested.readAll(), QByteArray("nested"));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/delete.txt"))));
        QCOMPARE(QDir(root.filePath(QStringLiteral("right"))).entryList({QStringLiteral(".opus-sync-*")}, QDir::Hidden | QDir::Dirs).size(), 0);
    }

    void refusesStaleAndUnsafePlans()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        const QString left = root.filePath(QStringLiteral("left/item"));
        const QString right = root.filePath(QStringLiteral("right/item"));
        QVERIFY(writeFile(left, "source"));
        auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QCOMPARE(comparison.rows.size(), 1);
        const QList<FolderSync::Action> actions{FolderSync::Action::CopyToRight};
        QVERIFY(writeFile(right, "appeared"));
        const auto stale = FolderSync::apply(comparison, actions, true, true);
        QVERIFY(!stale.error.isEmpty());
        QFile destination(right);
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("appeared"));
        destination.close();
        comparison.rows[0].relativePath = QStringLiteral("../escape");
        QVERIFY(!FolderSync::apply(comparison, actions, true, true).error.isEmpty());
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("escape"))));
    }

    void cancelsBeforeChanges()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/item")), "source"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        const auto result = FolderSync::apply(comparison, {FolderSync::Action::CopyToRight}, false, false, [] {
            return true;
        });
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.completed.isEmpty());
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/item"))));
    }

    void cancelsDuringCopyWithoutReplacingDestination()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/item")), QByteArray(1024 * 1024, 'x')));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/item")), "original"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        int checks = 0;
        const auto result = FolderSync::apply(comparison, {FolderSync::Action::CopyToRight}, true, false, [&] {
            return ++checks >= 3;
        });
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.completed.isEmpty());
        QFile destination(root.filePath(QStringLiteral("right/item")));
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("original"));
        QCOMPARE(QDir(root.filePath(QStringLiteral("right"))).entryList({QStringLiteral(".opus-sync-*")}, QDir::Hidden | QDir::Dirs).size(), 0);
    }

    void rejectsReplacedParentAndSharedInodes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("outside")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/sub/item")), "source"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QList<FolderSync::Action> actions(comparison.rows.size(), FolderSync::Action::Skip);
        for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
            if (comparison.rows.at(i).relativePath == QLatin1String("sub/item")) {
                actions[i] = FolderSync::Action::CopyToRight;
            }
        }
        QVERIFY(QDir().rename(root.filePath(QStringLiteral("right/sub")), root.filePath(QStringLiteral("moved"))));
        QCOMPARE(::symlink(QFile::encodeName(root.filePath(QStringLiteral("outside"))).constData(),
                           QFile::encodeName(root.filePath(QStringLiteral("right/sub"))).constData()),
                 0);
        const auto result = FolderSync::apply(comparison, actions, true, true);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("outside/item"))));
        QVERIFY(QFile::remove(root.filePath(QStringLiteral("right/sub"))));
        QVERIFY(QDir().rename(root.filePath(QStringLiteral("moved")), root.filePath(QStringLiteral("right/sub"))));
        const auto first = QFile::encodeName(root.filePath(QStringLiteral("left/sub/item")));
        const auto second = QFile::encodeName(root.filePath(QStringLiteral("right/sub/item")));
        QCOMPARE(::link(first.constData(), second.constData()), 0);
        const auto shared = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(shared.error.isEmpty(), qPrintable(shared.error));
        QList<FolderSync::Action> sharedActions(shared.rows.size(), FolderSync::Action::Skip);
        for (qsizetype i = 0; i < shared.rows.size(); ++i) {
            if (shared.rows.at(i).relativePath == QLatin1String("sub/item")) {
                sharedActions[i] = FolderSync::Action::CopyToRight;
            }
        }
        QVERIFY(!FolderSync::planError(shared, sharedActions, true, false).isEmpty());
    }

    void preservesReplacementOfTemporaryFile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/item")), "source"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/item")), "original"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        int checks = 0;
        QString temporaryPath;
        bool replaced = false;
        bool privateDirectory = false;
        const auto result = FolderSync::apply(comparison, {FolderSync::Action::CopyToRight}, true, false, [&] {
            if (++checks == 2) {
                const QDir destination(root.filePath(QStringLiteral("right")));
                const QStringList names = destination.entryList({QStringLiteral(".opus-sync-*")}, QDir::Hidden | QDir::Dirs);
                if (names.size() == 1) {
                    const QString stage = destination.filePath(names.first());
                    struct stat data;
                    privateDirectory = ::stat(QFile::encodeName(stage).constData(), &data) == 0 && data.st_uid == ::geteuid() && (data.st_mode & 0077) == 0;
                    temporaryPath = QDir(stage).filePath(QStringLiteral("copy"));
                    replaced = QFile::remove(temporaryPath) && writeFile(temporaryPath, "unrelated");
                }
            }
            return false;
        });
        QVERIFY(privateDirectory);
        QVERIFY(replaced);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.completed.isEmpty());
        QFile destination(root.filePath(QStringLiteral("right/item")));
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("original"));
        QFile replacement(temporaryPath);
        QVERIFY(replacement.open(QIODevice::ReadOnly));
        QCOMPARE(replacement.readAll(), QByteArray("unrelated"));
    }

    void refusesParentMovedDuringCopy()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/sub/item")), "source"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/sub/item")), "original"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QList<FolderSync::Action> actions(comparison.rows.size(), FolderSync::Action::Skip);
        for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
            if (comparison.rows.at(i).relativePath == QLatin1String("sub/item")) {
                actions[i] = FolderSync::Action::CopyToRight;
            }
        }
        int checks = 0;
        bool moved = false;
        const auto result = FolderSync::apply(comparison, actions, true, false, [&] {
            if (++checks == 2) {
                moved = QDir().rename(root.filePath(QStringLiteral("right/sub")), root.filePath(QStringLiteral("moved")));
            }
            return false;
        });
        QVERIFY(moved);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.completed.isEmpty());
        QFile original(root.filePath(QStringLiteral("moved/item")));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("original"));
        QCOMPARE(QDir(root.filePath(QStringLiteral("moved"))).entryList({QStringLiteral(".opus-sync-*")}, QDir::Hidden | QDir::Dirs).size(), 0);
    }

    void reportsCompletedOperationsBeforeFailure()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/a")), "first"));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/b")), "second"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QVERIFY(writeFile(root.filePath(QStringLiteral("right/b")), "unexpected"));
        const auto result = FolderSync::apply(comparison, {FolderSync::Action::CopyToRight, FolderSync::Action::CopyToRight}, false, false);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(result.completed, QStringList({QStringLiteral("a")}));
        QFile first(root.filePath(QStringLiteral("right/a")));
        QVERIFY(first.open(QIODevice::ReadOnly));
        QCOMPARE(first.readAll(), QByteArray("first"));
        QFile second(root.filePath(QStringLiteral("right/b")));
        QVERIFY(second.open(QIODevice::ReadOnly));
        QCOMPARE(second.readAll(), QByteArray("unexpected"));
    }

    void refusesChangedSourcesAndDeletionTargets()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        const QString left = root.filePath(QStringLiteral("left/item"));
        const QString right = root.filePath(QStringLiteral("right/item"));
        QVERIFY(writeFile(left, "source"));
        QVERIFY(writeFile(right, "original"));
        auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QVERIFY(writeFile(left, "changed source"));
        auto result = FolderSync::apply(comparison, {FolderSync::Action::CopyToRight}, true, false);
        QVERIFY(!result.error.isEmpty());
        QFile destination(right);
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("original"));
        destination.close();
        comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QVERIFY(writeFile(right, "changed target"));
        result = FolderSync::apply(comparison, {FolderSync::Action::DeleteRight}, false, true);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("changed target"));
    }

    void rejectsMissingPlannedParentsAndInvalidActions()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QVERIFY(writeFile(root.filePath(QStringLiteral("left/sub/item")), "source"));
        const auto comparison = FolderSync::compare(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), {});
        QVERIFY2(comparison.error.isEmpty(), qPrintable(comparison.error));
        QList<FolderSync::Action> actions(comparison.rows.size(), FolderSync::Action::Skip);
        for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
            if (comparison.rows.at(i).relativePath == QLatin1String("sub/item")) {
                actions[i] = FolderSync::Action::CopyToRight;
            }
        }
        QVERIFY(!FolderSync::planError(comparison, actions, false, false).isEmpty());
        QVERIFY(!FolderSync::apply(comparison, actions, false, false).error.isEmpty());
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/sub"))));
        actions[0] = static_cast<FolderSync::Action>(999);
        QVERIFY(!FolderSync::planError(comparison, actions, true, true).isEmpty());
    }
};

QTEST_GUILESS_MAIN(FolderSyncTest)
#include "foldersynctest.moc"
