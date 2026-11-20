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
};

QTEST_MAIN(FolderSyncDialogTest)
#include "foldersyncdialogtest.moc"
