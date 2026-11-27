// SPDX-License-Identifier: GPL-3.0-or-later

#include "sendbyemaildialog.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KMessageWidget>
#include <KService>
#include <KSharedConfig>
#include <KSycoca>
#include <KTar>

#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

namespace
{
const QString ImpresarioId = QStringLiteral("io.github.mikepadiernos.plasma_impresario.desktop");
}

class SendByEmailDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ksycoca_ms_between_checks = 0;
        QVERIFY(m_dir.isValid());
        m_applications = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        QDir(m_applications).removeRecursively();
        QVERIFY(QDir().mkpath(m_applications));
        QVERIFY(QDir().mkpath(QFileInfo(mimeAppsPath()).path()));
        QFile::remove(mimeAppsPath());
        KSycoca::self()->ensureCacheValid();
    }

    void init()
    {
        m_file = m_dir.filePath(QStringLiteral("report.txt"));
        QFile out(m_file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("report");
        m_output = m_dir.filePath(QStringLiteral("composer-arguments"));
        QFile::remove(m_output);
    }

    void reportsAMissingMailClient()
    {
        if (MailHandoff::preferredClient()) {
            QSKIP("A mail client is registered on this system");
        }
        SendByEmailDialog dialog({KFileItem(QUrl::fromLocalFile(m_file))});
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mailMessage"));
        QVERIFY(!message->isHidden());
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("mailCompose"))->isEnabled());
    }

    void attachesLocalFilesAndExplainsSkippedItems()
    {
        installClient(QStringLiteral("fakemail.desktop"), QStringLiteral("Fake Mail"));
        const QString folder = m_dir.filePath(QStringLiteral("folder"));
        QVERIFY(QDir().mkpath(folder));
        SendByEmailDialog dialog({KFileItem(QUrl::fromLocalFile(m_file)), KFileItem(QUrl::fromLocalFile(folder))});

        auto *items = dialog.findChild<QListWidget *>(QStringLiteral("mailItems"));
        QCOMPARE(items->count(), 2);
        QVERIFY(items->item(0)->text().contains(m_file));
        QVERIFY(!(items->item(1)->flags() & Qt::ItemIsEnabled));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("mailSummary"))->text().contains(QStringLiteral("skipped")));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("mailClient"))->text().contains(QStringLiteral("Fake Mail")));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("mailPrepareCopies"))->isHidden());
        QVERIFY(dialog.findChild<KMessageWidget *>(QStringLiteral("mailMessage"))->isHidden());

        auto *compose = dialog.findChild<QPushButton *>(QStringLiteral("mailCompose"));
        QVERIFY(compose->isEnabled());
        QSignalSpy finished(&dialog, &SendByEmailDialog::composeFinished);
        compose->click();
        QVERIFY(finished.wait(10000));
        QCOMPARE(finished.first().first().toString(), QString());
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mailMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("Fake Mail")));
        QVERIFY(!compose->isEnabled());

        const QString arguments = composerArguments();
        QVERIFY2(arguments.startsWith(QStringLiteral("mailto:")), qPrintable(arguments));
        QVERIFY2(arguments.contains(QStringLiteral("attach=")), qPrintable(arguments));
        QVERIFY2(arguments.contains(QStringLiteral("report.txt")), qPrintable(arguments));
        QVERIFY2(!arguments.contains(QStringLiteral("folder")), qPrintable(arguments));
    }

    void copiesRemoteFilesOnlyWithConsent()
    {
        installClient(QStringLiteral("fakemail.desktop"), QStringLiteral("Fake Mail"));
        const QUrl remote = archiveMember(QStringLiteral("remote.txt"));
        auto *dialog = new SendByEmailDialog({KFileItem(QUrl::fromLocalFile(m_file)), KFileItem(remote, QStringLiteral("text/plain"), S_IFREG)});
        auto *items = dialog->findChild<QListWidget *>(QStringLiteral("mailItems"));
        QCOMPARE(items->count(), 2);
        QVERIFY(items->item(1)->text().contains(QStringLiteral("local copy")));
        auto *consent = dialog->findChild<QCheckBox *>(QStringLiteral("mailPrepareCopies"));
        QVERIFY(!consent->isHidden());
        QVERIFY(consent->isEnabled());
        QVERIFY(!consent->isChecked());
        auto *summary = dialog->findChild<QLabel *>(QStringLiteral("mailSummary"));
        QVERIFY(summary->text().contains(QStringLiteral("skipped")));

        consent->setChecked(true);
        QVERIFY(!summary->text().contains(QStringLiteral("skipped")));
        QSignalSpy finished(dialog, &SendByEmailDialog::composeFinished);
        dialog->compose();
        QVERIFY(finished.wait(20000));
        QCOMPARE(finished.first().first().toString(), QString());

        const QString copies = dialog->copiesPath();
        QVERIFY(!copies.isEmpty());
        QCOMPARE(QFileInfo(copies).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions());
        const QString copy = copies + QStringLiteral("/1/remote.txt");
        QFile prepared(copy);
        QVERIFY(prepared.open(QIODevice::ReadOnly));
        QCOMPARE(prepared.readAll(), QByteArray("remote"));
        const QString arguments = composerArguments();
        QVERIFY2(arguments.contains(QStringLiteral("report.txt")), qPrintable(arguments));
        QVERIFY2(arguments.contains(QStringLiteral("remote.txt")), qPrintable(arguments));

        // The composer may read the copies later, so they outlive the dialog.
        delete dialog;
        QVERIFY(QFile::exists(copy));
        QVERIFY(QFile::exists(m_dir.filePath(QStringLiteral("remote.tar"))));
    }

    void reportsAFailedCopyWithoutOpeningTheComposer()
    {
        installClient(QStringLiteral("fakemail.desktop"), QStringLiteral("Fake Mail"));
        const QUrl missing = archiveMember(QStringLiteral("missing.txt"));
        SendByEmailDialog dialog({KFileItem(missing, QStringLiteral("text/plain"), S_IFREG)});
        auto *compose = dialog.findChild<QPushButton *>(QStringLiteral("mailCompose"));
        QVERIFY(!compose->isEnabled());
        dialog.findChild<QCheckBox *>(QStringLiteral("mailPrepareCopies"))->setChecked(true);
        QVERIFY(compose->isEnabled());
        compose->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mailMessage"));
        QTRY_COMPARE_WITH_TIMEOUT(message->messageType(), KMessageWidget::Error, 20000);
        QVERIFY(message->text().contains(QStringLiteral("missing.txt")));
        QVERIFY(dialog.copiesPath().isEmpty());
        QVERIFY(compose->isEnabled());
        QTest::qWait(200);
        QVERIFY(!QFile::exists(m_output));
    }

    void warnsWhenTheClientIgnoresAttachments()
    {
        installClient(ImpresarioId, QStringLiteral("Plasma Impresario"));
        const QUrl remote = archiveMember(QStringLiteral("remote.txt"));
        SendByEmailDialog dialog({KFileItem(QUrl::fromLocalFile(m_file)), KFileItem(remote, QStringLiteral("text/plain"), S_IFREG)});
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mailMessage"));
        QVERIFY(!message->isHidden());
        QCOMPARE(message->messageType(), KMessageWidget::Warning);
        QVERIFY(message->text().contains(QStringLiteral("Plasma Impresario")));
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("mailPrepareCopies"))->isEnabled());

        QSignalSpy finished(&dialog, &SendByEmailDialog::composeFinished);
        dialog.compose();
        QVERIFY(finished.wait(10000));
        QCOMPARE(message->messageType(), KMessageWidget::Warning);
        QVERIFY(message->text().contains(QStringLiteral("nothing was attached")));
        QVERIFY(dialog.copiesPath().isEmpty());
        QVERIFY(composerArguments().startsWith(QStringLiteral("mailto:")));
    }

    void windowActionNeedsAFile()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        QAction *action = window.actionCollection()->action(QStringLiteral("send_by_email"));
        QVERIFY(action);
        QVERIFY(!action->text().isEmpty());
        QVERIFY(!action->isEnabled());
        window.sendByEmail();
        QVERIFY(!window.findChild<SendByEmailDialog *>());
    }

private:
    static QString mimeAppsPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list");
    }

    /// Registers a fake mail client that records its arguments and makes it
    /// the preferred `mailto:` handler.
    void installClient(const QString &desktopId, const QString &name)
    {
        const QString script = m_dir.filePath(QStringLiteral("composer.sh"));
        QFile scriptFile(script);
        QVERIFY(scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        scriptFile.write(QByteArray("#!/bin/sh\nprintf '%s\\n' \"$@\" > '") + QFile::encodeName(m_output) + "'\n");
        scriptFile.close();
        QVERIFY(scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

        for (const QString &entry : QDir(m_applications).entryList({QStringLiteral("*.desktop")})) {
            QFile::remove(m_applications + QLatin1Char('/') + entry);
        }
        QFile desktop(m_applications + QLatin1Char('/') + desktopId);
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArray("[Desktop Entry]\nType=Application\nName=") + name.toUtf8() + "\nExec=" + QFile::encodeName(script)
                      + " %u\nMimeType=x-scheme-handler/mailto;\n");
        desktop.close();

        QFile mimeApps(mimeAppsPath());
        QVERIFY(mimeApps.open(QIODevice::WriteOnly | QIODevice::Truncate));
        mimeApps.write(QByteArray("[Default Applications]\nx-scheme-handler/mailto=") + desktopId.toUtf8() + "\n");
        mimeApps.close();

        KSycoca::self()->ensureCacheValid();
        const auto client = MailHandoff::preferredClient();
        QVERIFY(client);
        QCOMPARE(client->desktopId, desktopId);
    }

    /// A `tar:` URL for @p name inside an archive that contains `remote.txt`.
    QUrl archiveMember(const QString &name)
    {
        const QString archive = m_dir.filePath(QStringLiteral("remote.tar"));
        if (!QFile::exists(archive)) {
            KTar tar(archive);
            if (tar.open(QIODevice::WriteOnly)) {
                tar.writeFile(QStringLiteral("remote.txt"), QByteArray("remote"));
                tar.close();
            }
        }
        return QUrl(QStringLiteral("tar:") + archive + QLatin1Char('/') + name);
    }

    QString composerArguments() const
    {
        QFile file(m_output);
        if (!QTest::qWaitFor(
                [this] {
                    return QFileInfo(m_output).size() > 0;
                },
                10000)
            || !file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(QByteArray::fromPercentEncoding(file.readAll()));
    }

    QTemporaryDir m_dir;
    QString m_applications;
    QString m_file;
    QString m_output;
};

QTEST_MAIN(SendByEmailDialogTest)
#include "sendbyemaildialogtest.moc"
