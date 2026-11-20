// SPDX-License-Identifier: GPL-3.0-or-later

#include "transferqueuedialog.h"
#include "fileoperations.h"
#include "settings.h"
#include "transferqueue.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
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

    void appliesConflictPolicyToQueuedTransfers()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("source")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        const QString sourcePath = root.filePath(QStringLiteral("source/item.txt"));
        const QString destinationPath = root.filePath(QStringLiteral("destination/item.txt"));
        const auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
        };
        const auto read = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        QVERIFY(write(sourcePath, "new"));
        QVERIFY(write(destinationPath, "existing"));
        const auto previous = Settings::transferConflictPolicy();
        const auto restore = qScopeGuard([previous] {
            Settings::setTransferConflictPolicy(previous);
            FileOperations::transferQueue()->clearFinished();
        });
        const auto source = QUrl::fromLocalFile(sourcePath);
        const auto destination = QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")));
        Settings::setTransferConflictPolicy(Settings::EnumTransferConflictPolicy::Skip);
        auto *skipped = FileOperations::copy({source}, destination, nullptr);
        QVERIFY(skipped);
        QSignalSpy skipResult(skipped, &KJob::result);
        QTRY_COMPARE_WITH_TIMEOUT(skipResult.size(), 1, 15000);
        QCOMPARE(skipped->error(), 0);
        QCOMPARE(read(destinationPath), QByteArray("existing"));
        Settings::setTransferConflictPolicy(Settings::EnumTransferConflictPolicy::Rename);
        auto *renamed = FileOperations::copy({source}, destination, nullptr);
        QVERIFY(renamed);
        QSignalSpy renameResult(renamed, &KJob::result);
        QTRY_COMPARE_WITH_TIMEOUT(renameResult.size(), 1, 15000);
        QCOMPARE(renamed->error(), 0);
        const QStringList entries = QDir(root.filePath(QStringLiteral("destination"))).entryList(QDir::Files);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(read(destinationPath), QByteArray("existing"));
        const QString renamedPath =
            root.filePath(QStringLiteral("destination/") + (entries.first() == QLatin1String("item.txt") ? entries.last() : entries.first()));
        QCOMPARE(read(renamedPath), QByteArray("new"));
        Settings::setTransferConflictPolicy(Settings::EnumTransferConflictPolicy::Overwrite);
        auto *overwritten = FileOperations::move({source}, destination, nullptr);
        QVERIFY(overwritten);
        QSignalSpy overwriteResult(overwritten, &KJob::result);
        QTRY_COMPARE_WITH_TIMEOUT(overwriteResult.size(), 1, 15000);
        QCOMPARE(overwritten->error(), 0);
        QCOMPARE(read(destinationPath), QByteArray("new"));
        QVERIFY(!QFile::exists(sourcePath));
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
