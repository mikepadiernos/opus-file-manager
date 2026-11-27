// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudactivitydialog.h"

#include "cloudactivity.h"
#include "cloudsyncrunner.h"
#include "remoteprofiles.h"
#include "transferqueue.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

namespace
{
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

QTreeWidgetItem *findRow(QTreeWidget *tree, const QString &text)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0).contains(text)) {
            return tree->topLevelItem(i);
        }
    }
    return nullptr;
}

/// Clicks the button named @p name in the upload wait dialog once it shows.
void clickWhenShown(const QString &name, bool *shown)
{
    auto *timer = new QTimer;
    timer->setInterval(20);
    QObject::connect(timer, &QTimer::timeout, [timer, name, shown] {
        auto *dialog = qobject_cast<CloudUploadWaitDialog *>(QApplication::activeModalWidget());
        if (!dialog) {
            return;
        }
        timer->stop();
        timer->deleteLater();
        *shown = true;
        dialog->findChild<QPushButton *>(name)->click();
    });
    timer->start();
}
}

class CloudActivityDialogTest : public QObject
{
    Q_OBJECT

private:
    struct Fixture {
        QTemporaryDir dir;
        KSharedConfig::Ptr config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("opusrc")), KConfig::SimpleConfig);
        TransferQueue queue;
        CloudSync::Scheduler scheduler{config};
        CloudActivity::Monitor monitor;
        QString cacheDir = dir.filePath(QStringLiteral("cache"));

        Fixture()
        {
            monitor.setTransferQueue(&queue);
            monitor.setScheduler(&scheduler);
        }

        CloudActivityDialog::Sources sources()
        {
            CloudActivityDialog::Sources sources;
            sources.monitor = &monitor;
            sources.queue = &queue;
            sources.scheduler = &scheduler;
            sources.config = config;
            sources.cacheDir = cacheDir;
            return sources;
        }

        CloudSync::Pair addPair(const QString &remote)
        {
            CloudSync::Pair pair;
            pair.id = CloudSync::newId();
            pair.localPath = dir.filePath(QStringLiteral("local-") + remote);
            pair.rcloneRemote = remote + QStringLiteral(":Documents");
            QList<CloudSync::Pair> pairs = scheduler.pairs();
            pairs.append(pair);
            QString error;
            if (!scheduler.setPairs(pairs, &error)) {
                qWarning() << error;
            }
            return pair;
        }
    };

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void listsConnectionsWithPendingUploads()
    {
        Fixture fixture;
        QVERIFY(writeMetadata(fixture.cacheDir, QStringLiteral("box/report.odt"), true, 2048));
        fixture.addPair(QStringLiteral("drive"));
        CloudActivityDialog dialog(fixture.sources());
        auto *connections = dialog.findChild<QTreeWidget *>(QStringLiteral("cloudActivityConnections"));
        QVERIFY(connections);
        QTreeWidgetItem *box = findRow(connections, QStringLiteral("box"));
        QVERIFY(box);
        QCOMPARE(box->text(2), QStringLiteral("Not mounted"));
        QVERIFY2(box->text(4).startsWith(QStringLiteral("1 file")), qPrintable(box->text(4)));
        QTreeWidgetItem *pair = findRow(connections, QStringLiteral("drive:Documents"));
        QVERIFY(pair);
        QCOMPARE(pair->text(1), QStringLiteral("Sync pair"));
        QCOMPARE(pair->text(2), QStringLiteral("Not run yet"));
    }

    void showsAndClearsErrors()
    {
        Fixture fixture;
        CloudActivityDialog dialog(fixture.sources());
        auto *errors = dialog.findChild<QTreeWidget *>(QStringLiteral("cloudActivityErrors"));
        QCOMPARE(errors->topLevelItemCount(), 0);
        fixture.monitor.addEvent(QStringLiteral("drive"), QStringLiteral("first"));
        fixture.monitor.addEvent(QStringLiteral("box"), QStringLiteral("second"));
        QCOMPARE(errors->topLevelItemCount(), 2);
        // The newest error comes first.
        QCOMPARE(errors->topLevelItem(0)->text(1), QStringLiteral("box"));
        QCOMPARE(errors->topLevelItem(0)->text(2), QStringLiteral("second"));
        dialog.findChild<QPushButton *>(QStringLiteral("cloudActivityClearErrors"))->click();
        QCOMPARE(errors->topLevelItemCount(), 0);
        QVERIFY(fixture.monitor.events().isEmpty());
    }

    void savesLimits()
    {
        Fixture fixture;
        CloudActivityDialog dialog(fixture.sources());
        QSignalSpy changed(&dialog, &CloudActivityDialog::limitsChanged);
        auto *bandwidth = dialog.findChild<QSpinBox *>(QStringLiteral("cloudActivityBandwidth"));
        QCOMPARE(bandwidth->value(), 0);
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("cloudActivityParallel"))->value(), 4);
        bandwidth->setValue(512);
        auto *cacheAge = dialog.findChild<QSpinBox *>(QStringLiteral("cloudActivityCacheAge"));
        QCOMPARE(cacheAge->value(), 1);
        QVERIFY(!cacheAge->isEnabled());
        dialog.findChild<QSpinBox *>(QStringLiteral("cloudActivityCache"))->setValue(2048);
        QVERIFY(cacheAge->isEnabled());
        cacheAge->setValue(24);
        QCOMPARE(changed.size(), 3);
        const auto limits = changed.last().first().value<CloudActivity::Limits>();
        QCOMPARE(limits.bandwidthKiB, 512);
        QCOMPARE(limits.cacheMiB, 2048);
        QCOMPARE(limits.cacheMaxAgeHours, 24);
        const CloudActivity::Limits saved = CloudActivity::loadLimits(fixture.config->group(QStringLiteral("CloudActivity")));
        QCOMPARE(saved, limits);

        // Limits changed elsewhere, such as in Preferences, are shown without saving them again.
        CloudActivity::Limits elsewhere;
        elsewhere.parallelTransfers = 2;
        CloudActivity::saveLimits(fixture.config->group(QStringLiteral("CloudActivity")), elsewhere);
        dialog.reloadLimits();
        QCOMPARE(changed.size(), 3);
        QCOMPARE(bandwidth->value(), 0);
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("cloudActivityParallel"))->value(), 2);
        QCOMPARE(cacheAge->value(), 1);
        QVERIFY(!cacheAge->isEnabled());
    }

    void pausesTransfersAndBackgroundRuns()
    {
        Fixture fixture;
        CloudActivityDialog dialog(fixture.sources());
        auto *pause = dialog.findChild<QPushButton *>(QStringLiteral("cloudActivityPause"));
        QCOMPARE(pause->text(), QStringLiteral("Pause All"));
        pause->click();
        QVERIFY(fixture.queue.isPaused());
        QVERIFY(fixture.scheduler.isBackgroundPaused());
        QCOMPARE(pause->text(), QStringLiteral("Resume All"));
        pause->click();
        QVERIFY(!fixture.queue.isPaused());
        QVERIFY(!fixture.scheduler.isBackgroundPaused());
        // Pausing elsewhere updates the button.
        fixture.monitor.setPaused(true);
        QVERIFY(pause->isChecked());
        fixture.monitor.setPaused(false);
    }

    void exportsAndImportsDefinitions()
    {
        Fixture source;
        RemoteProfiles::Profile profile;
        profile.name = QStringLiteral("Work server");
        profile.protocol = RemoteProfiles::Protocol::Sftp;
        profile.host = QStringLiteral("files.example.org");
        profile.user = QStringLiteral("me");
        QList<RemoteProfiles::Profile> profiles{profile};
        QString error;
        QVERIFY2(RemoteProfiles::save(source.config->group(QStringLiteral("RemoteProfiles")), profiles, &error), qPrintable(error));
        source.addPair(QStringLiteral("drive"));
        CloudActivityDialog exporter(source.sources());
        const QString path = source.dir.filePath(QStringLiteral("definitions.opusdefs"));
        QVERIFY(exporter.exportTo(path));
        QVERIFY(exporter.findChild<QLabel *>(QStringLiteral("cloudActivityMessage"))->text().contains(QStringLiteral("Passwords are not included")));

        Fixture target;
        CloudActivityDialog importer(target.sources());
        QVERIFY(importer.importFrom(path));
        const QList<RemoteProfiles::Profile> imported = RemoteProfiles::load(target.config->group(QStringLiteral("RemoteProfiles")));
        QCOMPARE(imported.size(), 1);
        QCOMPARE(imported.first().host, QStringLiteral("files.example.org"));
        QCOMPARE(target.scheduler.pairs().size(), 1);
        QVERIFY(target.scheduler.pairs().first().paused);
        const QString message = importer.findChild<QLabel *>(QStringLiteral("cloudActivityMessage"))->text();
        QVERIFY2(message.contains(QStringLiteral("Imported 1 connections and 1 sync pairs.")), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("sign in again")), qPrintable(message));
        QVERIFY(findRow(importer.findChild<QTreeWidget *>(QStringLiteral("cloudActivityConnections")), QStringLiteral("drive:Documents")));

        // Importing again skips what already exists.
        QVERIFY(importer.importFrom(path));
        QCOMPARE(RemoteProfiles::load(target.config->group(QStringLiteral("RemoteProfiles"))).size(), 1);
        QCOMPARE(target.scheduler.pairs().size(), 1);
        QVERIFY(importer.findChild<QLabel *>(QStringLiteral("cloudActivityMessage"))->text().contains(QStringLiteral("already exists")));

        QVERIFY(!importer.importFrom(source.dir.filePath(QStringLiteral("missing.opusdefs"))));
    }

    void uploadWaitSkipsWhenIdle()
    {
        int calls = 0;
        QVERIFY(CloudUploadWaitDialog::wait(nullptr, QStringLiteral("Waiting"), [&calls] {
            ++calls;
            return CloudActivity::UploadStatus();
        }));
        QCOMPARE(calls, 1);
        QVERIFY(QApplication::topLevelWidgets().isEmpty() || !QApplication::activeModalWidget());
    }

    void uploadWaitClosesWhenUploadsFinish()
    {
        CloudActivity::UploadStatus status;
        status.uploads = {2, 4096};
        CloudUploadWaitDialog dialog(QStringLiteral("Waiting"), [&status] {
            return status;
        });
        dialog.setPollInterval(10);
        QSignalSpy finished(&dialog, &QDialog::finished);
        dialog.open();
        auto *label = dialog.findChild<QLabel *>(QStringLiteral("cloudUploadStatus"));
        QVERIFY2(label->text().contains(QStringLiteral("2 files")), qPrintable(label->text()));
        status.uploads = {1, 1024};
        status.syncRuns = 1;
        QTRY_VERIFY(label->text().contains(QStringLiteral("1 cloud sync run")));
        status = {};
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void uploadWaitCanContinueOrCancel()
    {
        CloudActivity::UploadStatus status;
        status.syncRuns = 1;
        const auto provider = [&status] {
            return status;
        };
        bool shown = false;
        clickWhenShown(QStringLiteral("cloudUploadContinue"), &shown);
        QVERIFY(CloudUploadWaitDialog::wait(nullptr, QStringLiteral("Waiting"), provider));
        QVERIFY(shown);

        shown = false;
        clickWhenShown(QStringLiteral("cloudUploadCancel"), &shown);
        QVERIFY(!CloudUploadWaitDialog::wait(nullptr, QStringLiteral("Waiting"), provider));
        QVERIFY(shown);
    }
};

QTEST_MAIN(CloudActivityDialogTest)
#include "cloudactivitydialogtest.moc"
