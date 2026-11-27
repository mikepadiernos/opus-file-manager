// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsyncrunner.h"

#include <KSharedConfig>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace CloudSync;

namespace
{
constexpr qint64 Epoch = 1'700'000'000;

bool writeFile(const QString &path, const QByteArray &data, qint64 modified)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.flush()) {
        return false;
    }
    return file.setFileTime(QDateTime::fromSecsSinceEpoch(Epoch + modified), QFileDevice::FileModificationTime);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}

Report run(Runner &runner, Runner::Run mode)
{
    QSignalSpy spy(&runner, &Runner::finished);
    runner.start(mode);
    if (spy.isEmpty()) {
        spy.wait(60000);
    }
    return runner.report();
}
}

class CloudSyncRunnerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_local;
    QString m_remote;
    Pair m_pair;

    QString local(const QString &path) const
    {
        return m_local + QLatin1Char('/') + path;
    }

    QString remote(const QString &path) const
    {
        return m_remote + QLatin1Char('/') + path;
    }

    std::unique_ptr<Runner> runner() const
    {
        auto result = std::make_unique<Runner>(m_pair);
        result->setSnapshotPath(m_dir->filePath(QStringLiteral("snapshot.json")));
        result->setUseTrash(false);
        result->setConflictName(QStringLiteral("conflict T"));
        return result;
    }

    Report sync(Runner::Run mode = Runner::Run::Automatic, const QHash<QString, Resolution> &resolutions = {})
    {
        auto instance = runner();
        instance->setResolutions(resolutions);
        return run(*instance, mode);
    }

    /// Runs the first synchronization, which needs review, and confirms it.
    Report initialSync()
    {
        const Report preview = sync(Runner::Run::Preview);
        auto instance = runner();
        instance->setConfirmedPreview(preview.preview);
        instance->setConfirmedConflicts(preview.conflicts);
        return run(*instance, Runner::Run::Confirmed);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        m_dir = new QTemporaryDir;
        QVERIFY(m_dir->isValid());
        m_local = m_dir->filePath(QStringLiteral("local"));
        m_remote = m_dir->filePath(QStringLiteral("remote"));
        QVERIFY(QDir().mkpath(m_local));
        QVERIFY(QDir().mkpath(m_remote));
        m_pair = {};
        m_pair.id = newId();
        m_pair.localPath = m_local;
        m_pair.remote = QUrl::fromLocalFile(m_remote);
    }

    void cleanup()
    {
        delete m_dir;
        m_dir = nullptr;
    }

    void firstRunNeedsReview()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "local", 10));
        QVERIFY(writeFile(remote(QStringLiteral("sub/b.txt")), "remote", 20));

        Report report = sync();
        QCOMPARE(report.outcome, Report::Outcome::NeedsReview);
        QCOMPARE(report.preview.size(), 3);
        QVERIFY(!QFile::exists(remote(QStringLiteral("a.txt"))));
        QVERIFY(!QFile::exists(m_dir->filePath(QStringLiteral("snapshot.json"))));

        // A confirmation that no longer matches the folders asks again.
        QVERIFY(writeFile(local(QStringLiteral("late.txt")), "late", 30));
        auto stale = runner();
        stale->setConfirmedPreview(report.preview);
        report = run(*stale, Runner::Run::Confirmed);
        QCOMPARE(report.outcome, Report::Outcome::NeedsReview);
        QVERIFY(!QFile::exists(remote(QStringLiteral("a.txt"))));

        report = initialSync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(report.uploaded, 2);
        QCOMPARE(report.downloaded, 1);
        QCOMPARE(report.folders, 1);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("local"));
        QCOMPARE(readFile(local(QStringLiteral("sub/b.txt"))), QByteArray("remote"));
        QVERIFY(QFile::exists(m_dir->filePath(QStringLiteral("snapshot.json"))));

        report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(summary(report), QStringLiteral("Up to date"));
    }

    void twoWayChanges()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        QVERIFY(writeFile(local(QStringLiteral("b.txt")), "two", 10));
        QVERIFY(writeFile(local(QStringLiteral("old.txt")), "renamed", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);

        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "edited locally", 50));
        QVERIFY(QFile::remove(remote(QStringLiteral("b.txt"))));
        QVERIFY(writeFile(remote(QStringLiteral("c.txt")), "new remotely", 60));
        QVERIFY(QFile::rename(local(QStringLiteral("old.txt")), local(QStringLiteral("new.txt"))));

        const Report report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(report.uploaded, 1);
        QCOMPARE(report.downloaded, 1);
        QCOMPARE(report.deleted, 1);
        QCOMPARE(report.renamed, 1);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("edited locally"));
        QVERIFY(!QFile::exists(local(QStringLiteral("b.txt"))));
        QCOMPARE(readFile(local(QStringLiteral("c.txt"))), QByteArray("new remotely"));
        QVERIFY(!QFile::exists(remote(QStringLiteral("old.txt"))));
        QCOMPARE(readFile(remote(QStringLiteral("new.txt"))), QByteArray("renamed"));

        QCOMPARE(summary(sync()), QStringLiteral("Up to date"));
    }

    void conflictKeepsBoth()
    {
        QVERIFY(writeFile(local(QStringLiteral("doc.txt")), "base", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);
        QVERIFY(writeFile(local(QStringLiteral("doc.txt")), "local edit", 40));
        QVERIFY(writeFile(remote(QStringLiteral("doc.txt")), "remote edit!", 50));

        Report report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(report.conflicts.size(), 1);
        QCOMPARE(readFile(remote(QStringLiteral("doc.txt"))), QByteArray("remote edit!"));

        report = sync(Runner::Run::Automatic, {{QStringLiteral("doc.txt"), Resolution::KeepBoth}});
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QVERIFY(report.conflicts.isEmpty());
        const QString copy = QStringLiteral("doc (conflict T).txt");
        QCOMPARE(readFile(local(QStringLiteral("doc.txt"))), QByteArray("local edit"));
        QCOMPARE(readFile(local(copy)), QByteArray("remote edit!"));
        QCOMPARE(readFile(remote(QStringLiteral("doc.txt"))), QByteArray("local edit"));

        // The conflict copy is uploaded by the next run.
        report = sync();
        QCOMPARE(report.uploaded, 1);
        QCOMPARE(readFile(remote(copy)), QByteArray("remote edit!"));
        QCOMPARE(summary(sync()), QStringLiteral("Up to date"));
    }

    void confirmedResolutions()
    {
        QVERIFY(writeFile(local(QStringLiteral("doc.txt")), "base", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);
        QVERIFY(writeFile(local(QStringLiteral("doc.txt")), "local edit", 40));
        QVERIFY(writeFile(remote(QStringLiteral("doc.txt")), "remote edit!", 50));
        QVERIFY(writeFile(local(QStringLiteral("new.txt")), "new", 50));

        const Report preview = sync(Runner::Run::Preview);
        QCOMPARE(preview.outcome, Report::Outcome::NeedsReview);
        QCOMPARE(preview.preview.size(), 1);
        QCOMPARE(preview.conflicts.size(), 1);
        QCOMPARE(readFile(remote(QStringLiteral("doc.txt"))), QByteArray("remote edit!"));

        // The remote version changed again after the review.
        QVERIFY(writeFile(remote(QStringLiteral("doc.txt")), "remote edit 2!", 60));
        auto stale = runner();
        stale->setResolutions({{QStringLiteral("doc.txt"), Resolution::KeepLocal}});
        stale->setConfirmedPreview(preview.preview);
        stale->setConfirmedConflicts(preview.conflicts);
        QCOMPARE(run(*stale, Runner::Run::Confirmed).outcome, Report::Outcome::NeedsReview);
        QCOMPARE(readFile(remote(QStringLiteral("doc.txt"))), QByteArray("remote edit 2!"));

        const Report again = sync(Runner::Run::Preview);
        auto confirmed = runner();
        confirmed->setResolutions({{QStringLiteral("doc.txt"), Resolution::KeepLocal}});
        confirmed->setConfirmedPreview(again.preview);
        confirmed->setConfirmedConflicts(again.conflicts);
        const Report report = run(*confirmed, Runner::Run::Confirmed);
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(report.uploaded, 2);
        QCOMPARE(readFile(remote(QStringLiteral("doc.txt"))), QByteArray("local edit"));
        QCOMPARE(readFile(remote(QStringLiteral("new.txt"))), QByteArray("new"));
    }

    void changeDuringRunIsLeftForNextTime()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "two", 20));

        auto instance = runner();
        bool edited = false;
        connect(instance.get(), &Runner::progressChanged, this, [&](const QString &message) {
            if (!edited && message.endsWith(QStringLiteral(": a.txt"))) {
                edited = true;
                QVERIFY(writeFile(local(QStringLiteral("a.txt")), "three!", 30));
            }
        });
        Report report = run(*instance, Runner::Run::Automatic);
        QVERIFY(edited);
        QCOMPARE(report.outcome, Report::Outcome::Partial);
        QCOMPARE(report.errors.size(), 1);
        QCOMPARE(report.errors.constFirst().path, QStringLiteral("a.txt"));
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("one"));

        report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("three!"));
    }

    void remoteChangeIsNotOverwritten()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "two", 20));

        auto instance = runner();
        bool edited = false;
        connect(instance.get(), &Runner::progressChanged, this, [&](const QString &message) {
            if (!edited && message.endsWith(QStringLiteral(": a.txt"))) {
                edited = true;
                QVERIFY(writeFile(remote(QStringLiteral("a.txt")), "remote!", 30));
            }
        });
        const Report report = run(*instance, Runner::Run::Automatic);
        QCOMPARE(report.outcome, Report::Outcome::Partial);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("remote!"));
        QCOMPARE(sync().conflicts.size(), 1);
    }

    void missingRemoteIsCreated()
    {
        m_pair.remote = QUrl::fromLocalFile(remote(QStringLiteral("new/folder")));
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        const Report report = initialSync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(readFile(remote(QStringLiteral("new/folder/a.txt"))), QByteArray("one"));
    }

    void missingFoldersAfterSyncFail()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);

        QVERIFY(QDir(m_remote).removeRecursively());
        Report report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Failed);
        QVERIFY(QFile::exists(local(QStringLiteral("a.txt"))));

        QVERIFY(QDir().mkpath(m_remote));
        QVERIFY(QDir(m_local).removeRecursively());
        report = sync();
        QCOMPARE(report.outcome, Report::Outcome::Failed);
        QVERIFY(QDir(m_remote).isEmpty());
    }

    void largeDeletionNeedsReview()
    {
        for (int i = 0; i < 6; ++i) {
            QVERIFY(writeFile(local(QStringLiteral("f%1.txt").arg(i)), QByteArray::number(i), 10));
        }
        QCOMPARE(initialSync().outcome, Report::Outcome::Succeeded);
        for (int i = 0; i < 5; ++i) {
            QVERIFY(QFile::remove(remote(QStringLiteral("f%1.txt").arg(i))));
        }
        const Report report = sync();
        QCOMPARE(report.outcome, Report::Outcome::NeedsReview);
        QCOMPARE(report.preview.size(), 5);
        QVERIFY(QFile::exists(local(QStringLiteral("f0.txt"))));
    }

    void filtersAndBackupModes()
    {
        m_pair.mode = Mode::CloudBackup;
        m_pair.options.excludePatterns = {QStringLiteral("*.tmp")};
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        QVERIFY(writeFile(local(QStringLiteral("scratch.tmp")), "tmp", 10));
        QVERIFY(writeFile(remote(QStringLiteral("remote.txt")), "remote", 10));
        const Report report = initialSync();
        QCOMPARE(report.outcome, Report::Outcome::Succeeded);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("one"));
        QVERIFY(!QFile::exists(remote(QStringLiteral("scratch.tmp"))));
        QVERIFY(!QFile::exists(local(QStringLiteral("remote.txt"))));
        QCOMPARE(report.skipped.size(), 1);
    }

    void cancel()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        auto instance = runner();
        QSignalSpy spy(instance.get(), &Runner::finished);
        instance->start(Runner::Run::Preview);
        instance->cancel();
        if (spy.isEmpty()) {
            QVERIFY(spy.wait(60000));
        }
        QCOMPARE(instance->report().outcome, Report::Outcome::Canceled);
        QVERIFY(!instance->isRunning());
    }

    void scheduler()
    {
        QVERIFY(writeFile(local(QStringLiteral("a.txt")), "one", 10));
        KSharedConfig::Ptr config = KSharedConfig::openConfig(m_dir->filePath(QStringLiteral("schedulerrc")), KConfig::SimpleConfig);
        Scheduler scheduler(config);
        bool metered = true;
        scheduler.setMeteredCheck([&metered] {
            return metered;
        });
        scheduler.setRunnerSetup([](Runner *runner) {
            runner->setUseTrash(false);
        });
        m_pair.intervalMinutes = 5;
        QString error;
        QVERIFY2(scheduler.setPairs({m_pair}, &error), qPrintable(error));
        QCOMPARE(Scheduler(config).pairs(), QList<Pair>{m_pair});

        QVERIFY(scheduler.pausesOnMeteredConnections());
        scheduler.checkDue();
        QCOMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);

        metered = false;
        scheduler.setAccountPaused(accountKey(m_pair), true);
        QVERIFY(scheduler.isPaused(m_pair));
        scheduler.checkDue();
        QCOMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);
        scheduler.setAccountPaused(accountKey(m_pair), false);
        QVERIFY(!scheduler.isPaused(m_pair));

        QSignalSpy attention(&scheduler, &Scheduler::attentionNeeded);
        QSignalSpy status(&scheduler, &Scheduler::statusChanged);
        scheduler.checkDue();
        QVERIFY(scheduler.state(m_pair.id) != Scheduler::State::Idle);
        QTRY_COMPARE_WITH_TIMEOUT(attention.size(), 1, 60000);
        QCOMPARE(attention.constFirst().at(0).toString(), m_pair.id);
        QVERIFY(scheduler.needsReview(m_pair.id));
        QTRY_COMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);

        // A pair waiting for review is not run in the background.
        scheduler.checkDue();
        QCOMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);

        scheduler.confirm(m_pair.id, {}, scheduler.lastReport(m_pair.id));
        QTRY_COMPARE_WITH_TIMEOUT(scheduler.lastReport(m_pair.id).outcome, Report::Outcome::Succeeded, 60000);
        QTRY_COMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);
        QCOMPARE(readFile(remote(QStringLiteral("a.txt"))), QByteArray("one"));
        QVERIFY(QFile::exists(snapshotPath(m_pair.id)));
        QVERIFY(status.size() > 0);

        // The interval has not passed yet.
        scheduler.checkDue();
        QCOMPARE(scheduler.state(m_pair.id), Scheduler::State::Idle);

        // Stopping cancels the running pair and drops queued ones.
        Pair second = m_pair;
        second.id = newId();
        second.localPath = local(QStringLiteral("second"));
        second.remote = QUrl::fromLocalFile(remote(QStringLiteral("second")));
        Pair first = m_pair;
        first.localPath = local(QStringLiteral("first"));
        first.remote = QUrl::fromLocalFile(remote(QStringLiteral("first")));
        first.id = newId();
        QVERIFY2(scheduler.setPairs({m_pair, first, second}, &error), qPrintable(error));
        scheduler.syncNow(first.id);
        scheduler.syncNow(second.id);
        QCOMPARE(scheduler.state(second.id), Scheduler::State::Queued);
        scheduler.stop();
        QCOMPARE(scheduler.state(second.id), Scheduler::State::Idle);
        QTRY_COMPARE(scheduler.state(first.id), Scheduler::State::Idle);
        QCOMPARE(scheduler.lastReport(first.id).outcome, Report::Outcome::Canceled);

        QVERIFY(scheduler.setPairs({}));
        QVERIFY(!QFile::exists(snapshotPath(m_pair.id)));
        QVERIFY(Scheduler(config).pairs().isEmpty());
    }
};

QTEST_GUILESS_MAIN(CloudSyncRunnerTest)

#include "cloudsyncrunnertest.moc"
