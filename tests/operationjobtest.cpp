// SPDX-License-Identifier: GPL-3.0-or-later

#include "operationjob.h"
#include "settings.h"

#include <KJobTrackerInterface>

#include <QFutureWatcher>
#include <QPointer>
#include <QPromise>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <memory>

namespace
{
class RecordingTracker : public KJobTrackerInterface
{
public:
    QString title;
    std::pair<QString, QString> field1;
    std::pair<QString, QString> field2;
    qulonglong total = 0;
    qulonglong processed = 0;
    int finishedCount = 0;
    int registered = 0;

    void registerJob(KJob *job) override
    {
        ++registered;
        KJobTrackerInterface::registerJob(job);
    }
    // The base tracker reports a finished job by unregistering it.
    void unregisterJob(KJob *job) override
    {
        ++finishedCount;
        KJobTrackerInterface::unregisterJob(job);
    }

protected:
    void description(KJob *, const QString &jobTitle, const std::pair<QString, QString> &first, const std::pair<QString, QString> &second) override
    {
        title = jobTitle;
        field1 = first;
        field2 = second;
    }
    void totalAmount(KJob *, KJob::Unit unit, qulonglong amount) override
    {
        if (unit == KJob::Items) {
            total = amount;
        }
    }
    void processedAmount(KJob *, KJob::Unit unit, qulonglong amount) override
    {
        if (unit == KJob::Items) {
            processed = amount;
        }
    }
};
}

class OperationJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
    }

    void displayNameHidesPaths_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<QString>("name");
        QTest::newRow("file") << QUrl::fromLocalFile(QStringLiteral("/home/user/private/report.sha256")) << QStringLiteral("report.sha256");
        QTest::newRow("folder with slash") << QUrl::fromLocalFile(QStringLiteral("/home/user/Photos/")) << QStringLiteral("Photos");
        QTest::newRow("remote root") << QUrl(QStringLiteral("sftp://server.example/")) << QStringLiteral("server.example");
        QTest::newRow("local root") << QUrl::fromLocalFile(QStringLiteral("/")) << QStringLiteral("/");
        QTest::newRow("empty") << QUrl() << QString();
    }

    void displayNameHidesPaths()
    {
        QFETCH(QUrl, url);
        QFETCH(QString, name);
        QCOMPARE(OperationJob::displayName(url), name);
    }

    void reportsDescriptionProgressAndSuccess()
    {
        RecordingTracker tracker;
        auto jobOwner = std::make_unique<OperationJob>(OperationJob::Kind::FolderSync, QStringLiteral("Synchronizing folders"));
        auto *job = jobOwner.get();
        job->setAutoDelete(false);
        QSignalSpy result(job, &KJob::result);
        QPromise<int> promise;
        QFutureWatcher<int> watcher;
        watcher.setFuture(promise.future());
        promise.start();
        OperationJob::track(job, &watcher, &tracker);
        job->setSource(QUrl::fromLocalFile(QStringLiteral("/home/user/secret/Left")));
        job->setDestination(QUrl::fromLocalFile(QStringLiteral("/home/user/secret/Right")));
        QCOMPARE(tracker.registered, 1);
        QCOMPARE(tracker.title, QStringLiteral("Synchronizing folders"));
        QCOMPARE(tracker.field1.second, QStringLiteral("Left"));
        QCOMPARE(tracker.field2.second, QStringLiteral("Right"));
        QVERIFY(!tracker.field1.second.contains(QStringLiteral("secret")));
        QCOMPARE(job->property("destUrl").toUrl(), QUrl::fromLocalFile(QStringLiteral("/home/user/secret/Right")));
        QVERIFY(!(job->capabilities() & KJob::Killable));

        promise.setProgressRange(0, 4);
        promise.setProgressValue(1);
        QTRY_COMPARE(tracker.total, 4ULL);
        QTRY_COMPARE(tracker.processed, 1ULL);
        QCOMPARE(job->percent(), 25UL);
        promise.addResult(1);
        promise.finish();
        QTRY_COMPARE(result.count(), 1);
        QCOMPARE(job->error(), 0);
        QCOMPARE(tracker.processed, 4ULL);
        QCOMPARE(tracker.finishedCount, 1);
    }

    void ownerFailureWinsOverWatcherFinish()
    {
        RecordingTracker tracker;
        auto jobOwner = std::make_unique<OperationJob>(OperationJob::Kind::Checksums, QStringLiteral("Verifying checksum file"));
        auto *job = jobOwner.get();
        job->setAutoDelete(false);
        QSignalSpy result(job, &KJob::result);
        QPromise<int> promise;
        QFutureWatcher<int> watcher;
        QPointer<OperationJob> guard(job);
        connect(&watcher, &QFutureWatcherBase::finished, this, [guard] {
            if (guard) {
                guard->finish(QStringLiteral("2 files failed verification."));
            }
        });
        watcher.setFuture(promise.future());
        promise.start();
        OperationJob::track(job, &watcher, &tracker);
        promise.finish();
        QTRY_COMPARE(result.count(), 1);
        QCOMPARE(job->error(), int(KJob::UserDefinedError));
        QCOMPARE(job->errorText(), QStringLiteral("2 files failed verification."));
        job->finish();
        QCOMPARE(result.count(), 1);
    }

    void cancelFromPlasmaCallsHandlerOnce()
    {
        RecordingTracker tracker;
        auto jobOwner = std::make_unique<OperationJob>(OperationJob::Kind::DuplicateFinder, QStringLiteral("Finding duplicate files"));
        auto *job = jobOwner.get();
        job->setAutoDelete(false);
        QSignalSpy result(job, &KJob::result);
        int calls = 0;
        job->setCancelHandler([&calls, job] {
            ++calls;
            // The owner reacts to the request by canceling the job itself.
            job->cancel();
        });
        OperationJob::track(job, nullptr, &tracker);
        QVERIFY(job->capabilities() & KJob::Killable);
        QVERIFY(job->kill(KJob::EmitResult));
        QCOMPARE(calls, 1);
        QCOMPARE(result.count(), 1);
        QCOMPARE(job->error(), int(KJob::KilledJobError));
        job->finish(QStringLiteral("late"));
        QCOMPARE(job->error(), int(KJob::KilledJobError));
    }

    void ownerCancelSkipsHandler()
    {
        auto jobOwner = std::make_unique<OperationJob>(OperationJob::Kind::FileSplit, QStringLiteral("Splitting file"));
        auto *job = jobOwner.get();
        job->setAutoDelete(false);
        QSignalSpy result(job, &KJob::result);
        int calls = 0;
        job->setCancelHandler([&calls] {
            ++calls;
        });
        OperationJob::track(job, nullptr, nullptr);
        job->cancel();
        QCOMPARE(calls, 0);
        QCOMPARE(result.count(), 1);
        QCOMPARE(job->error(), int(KJob::KilledJobError));
    }

    void canceledOrDestroyedWatcherCancelsJob()
    {
        auto canceledJobOwner = std::make_unique<OperationJob>(OperationJob::Kind::BatchRename, QStringLiteral("Renaming items"));
        auto *canceledJob = canceledJobOwner.get();
        canceledJob->setAutoDelete(false);
        QSignalSpy canceledResult(canceledJob, &KJob::result);
        QPromise<int> promise;
        QFutureWatcher<int> watcher;
        watcher.setFuture(promise.future());
        promise.start();
        OperationJob::track(canceledJob, &watcher, nullptr);
        watcher.cancel();
        promise.finish();
        QTRY_COMPARE(canceledResult.count(), 1);
        QCOMPARE(canceledJob->error(), int(KJob::KilledJobError));

        auto orphanedJobOwner = std::make_unique<OperationJob>(OperationJob::Kind::FileSplit, QStringLiteral("Combining files"));
        auto *orphanedJob = orphanedJobOwner.get();
        orphanedJob->setAutoDelete(false);
        QSignalSpy orphanedResult(orphanedJob, &KJob::result);
        QPromise<int> running;
        auto orphanWatcher = std::make_unique<QFutureWatcher<int>>();
        orphanWatcher->setFuture(running.future());
        running.start();
        OperationJob::track(orphanedJob, orphanWatcher.get(), nullptr);
        orphanWatcher.reset();
        QCOMPARE(orphanedResult.count(), 1);
        QCOMPARE(orphanedJob->error(), int(KJob::KilledJobError));
        running.finish();
    }

    void defaultTrackerIsUsed()
    {
        RecordingTracker tracker;
        OperationJob::setDefaultTracker(&tracker);
        auto *job = OperationJob::track(new OperationJob(OperationJob::Kind::LibraryRefresh, QStringLiteral("Refreshing tag library")));
        job->setAutoDelete(false);
        const std::unique_ptr<OperationJob> owner(job);
        QCOMPARE(tracker.registered, 1);
        job->finish();
        QCOMPARE(tracker.finishedCount, 1);
        OperationJob::setDefaultTracker(nullptr);
        QCOMPARE(OperationJob::defaultTracker(), nullptr);
    }

    void reportingFollowsTheSettings()
    {
        QVERIFY(OperationJob::reports(OperationJob::Kind::Checksums));
        Settings::setNotifyChecksums(false);
        QVERIFY(!OperationJob::reports(OperationJob::Kind::Checksums));
        QVERIFY(OperationJob::reports(OperationJob::Kind::FolderSync));

        // An unreported job still runs and finishes for its owner.
        RecordingTracker tracker;
        auto jobOwner = std::make_unique<OperationJob>(OperationJob::Kind::Checksums, QStringLiteral("Creating checksum file"));
        auto *job = jobOwner.get();
        job->setAutoDelete(false);
        QSignalSpy result(job, &KJob::result);
        OperationJob::track(job, nullptr, &tracker);
        job->setProgress(1, 2);
        job->finish();
        QCOMPARE(tracker.registered, 0);
        QCOMPARE(tracker.finishedCount, 0);
        QCOMPARE(result.count(), 1);
        QCOMPARE(job->error(), 0);

        const auto settings = {&Settings::setNotifyFolderSync,
                               &Settings::setNotifyDuplicateFinder,
                               &Settings::setNotifyFileSplit,
                               &Settings::setNotifyBatchRename,
                               &Settings::setNotifyImageConversion,
                               &Settings::setNotifyLibraryRefresh};
        for (const auto set : settings) {
            set(false);
        }
        for (const auto kind : {OperationJob::Kind::FolderSync,
                                OperationJob::Kind::DuplicateFinder,
                                OperationJob::Kind::FileSplit,
                                OperationJob::Kind::BatchRename,
                                OperationJob::Kind::ImageConversion,
                                OperationJob::Kind::LibraryRefresh}) {
            QVERIFY(!OperationJob::reports(kind));
        }
    }
};

QTEST_GUILESS_MAIN(OperationJobTest)

#include "operationjobtest.moc"
