// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofilesdialog.h"

#include <KSharedConfig>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using RemoteProfiles::Authentication;
using RemoteProfiles::Profile;
using RemoteProfiles::Protocol;

class RemoteProfilesDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QVERIFY(m_root.isValid());
        m_config = KSharedConfig::openConfig(m_root.filePath(QStringLiteral("remotesrc")), KConfig::SimpleConfig);
        m_config->deleteGroup(QStringLiteral("RemoteProfiles"));
    }

    void addsEditsAndRemovesProfiles()
    {
        RemoteProfilesDialog dialog(group());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("remoteProfileList"));
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileName"));
        auto *host = dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileHost"));
        auto *port = dialog.findChild<QSpinBox *>(QStringLiteral("remoteProfilePort"));
        auto *user = dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileUser"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileSave"));
        auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileRemove"));
        auto *add = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileAdd"));
        QVERIFY(list && name && host && port && user && save && remove && add);
        QCOMPARE(list->count(), 0);
        QVERIFY(!save->isEnabled());
        QVERIFY(!remove->isEnabled());

        name->setText(QStringLiteral("Web"));
        host->setText(QStringLiteral("-oProxyCommand=x"));
        QVERIFY(!save->isEnabled());
        host->setText(QStringLiteral("web.example.org"));
        port->setValue(2200);
        user->setText(QStringLiteral("www"));
        QVERIFY(save->isEnabled());
        save->click();
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->currentRow(), 0);
        QVERIFY(remove->isEnabled());

        QList<Profile> saved = RemoteProfiles::load(group());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().port, 2200);
        const QString id = saved.first().id;

        name->setText(QStringLiteral("Web server"));
        save->click();
        saved = RemoteProfiles::load(group());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().id, id);
        QCOMPARE(saved.first().name, QStringLiteral("Web server"));

        add->click();
        QVERIFY(name->text().isEmpty());
        name->setText(QStringLiteral("web SERVER"));
        host->setText(QStringLiteral("other.example.org"));
        save->click();
        QCOMPARE(RemoteProfiles::load(group()).size(), 1);
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("remoteProfileStatus"));
        QVERIFY(!status->text().isEmpty());

        list->setCurrentRow(0);
        QCOMPARE(name->text(), QStringLiteral("Web server"));
        remove->click();
        QVERIFY(RemoteProfiles::load(group()).isEmpty());
        QCOMPARE(list->count(), 0);
    }

    void limitsKeySignInToSsh()
    {
        RemoteProfilesDialog dialog(group());
        auto *protocol = dialog.findChild<QComboBox *>(QStringLiteral("remoteProfileProtocol"));
        auto *authentication = dialog.findChild<QComboBox *>(QStringLiteral("remoteProfileAuthentication"));
        auto *identity = dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileIdentityFile"));
        auto *ssh = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileSsh"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("remoteProfileStatus"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileName"))->setText(QStringLiteral("Box"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileHost"))->setText(QStringLiteral("box.example.org"));

        authentication->setCurrentIndex(authentication->findData(static_cast<int>(Authentication::KeyOrAgent)));
        QVERIFY(identity->isEnabled());
        QVERIFY(ssh->isEnabled());

        protocol->setCurrentIndex(protocol->findData(static_cast<int>(Protocol::WebDav)));
        QCOMPARE(authentication->currentData().toInt(), static_cast<int>(Authentication::Password));
        QVERIFY(!authentication->isEnabled());
        QVERIFY(!identity->isEnabled());
        QVERIFY(!ssh->isEnabled());
        QVERIFY(!status->text().isEmpty());
        QVERIFY(dialog.formProfile().identityFile.isEmpty());
    }

    void requestsOpeningAndSsh()
    {
        RemoteProfilesDialog dialog(group());
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileName"))->setText(QStringLiteral("Box"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileHost"))->setText(QStringLiteral("box.example.org"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfilePath"))->setText(QStringLiteral("/srv"));
        auto *other = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileOpenOther"));
        dialog.setOtherPaneAvailable(false);
        QVERIFY(!other->isEnabled());
        dialog.setOtherPaneAvailable(true);
        QVERIFY(other->isEnabled());

        QSignalSpy opened(&dialog, &RemoteProfilesDialog::openRequested);
        other->click();
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.first().at(0).toUrl(), QUrl(QStringLiteral("sftp://box.example.org/srv")));
        QCOMPARE(opened.first().at(1).toBool(), true);
        QVERIFY(RemoteProfiles::load(group()).isEmpty());

        QSignalSpy ssh(&dialog, &RemoteProfilesDialog::sshRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileSsh"))->click();
        QCOMPARE(ssh.size(), 1);
        QCOMPARE(ssh.first().at(0).value<Profile>().host, QStringLiteral("box.example.org"));
    }

    void reportsMissingWorkers()
    {
        RemoteProfilesDialog dialog(group(), nullptr, [](const QString &scheme) {
            return scheme != QLatin1String("ftps");
        });
        auto *protocol = dialog.findChild<QComboBox *>(QStringLiteral("remoteProfileProtocol"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("remoteProfileStatus"));
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileOpen"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileSave"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileName"))->setText(QStringLiteral("Files"));
        dialog.findChild<QLineEdit *>(QStringLiteral("remoteProfileHost"))->setText(QStringLiteral("files.example.org"));
        protocol->setCurrentIndex(protocol->findData(static_cast<int>(Protocol::Ftps)));
        QVERIFY(status->text().contains(QStringLiteral("FTP over TLS")));
        QVERIFY(!open->isEnabled());
        QVERIFY(save->isEnabled());

        protocol->setCurrentIndex(protocol->findData(static_cast<int>(Protocol::Ftp)));
        QVERIFY(status->text().contains(QStringLiteral("plain text")));
        QVERIFY(open->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("remoteProfileSsh"))->isEnabled());
    }

private:
    KConfigGroup group() const
    {
        return m_config->group(QStringLiteral("RemoteProfiles"));
    }

    QTemporaryDir m_root;
    KSharedConfig::Ptr m_config;
};

QTEST_MAIN(RemoteProfilesDialogTest)

#include "remoteprofilesdialogtest.moc"
