// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncdialog.h"

#include <KSharedConfig>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
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
};

QTEST_MAIN(FolderSyncDialogTest)
#include "foldersyncdialogtest.moc"
