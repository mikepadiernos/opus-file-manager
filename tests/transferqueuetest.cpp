// SPDX-License-Identifier: GPL-3.0-or-later

#include "transferqueue.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class FakeTransfer : public KJob
{
public:
    FakeTransfer()
    {
        setCapabilities(Killable | Suspendable);
    }

    void start() override
    {
    }

    void finish(int error = 0)
    {
        if (error) {
            setError(error);
            setErrorText(QStringLiteral("failure"));
        }
        emitResult();
    }

    void progress(unsigned long percent, unsigned long speed)
    {
        setPercent(percent);
        emitSpeed(speed);
    }

    void amounts(qulonglong total, qulonglong processed)
    {
        setTotalAmount(Bytes, total);
        setProcessedAmount(Bytes, processed);
    }

protected:
    bool doKill() override
    {
        return true;
    }
    bool doSuspend() override
    {
        return true;
    }
    bool doResume() override
    {
        return true;
    }
};

class TransferQueueTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void serializesAndControlsTransfers()
    {
        TransferQueue queue;
        FakeTransfer *first = nullptr;
        FakeTransfer *second = nullptr;
        auto *one = queue.enqueue(QStringLiteral("one"), [&] {
            first = new FakeTransfer;
            return first;
        });
        auto *two = queue.enqueue(QStringLiteral("two"), [&] {
            second = new FakeTransfer;
            return second;
        });
        QSignalSpy firstResult(one, &KJob::result);
        QTRY_VERIFY(first);
        QVERIFY(!second);
        QCOMPARE(one->state(), QueuedTransfer::State::Running);
        QCOMPARE(two->state(), QueuedTransfer::State::Waiting);
        first->progress(37, 1024);
        QCOMPARE(one->percent(), 37UL);
        QCOMPARE(one->bytesPerSecond(), 1024UL);
        QVERIFY(one->suspend());
        QCOMPARE(one->state(), QueuedTransfer::State::Paused);
        QVERIFY(!second);
        QVERIFY(one->resume());
        first->finish();
        QCOMPARE(firstResult.size(), 1);
        QCOMPARE(one->state(), QueuedTransfer::State::Completed);
        QTRY_VERIFY(second);
        second->finish(KJob::UserDefinedError);
        QCOMPARE(two->state(), QueuedTransfer::State::Failed);
        QVERIFY(!two->failure().isEmpty());
        queue.clearFinished();
        QVERIFY(queue.entries().isEmpty());
    }

    void respectsChangingConcurrencyAndPausedTransfers()
    {
        TransferQueue queue;
        queue.setConcurrency(2);
        QList<FakeTransfer *> jobs;
        const auto factory = [&jobs] {
            auto *job = new FakeTransfer;
            jobs.append(job);
            return job;
        };
        auto *one = queue.enqueue(QStringLiteral("one"), factory);
        auto *two = queue.enqueue(QStringLiteral("two"), factory);
        auto *three = queue.enqueue(QStringLiteral("three"), factory);
        auto *four = queue.enqueue(QStringLiteral("four"), factory);
        QTRY_COMPARE(jobs.size(), 2);
        QCOMPARE(three->state(), QueuedTransfer::State::Waiting);
        QVERIFY(one->suspend());
        queue.setConcurrency(1);
        jobs.at(1)->finish();
        QCoreApplication::processEvents();
        QCOMPARE(jobs.size(), 2);
        QCOMPARE(two->state(), QueuedTransfer::State::Completed);
        QVERIFY(one->resume());
        jobs.at(0)->finish();
        QTRY_COMPARE(jobs.size(), 3);
        QCOMPARE(three->state(), QueuedTransfer::State::Running);
        QCOMPARE(four->state(), QueuedTransfer::State::Waiting);
        queue.setConcurrency(0);
        QTRY_COMPARE(jobs.size(), 4);
        QCOMPARE(four->state(), QueuedTransfer::State::Running);
        jobs.at(2)->finish();
        jobs.at(3)->finish();
    }

    void cancelsWaitingAndRunningJobs()
    {
        TransferQueue queue;
        FakeTransfer *running = nullptr;
        bool waitingStarted = false;
        auto *one = queue.enqueue(QStringLiteral("one"), [&] {
            running = new FakeTransfer;
            return running;
        });
        auto *two = queue.enqueue(QStringLiteral("two"), [&] {
            waitingStarted = true;
            return new FakeTransfer;
        });
        QTRY_VERIFY(running);
        QSignalSpy canceled(one, &KJob::result);
        QVERIFY(two->kill(KJob::EmitResult));
        QCOMPARE(two->state(), QueuedTransfer::State::Canceled);
        QVERIFY(one->kill(KJob::EmitResult));
        QCOMPARE(one->state(), QueuedTransfer::State::Canceled);
        QCOMPARE(canceled.size(), 1);
        QCoreApplication::processEvents();
        QVERIFY(!waitingStarted);
    }

    void handlesFactoryFailure()
    {
        TransferQueue queue;
        auto *entry = queue.enqueue(QStringLiteral("broken"), []() -> KJob * {
            return nullptr;
        });
        QSignalSpy result(entry, &KJob::result);
        QTRY_COMPARE(entry->state(), QueuedTransfer::State::Failed);
        QCOMPARE(result.size(), 1);
        QVERIFY(!entry->failure().isEmpty());
    }

    void reportsUnexpectedJobDestruction()
    {
        TransferQueue queue;
        FakeTransfer *running = nullptr;
        auto *entry = queue.enqueue(QStringLiteral("unexpected"), [&] {
            running = new FakeTransfer;
            return running;
        });
        QTRY_VERIFY(running);
        QSignalSpy result(entry, &KJob::result);
        delete running;
        QCOMPARE(entry->state(), QueuedTransfer::State::Failed);
        QCOMPARE(result.size(), 1);
        QVERIFY(!entry->failure().isEmpty());
    }

    void forwardsByteAmounts()
    {
        TransferQueue queue;
        FakeTransfer *running = nullptr;
        auto *entry = queue.enqueue(QStringLiteral("amounts"), [&] {
            running = new FakeTransfer;
            return running;
        });
        QTRY_VERIFY(running);
        running->amounts(4096, 1024);
        QCOMPARE(entry->totalAmount(KJob::Bytes), 4096ULL);
        QCOMPARE(entry->processedAmount(KJob::Bytes), 1024ULL);
        running->finish();
    }

    void pausesTheWholeQueue()
    {
        TransferQueue queue;
        queue.setConcurrency(1);
        FakeTransfer *first = nullptr;
        FakeTransfer *second = nullptr;
        auto *one = queue.enqueue(QStringLiteral("one"), [&] {
            first = new FakeTransfer;
            return first;
        });
        auto *two = queue.enqueue(QStringLiteral("two"), [&] {
            second = new FakeTransfer;
            return second;
        });
        QTRY_VERIFY(first);
        queue.setPaused(true);
        QVERIFY(queue.isPaused());
        QCOMPARE(one->state(), QueuedTransfer::State::Paused);

        // A transfer finishing while paused doesn't start the next one.
        first->finish();
        QCOMPARE(one->state(), QueuedTransfer::State::Completed);
        QTest::qWait(20);
        QVERIFY(!second);
        QCOMPARE(two->state(), QueuedTransfer::State::Waiting);

        queue.setPaused(false);
        QVERIFY(!queue.isPaused());
        QTRY_VERIFY(second);
        QCOMPARE(two->state(), QueuedTransfer::State::Running);
        second->finish();
    }

    void resumesSuspendedTransfers()
    {
        TransferQueue queue;
        FakeTransfer *running = nullptr;
        auto *entry = queue.enqueue(QStringLiteral("resume"), [&] {
            running = new FakeTransfer;
            return running;
        });
        QTRY_VERIFY(running);
        queue.setPaused(true);
        QCOMPARE(entry->state(), QueuedTransfer::State::Paused);
        queue.setPaused(false);
        QCOMPARE(entry->state(), QueuedTransfer::State::Running);
        running->finish();
    }
};

QTEST_GUILESS_MAIN(TransferQueueTest)
#include "transferqueuetest.moc"
