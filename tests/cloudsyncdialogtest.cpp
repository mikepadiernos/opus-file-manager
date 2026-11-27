// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsyncdialog.h"

#include <KSharedConfig>
#include <KUrlRequester>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

#include <functional>

using namespace CloudSync;

namespace
{
bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class CloudSyncDialogTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *m_dir = nullptr;
    KSharedConfig::Ptr m_config;

    QString path(const QString &name) const
    {
        return m_dir->filePath(name);
    }

    EngineLocations emptyEngines() const
    {
        return {path(QStringLiteral("config")), path(QStringLiteral("state"))};
    }

    Pair makePair(const QString &local, const QString &remote) const
    {
        Pair pair;
        pair.id = newId();
        pair.localPath = path(local);
        pair.remote = QUrl::fromLocalFile(path(remote));
        QDir().mkpath(pair.localPath);
        QDir().mkpath(path(remote));
        return pair;
    }

    std::unique_ptr<Scheduler> scheduler() const
    {
        auto result = std::make_unique<Scheduler>(m_config);
        result->setMeteredCheck([] {
            return false;
        });
        const QString snapshots = path(QStringLiteral("snapshots"));
        result->setRunnerSetup([snapshots](Runner *runner) {
            runner->setUseTrash(false);
            runner->setSnapshotPath(snapshots + QLatin1Char('/') + runner->pair().id + QStringLiteral(".json"));
        });
        return result;
    }

    static QPushButton *button(QWidget *parent, const char *name)
    {
        return parent->findChild<QPushButton *>(QLatin1String(name));
    }

    template<typename Dialog>
    static Dialog *openDialog(QWidget *parent)
    {
        Dialog *found = nullptr;
        for (int i = 0; i < 3000 && !found; i += 20) {
            const auto dialogs = parent->findChildren<Dialog *>();
            for (Dialog *dialog : dialogs) {
                if (dialog->isVisible()) {
                    found = dialog;
                }
            }
            if (!found) {
                QTest::qWait(20);
            }
        }
        return found;
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
        m_config = KSharedConfig::openConfig(path(QStringLiteral("cloudsyncrc")), KConfig::SimpleConfig);
    }

    void cleanup()
    {
        m_config.reset();
        delete m_dir;
        m_dir = nullptr;
    }

    void pairDialogValidatesTheLocations()
    {
        CloudSyncPairDialog dialog(Pair{}, {}, m_config, nullptr);
        dialog.setEngineLocations(emptyEngines());
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        auto *error = dialog.findChild<QLabel *>(QStringLiteral("cloudSyncPairError"));
        QVERIFY(!ok->isEnabled());
        QVERIFY(!error->text().isEmpty());

        QDir().mkpath(path(QStringLiteral("local")));
        dialog.findChild<KUrlRequester *>(QStringLiteral("cloudSyncLocal"))->setText(path(QStringLiteral("local")));
        QVERIFY(!ok->isEnabled());
        dialog.findChild<QLineEdit *>(QStringLiteral("cloudSyncRemote"))->setText(QUrl::fromLocalFile(path(QStringLiteral("remote"))).toString());
        QVERIFY2(ok->isEnabled(), qPrintable(error->text()));

        // A malformed rclone location is rejected.
        dialog.findChild<QComboBox *>(QStringLiteral("cloudSyncKind"))->setCurrentIndex(1);
        QVERIFY(!ok->isEnabled());
        dialog.findChild<QLineEdit *>(QStringLiteral("cloudSyncRcloneRemote"))->setText(QStringLiteral("drive:Documents"));
        QVERIFY2(ok->isEnabled(), qPrintable(error->text()));
        Pair pair = dialog.pair();
        QCOMPARE(pair.rcloneRemote, QStringLiteral("drive:Documents"));
        QVERIFY(pair.remote.isEmpty());
        QCOMPARE(pair.localPath, path(QStringLiteral("local")));

        // Another client's marker file blocks the pair.
        QVERIFY(writeFile(path(QStringLiteral("local/.stfolder/.keep")), ""));
        dialog.setEngineLocations(emptyEngines());
        QVERIFY(!ok->isEnabled());
        QVERIFY2(error->text().contains(QStringLiteral("Syncthing")), qPrintable(error->text()));
    }

    void pairDialogEditsOptions()
    {
        auto group = m_config->group(QStringLiteral("FolderSyncProfiles"));
        FolderSync::Options saved;
        saved.excludePatterns = {QStringLiteral("*.tmp")};
        saved.recursive = false;
        QVERIFY(FolderSyncProfiles::save(group, {{QStringLiteral("No temporary files"), saved, QString()}}));

        const Pair original = makePair(QStringLiteral("local"), QStringLiteral("remote"));
        CloudSyncPairDialog dialog(original, {}, m_config, nullptr);
        dialog.setEngineLocations(emptyEngines());
        QCOMPARE(dialog.pair(), original);

        auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("cloudSyncMode"));
        auto *mirror = dialog.findChild<QCheckBox *>(QStringLiteral("cloudSyncMirror"));
        QVERIFY(!mirror->isEnabled());
        mode->setCurrentIndex(mode->findData(int(Mode::CloudBackup)));
        QVERIFY(mirror->isEnabled());
        mirror->setChecked(true);

        auto *profiles = dialog.findChild<QComboBox *>(QStringLiteral("cloudSyncProfiles"));
        QCOMPARE(profiles->count(), 2);
        profiles->setCurrentIndex(1);
        Q_EMIT profiles->activated(1);
        dialog.findChild<QSpinBox *>(QStringLiteral("cloudSyncInterval"))->setValue(30);
        dialog.findChild<QCheckBox *>(QStringLiteral("cloudSyncPaused"))->setChecked(true);

        const Pair edited = dialog.pair();
        QCOMPARE(edited.id, original.id);
        QCOMPARE(edited.mode, Mode::CloudBackup);
        QVERIFY(edited.mirrorDeletions);
        QCOMPARE(edited.options.excludePatterns, QStringList{QStringLiteral("*.tmp")});
        QVERIFY(!edited.options.recursive);
        QCOMPARE(edited.intervalMinutes, 30);
        QVERIFY(edited.paused);

        // Mirroring only applies to backups.
        mode->setCurrentIndex(mode->findData(int(Mode::Sync)));
        QVERIFY(!dialog.pair().mirrorDeletions);
    }

    void nestedPairIsExcludedFromTheOuterPair()
    {
        const Pair outer = makePair(QStringLiteral("docs"), QStringLiteral("remote-docs"));
        Pair inner = makePair(QStringLiteral("docs/work*"), QStringLiteral("remote-work"));
        CloudSyncPairDialog dialog(inner, {outer}, m_config, nullptr);
        dialog.setEngineLocations(emptyEngines());
        QString question;
        answerWhenShown(QMessageBox::Yes, [&question](QMessageBox *box) {
            question = box->text();
        });
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY2(question.contains(QStringLiteral("work*")), qPrintable(question));
        QVERIFY(dialog.outerExclusion() == std::make_optional(std::pair{outer.id, QStringLiteral("work[*]")}));
        QVERIFY(dialog.pair().options.excludePatterns.isEmpty());
    }

    void outerPairExcludesTheNestedPair()
    {
        const Pair inner = makePair(QStringLiteral("docs/work"), QStringLiteral("remote-work"));
        const Pair outer = makePair(QStringLiteral("docs"), QStringLiteral("remote-docs"));
        {
            CloudSyncPairDialog dialog(outer, {inner}, m_config, nullptr);
            dialog.setEngineLocations(emptyEngines());
            answerWhenShown(QMessageBox::Cancel);
            dialog.accept();
            QCOMPARE(dialog.result(), int(QDialog::Rejected));
        }
        {
            CloudSyncPairDialog dialog(outer, {inner}, m_config, nullptr);
            dialog.setEngineLocations(emptyEngines());
            answerWhenShown(QMessageBox::No);
            dialog.accept();
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QVERIFY(!dialog.outerExclusion());
            QVERIFY(dialog.pair().options.excludePatterns.isEmpty());
        }
        {
            CloudSyncPairDialog dialog(outer, {inner}, m_config, nullptr);
            dialog.setEngineLocations(emptyEngines());
            answerWhenShown(QMessageBox::Yes);
            dialog.accept();
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QVERIFY(!dialog.outerExclusion());
            QCOMPARE(dialog.pair().options.excludePatterns, QStringList{QStringLiteral("work")});
        }
    }

    void reviewDialogCollectsResolutions()
    {
        const Pair pair = makePair(QStringLiteral("local"), QStringLiteral("remote"));
        Report report;
        report.outcome = Report::Outcome::NeedsReview;
        report.message = QStringLiteral("This is the first synchronization.");
        report.preview = {{QStringLiteral("a.txt"), QStringLiteral("Upload"), false}, {QStringLiteral("old.txt"), QStringLiteral("Delete remotely"), true}};
        report.conflicts = {{QStringLiteral("doc.txt"), State{false, 4, 10}, State{false, 5, 20}},
                            {QStringLiteral("both.txt"), State{false, 1, 10}, State{false, 2, 20}},
                            {QStringLiteral("mixed"), State{true, 0, 0}, State{false, 3, 30}}};
        report.skipped = {QStringLiteral("remote.txt was changed remotely")};
        CloudSyncReviewDialog dialog(pair, report);

        auto *changes = dialog.findChild<QTreeWidget *>(QStringLiteral("cloudSyncChanges"));
        QCOMPARE(changes->topLevelItemCount(), 2);
        QVERIFY(changes->topLevelItem(0)->icon(0).isNull());
        QVERIFY(!changes->topLevelItem(1)->icon(0).isNull());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("cloudSyncChangesSummary"))->text().contains(QStringLiteral("1 item will be deleted")));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("cloudSyncSkipped")));
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("cloudSyncReviewMessage"))->text(), report.message);

        auto *conflicts = dialog.findChild<QTreeWidget *>(QStringLiteral("cloudSyncConflicts"));
        QCOMPARE(conflicts->topLevelItemCount(), 3);
        // A file and a folder cannot be resolved here.
        QVERIFY(!conflicts->itemWidget(conflicts->topLevelItem(2), 3));
        QCOMPARE(dialog.resolutions(),
                 (QHash<QString, Resolution>{{QStringLiteral("doc.txt"), Resolution::Skip}, {QStringLiteral("both.txt"), Resolution::Skip}}));

        auto *all = dialog.findChild<QComboBox *>(QStringLiteral("cloudSyncResolveAll"));
        all->setCurrentIndex(all->findData(int(Resolution::KeepBoth)));
        button(&dialog, "cloudSyncApplyAll")->click();
        auto *first = qobject_cast<QComboBox *>(conflicts->itemWidget(conflicts->topLevelItem(0), 3));
        first->setCurrentIndex(first->findData(int(Resolution::KeepRemote)));
        QCOMPARE(dialog.resolutions(),
                 (QHash<QString, Resolution>{{QStringLiteral("doc.txt"), Resolution::KeepRemote}, {QStringLiteral("both.txt"), Resolution::KeepBoth}}));
    }

    void managesPairsAndReviewsTheFirstRun()
    {
        auto sched = scheduler();
        CloudSyncDialog dialog(sched.get(), m_config, nullptr);
        dialog.show();
        auto *pairs = dialog.findChild<QTreeWidget *>(QStringLiteral("cloudSyncPairs"));
        QCOMPARE(pairs->topLevelItemCount(), 0);
        QVERIFY(!button(&dialog, "cloudSyncNow")->isEnabled());

        QDir().mkpath(path(QStringLiteral("local")));
        QDir().mkpath(path(QStringLiteral("remote")));
        QVERIFY(writeFile(path(QStringLiteral("local/a.txt")), "local"));
        QVERIFY(writeFile(path(QStringLiteral("remote/b.txt")), "remote"));

        button(&dialog, "cloudSyncAdd")->click();
        auto *editor = openDialog<CloudSyncPairDialog>(&dialog);
        QVERIFY(editor);
        editor->setEngineLocations(emptyEngines());
        editor->findChild<KUrlRequester *>(QStringLiteral("cloudSyncLocal"))->setText(path(QStringLiteral("local")));
        editor->findChild<QLineEdit *>(QStringLiteral("cloudSyncRemote"))->setText(QUrl::fromLocalFile(path(QStringLiteral("remote"))).toString());
        editor->findChild<QSpinBox *>(QStringLiteral("cloudSyncInterval"))->setValue(0);
        editor->accept();
        QCOMPARE(sched->pairs().size(), 1);
        QCOMPARE(pairs->topLevelItemCount(), 1);
        const QString id = sched->pairs().constFirst().id;
        QCOMPARE(pairs->topLevelItem(0)->text(0), path(QStringLiteral("local")));
        QCOMPARE(pairs->topLevelItem(0)->text(3), QStringLiteral("Not synchronized yet"));

        // The first run stops for review, which opens by itself.
        button(&dialog, "cloudSyncNow")->click();
        auto *review = openDialog<CloudSyncReviewDialog>(&dialog);
        QVERIFY(review);
        QVERIFY(sched->needsReview(id));
        QCOMPARE(review->findChild<QTreeWidget *>(QStringLiteral("cloudSyncChanges"))->topLevelItemCount(), 2);
        button(review, "cloudSyncConfirm")->click();
        QTRY_COMPARE_WITH_TIMEOUT(sched->lastReport(id).outcome, Report::Outcome::Succeeded, 60000);
        QTRY_COMPARE(sched->state(id), Scheduler::State::Idle);
        QCOMPARE(QFile(path(QStringLiteral("remote/a.txt"))).size(), 5);
        QCOMPARE(QFile(path(QStringLiteral("local/b.txt"))).size(), 6);
        QVERIFY2(pairs->topLevelItem(0)->text(3).startsWith(QStringLiteral("Synchronized")), qPrintable(pairs->topLevelItem(0)->text(3)));

        // Reviewing on request shows a preview without changing anything.
        QVERIFY(writeFile(path(QStringLiteral("local/c.txt")), "new"));
        button(&dialog, "cloudSyncReview")->click();
        QPointer<CloudSyncReviewDialog> preview = openDialog<CloudSyncReviewDialog>(&dialog);
        QVERIFY(preview);
        QCOMPARE(preview->findChild<QTreeWidget *>(QStringLiteral("cloudSyncChanges"))->topLevelItemCount(), 1);
        QVERIFY(!QFile::exists(path(QStringLiteral("remote/c.txt"))));
        preview->reject();
        QTRY_VERIFY(!preview);
        QVERIFY(!QFile::exists(path(QStringLiteral("remote/c.txt"))));

        // Pausing the pair and its account.
        button(&dialog, "cloudSyncPause")->click();
        QVERIFY(sched->pair(id)->paused);
        QVERIFY(pairs->topLevelItem(0)->text(3).startsWith(QStringLiteral("Paused")));
        QCOMPARE(button(&dialog, "cloudSyncPause")->text(), QStringLiteral("Resume"));
        button(&dialog, "cloudSyncPause")->click();
        QVERIFY(!sched->pair(id)->paused);
        button(&dialog, "cloudSyncPauseAccount")->click();
        QVERIFY(sched->isPaused(*sched->pair(id)));
        QVERIFY(pairs->topLevelItem(0)->text(3).startsWith(QStringLiteral("Account paused")));
        button(&dialog, "cloudSyncPauseAccount")->click();
        QVERIFY(!sched->isPaused(*sched->pair(id)));

        // Removing keeps both folders.
        button(&dialog, "cloudSyncRemove")->click();
        QVERIFY(sched->pairs().isEmpty());
        QCOMPARE(pairs->topLevelItemCount(), 0);
        QVERIFY(QFile::exists(path(QStringLiteral("local/a.txt"))));
        QVERIFY(QFile::exists(path(QStringLiteral("remote/a.txt"))));
    }

    void addingANestedPairExcludesItFromTheOuterPair()
    {
        auto sched = scheduler();
        const Pair outer = makePair(QStringLiteral("docs"), QStringLiteral("remote-docs"));
        QString error;
        QVERIFY2(sched->setPairs({outer}, &error), qPrintable(error));
        CloudSyncDialog dialog(sched.get(), m_config, nullptr);
        dialog.show();
        QDir().mkpath(path(QStringLiteral("docs/work")));

        button(&dialog, "cloudSyncAdd")->click();
        auto *editor = openDialog<CloudSyncPairDialog>(&dialog);
        QVERIFY(editor);
        editor->setEngineLocations(emptyEngines());
        editor->findChild<KUrlRequester *>(QStringLiteral("cloudSyncLocal"))->setText(path(QStringLiteral("docs/work")));
        editor->findChild<QLineEdit *>(QStringLiteral("cloudSyncRemote"))->setText(QUrl::fromLocalFile(path(QStringLiteral("remote-work"))).toString());
        answerWhenShown(QMessageBox::Yes);
        editor->accept();
        QCOMPARE(sched->pairs().size(), 2);
        QCOMPARE(sched->pair(outer.id)->options.excludePatterns, QStringList{QStringLiteral("work")});
    }

    void startAtLoginWritesTheAutostartEntry()
    {
        QFile::remove(autostartPath());
        auto sched = scheduler();
        CloudSyncDialog dialog(sched.get(), m_config, nullptr);
        auto *startAtLogin = dialog.findChild<QCheckBox *>(QStringLiteral("cloudSyncStartAtLogin"));
        QVERIFY(!startAtLogin->isChecked());
        startAtLogin->setChecked(true);
        QVERIFY(QFile::exists(autostartPath()));
        QVERIFY(startsAtLogin());
        startAtLogin->setChecked(false);
        QVERIFY(!QFile::exists(autostartPath()));

        auto *metered = dialog.findChild<QCheckBox *>(QStringLiteral("cloudSyncMetered"));
        QVERIFY(metered->isChecked());
        metered->setChecked(false);
        QVERIFY(!sched->pausesOnMeteredConnections());
    }

private:
    static void answerWhenShown(QMessageBox::StandardButton button, const std::function<void(QMessageBox *)> &check = {})
    {
        auto *timer = new QTimer;
        timer->setInterval(20);
        QObject::connect(timer, &QTimer::timeout, [timer, button, check] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) {
                return;
            }
            timer->stop();
            timer->deleteLater();
            if (check) {
                check(box);
            }
            box->button(button)->click();
        });
        timer->start();
    }
};

QTEST_MAIN(CloudSyncDialogTest)
#include "cloudsyncdialogtest.moc"
