// SPDX-License-Identifier: GPL-3.0-or-later

#include "adminmode.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class AdminModeUiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_folder = QUrl::fromLocalFile(m_dir.path());
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Notification Messages"));
    }

    void paneWarnsWhileActingAsAdministrator()
    {
        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(m_folder);
        auto *bar = pane->findChild<KMessageWidget *>(QStringLiteral("adminBar"));
        QVERIFY(bar);
        QVERIFY(bar->isHidden());
        QAction *action = window.actionCollection()->action(QStringLiteral("act_as_admin"));
        QVERIFY(action);
        QVERIFY(action->isEnabled());
        QVERIFY(!action->isChecked());

        pane->setUrl(AdminMode::toAdmin(m_folder));
        QVERIFY(!bar->isHidden());
        QVERIFY(action->isChecked());
        QCOMPARE(pane->saveState().tabs, QList<QUrl>{m_folder});

        auto *stop = bar->findChild<QAction *>(QStringLiteral("stopAdmin"));
        QVERIFY(stop);
        stop->trigger();
        QCOMPARE(pane->url(), m_folder);
        QVERIFY(bar->isHidden());
        QVERIFY(!action->isChecked());
    }

    void turningTheActionOffLeavesAdministratorMode()
    {
        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(AdminMode::toAdmin(m_folder));
        QAction *action = window.actionCollection()->action(QStringLiteral("act_as_admin"));
        QVERIFY(action->isChecked());
        action->trigger();
        QCOMPARE(pane->url(), m_folder);
        QVERIFY(!action->isChecked());
    }

    void onlyLocalFoldersCanActAsAdministrator()
    {
        MainWindow window;
        window.activePane()->setUrl(QUrl(QStringLiteral("trash:/")));
        QAction *action = window.actionCollection()->action(QStringLiteral("act_as_admin"));
        QVERIFY(!action->isEnabled());
        QVERIFY(!action->isChecked());
    }

    void reportsAMissingWorker()
    {
        if (AdminMode::isAvailable()) {
            QSKIP("kio-admin is installed");
        }
        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(m_folder);
        const QString text = answerNextDialog(QDialogButtonBox::Ok, [&window] {
            window.actionCollection()->action(QStringLiteral("act_as_admin"))->trigger();
        });
        QVERIFY2(text.contains(QStringLiteral("kio-admin")), qPrintable(text));
        QCOMPARE(pane->url(), m_folder);
        QVERIFY(!window.actionCollection()->action(QStringLiteral("act_as_admin"))->isChecked());
    }

    void cancellingTheWarningKeepsTheFolder()
    {
        if (!AdminMode::isAvailable()) {
            QSKIP("kio-admin is not installed");
        }
        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(m_folder);
        const QString text = answerNextDialog(QDialogButtonBox::Cancel, [&window] {
            window.actionCollection()->action(QStringLiteral("act_as_admin"))->trigger();
        });
        QVERIFY2(text.contains(QStringLiteral("administrator")), qPrintable(text));
        QCOMPARE(pane->url(), m_folder);
        QVERIFY(!window.actionCollection()->action(QStringLiteral("act_as_admin"))->isChecked());
    }

private:
    /// Runs @p trigger, answers the modal dialog it opens with @p button and returns the dialog's text.
    template<typename Trigger>
    QString answerNextDialog(QDialogButtonBox::StandardButton button, Trigger trigger)
    {
        QString text;
        bool answered = false;
        QTimer timer;
        timer.setInterval(50);
        connect(&timer, &QTimer::timeout, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || answered) {
                return;
            }
            const QList<QLabel *> labels = dialog->findChildren<QLabel *>();
            for (const QLabel *label : labels) {
                text += label->text() + QLatin1Char('\n');
            }
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QPushButton *answer = buttons ? buttons->button(button) : nullptr;
            answered = true;
            if (answer) {
                answer->click();
            } else {
                dialog->reject();
            }
        });
        timer.start();
        trigger();
        timer.stop();
        return text;
    }

    QTemporaryDir m_dir;
    QUrl m_folder;
};

QTEST_MAIN(AdminModeUiTest)
#include "adminmodeuitest.moc"
