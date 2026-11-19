// SPDX-License-Identifier: GPL-3.0-or-later

#include "transferqueuedialog.h"
#include "fileoperations.h"
#include "transferqueue.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class ControllableJob : public KJob
{
public:
    ControllableJob()
    {
        setCapabilities(Killable | Suspendable);
    }
    void start() override
    {
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

class TransferQueueDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void controlsSelectedJob()
    {
        TransferQueue queue;
        TransferQueueDialog dialog(&queue);
        auto *entry = queue.enqueue(QStringLiteral("example"), [] {
            return new ControllableJob;
        });
        QTRY_COMPARE(entry->state(), QueuedTransfer::State::Running);
        auto *list = dialog.findChild<QTreeWidget *>(QStringLiteral("transferQueueList"));
        QCOMPARE(list->topLevelItemCount(), 1);
        list->setCurrentItem(list->topLevelItem(0));
        auto *pause = dialog.findChild<QPushButton *>(QStringLiteral("transferQueuePause"));
        auto *resume = dialog.findChild<QPushButton *>(QStringLiteral("transferQueueResume"));
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("transferQueueCancel"));
        QVERIFY(pause->isEnabled());
        pause->click();
        QCOMPARE(entry->state(), QueuedTransfer::State::Paused);
        QVERIFY(resume->isEnabled());
        resume->click();
        QCOMPARE(entry->state(), QueuedTransfer::State::Running);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        cancel->click();
        QCOMPARE(entry->state(), QueuedTransfer::State::Canceled);
        dialog.findChild<QPushButton *>(QStringLiteral("transferQueueClear"))->click();
        QCOMPARE(list->topLevelItemCount(), 0);
    }

    void copiesThroughKio()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        const QString path = root.filePath(QStringLiteral("source.txt"));
        QFile source(path);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("queue contents");
        source.close();
        auto *job = FileOperations::copy({QUrl::fromLocalFile(path)}, QUrl::fromLocalFile(root.filePath(QStringLiteral("destination"))), nullptr);
        QVERIFY(job);
        auto *entry = qobject_cast<QueuedTransfer *>(job);
        QVERIFY(entry);
        QVERIFY(entry->description().contains(root.filePath(QStringLiteral("destination"))));
        QSignalSpy result(job, &KJob::result);
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 15000);
        QCOMPARE(job->error(), 0);
        QFile output(root.filePath(QStringLiteral("destination/source.txt")));
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), QByteArray("queue contents"));
        FileOperations::transferQueue()->clearFinished();
    }
};

QTEST_MAIN(TransferQueueDialogTest)
#include "transferqueuedialogtest.moc"
