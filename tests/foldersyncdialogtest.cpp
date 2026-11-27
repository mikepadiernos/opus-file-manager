// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncdialog.h"
#include "settings.h"

#include <KSharedConfig>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class FolderSyncDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void comparesFiltersAndPersistsProfile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        QFile file(root.filePath(QStringLiteral("left/sub/test.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("profilesrc")), KConfig::SimpleConfig);
        {
            FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), nullptr, config);
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncInclude"))->setPlainText(QStringLiteral("*.txt"));
            dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncContents"))->setChecked(true);
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Text"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("folderSyncSaveProfile"))->click();
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("folderSyncProfiles"))->count(), 1);
        }

        config->reparseConfiguration();
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), nullptr, config);
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncLoadProfile"))->click();
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncInclude"))->toPlainText(), QStringLiteral("*.txt"));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncContents"))->isChecked());
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        QCOMPARE(rows->topLevelItemCount(), 2);
        QCOMPARE(rows->topLevelItem(1)->text(0), QStringLiteral("sub/test.txt"));
        QCOMPARE(rows->topLevelItem(1)->text(1), QStringLiteral("Only in first folder"));
        dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncExclude"))->setPlainText(QStringLiteral("sub"));
        QCOMPARE(rows->topLevelItemCount(), 0);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"))->isEnabled());
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QCOMPARE(rows->topLevelItemCount(), 0);
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text(), QStringLiteral("Compared 0 items."));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncDeleteProfile"))->click();
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("folderSyncProfiles"))->count(), 0);
    }

    void comparisonDefaultsAndProfiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("profilesrc")), KConfig::SimpleConfig);
        const bool recursive = Settings::folderSyncRecursive();
        const bool contents = Settings::folderSyncCompareContents();
        const bool links = Settings::folderSyncCompareLinks();
        const auto restore = qScopeGuard([recursive, contents, links] {
            Settings::setFolderSyncRecursive(recursive);
            Settings::setFolderSyncCompareContents(contents);
            Settings::setFolderSyncCompareLinks(links);
        });
        Settings::setFolderSyncRecursive(false);
        Settings::setFolderSyncCompareContents(true);
        Settings::setFolderSyncCompareLinks(true);
        FolderSync::Options options;
        options.recursive = true;
        options.compareContents = false;
        options.compareSymlinks = false;
        QString error;
        QVERIFY2(FolderSyncProfiles::save(config->group(QStringLiteral("FolderSyncProfiles")), {{QStringLiteral("Profile"), options}}, &error),
                 qPrintable(error));
        FolderSyncDialog dialog(root.path(), root.path(), nullptr, config);
        auto *recursion = dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncRecursive"));
        auto *content = dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncContents"));
        auto *link = dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncLinks"));
        QVERIFY(recursion && content && link);
        QVERIFY(!recursion->isChecked() && content->isChecked() && link->isChecked());
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncLoadProfile"))->click();
        QVERIFY(recursion->isChecked() && !content->isChecked() && !link->isChecked());
    }

    void transferPolicyShapesTheReviewedPlan()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("right")));
        const auto write = [](const QString &path, qint64 modified) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write("data") == 4 && file.flush()
                && file.setFileTime(QDateTime::fromSecsSinceEpoch(modified), QFileDevice::FileModificationTime);
        };
        QVERIFY(write(root.filePath(QStringLiteral("left/newer")), 1002));
        QVERIFY(write(root.filePath(QStringLiteral("right/newer")), 1001));
        QVERIFY(write(root.filePath(QStringLiteral("left/older")), 1000));
        QVERIFY(write(root.filePath(QStringLiteral("right/older")), 1001));
        QVERIFY(write(root.filePath(QStringLiteral("left/missing")), 1000));
        const auto oldPolicy = Settings::folderSyncCopyPolicy();
        const int oldSpeed = Settings::folderSyncSpeedLimitKiB();
        const auto restore = qScopeGuard([oldPolicy, oldSpeed] {
            Settings::setFolderSyncCopyPolicy(oldPolicy);
            Settings::setFolderSyncSpeedLimitKiB(oldSpeed);
        });
        Settings::setFolderSyncCopyPolicy(Settings::EnumFolderSyncCopyPolicy::NewerOnly);
        Settings::setFolderSyncSpeedLimitKiB(42);
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")));
        auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncCopyPolicy"));
        auto *speed = dialog.findChild<QSpinBox *>(QStringLiteral("folderSyncSpeedLimit"));
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        auto *action = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncAction"));
        QCOMPARE(policy->currentIndex(), int(FolderSync::CopyPolicy::NewerOnly));
        QCOMPARE(speed->value(), 42);
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QCOMPARE(rows->topLevelItemCount(), 3);
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncPlanToRight"))->click();
        const auto row = [rows](const QString &name) -> QTreeWidgetItem * {
            for (int i = 0; i < rows->topLevelItemCount(); ++i) {
                if (rows->topLevelItem(i)->text(0) == name) {
                    return rows->topLevelItem(i);
                }
            }
            return nullptr;
        };
        QVERIFY(row(QStringLiteral("newer")) && row(QStringLiteral("older")) && row(QStringLiteral("missing")));
        QVERIFY(row(QStringLiteral("newer"))->text(2).startsWith(QStringLiteral("Overwrite")));
        QCOMPARE(row(QStringLiteral("older"))->text(2), QStringLiteral("Skip"));
        QCOMPARE(row(QStringLiteral("missing"))->text(2), QStringLiteral("Copy first to second"));
        rows->setCurrentItem(row(QStringLiteral("older")));
        QCOMPARE(action->findData(int(FolderSync::Action::CopyToRight)), -1);
        policy->setCurrentIndex(int(FolderSync::CopyPolicy::SkipExisting));
        QCOMPARE(row(QStringLiteral("newer"))->text(2), QStringLiteral("Skip"));
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncPlanToRight"))->click();
        QCOMPARE(row(QStringLiteral("newer"))->text(2), QStringLiteral("Skip"));
        QCOMPARE(row(QStringLiteral("missing"))->text(2), QStringLiteral("Copy first to second"));
    }

    void appliesPerOperationSpeedLimit()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("right")));
        QFile source(root.filePath(QStringLiteral("left/item")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(QByteArray(8192, 'x')), qint64(8192));
        source.close();
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")));
        dialog.findChild<QSpinBox *>(QStringLiteral("folderSyncSpeedLimit"))->setValue(16);
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncPlanToRight"))->click();
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        QElapsedTimer timer;
        timer.start();
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"))->click();
        QTRY_VERIFY(compare->isEnabled());
        QVERIFY(timer.elapsed() >= 500);
        QFile result(root.filePath(QStringLiteral("right/item")));
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), QByteArray(8192, 'x'));
    }

    void reportsOverlappingRoots()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("sub")));
        FolderSyncDialog dialog(root.path(), root.filePath(QStringLiteral("sub")));
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text().contains(QStringLiteral("separate folders")));
    }

    void previewsConfirmsAndAppliesSelectedOperations()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        auto write = [](const QString &path, const QByteArray &contents) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
        };
        QVERIFY(write(root.filePath(QStringLiteral("left/a")), "new"));
        QVERIFY(write(root.filePath(QStringLiteral("right/a")), "old"));
        QVERIFY(write(root.filePath(QStringLiteral("left/sub/item")), "nested"));
        QVERIFY(write(root.filePath(QStringLiteral("right/remove")), "remove"));
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")));
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"));
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        auto *action = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncAction"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QCOMPARE(rows->topLevelItemCount(), 4);
        auto selectAction = [&](const QString &path, FolderSync::Action value) {
            for (int i = 0; i < rows->topLevelItemCount(); ++i) {
                if (rows->topLevelItem(i)->text(0) == path) {
                    rows->setCurrentItem(rows->topLevelItem(i));
                    const int index = action->findData(int(value));
                    if (index >= 0) {
                        action->setCurrentIndex(index);
                        return true;
                    }
                }
            }
            return false;
        };
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncPlanToRight"))->click();
        QVERIFY(selectAction(QStringLiteral("remove"), FolderSync::Action::DeleteRight));
        QVERIFY(selectAction(QStringLiteral("sub/item"), FolderSync::Action::Skip));
        QVERIFY(selectAction(QStringLiteral("sub/item"), FolderSync::Action::CopyToRight));
        QCOMPARE(rows->topLevelItem(2)->text(2), QStringLiteral("Create folder in second"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            QVERIFY(box->text().contains(QStringLiteral("One overwrite")));
            QVERIFY(box->text().contains(QStringLiteral("One deletion")));
            QVERIFY(box->detailedText().contains(QStringLiteral("Overwrite in second from first: a")));
            QVERIFY(box->detailedText().contains(QStringLiteral("Delete from second: remove")));
            box->button(QMessageBox::No)->click();
        });
        apply->click();
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("right/remove"))));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/sub/item"))));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        apply->click();
        QTRY_VERIFY(compare->isEnabled());
        QVERIFY(!apply->isEnabled());
        QFile updated(root.filePath(QStringLiteral("right/a")));
        QVERIFY(updated.open(QIODevice::ReadOnly));
        QCOMPARE(updated.readAll(), QByteArray("new"));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/remove"))));
        QFile nested(root.filePath(QStringLiteral("right/sub/item")));
        QVERIFY(nested.open(QIODevice::ReadOnly));
        QCOMPARE(nested.readAll(), QByteArray("nested"));
        const QString report = dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncReport"))->toPlainText();
        QVERIFY(report.contains(QStringLiteral("Completed 4 operations.")));
        QVERIFY(report.contains(QStringLiteral("sub/item")));
    }

    void reportsChangedDestinationAndRequiresFreshComparison()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        QFile source(root.filePath(QStringLiteral("left/item")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("source"), 6);
        source.close();
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")));
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"));
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        auto *action = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncAction"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        rows->setCurrentItem(rows->topLevelItem(0));
        action->setCurrentIndex(action->findData(int(FolderSync::Action::CopyToRight)));
        QFile destination(root.filePath(QStringLiteral("right/item")));
        QVERIFY(destination.open(QIODevice::WriteOnly));
        QCOMPARE(destination.write("unexpected"), 10);
        destination.close();
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        apply->click();
        QTRY_VERIFY(compare->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncReport"))->toPlainText().contains(QStringLiteral("changed")));
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("unexpected"));
    }

    void offersRsyncBackend()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        FolderSyncDialog dialog(root.path(), root.path());
        auto *backend = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
        QVERIFY(backend);
        QCOMPARE(backend->count(), 2);
        QCOMPARE(backend->currentData().toString(), QStringLiteral("builtin"));
        auto *model = qobject_cast<QStandardItemModel *>(backend->model());
        QVERIFY(model->item(0)->isEnabled());
        QCOMPARE(model->item(1)->isEnabled(), !RsyncSync::program().isEmpty());
    }

    void gatesRemoteLocations()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QUrl local = QUrl::fromLocalFile(root.path());
        {
            FolderSyncDialog dialog(QUrl(QStringLiteral("sftp://me@host/srv")), local);
            auto *backend = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
            auto *model = qobject_cast<QStandardItemModel *>(backend->model());
            QVERIFY(!model->item(0)->isEnabled());
            QCOMPARE(backend->currentData().toString(), QStringLiteral("rsync"));
            QCOMPARE(dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"))->isEnabled(), !RsyncSync::program().isEmpty());
        }
        for (const auto &[left, right] : {std::pair{QUrl(QStringLiteral("sftp://-oProxyCommand=x/srv")), local},
                                          std::pair{QUrl(QStringLiteral("sftp://host/a")), QUrl(QStringLiteral("fish://host/b"))}}) {
            FolderSyncDialog dialog(left, right);
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"))->isEnabled());
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text().isEmpty());
        }
    }

    void synchronizesWithRsync()
    {
        if (RsyncSync::program().isEmpty()) {
            QSKIP("rsync is not installed");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        auto write = [](const QString &path, const QByteArray &contents) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
        };
        QVERIFY(write(root.filePath(QStringLiteral("left/a")), "newer"));
        QVERIFY(write(root.filePath(QStringLiteral("right/a")), "old"));
        QVERIFY(write(root.filePath(QStringLiteral("left/sub/item")), "nested"));
        QVERIFY(write(root.filePath(QStringLiteral("right/remove")), "remove"));
        const auto oldPolicy = Settings::rsyncCopyPolicy();
        const bool oldDeletions = Settings::rsyncAllowDeletions();
        const auto restore = qScopeGuard([oldPolicy, oldDeletions] {
            Settings::setRsyncCopyPolicy(oldPolicy);
            Settings::setRsyncAllowDeletions(oldDeletions);
        });
        Settings::setRsyncCopyPolicy(Settings::EnumRsyncCopyPolicy::All);
        Settings::setRsyncAllowDeletions(true);
        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")));
        auto *backend = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"));
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        auto *action = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncAction"));
        auto *report = dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncReport"));
        backend->setCurrentIndex(backend->findData(QStringLiteral("rsync")));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QCOMPARE(rows->topLevelItemCount(), 4);
        const auto select = [&](const QString &path, FolderSync::Action value) {
            for (int i = 0; i < rows->topLevelItemCount(); ++i) {
                if (rows->topLevelItem(i)->text(0) == path) {
                    rows->setCurrentItem(rows->topLevelItem(i));
                    const int index = action->findData(int(value));
                    action->setCurrentIndex(index);
                    return index >= 0;
                }
            }
            return false;
        };

        // rsync works in one direction per run.
        QVERIFY(select(QStringLiteral("a"), FolderSync::Action::CopyToRight));
        QVERIFY(select(QStringLiteral("remove"), FolderSync::Action::CopyToLeft));
        apply->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text().contains(QStringLiteral("one direction")));

        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncPlanToRight"))->click();
        QVERIFY(select(QStringLiteral("remove"), FolderSync::Action::DeleteRight));
        answerWhenShown(QMessageBox::No, [](QMessageBox *box) {
            QVERIFY(box->text().contains(QStringLiteral("with rsync")));
            QVERIFY(box->text().contains(QStringLiteral("One new file")));
            QVERIFY(box->text().contains(QStringLiteral("One new folder")));
            QVERIFY(box->text().contains(QStringLiteral("One overwrite")));
            QVERIFY(box->text().contains(QStringLiteral("One deletion")));
            QVERIFY(box->detailedText().contains(QStringLiteral("*deleting   remove")));
        });
        apply->click();
        QTRY_VERIFY(apply->isEnabled());
        QTRY_VERIFY(!QApplication::activeModalWidget());
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("right/remove"))));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/sub/item"))));

        answerWhenShown(QMessageBox::Yes);
        apply->click();
        QTRY_VERIFY(!report->isHidden());
        QVERIFY(compare->isEnabled());
        QVERIFY(!apply->isEnabled());
        QFile updated(root.filePath(QStringLiteral("right/a")));
        QVERIFY(updated.open(QIODevice::ReadOnly));
        QCOMPARE(updated.readAll(), QByteArray("newer"));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("right/remove"))));
        QFile nested(root.filePath(QStringLiteral("right/sub/item")));
        QVERIFY(nested.open(QIODevice::ReadOnly));
        QCOMPARE(nested.readAll(), QByteArray("nested"));
        QVERIFY2(report->toPlainText().contains(QStringLiteral("rsync changed 4 items.")), qPrintable(report->toPlainText()));
    }

    void startsFromTheRsyncDefaults()
    {
        if (RsyncSync::program().isEmpty()) {
            QSKIP("rsync is not installed");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right")));
        const auto write = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write("data") == 4;
        };
        QVERIFY(write(root.filePath(QStringLiteral("left/a"))));
        QVERIFY(write(root.filePath(QStringLiteral("right/b"))));
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("profilesrc")), KConfig::SimpleConfig);
        const QString id = QStringLiteral("0f8fad5b-d9cb-469f-a165-70867728950e");
        FolderSync::Options options;
        options.includePatterns = {QStringLiteral("*.txt")};
        options.compareContents = true;
        QString error;
        QVERIFY2(FolderSyncProfiles::save(config->group(QStringLiteral("FolderSyncProfiles")),
                                          {{QStringLiteral("Other"), {}, {}}, {QStringLiteral("Text"), options, id}},
                                          &error),
                 qPrintable(error));
        const auto restore = qScopeGuard(&FolderSyncDialogTest::restoreRsyncDefaults);
        const auto oldPolicy = Settings::folderSyncCopyPolicy();
        const auto restorePolicy = qScopeGuard([oldPolicy] {
            Settings::setFolderSyncCopyPolicy(oldPolicy);
        });
        Settings::setFolderSyncCopyPolicy(Settings::EnumFolderSyncCopyPolicy::All);
        Settings::setRsyncForLocalFolders(true);
        Settings::setRsyncProfile(id);
        Settings::setRsyncCopyPolicy(Settings::EnumRsyncCopyPolicy::SkipExisting);

        FolderSyncDialog dialog(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), nullptr, config);
        auto *backend = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
        auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncCopyPolicy"));
        auto *include = dialog.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncInclude"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"));
        QCOMPARE(backend->currentData().toString(), QStringLiteral("rsync"));
        QCOMPARE(policy->currentIndex(), int(FolderSync::CopyPolicy::SkipExisting));
        QCOMPARE(include->toPlainText(), QStringLiteral("*.txt"));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("folderSyncContents"))->isChecked());
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("folderSyncProfiles"))->currentText(), QStringLiteral("Text"));
        QVERIFY2(status->text().contains(QStringLiteral("Text")), qPrintable(status->text()));

        // Each backend starts from its own copy policy.
        backend->setCurrentIndex(backend->findData(QStringLiteral("builtin")));
        QCOMPARE(policy->currentIndex(), int(FolderSync::CopyPolicy::All));
        backend->setCurrentIndex(backend->findData(QStringLiteral("rsync")));
        QCOMPARE(policy->currentIndex(), int(FolderSync::CopyPolicy::SkipExisting));

        // Deleting with rsync is turned off by default. Both backends now share
        // a copy policy, so switching keeps the plan.
        Settings::setFolderSyncCopyPolicy(Settings::EnumFolderSyncCopyPolicy::SkipExisting);
        include->clear();
        auto *compare = dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"));
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("folderSyncResults"));
        auto *action = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncAction"));
        compare->click();
        QTRY_VERIFY(compare->isEnabled());
        QCOMPARE(rows->topLevelItemCount(), 2);
        QCOMPARE(rows->topLevelItem(1)->text(0), QStringLiteral("b"));
        rows->setCurrentItem(rows->topLevelItem(1));
        QCOMPARE(action->findData(int(FolderSync::Action::DeleteRight)), -1);
        backend->setCurrentIndex(backend->findData(QStringLiteral("builtin")));
        action->setCurrentIndex(action->findData(int(FolderSync::Action::DeleteRight)));
        QCOMPARE(rows->topLevelItem(1)->text(2), QStringLiteral("Delete from second"));
        backend->setCurrentIndex(backend->findData(QStringLiteral("rsync")));
        QCOMPARE(rows->topLevelItem(1)->text(2), QStringLiteral("Skip"));
        QVERIFY2(status->text().contains(QStringLiteral("skipped")), qPrintable(status->text()));

        // A plan made while deletions were allowed is refused once they are not.
        Settings::setRsyncAllowDeletions(true);
        rows->setCurrentItem(rows->topLevelItem(0));
        rows->setCurrentItem(rows->topLevelItem(1));
        action->setCurrentIndex(action->findData(int(FolderSync::Action::DeleteRight)));
        QCOMPARE(rows->topLevelItem(1)->text(2), QStringLiteral("Delete from second"));
        Settings::setRsyncAllowDeletions(false);
        dialog.findChild<QPushButton *>(QStringLiteral("folderSyncApply"))->click();
        QVERIFY2(status->text().contains(QStringLiteral("turned off")), qPrintable(status->text()));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("right/b"))));

        // A deleted profile is reported rather than loaded.
        Settings::setRsyncProfile(QStringLiteral("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
        FolderSyncDialog missing(root.filePath(QStringLiteral("left")), root.filePath(QStringLiteral("right")), nullptr, config);
        QVERIFY(missing.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncInclude"))->toPlainText().isEmpty());
        QVERIFY(missing.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text().contains(QStringLiteral("no longer exists")));
    }

    void turnsOffTheRsyncBackend()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto restore = qScopeGuard(&FolderSyncDialogTest::restoreRsyncDefaults);
        Settings::setRsyncEnabled(false);
        Settings::setRsyncForLocalFolders(true);
        {
            FolderSyncDialog dialog(root.path(), root.path());
            auto *backend = dialog.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
            auto *model = qobject_cast<QStandardItemModel *>(backend->model());
            QCOMPARE(backend->currentData().toString(), QStringLiteral("builtin"));
            QVERIFY(!model->item(1)->isEnabled());
            QVERIFY(model->item(1)->toolTip().contains(QStringLiteral("turned off")));
        }
        FolderSyncDialog dialog(QUrl(QStringLiteral("sftp://me@host/srv")), QUrl::fromLocalFile(root.path()));
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"))->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("folderSyncStatus"))->text().contains(QStringLiteral("turned off")));
    }

private:
    static void restoreRsyncDefaults()
    {
        for (const QString &name : {QStringLiteral("RsyncEnabled"),
                                    QStringLiteral("RsyncForLocalFolders"),
                                    QStringLiteral("RsyncProfile"),
                                    QStringLiteral("RsyncCopyPolicy"),
                                    QStringLiteral("RsyncAllowDeletions")}) {
            Settings::self()->findItem(name)->setDefault();
        }
    }
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

QTEST_MAIN(FolderSyncDialogTest)
#include "foldersyncdialogtest.moc"
