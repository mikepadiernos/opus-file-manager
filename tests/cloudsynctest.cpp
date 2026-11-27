// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsync.h"

#include <KConfig>
#include <KConfigGroup>
#include <KIO/Global>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace CloudSync;

namespace
{
State file(qint64 size, qint64 modified)
{
    return State{false, size, modified};
}

State folder()
{
    return State{true, 0, 0};
}

Snapshot snapshotOf(const Listing &local, const Listing &remote)
{
    Snapshot snapshot;
    snapshot.local = local;
    snapshot.remote = remote;
    return snapshot;
}

Change change(Operation operation, const QString &path, qint64 size = 0, bool overwrite = false, const QString &source = {}, bool isFolder = false)
{
    Change result;
    result.operation = operation;
    result.path = path;
    result.source = source;
    result.folder = isFolder;
    result.size = size;
    result.overwrite = overwrite;
    return result;
}

Pair webdavPair(const QString &local, const QString &remote)
{
    Pair pair;
    pair.id = newId();
    pair.localPath = local;
    pair.remote = QUrl(remote);
    return pair;
}

bool touch(const QString &path, const QByteArray &data = {})
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class CloudSyncTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void firstSyncCopiesBothWays()
    {
        const Listing local{{QStringLiteral("a.txt"), file(3, 100)}, {QStringLiteral("same.txt"), file(5, 100)}, {QStringLiteral("both.txt"), file(1, 100)}};
        const Listing remote{{QStringLiteral("b.txt"), file(4, 200)}, {QStringLiteral("same.txt"), file(5, 101)}, {QStringLiteral("both.txt"), file(2, 100)}};
        const Plan result = plan(local, remote, {}, Mode::Sync, false);
        QVERIFY(result.firstSync);
        QCOMPARE(result.changes,
                 (QList<Change>{change(Operation::Upload, QStringLiteral("a.txt"), 3), change(Operation::Download, QStringLiteral("b.txt"), 4)}));
        QCOMPARE(result.inSync, QStringList{QStringLiteral("same.txt")});
        QCOMPARE(result.conflicts.size(), 1);
        QCOMPARE(result.conflicts.constFirst().path, QStringLiteral("both.txt"));
        QVERIFY(result.conflicts.constFirst().isResolvable());
        QVERIFY(needsReview(result, {}));

        const Plan empty = plan(local, local, {}, Mode::Sync, false);
        QVERIFY(empty.firstSync);
        QVERIFY(empty.changes.isEmpty());
        QVERIFY(!needsReview(empty, {}));
    }

    void editsAndDeletionsOnEachSide()
    {
        const Listing base{{QStringLiteral("a"), file(1, 10)},
                           {QStringLiteral("b"), file(1, 10)},
                           {QStringLiteral("c"), file(1, 10)},
                           {QStringLiteral("d"), file(1, 10)}};
        Listing local = base;
        local[QStringLiteral("a")] = file(2, 20);
        local.remove(QStringLiteral("b"));
        Listing remote = base;
        remote[QStringLiteral("c")] = file(3, 30);
        remote.remove(QStringLiteral("d"));

        const Plan result = plan(local, remote, snapshotOf(base, base), Mode::Sync, false);
        QVERIFY(!result.firstSync);
        QCOMPARE(result.changes,
                 (QList<Change>{change(Operation::Upload, QStringLiteral("a"), 2, true),
                                change(Operation::Download, QStringLiteral("c"), 3, true),
                                change(Operation::DeleteRemote, QStringLiteral("b")),
                                change(Operation::DeleteLocal, QStringLiteral("d"))}));
        QVERIFY(result.conflicts.isEmpty());
        QCOMPARE(result.deletions(), 2);
        QCOMPARE(result.uploadBytes(), 2);
    }

    void changesOnBothSides()
    {
        const Listing base{{QStringLiteral("edited"), file(1, 10)}, {QStringLiteral("gone"), file(1, 10)}, {QStringLiteral("both-gone"), file(1, 10)}};
        const Listing local{{QStringLiteral("edited"), file(2, 20)}};
        const Listing remote{{QStringLiteral("edited"), file(3, 30)}, {QStringLiteral("gone"), file(4, 40)}};
        const Plan result = plan(local, remote, snapshotOf(base, base), Mode::Sync, false);
        // A deletion gives way to an edit.
        QCOMPARE(result.changes, QList<Change>{change(Operation::Download, QStringLiteral("gone"), 4)});
        QCOMPARE(result.conflicts.size(), 1);
        QCOMPARE(result.conflicts.constFirst().path, QStringLiteral("edited"));
        QVERIFY(result.inSync.contains(QStringLiteral("both-gone")));

        // The same edit on both sides is in sync.
        const Listing same{{QStringLiteral("edited"), file(2, 20)}};
        const Plan identical = plan(same, same, snapshotOf(base, base), Mode::Sync, false);
        QVERIFY(identical.conflicts.isEmpty());
        QVERIFY(identical.inSync.contains(QStringLiteral("edited")));
    }

    void fileAndFolderConflict()
    {
        const Listing local{{QStringLiteral("x"), folder()}};
        const Listing remote{{QStringLiteral("x"), file(1, 10)}};
        const QHash<QString, Resolution> resolutions{{QStringLiteral("x"), Resolution::KeepLocal}};
        const Plan result = plan(local, remote, {}, Mode::Sync, false, resolutions);
        QCOMPARE(result.conflicts.size(), 1);
        QVERIFY(!result.conflicts.constFirst().isResolvable());
        QVERIFY(result.changes.isEmpty());
    }

    void renames()
    {
        const Listing base{{QStringLiteral("old.txt"), file(5, 50)}};
        const Listing renamed{{QStringLiteral("new.txt"), file(5, 50)}};
        const Plan local = plan(renamed, base, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(local.changes, QList<Change>{change(Operation::RenameRemote, QStringLiteral("new.txt"), 5, false, QStringLiteral("old.txt"))});

        const Plan remote = plan(base, renamed, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(remote.changes, QList<Change>{change(Operation::RenameLocal, QStringLiteral("new.txt"), 5, false, QStringLiteral("old.txt"))});

        // Two deleted files with the same size and time are ambiguous.
        const Listing twins{{QStringLiteral("one"), file(5, 50)}, {QStringLiteral("two"), file(5, 50)}};
        const Listing moved{{QStringLiteral("three"), file(5, 50)}};
        const Plan ambiguous = plan(moved, twins, snapshotOf(twins, twins), Mode::Sync, false);
        QCOMPARE(ambiguous.changes,
                 (QList<Change>{change(Operation::Upload, QStringLiteral("three"), 5),
                                change(Operation::DeleteRemote, QStringLiteral("one")),
                                change(Operation::DeleteRemote, QStringLiteral("two"))}));
    }

    void cloudBackup()
    {
        const Listing base{{QStringLiteral("a"), file(1, 10)}, {QStringLiteral("b"), file(1, 10)}, {QStringLiteral("kept"), file(1, 10)}};
        Listing local = base;
        local[QStringLiteral("a")] = file(2, 20);
        local.remove(QStringLiteral("b"));
        Listing remote = base;
        remote.insert(QStringLiteral("c"), file(3, 30));
        // The backup copy was deleted, so it is restored.
        remote.remove(QStringLiteral("kept"));

        const Plan result = plan(local, remote, snapshotOf(base, base), Mode::CloudBackup, false);
        QCOMPARE(result.changes,
                 (QList<Change>{change(Operation::Upload, QStringLiteral("a"), 2, true), change(Operation::Upload, QStringLiteral("kept"), 1)}));
        QCOMPARE(result.skipped.size(), 2);
        QVERIFY(result.skipped.at(0).contains(QStringLiteral("b")));
        QVERIFY(result.skipped.at(1).contains(QStringLiteral("c")));

        const Plan mirrored = plan(local, remote, snapshotOf(base, base), Mode::CloudBackup, true);
        QVERIFY(mirrored.changes.contains(change(Operation::DeleteRemote, QStringLiteral("b"))));
        QCOMPARE(mirrored.skipped.size(), 1);
    }

    void localBackup()
    {
        const Listing base{{QStringLiteral("a"), file(1, 10)}};
        const Listing local{{QStringLiteral("a"), file(1, 10)}, {QStringLiteral("d"), file(4, 40)}};
        const Listing remote{{QStringLiteral("a"), file(1, 10)}, {QStringLiteral("c"), file(3, 30)}};
        const Plan result = plan(local, remote, snapshotOf(base, base), Mode::LocalBackup, false);
        QCOMPARE(result.changes, QList<Change>{change(Operation::Download, QStringLiteral("c"), 3)});
        QCOMPARE(result.skipped.size(), 1);
        QVERIFY(result.skipped.constFirst().contains(QStringLiteral("d")));

        // A source rename keeps the backup of the old name unless deletions are mirrored.
        const Listing old{{QStringLiteral("old"), file(5, 50)}};
        const Listing renamed{{QStringLiteral("new"), file(5, 50)}};
        const Plan copy = plan(old, renamed, snapshotOf(old, old), Mode::LocalBackup, false);
        QCOMPARE(copy.changes, QList<Change>{change(Operation::Download, QStringLiteral("new"), 5)});
        const Plan rename = plan(old, renamed, snapshotOf(old, old), Mode::LocalBackup, true);
        QCOMPARE(rename.changes, QList<Change>{change(Operation::RenameLocal, QStringLiteral("new"), 5, false, QStringLiteral("old"))});
    }

    void resolutions()
    {
        const Listing base{{QStringLiteral("doc.txt"), file(1, 10)}};
        const Listing local{{QStringLiteral("doc.txt"), file(2, 20)}};
        const Listing remote{{QStringLiteral("doc.txt"), file(3, 30)}, {QStringLiteral("doc (conflict X).txt"), file(9, 90)}};
        const Listing remoteBase{{QStringLiteral("doc.txt"), file(1, 10)}, {QStringLiteral("doc (conflict X).txt"), file(9, 90)}};
        const Snapshot snapshot = snapshotOf({{QStringLiteral("doc.txt"), file(1, 10)}, {QStringLiteral("doc (conflict X).txt"), file(9, 90)}}, remoteBase);
        Listing localWithCopy = local;
        localWithCopy.insert(QStringLiteral("doc (conflict X).txt"), file(9, 90));

        const auto resolve = [&](Resolution resolution) {
            return plan(localWithCopy, remote, snapshot, Mode::Sync, false, {{QStringLiteral("doc.txt"), resolution}}, QStringLiteral("conflict X"));
        };
        QCOMPARE(resolve(Resolution::Skip).conflicts.size(), 1);
        QVERIFY(resolve(Resolution::Skip).changes.isEmpty());
        QCOMPARE(resolve(Resolution::KeepLocal).changes, QList<Change>{change(Operation::Upload, QStringLiteral("doc.txt"), 2, true)});
        QCOMPARE(resolve(Resolution::KeepRemote).changes, QList<Change>{change(Operation::Download, QStringLiteral("doc.txt"), 3, true)});
        const Plan both = resolve(Resolution::KeepBoth);
        QVERIFY(both.conflicts.isEmpty());
        QCOMPARE(both.changes,
                 (QList<Change>{change(Operation::Download, QStringLiteral("doc (conflict X 2).txt"), 3, false, QStringLiteral("doc.txt")),
                                change(Operation::Upload, QStringLiteral("doc.txt"), 2, true)}));

        // Resolutions apply in backup modes too.
        const Plan backup = plan(local,
                                 {{QStringLiteral("doc.txt"), file(3, 30)}},
                                 snapshotOf(base, base),
                                 Mode::CloudBackup,
                                 false,
                                 {{QStringLiteral("doc.txt"), Resolution::KeepRemote}});
        QCOMPARE(backup.changes, QList<Change>{change(Operation::Download, QStringLiteral("doc.txt"), 3, true)});
    }

    void defaultResolutions()
    {
        const Listing base{{QStringLiteral("doc.txt"), file(1, 10)}};
        const Listing local{{QStringLiteral("doc.txt"), file(2, 20)}, {QStringLiteral("x"), folder()}};
        const Listing remote{{QStringLiteral("doc.txt"), file(3, 30)}, {QStringLiteral("x"), file(1, 10)}};
        const Plan conflicting = plan(local, remote, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(conflicting.conflicts.size(), 2);

        // Only conflicts that can keep one or both versions are resolved; a file and a folder wait for review.
        QVERIFY(CloudSync::defaultResolutions(conflicting.conflicts, Resolution::Skip).isEmpty());
        const QHash<QString, Resolution> keepRemote = CloudSync::defaultResolutions(conflicting.conflicts, Resolution::KeepRemote);
        QCOMPARE(keepRemote, (QHash<QString, Resolution>{{QStringLiteral("doc.txt"), Resolution::KeepRemote}}));
        const Plan resolved = plan(local, remote, snapshotOf(base, base), Mode::Sync, false, keepRemote);
        QCOMPARE(resolved.conflicts.size(), 1);
        QCOMPARE(resolved.conflicts.constFirst().path, QStringLiteral("x"));
        QCOMPARE(resolved.changes, QList<Change>{change(Operation::Download, QStringLiteral("doc.txt"), 3, true)});

        QCOMPARE(resolutionName(Resolution::Skip), QStringLiteral("Ask When Reviewing"));
        QCOMPARE(resolutionName(Resolution::KeepBoth), QStringLiteral("Keep Both"));
    }

    void folderDeletions()
    {
        const Listing base{{QStringLiteral("d"), folder()},
                           {QStringLiteral("d/e"), folder()},
                           {QStringLiteral("d/e/x"), file(1, 10)},
                           {QStringLiteral("d/y"), file(1, 10)}};
        const Plan result = plan({}, base, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(result.changes,
                 (QList<Change>{change(Operation::DeleteRemote, QStringLiteral("d/e/x")),
                                change(Operation::DeleteRemote, QStringLiteral("d/y")),
                                change(Operation::DeleteRemote, QStringLiteral("d/e"), 0, false, {}, true),
                                change(Operation::DeleteRemote, QStringLiteral("d"), 0, false, {}, true)}));
        QCOMPARE(result.deletions(), 2);

        // A file added remotely keeps its folder.
        Listing remote = base;
        remote.insert(QStringLiteral("d/z"), file(2, 20));
        const Plan kept = plan({}, remote, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(kept.changes,
                 (QList<Change>{change(Operation::Download, QStringLiteral("d/z"), 2),
                                change(Operation::DeleteRemote, QStringLiteral("d/e/x")),
                                change(Operation::DeleteRemote, QStringLiteral("d/y")),
                                change(Operation::DeleteRemote, QStringLiteral("d/e"), 0, false, {}, true)}));

        // New folders are created parents first.
        const Listing tree{{QStringLiteral("n"), folder()}, {QStringLiteral("n/m"), folder()}};
        const Plan created = plan(tree, {}, snapshotOf(base, base), Mode::Sync, false);
        QCOMPARE(created.changes.at(0), change(Operation::CreateRemoteFolder, QStringLiteral("n"), 0, false, {}, true));
        QCOMPARE(created.changes.at(1), change(Operation::CreateRemoteFolder, QStringLiteral("n/m"), 0, false, {}, true));
    }

    void largeDeletions()
    {
        Listing base;
        for (int i = 0; i < 10; ++i) {
            base.insert(QStringLiteral("f%1").arg(i), file(1, 10));
        }
        const auto deleting = [&base](int count) {
            Listing local = base;
            for (int i = 0; i < count; ++i) {
                local.remove(QStringLiteral("f%1").arg(i));
            }
            return plan(local, base, snapshotOf(base, base), Mode::Sync, false);
        };
        QVERIFY(!isLargeDeletion(deleting(4), snapshotOf(base, base)));
        QVERIFY(!needsReview(deleting(4), snapshotOf(base, base)));
        QVERIFY(isLargeDeletion(deleting(5), snapshotOf(base, base)));
        QVERIFY(needsReview(deleting(5), snapshotOf(base, base)));

        Listing many;
        for (int i = 0; i < 200; ++i) {
            many.insert(QStringLiteral("g%1").arg(i, 3, 10, QLatin1Char('0')), file(1, 10));
        }
        Listing fewer = many;
        for (int i = 0; i < 50; ++i) {
            fewer.remove(QStringLiteral("g%1").arg(i, 3, 10, QLatin1Char('0')));
        }
        QVERIFY(isLargeDeletion(plan(fewer, many, snapshotOf(many, many), Mode::Sync, false), snapshotOf(many, many)));
    }

    void nextSnapshotRecordsDoneChanges()
    {
        const Listing base{{QStringLiteral("a"), file(1, 10)}, {QStringLiteral("b"), file(1, 10)}, {QStringLiteral("c"), file(1, 10)}};
        Listing local = base;
        local[QStringLiteral("a")] = file(2, 20);
        local[QStringLiteral("c")] = file(3, 30);
        local.remove(QStringLiteral("b"));
        local.insert(QStringLiteral("new"), file(4, 40));
        Listing remote = base;
        remote.insert(QStringLiteral("new"), file(4, 41));
        const Snapshot snapshot = snapshotOf(base, base);
        const Plan result = plan(local, remote, snapshot, Mode::Sync, false);
        QVERIFY(result.inSync.contains(QStringLiteral("new")));

        // The upload of "a" and the deletion of "b" succeeded; the upload of "c" failed.
        const QList<Change> done{change(Operation::Upload, QStringLiteral("a"), 2, true), change(Operation::DeleteRemote, QStringLiteral("b"))};
        const Snapshot next = nextSnapshot(snapshot, local, remote, result, done, {}, {{QStringLiteral("a"), file(2, 21)}});
        QCOMPARE(next.local.value(QStringLiteral("a")), file(2, 20));
        QCOMPARE(next.remote.value(QStringLiteral("a")), file(2, 21));
        QVERIFY(!next.local.contains(QStringLiteral("b")));
        QVERIFY(!next.remote.contains(QStringLiteral("b")));
        QCOMPARE(next.local.value(QStringLiteral("c")), file(1, 10));
        QCOMPARE(next.local.value(QStringLiteral("new")), file(4, 40));
        QCOMPARE(next.remote.value(QStringLiteral("new")), file(4, 41));

        // The failed upload is planned again.
        Listing remoteAfter = remote;
        remoteAfter[QStringLiteral("a")] = file(2, 21);
        remoteAfter.remove(QStringLiteral("b"));
        const Plan again = plan(local, remoteAfter, next, Mode::Sync, false);
        QCOMPARE(again.changes, QList<Change>{change(Operation::Upload, QStringLiteral("c"), 3, true)});

        // A conflict copy is not recorded, so it is uploaded next time.
        const Change copy = change(Operation::Download, QStringLiteral("x (conflict).txt"), 1, false, QStringLiteral("x.txt"));
        const Snapshot withCopy = nextSnapshot({}, {}, {}, {}, {copy}, {{copy.path, file(1, 10)}}, {});
        QVERIFY(!withCopy.local.contains(copy.path));
    }

    void conflictNames()
    {
        QCOMPARE(conflictLabel(QDateTime(QDate(2026, 11, 26), QTime(15, 30)), QStringLiteral("lap/top ")), QStringLiteral("conflict 2026-11-26 1530 laptop"));
        QCOMPARE(conflictLabel(QDateTime(QDate(2026, 1, 2), QTime(3, 4)), QString()), QStringLiteral("conflict 2026-01-02 0304"));
        QCOMPARE(conflictPath(QStringLiteral("dir/notes.txt"), QStringLiteral("L")), QStringLiteral("dir/notes (L).txt"));
        QCOMPARE(conflictPath(QStringLiteral(".bashrc"), QStringLiteral("L")), QStringLiteral(".bashrc (L)"));
        QCOMPARE(conflictPath(QStringLiteral("a.tar.gz"), QStringLiteral("L")), QStringLiteral("a.tar (L).gz"));
        QCOMPARE(conflictPath(QStringLiteral("Makefile"), QString()), QStringLiteral("Makefile (conflict)"));
    }

    void filter()
    {
        FolderSync::Options options;
        options.excludePatterns = {QStringLiteral("build"), QStringLiteral("*.o")};
        options.includePatterns = {QStringLiteral("*.txt")};
        const Filter rules(options);
        QVERIFY(!rules.includes(QStringLiteral("build"), true));
        QVERIFY(!rules.includes(QStringLiteral("build/x/y.txt"), false));
        QVERIFY(!rules.includes(QStringLiteral("src/main.o"), false));
        QVERIFY(rules.includes(QStringLiteral("src"), true));
        QVERIFY(rules.includes(QStringLiteral("src/notes.txt"), false));
        QVERIFY(!rules.includes(QStringLiteral("src/main.cpp"), false));

        options = {};
        options.recursive = false;
        const Filter flat(options);
        QVERIFY(flat.includes(QStringLiteral("a"), true));
        QVERIFY(!flat.includes(QStringLiteral("a/b"), false));

        QCOMPARE(exactPattern(QStringLiteral("a*b?[c")), QStringLiteral("a[*]b[?][[]c"));
        options = {};
        options.excludePatterns = {exactPattern(QStringLiteral("we*ird"))};
        const Filter exact(options);
        QVERIFY(!exact.includes(QStringLiteral("we*ird"), true));
        QVERIFY(!exact.includes(QStringLiteral("we*ird/file"), false));
        QVERIFY(exact.includes(QStringLiteral("weXird"), true));
    }

    void managingEngine()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        EngineLocations locations{root + QStringLiteral("/config"), root + QStringLiteral("/state")};
        const QString nextcloud = root + QStringLiteral("/Nextcloud");
        const QString sync = root + QStringLiteral("/Sync");
        QVERIFY(touch(locations.configHome + QStringLiteral("/Nextcloud/nextcloud.cfg"),
                      QStringLiteral("[Accounts]\n0\\Folders\\1\\localPath=%1/\n").arg(nextcloud).toUtf8()));
        QVERIFY(touch(locations.stateHome + QStringLiteral("/syncthing/config.xml"),
                      QStringLiteral("<configuration><folder id=\"a\" path=\"%1\"></folder></configuration>").arg(sync).toUtf8()));

        QCOMPARE(CloudSync::managingEngine(nextcloud + QStringLiteral("/Work"), locations), QStringLiteral("Nextcloud"));
        QCOMPARE(CloudSync::managingEngine(root, locations), QStringLiteral("Nextcloud"));
        QCOMPARE(CloudSync::managingEngine(sync, locations), QStringLiteral("Syncthing"));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/Plain"), {}), QString());

        QVERIFY(touch(root + QStringLiteral("/st/.stfolder/marker")));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/st/deep/er"), {}), QStringLiteral("Syncthing"));
        QVERIFY(touch(root + QStringLiteral("/db/.dropbox")));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/db/x"), {}), QStringLiteral("Dropbox"));
        QVERIFY(touch(root + QStringLiteral("/nc/._sync_abc123.db")));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/nc"), {}), QStringLiteral("Nextcloud"));
        QCOMPARE(CloudSync::managingEngine(QString(), locations), QString());
    }

    void loadAndSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("cloudsyncrc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("CloudSyncPairs"));

        Pair webdav = webdavPair(QStringLiteral("/home/u/Docs/"), QStringLiteral("webdavs://user:secret@cloud.example/dav/Docs/"));
        webdav.mode = Mode::CloudBackup;
        webdav.mirrorDeletions = true;
        webdav.options.excludePatterns = {QStringLiteral("*.tmp")};
        webdav.options.recursive = false;
        webdav.intervalMinutes = 15;
        webdav.conflictChoice = Resolution::KeepBoth;
        webdav.paused = true;
        Pair rclone;
        rclone.id = newId();
        rclone.localPath = QStringLiteral("/home/u/Drive");
        rclone.rcloneRemote = QStringLiteral("drive:Documents");
        rclone.mode = Mode::LocalBackup;
        rclone.useDefaultInterval = true;
        rclone.conflictChoice = Resolution::KeepRemote;

        QString error;
        QVERIFY2(save(group, {webdav, rclone}, &error), qPrintable(error));
        QVERIFY(!config.group(QStringLiteral("CloudSyncPairs")).group(webdav.id).readEntry("Remote").contains(QStringLiteral("secret")));

        const QList<Pair> loaded = load(group);
        QCOMPARE(loaded.size(), 2);
        const Pair &first = loaded.at(0).id == webdav.id ? loaded.at(0) : loaded.at(1);
        const Pair &second = loaded.at(0).id == webdav.id ? loaded.at(1) : loaded.at(0);
        Pair expected = webdav;
        expected.localPath = QStringLiteral("/home/u/Docs");
        expected.remote = QUrl(QStringLiteral("webdavs://user@cloud.example/dav/Docs"));
        QCOMPARE(first, expected);
        QCOMPARE(second, rclone);
        QCOMPARE(group.group(rclone.id).readEntry("ConflictChoice"), QStringLiteral("keep-remote"));

        // Pairs saved before the conflict choice existed, or with an unknown one, ask.
        group.group(rclone.id).writeEntry("ConflictChoice", QStringLiteral("newest"));
        group.group(webdav.id).deleteEntry("ConflictChoice");
        group.group(webdav.id).deleteEntry("DefaultInterval");
        for (const Pair &pair : load(group)) {
            QVERIFY(pair.conflictChoice == Resolution::Skip);
            QCOMPARE(pair.useDefaultInterval, pair.id == rclone.id);
        }
        QVERIFY(save(group, {webdav, rclone}));

        // Invalid entries are skipped.
        group.group(QStringLiteral("not-a-uuid")).writeEntry("LocalPath", QStringLiteral("/x"));
        group.group(newId()).writeEntry("LocalPath", QStringLiteral("relative"));
        QCOMPARE(load(group).size(), 2);

        // Duplicate identifiers are rejected and nothing is changed.
        QVERIFY(!save(group, {webdav, webdav}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(load(group).size(), 2);
        QVERIFY(save(group, {rclone}));
        QCOMPARE(load(group), QList<Pair>{rclone});
    }

    void pairErrors()
    {
        const Pair valid = webdavPair(QStringLiteral("/home/u/Docs"), QStringLiteral("webdavs://cloud.example/dav/Docs"));
        QCOMPARE(pairError(valid, {}), QString());

        Pair pair = valid;
        pair.localPath.clear();
        QVERIFY(!pairError(pair, {}).isEmpty());
        pair.localPath = QStringLiteral("relative/path");
        QVERIFY(!pairError(pair, {}).isEmpty());

        pair = valid;
        pair.remote = QUrl(QStringLiteral("relative/path"));
        QVERIFY(!pairError(pair, {}).isEmpty());
        pair.remote = QUrl::fromLocalFile(QStringLiteral("/home/u/Docs/Backup"));
        QVERIFY(!pairError(pair, {}).isEmpty());
        pair.remote = QUrl::fromLocalFile(QStringLiteral("/mnt/backup/Docs"));
        QCOMPARE(pairError(pair, {}), QString());

        pair = valid;
        pair.intervalMinutes = -1;
        QVERIFY(!pairError(pair, {}).isEmpty());

        pair = valid;
        pair.rcloneRemote = QStringLiteral("no-colon");
        QVERIFY(!pairError(pair, {}).isEmpty());
        pair.rcloneRemote = QStringLiteral("-bad:path");
        QVERIFY(!pairError(pair, {}).isEmpty());
        pair.rcloneRemote = QStringLiteral("drive:Docs");
        QCOMPARE(pairError(pair, {}), QString());
        pair.options.excludePatterns = {QStringLiteral("a?b")};
        QVERIFY(!pairError(pair, {}).isEmpty());

        Pair other = webdavPair(QStringLiteral("/home/u/Docs/"), QStringLiteral("webdavs://cloud.example/dav/Other"));
        QVERIFY(!pairError(valid, {other}).isEmpty());
        other = webdavPair(QStringLiteral("/home/u/Other"), QStringLiteral("webdavs://cloud.example/dav/Docs/"));
        QVERIFY(!pairError(valid, {other}).isEmpty());
        other.id = valid.id;
        QCOMPARE(pairError(valid, {other}), QString());
    }

    void nesting()
    {
        const Pair outer = webdavPair(QStringLiteral("/home/u/Docs"), QStringLiteral("webdavs://me@cloud.example/dav/Docs"));
        const Pair inner = webdavPair(QStringLiteral("/home/u/Docs/Work"), QStringLiteral("webdavs://me@cloud.example/dav/Work"));
        auto result = CloudSync::nesting(inner, {outer});
        QVERIFY(result);
        QCOMPARE(result->otherId, outer.id);
        QVERIFY(result->inside);
        QVERIFY(!result->remote);
        QCOMPARE(result->relativePath, QStringLiteral("Work"));

        result = CloudSync::nesting(outer, {inner});
        QVERIFY(result);
        QVERIFY(!result->inside);
        QCOMPARE(result->relativePath, QStringLiteral("Work"));

        const Pair remoteInner = webdavPair(QStringLiteral("/home/u/Sub"), QStringLiteral("webdavs://me@cloud.example/dav/Docs/Sub/Deeper"));
        result = CloudSync::nesting(remoteInner, {outer});
        QVERIFY(result);
        QVERIFY(result->inside);
        QVERIFY(result->remote);
        QCOMPARE(result->relativePath, QStringLiteral("Sub/Deeper"));

        const Pair otherAccount = webdavPair(QStringLiteral("/home/u/Sub"), QStringLiteral("webdavs://you@cloud.example/dav/Docs/Sub"));
        QVERIFY(!CloudSync::nesting(otherAccount, {outer}));
        const Pair sibling = webdavPair(QStringLiteral("/home/u/Docs2"), QStringLiteral("webdavs://me@cloud.example/dav/Docs2"));
        QVERIFY(!CloudSync::nesting(sibling, {outer}));
    }

    void accounts()
    {
        const Pair pair = webdavPair(QStringLiteral("/home/u/Docs"), QStringLiteral("webdavs://me:pw@Cloud.Example:8443/dav/Docs"));
        QCOMPARE(accountKey(pair), QStringLiteral("webdavs://me@cloud.example:8443"));
        QCOMPARE(accountDisplayName(pair), QStringLiteral("me@cloud.example"));
        QVERIFY(!remoteDisplayName(pair).contains(QStringLiteral("pw")));

        const Pair anonymous = webdavPair(QStringLiteral("/home/u/Docs"), QStringLiteral("sftp://host.example/srv"));
        QCOMPARE(accountKey(anonymous), QStringLiteral("sftp://host.example"));

        Pair rclone;
        rclone.rcloneRemote = QStringLiteral("drive:Documents/Work");
        QCOMPARE(accountKey(rclone), QStringLiteral("rclone:drive"));
        QCOMPARE(remoteDisplayName(rclone), QStringLiteral("drive:Documents/Work"));
        QVERIFY(accountDisplayName(rclone).contains(QStringLiteral("drive")));
        QVERIFY(newId() != newId());
    }

    void snapshots()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("state/pair.json"));
        auto loaded = loadSnapshot(path);
        QVERIFY(loaded);
        QVERIFY(loaded->isEmpty());

        Snapshot snapshot = snapshotOf({{QStringLiteral("a"), file(1, 10)}, {QStringLiteral("d"), folder()}}, {{QStringLiteral("a"), file(1, 11)}});
        snapshot.localPath = QStringLiteral("/home/u/Docs");
        snapshot.remoteLocation = QStringLiteral("webdavs://cloud.example/dav/Docs");
        snapshot.rcloneFilters = {QStringLiteral("- /x")};
        QVERIFY(saveSnapshot(path, snapshot));
        loaded = loadSnapshot(path);
        QVERIFY(loaded);
        QCOMPARE(*loaded, snapshot);

        QFile corrupt(path);
        QVERIFY(corrupt.open(QIODevice::WriteOnly));
        corrupt.write("{\"version\": 2}");
        corrupt.close();
        QString error;
        QVERIFY(!loadSnapshot(path, &error));
        QVERIFY(!error.isEmpty());

        const Pair pair = webdavPair(QStringLiteral("/home/u/Docs/"), QStringLiteral("webdavs://cloud.example/dav/Docs/"));
        QCOMPARE(snapshotLocation(pair), snapshot.remoteLocation);
        QCOMPARE(baseFor(pair, snapshot), snapshot);
        const Pair moved = webdavPair(QStringLiteral("/home/u/Elsewhere"), QStringLiteral("webdavs://cloud.example/dav/Docs"));
        const Snapshot fresh = baseFor(moved, snapshot);
        QVERIFY(fresh.isEmpty());
        QCOMPARE(fresh.localPath, QStringLiteral("/home/u/Elsewhere"));
        QVERIFY(snapshotPath(pair.id).endsWith(QStringLiteral("cloud-sync/%1.json").arg(pair.id)));
    }

    void previewAndErrors()
    {
        Plan result;
        result.changes = {change(Operation::Upload, QStringLiteral("a"), 1), change(Operation::DeleteLocal, QStringLiteral("b"))};
        const QList<PreviewItem> items = previewItems(result);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0).path, QStringLiteral("a"));
        QVERIFY(!items.at(0).deletion);
        QVERIFY(items.at(1).deletion);
        QVERIFY(!describe(change(Operation::RenameRemote, QStringLiteral("n"), 1, false, QStringLiteral("o"))).isEmpty());

        QCOMPARE(classifyError(KIO::ERR_CANNOT_CONNECT), ErrorKind::Offline);
        QCOMPARE(classifyError(KIO::ERR_UNKNOWN_HOST), ErrorKind::Offline);
        QCOMPARE(classifyError(KIO::ERR_ACCESS_DENIED), ErrorKind::Permission);
        QCOMPARE(classifyError(KIO::ERR_DISK_FULL), ErrorKind::Quota);
        QCOMPARE(classifyError(KIO::ERR_DOES_NOT_EXIST), ErrorKind::Other);
    }

    void rcloneRules()
    {
        FolderSync::Options options;
        options.excludePatterns = {QStringLiteral("build"), QStringLiteral("*.o")};
        options.includePatterns = {QStringLiteral("*.txt")};
        QString error;
        QCOMPARE(rcloneFilterRules(options, &error),
                 (QStringList{QStringLiteral("- /build"),
                              QStringLiteral("- /build/**"),
                              QStringLiteral("- /**.o"),
                              QStringLiteral("- /**.o/**"),
                              QStringLiteral("+ /**.txt"),
                              QStringLiteral("- **")}));
        QVERIFY(error.isEmpty());
        QVERIFY(rcloneFilterRules({}).isEmpty());
        options.excludePatterns = {QStringLiteral("{a,b}")};
        QVERIFY(rcloneFilterRules(options, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rcloneArguments()
    {
        Pair pair;
        pair.id = newId();
        pair.localPath = QStringLiteral("/home/u/Drive/");
        pair.rcloneRemote = QStringLiteral("drive:Docs");
        QCOMPARE(CloudSync::rcloneArguments(pair, true, true),
                 (QStringList{QStringLiteral("bisync"),
                              QStringLiteral("/home/u/Drive"),
                              QStringLiteral("drive:Docs"),
                              QStringLiteral("--resync"),
                              QStringLiteral("--create-empty-src-dirs"),
                              QStringLiteral("--dry-run")}));

        // A conflict choice that keeps one version makes it bisync's winner; the others keep both.
        pair.conflictChoice = Resolution::KeepLocal;
        QVERIFY(CloudSync::rcloneArguments(pair, false, false).join(QLatin1Char(' ')).contains(QStringLiteral(" --conflict-resolve path1")));
        pair.conflictChoice = Resolution::KeepRemote;
        QVERIFY(CloudSync::rcloneArguments(pair, false, false).join(QLatin1Char(' ')).contains(QStringLiteral(" --conflict-resolve path2")));
        pair.conflictChoice = Resolution::KeepBoth;
        QVERIFY(!CloudSync::rcloneArguments(pair, false, false).contains(QStringLiteral("--conflict-resolve")));

        pair.mode = Mode::CloudBackup;
        pair.mirrorDeletions = true;
        pair.options.excludePatterns = {QStringLiteral("tmp")};
        QCOMPARE(CloudSync::rcloneArguments(pair, true, false),
                 (QStringList{QStringLiteral("sync"),
                              QStringLiteral("/home/u/Drive"),
                              QStringLiteral("drive:Docs"),
                              QStringLiteral("--update"),
                              QStringLiteral("--create-empty-src-dirs"),
                              QStringLiteral("--filter"),
                              QStringLiteral("- /tmp"),
                              QStringLiteral("--filter"),
                              QStringLiteral("- /tmp/**")}));

        pair.mode = Mode::LocalBackup;
        pair.mirrorDeletions = false;
        pair.options = {};
        pair.options.recursive = false;
        QCOMPARE(CloudSync::rcloneArguments(pair, false, false),
                 (QStringList{QStringLiteral("copy"),
                              QStringLiteral("drive:Docs"),
                              QStringLiteral("/home/u/Drive"),
                              QStringLiteral("--update"),
                              QStringLiteral("--create-empty-src-dirs"),
                              QStringLiteral("--max-depth"),
                              QStringLiteral("1")}));

        QString error;
        const Pair kio = webdavPair(QStringLiteral("/home/u/Docs"), QStringLiteral("webdavs://cloud.example/dav"));
        QVERIFY(CloudSync::rcloneArguments(kio, false, false, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rcloneOutput()
    {
        const QByteArray output =
            "2026/11/26 10:00:00 NOTICE: docs/a.txt: Skipped copy as --dry-run is set (size 3)\n"
            "2026/11/26 10:00:00 NOTICE: old.txt: Skipped delete as --dry-run is set (size 1)\n"
            "2026/11/26 10:00:00 NOTICE: sub: Skipped make directory as --dry-run is set\n"
            "2026/11/26 10:00:00 NOTICE: gone: Skipped remove directory as --dry-run is set\n"
            "2026/11/26 10:00:00 INFO  : There was nothing to transfer\n";
        const QList<PreviewItem> items = parseRcloneDryRun(output);
        QCOMPARE(items.size(), 4);
        QCOMPARE(items.at(0).path, QStringLiteral("docs/a.txt"));
        QVERIFY(!items.at(0).deletion);
        QCOMPARE(items.at(1).path, QStringLiteral("old.txt"));
        QVERIFY(items.at(1).deletion);
        QVERIFY(!items.at(2).deletion);
        QVERIFY(items.at(3).deletion);

        QVERIFY(rcloneNeedsResync("ERROR : Bisync critical error: cannot find prior Path1 or Path2 listings, likely due to critical error on prior run"));
        QVERIFY(rcloneNeedsResync("NOTICE: Must run --resync to recover."));
        QVERIFY(!rcloneNeedsResync("NOTICE: Bisync successful"));
    }

    void autostart()
    {
        QVERIFY(autostartPath().endsWith(QStringLiteral("/autostart/org.kde.opusfilemanager-cloud-sync.desktop")));
        const QString entry = QString::fromUtf8(autostartEntry(QStringLiteral("/opt/opus file manager"), QStringLiteral("icon")));
        QVERIFY(entry.startsWith(QStringLiteral("[Desktop Entry]\n")));
        QVERIFY(entry.contains(QStringLiteral("\nExec=\"/opt/opus file manager\" --background-sync\n")));
        QVERIFY(entry.contains(QStringLiteral("\nIcon=icon\n")));
        QVERIFY(autostartEntry(QStringLiteral("/usr/bin/opus"), {}).contains("\nExec=/usr/bin/opus --background-sync\n"));

        QFile::remove(autostartPath());
        QVERIFY(!startsAtLogin());
        QString error;
        QVERIFY2(setStartAtLogin(true, QStringLiteral("/usr/bin/opus"), QStringLiteral("icon"), &error), qPrintable(error));
        QVERIFY(startsAtLogin());
        QVERIFY(setStartAtLogin(false, {}, {}, &error));
        QVERIFY(!startsAtLogin());
        QVERIFY(setStartAtLogin(false, {}, {}, &error));
    }

    void migratesStartAtLogin()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("opusrc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("CloudSync"));
        QFile::remove(autostartPath());
        const auto restore = qScopeGuard([] {
            QFile::remove(autostartPath());
        });

        // Without an autostart entry, nothing is recorded.
        QVERIFY(!migrateStartAtLogin(group));
        QVERIFY(!group.hasKey("StartAtLogin"));

        // An entry made from File › Cloud Sync… before the setting existed turns it on.
        QVERIFY(setStartAtLogin(true, QStringLiteral("/usr/bin/opus"), {}));
        QVERIFY(migrateStartAtLogin(group));
        QVERIFY(group.readEntry("StartAtLogin", false));

        // A recorded choice is kept.
        group.writeEntry("StartAtLogin", false);
        QVERIFY(!migrateStartAtLogin(group));
        QVERIFY(!group.readEntry("StartAtLogin", true));

        // An unchanged entry isn't rewritten.
        const QDateTime written = QFileInfo(autostartPath()).lastModified();
        QTest::qWait(20);
        QVERIFY(setStartAtLogin(true, QStringLiteral("/usr/bin/opus"), {}));
        QCOMPARE(QFileInfo(autostartPath()).lastModified(), written);
    }
};

QTEST_GUILESS_MAIN(CloudSyncTest)

#include "cloudsynctest.moc"
