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
};

QTEST_GUILESS_MAIN(TransferQueueTest)
#include "transferqueuetest.moc"
