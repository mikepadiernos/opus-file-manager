// SPDX-License-Identifier: GPL-3.0-or-later

#include "customcommanddialog.h"

#include <KSharedConfig>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class CustomCommandDialogTest : public QObject
{
    Q_OBJECT

    static void confirm()
    {
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void previewsAndRunsWithoutShellInterpolation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString program = QStandardPaths::findExecutable(QStringLiteral("printf"));
        QVERIFY(!program.isEmpty());
        const QString selected = root.filePath(QStringLiteral("literal; touch unwanted"));
        CustomCommandDialog dialog(root.path(), {selected});
        auto *executable = dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"));
        auto *arguments = dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"));
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"));
        auto *run = dialog.findChild<QPushButton *>(QStringLiteral("commandRun"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("commandStatus"));
        executable->setText(program);
        arguments->setPlainText(QStringLiteral("%s\\n\n{path}"));
        preview->click();
        QVERIFY(run->isEnabled());
        auto *rows = dialog.findChild<QTreeWidget *>(QStringLiteral("commandPreviewList"));
        QCOMPARE(rows->topLevelItemCount(), 4);
        QCOMPARE(rows->topLevelItem(3)->text(1), selected);
        confirm();
        run->click();
        QTRY_VERIFY(status->text().contains(QStringLiteral("exited with code 0")));
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandOutput"))->toPlainText(), selected + QLatin1Char('\n'));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("unwanted"))));
        QVERIFY(!run->isEnabled());
    }

    void savesDefaultsAndReportsFailedLaunch()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("commandsrc")), KConfig::SimpleConfig);
        {
            CustomCommandDialog dialog(root.path(), {}, nullptr, config);
            dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"))->setText(root.filePath(QStringLiteral("missing-program")));
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"))->setPlainText(QStringLiteral("{message}"));
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandDefaults"))->setPlainText(QStringLiteral("message=hello world"));
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Example"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("commandSavePreset"))->click();
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("commandPresets"))->count(), 1);
        }
        config->reparseConfiguration();
        CustomCommandDialog dialog(root.path(), {}, nullptr, config);
        dialog.findChild<QPushButton *>(QStringLiteral("commandLoadPreset"))->click();
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandDefaults"))->toPlainText(), QStringLiteral("message=hello world"));
        dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        auto *run = dialog.findChild<QPushButton *>(QStringLiteral("commandRun"));
        QVERIFY(run->isEnabled());
        confirm();
        run->click();
        QTRY_VERIFY(dialog.findChild<QLabel *>(QStringLiteral("commandStatus"))->text().contains(QStringLiteral("Could not start")));
    }

    void managesAndUpdatesPresetsWithoutRunning()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("commandsrc")), KConfig::SimpleConfig);
        {
            CustomCommandDialog dialog({}, {}, nullptr, config, true);
            dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"))->setText(QStringLiteral("/bin/echo"));
            auto *arguments = dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"));
            arguments->setPlainText(QStringLiteral("first"));
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Example"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("commandSavePreset"))->click();
            arguments->setPlainText(QStringLiteral("{dir}"));
            confirm();
            dialog.findChild<QPushButton *>(QStringLiteral("commandUpdatePreset"))->click();
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("commandStatus"))->text().contains(QStringLiteral("updated")));
            dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("commandRun"))->isEnabled());
            QVERIFY(dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandOutput"))->isHidden());
        }
        config->reparseConfiguration();
        CustomCommandDialog dialog(root.path(), {}, nullptr, config);
        dialog.findChild<QPushButton *>(QStringLiteral("commandLoadPreset"))->click();
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"))->toPlainText(), QStringLiteral("{dir}"));
        dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("commandRun"))->isEnabled());
    }

    void stopsRunningCommand()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString program = QStandardPaths::findExecutable(QStringLiteral("sleep"));
        QVERIFY(!program.isEmpty());
        CustomCommandDialog dialog(root.path(), {});
        dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"))->setText(program);
        dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"))->setPlainText(QStringLiteral("30"));
        dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        confirm();
        dialog.findChild<QPushButton *>(QStringLiteral("commandRun"))->click();
        auto *stop = dialog.findChild<QPushButton *>(QStringLiteral("commandStop"));
        QTRY_VERIFY(stop->isEnabled());
        stop->click();
        QTRY_VERIFY(dialog.findChild<QLabel *>(QStringLiteral("commandStatus"))->text().contains(QStringLiteral("crashed or was stopped")));
        QVERIFY(!stop->isEnabled());
    }

    void invalidatesPreviewAndHonorsDecline()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        CustomCommandDialog dialog(root.path(), {});
        auto *program = dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"));
        auto *arguments = dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"));
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"));
        auto *run = dialog.findChild<QPushButton *>(QStringLiteral("commandRun"));
        program->setText(QStandardPaths::findExecutable(QStringLiteral("printf")));
        arguments->setPlainText(QStringLiteral("{path}"));
        preview->click();
        QVERIFY(!run->isEnabled());
        arguments->setPlainText(QStringLiteral("hello"));
        preview->click();
        QVERIFY(run->isEnabled());
        arguments->setPlainText(QStringLiteral("changed"));
        QVERIFY(!run->isEnabled());
        preview->click();
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        run->click();
        QVERIFY(run->isEnabled());
        QVERIFY(dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandOutput"))->toPlainText().isEmpty());
    }

    void truncatesOutputAndReportsNonzeroExit()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString program = QStandardPaths::findExecutable(QStringLiteral("head"));
        QVERIFY(!program.isEmpty());
        CustomCommandDialog dialog(root.path(), {});
        dialog.findChild<QLineEdit *>(QStringLiteral("commandProgram"))->setText(program);
        auto *arguments = dialog.findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"));
        arguments->setPlainText(QStringLiteral("-c\n300000\n/dev/zero"));
        dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        confirm();
        dialog.findChild<QPushButton *>(QStringLiteral("commandRun"))->click();
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("commandStatus"));
        QTRY_VERIFY(status->text().contains(QStringLiteral("Output was truncated.")));
        arguments->setPlainText(QStringLiteral("-c\n1\n") + root.filePath(QStringLiteral("missing")));
        dialog.findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        confirm();
        dialog.findChild<QPushButton *>(QStringLiteral("commandRun"))->click();
        QTRY_VERIFY(status->text().contains(QStringLiteral("exited with code 1")));
    }
};

QTEST_MAIN(CustomCommandDialogTest)
#include "customcommanddialogtest.moc"
