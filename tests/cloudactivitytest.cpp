// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudactivity.h"

#include "cloudsyncrunner.h"
#include "rclone.h"
#include "transferqueue.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace
{
class FakeTransfer : public KJob
{
public:
    void start() override
    {
    }

    void fail()
    {
        setError(UserDefinedError);
        setErrorText(QStringLiteral("disk full"));
        emitResult();
    }

    void report(unsigned long speed)
    {
        emitSpeed(speed);
    }
};

bool writeMetadata(const QString &cacheDir, const QString &relativePath, bool dirty, qint64 size)
{
    const QString path = QDir(cacheDir).filePath(QStringLiteral("vfsMeta/") + relativePath);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray json = QStringLiteral(R"({"ModTime":"2026-11-26T10:00:00Z","Size":%1,"Fingerprint":"x","Dirty":%2})")
                                .arg(size)
                                .arg(dirty ? QStringLiteral("true") : QStringLiteral("false"))
                                .toUtf8();
    return file.write(json) == json.size();
}

RemoteProfiles::Profile profile(const QString &name)
{
    RemoteProfiles::Profile profile;
    profile.name = name;
    profile.protocol = RemoteProfiles::Protocol::Sftp;
    profile.host = QStringLiteral("files.example.com");
    profile.user = QStringLiteral("ana");
    profile.path = QStringLiteral("/home/ana");
    return profile;
}

CloudSync::Pair pair(const QString &local, const QString &remote)
{
    CloudSync::Pair pair;
    pair.id = CloudSync::newId();
    pair.localPath = local;
    pair.remote = QUrl(remote);
    pair.intervalMinutes = 15;
    return pair;
}
}

class CloudActivityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void storesAndClampsLimits()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("limits")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("CloudActivity"));
        QCOMPARE(CloudActivity::loadLimits(group), CloudActivity::Limits());

        CloudActivity::Limits limits;
        limits.bandwidthKiB = 512;
        limits.parallelTransfers = 8;
        limits.cacheMiB = 2048;
        CloudActivity::saveLimits(group, limits);
        QCOMPARE(CloudActivity::loadLimits(group), limits);

        group.writeEntry("ParallelTransfers", 0);
        group.writeEntry("BandwidthLimitKiB", -5);
        QCOMPARE(CloudActivity::loadLimits(group).parallelTransfers, 1);
        QCOMPARE(CloudActivity::loadLimits(group).bandwidthKiB, 0);
    }

    void buildsRcloneOptions()
    {
        CloudActivity::Limits limits;
        QCOMPARE(CloudActivity::rcloneTransferOptions(limits), (QStringList{QStringLiteral("--transfers"), QStringLiteral("4")}));
        QCOMPARE(CloudActivity::rcloneMountOptions(limits, QStringLiteral("/cache")),
                 (QStringList{QStringLiteral("--cache-dir"),
                              QStringLiteral("/cache"),
                              QStringLiteral("--vfs-cache-mode"),
                              QStringLiteral("writes"),
                              QStringLiteral("--transfers"),
                              QStringLiteral("4")}));

        limits.bandwidthKiB = 256;
        limits.parallelTransfers = 2;
        limits.cacheMiB = 1024;
        const QStringList mount = CloudActivity::rcloneMountOptions(limits, QStringLiteral("/cache"));
        QCOMPARE(mount,
                 (QStringList{QStringLiteral("--cache-dir"),
                              QStringLiteral("/cache"),
                              QStringLiteral("--vfs-cache-mode"),
                              QStringLiteral("full"),
                              QStringLiteral("--vfs-cache-max-size"),
                              QStringLiteral("1024M"),
                              QStringLiteral("--transfers"),
                              QStringLiteral("2"),
                              QStringLiteral("--bwlimit"),
                              QStringLiteral("256K")}));
        // The options go before the remote, which ends the options.
        const QStringList arguments = Rclone::mountArguments(QStringLiteral("box"), QStringLiteral("/run/box"), mount);
        QCOMPARE(arguments.first(), QStringLiteral("mount"));
        QCOMPARE(arguments.mid(1, mount.size()), mount);
        QCOMPARE(arguments.mid(mount.size() + 1), (QStringList{QStringLiteral("--"), QStringLiteral("box:"), QStringLiteral("/run/box")}));
        QVERIFY(CloudActivity::rcloneCacheDir().startsWith(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    }

    void findsPendingUploads()
    {
        QTemporaryDir cache;
        QVERIFY(writeMetadata(cache.path(), QStringLiteral("drive/Documents/report.odt"), true, 2048));
        QVERIFY(writeMetadata(cache.path(), QStringLiteral("drive/Documents/notes.txt"), true, 100));
        QVERIFY(writeMetadata(cache.path(), QStringLiteral("drive/photo.jpg"), false, 5000000));
        QVERIFY(writeMetadata(cache.path(), QStringLiteral("box/read.pdf"), false, 300));
        QVERIFY(writeMetadata(cache.path(), QStringLiteral("dropbox/draft.md"), true, 10));
        QFile broken(QDir(cache.path()).filePath(QStringLiteral("vfsMeta/drive/broken")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not json");
        broken.close();

        const CloudActivity::PendingUploads drive = CloudActivity::pendingUploads(cache.path(), QStringLiteral("drive"));
        QCOMPARE(drive.files, 2);
        QCOMPARE(drive.bytes, 2148);
        QVERIFY(CloudActivity::pendingUploads(cache.path(), QStringLiteral("box")).isEmpty());
        QVERIFY(CloudActivity::pendingUploads(cache.path(), QStringLiteral("missing")).isEmpty());
        // Names that aren't rclone remotes can't reach outside the cache.
        QVERIFY(CloudActivity::pendingUploads(cache.path(), QStringLiteral("../vfsMeta/drive")).isEmpty());
        QCOMPARE(CloudActivity::remotesWithPendingUploads(cache.path()), (QStringList{QStringLiteral("drive"), QStringLiteral("dropbox")}));

        CloudActivity::UploadStatus status;
        QVERIFY(status.isIdle());
        status.syncRuns = 1;
        QVERIFY(!status.isIdle());
        status = {drive, 0};
        QVERIFY(!status.isIdle());
    }

    void estimatesRemainingTime()
    {
        QCOMPARE(CloudActivity::remainingSeconds(1000, 0, 100), 10);
        QCOMPARE(CloudActivity::remainingSeconds(1000, 950, 100), 1);
        QCOMPARE(CloudActivity::remainingSeconds(1000, 1000, 100), 0);
        QCOMPARE(CloudActivity::remainingSeconds(1000, 0, 0), -1);
        QCOMPARE(CloudActivity::remainingSeconds(0, 0, 100), -1);
    }

    void recognizesUnknownQuota()
    {
        QVERIFY(!CloudActivity::space(0, 0));
        QVERIFY(!CloudActivity::space(10, 5));
        QVERIFY(!CloudActivity::space(qint64(1) << 50, qint64(1) << 50));
        const auto known = CloudActivity::space(5, 15);
        QVERIFY(known);
        QCOMPARE(known->available, 5);
        QCOMPARE(known->total, 15);
        QTemporaryDir dir;
        // A folder that isn't a mount point has no quota of its own.
        QVERIFY(!CloudActivity::mountSpace(dir.path()));
    }

    void keepsRecentSpeeds()
    {
        CloudActivity::SpeedHistory history(3);
        QCOMPARE(history.peak(), 0ULL);
        history.add(5);
        history.add(50);
        history.add(10);
        history.add(20);
        QCOMPARE(history.samples(), (QList<quint64>{50, 10, 20}));
        QCOMPARE(history.peak(), 50ULL);
        QCOMPARE(history.latest(), 20ULL);
        history.clear();
        QVERIFY(history.samples().isEmpty());
    }

    void exportsAndImportsWithoutPasswords()
    {
        QTemporaryDir dir;
        const QString local = dir.filePath(QStringLiteral("Documents"));
        CloudActivity::Definitions definitions;
        definitions.profiles = {profile(QStringLiteral("Office"))};
        definitions.pairs = {pair(local, QStringLiteral("webdavs://ana:secret@cloud.example.com/dav/Documents"))};
        const QString path = dir.filePath(QStringLiteral("cloud.opusconnections"));
        QString error;
        QVERIFY2(CloudActivity::exportDefinitions(path, definitions, &error), qPrintable(error));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("secret"));

        const auto imported = CloudActivity::importDefinitions(path, &error);
        QVERIFY2(imported, qPrintable(error));
        QCOMPARE(imported->profiles.size(), 1);
        QCOMPARE(imported->profiles.first().name, QStringLiteral("Office"));
        QCOMPARE(imported->profiles.first().host, QStringLiteral("files.example.com"));
        QCOMPARE(imported->pairs.size(), 1);
        QCOMPARE(imported->pairs.first().localPath, local);
        QVERIFY(imported->pairs.first().remote.password().isEmpty());
        QCOMPARE(imported->pairs.first().intervalMinutes, 15);

        // Exporting again replaces the file's contents.
        definitions.profiles = {profile(QStringLiteral("Home"))};
        definitions.pairs.clear();
        QVERIFY(CloudActivity::exportDefinitions(path, definitions, &error));
        const auto replaced = CloudActivity::importDefinitions(path, &error);
        QVERIFY(replaced);
        QCOMPARE(replaced->profiles.size(), 1);
        QCOMPARE(replaced->profiles.first().name, QStringLiteral("Home"));
        QVERIFY(replaced->pairs.isEmpty());
    }

    void rejectsOtherFiles()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!CloudActivity::importDefinitions(dir.filePath(QStringLiteral("missing")), &error));
        QVERIFY(!error.isEmpty());

        const QString other = dir.filePath(QStringLiteral("other.conf"));
        QFile file(other);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[General]\nName=x\n");
        file.close();
        error.clear();
        QVERIFY(!CloudActivity::importDefinitions(other, &error));
        QVERIFY(!error.isEmpty());

        const QString empty = dir.filePath(QStringLiteral("empty.opusconnections"));
        QVERIFY(CloudActivity::exportDefinitions(empty, {}, &error));
        error.clear();
        QVERIFY(!CloudActivity::importDefinitions(empty, &error));
        QVERIFY(!error.isEmpty());
    }

    void mergesWithoutDuplicates()
    {
        QTemporaryDir dir;
        CloudActivity::Definitions existing;
        existing.profiles = {profile(QStringLiteral("Office"))};
        existing.profiles.first().id = QStringLiteral("existing");
        existing.pairs = {pair(dir.filePath(QStringLiteral("Documents")), QStringLiteral("webdavs://cloud.example.com/dav/Documents"))};

        CloudActivity::Definitions imported;
        imported.profiles = {profile(QStringLiteral(" office ")), profile(QStringLiteral("Server"))};
        imported.profiles.last().id = QStringLiteral("from-elsewhere");
        imported.pairs = {pair(dir.filePath(QStringLiteral("Documents")), QStringLiteral("webdavs://cloud.example.com/dav/Other")),
                          pair(dir.filePath(QStringLiteral("Music")), QStringLiteral("webdavs://cloud.example.com/dav/Music"))};
        const QString importedId = imported.pairs.last().id;

        const CloudActivity::MergeResult result = CloudActivity::merge(existing, imported);
        QCOMPARE(result.addedProfiles, 1);
        QCOMPARE(result.addedPairs, 1);
        QCOMPARE(result.skipped.size(), 2);
        QCOMPARE(result.profiles.size(), 2);
        QCOMPARE(result.profiles.last().name, QStringLiteral("Server"));
        QVERIFY(result.profiles.last().id.isEmpty());
        QCOMPARE(result.pairs.size(), 2);
        const CloudSync::Pair &added = result.pairs.last();
        QCOMPARE(added.localPath, dir.filePath(QStringLiteral("Music")));
        QVERIFY(added.paused);
        QVERIFY(added.id != importedId);
        QVERIFY(!existing.pairs.first().paused);
        QVERIFY(!result.pairs.first().paused);
    }

    void recordsTransferFailuresAndSpeed()
    {
        TransferQueue queue;
        queue.setConcurrency(2);
        CloudActivity::Monitor monitor;
        monitor.setSampleInterval(10);
        monitor.setTransferQueue(&queue);
        FakeTransfer *fast = nullptr;
        FakeTransfer *broken = nullptr;
        queue.enqueue(QStringLiteral("Upload video"), [&] {
            fast = new FakeTransfer;
            return fast;
        });
        queue.enqueue(QStringLiteral("Upload report"), [&] {
            broken = new FakeTransfer;
            return broken;
        });
        QTRY_VERIFY(fast && broken);
        fast->report(3000);
        QCOMPARE(monitor.currentSpeed(), 3000ULL);
        QTRY_COMPARE(monitor.speedHistory().latest(), 3000ULL);

        broken->fail();
        QCOMPARE(monitor.events().size(), 1);
        QCOMPARE(monitor.events().first().source, QStringLiteral("Upload report"));
        QCOMPARE(monitor.events().first().message, QStringLiteral("disk full"));
        // A failure is reported once.
        Q_EMIT queue.changed();
        QCOMPARE(monitor.events().size(), 1);
        monitor.clearEvents();
        QVERIFY(monitor.events().isEmpty());

        monitor.setPaused(true);
        QVERIFY(monitor.isPaused());
        QVERIFY(queue.isPaused());
        monitor.setPaused(false);
        QVERIFY(!queue.isPaused());
    }

    void keepsTheLatestEvents()
    {
        CloudActivity::Monitor monitor;
        QSignalSpy changed(&monitor, &CloudActivity::Monitor::changed);
        for (int i = 0; i < 120; ++i) {
            monitor.addEvent(QStringLiteral("drive"), QString::number(i));
        }
        QCOMPARE(changed.size(), 120);
        QCOMPARE(monitor.events().size(), 100);
        QCOMPARE(monitor.events().first().message, QStringLiteral("20"));
        QCOMPARE(monitor.events().last().message, QStringLiteral("119"));
    }

    void recordsFailedSyncRunsAndPausesBackgroundRuns()
    {
        QTemporaryDir dir;
        KSharedConfig::Ptr config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("cloudsyncrc")), KConfig::SimpleConfig);
        CloudSync::Scheduler scheduler(config);
        CloudSync::Pair rclonePair;
        rclonePair.id = CloudSync::newId();
        rclonePair.localPath = dir.filePath(QStringLiteral("local"));
        rclonePair.rcloneRemote = QStringLiteral("drive:Documents");
        rclonePair.intervalMinutes = 5;
        QString error;
        QVERIFY2(scheduler.setPairs({rclonePair}, &error), qPrintable(error));
        scheduler.setRunnerSetup([](CloudSync::Runner *runner) {
            runner->setRcloneProgram(QString());
        });
        scheduler.setRcloneOptions({QStringLiteral("--bwlimit"), QStringLiteral("1K")});

        CloudActivity::Monitor monitor;
        monitor.setScheduler(&scheduler);
        scheduler.syncNow(rclonePair.id);
        QTRY_COMPARE(scheduler.state(rclonePair.id), CloudSync::Scheduler::State::Idle);
        QCOMPARE(scheduler.lastReport(rclonePair.id).outcome, CloudSync::Report::Outcome::Failed);
        QCOMPARE(monitor.events().size(), 1);
        QCOMPARE(monitor.events().first().source, QStringLiteral("drive:Documents"));
        // Status changes after the run don't report it again.
        Q_EMIT scheduler.statusChanged(rclonePair.id);
        QCOMPARE(monitor.events().size(), 1);

        QCOMPARE(scheduler.activeRuns(), 0);
        // The pair has just run, so use one that hasn't.
        CloudSync::Pair duePair = rclonePair;
        duePair.id = CloudSync::newId();
        QVERIFY2(scheduler.setPairs({duePair}, &error), qPrintable(error));
        monitor.setPaused(true);
        QVERIFY(scheduler.isBackgroundPaused());
        QVERIFY(monitor.isPaused());
        scheduler.checkDue();
        QCOMPARE(scheduler.activeRuns(), 0);
        QCOMPARE(monitor.events().size(), 1);
        // The pause is remembered.
        CloudSync::Scheduler reopened(config);
        QVERIFY(reopened.isBackgroundPaused());

        monitor.setPaused(false);
        QVERIFY(!scheduler.isBackgroundPaused());
        scheduler.checkDue();
        // Without rclone, the due run fails and is reported.
        QTRY_COMPARE(monitor.events().size(), 2);
        scheduler.stop();
    }
};

QTEST_GUILESS_MAIN(CloudActivityTest)
#include "cloudactivitytest.moc"
