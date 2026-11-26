// SPDX-License-Identifier: GPL-3.0-or-later

#include "remotemountdialog.h"

#include <KConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using RemoteMounts::Backend;
using RemoteProfiles::Protocol;

class RemoteMountDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
    }

    void offersTheProtocolsBackends()
    {
        KConfig config(m_root.filePath(QStringLiteral("offers")), KConfig::SimpleConfig);
        RemoteMountDialog dialog(profile(Protocol::Sftp), config.group(QStringLiteral("RemoteMounts")), nullptr, none);
        auto *backend = dialog.findChild<QComboBox *>();
        QVERIFY(backend);
        QCOMPARE(backend->count(), 2);
        QCOMPARE(backend->itemText(0), QStringLiteral("KIO FUSE"));
        QCOMPARE(backend->itemText(1), QStringLiteral("sshfs (not installed)"));
        QVERIFY(backend->itemData(1, Qt::ToolTipRole).toString().contains(QStringLiteral("sshfs package")));

        auto *readOnly = findCheck(dialog, QStringLiteral("Read-only"));
        auto *location = dialog.findChild<QLineEdit *>();
        QVERIFY(!readOnly->isEnabled());
        QVERIFY(!location->isEnabled());
        backend->setCurrentIndex(1);
        QVERIFY(readOnly->isEnabled());
        QVERIFY(location->isEnabled());
        QVERIFY(location->placeholderText().contains(QStringLiteral("opus-file-manager/mounts/Server")));
        QVERIFY(!mountButton(dialog)->isEnabled());
        QVERIFY(saveButton(dialog)->isEnabled());

        location->setText(QStringLiteral("relative"));
        QVERIFY(!saveButton(dialog)->isEnabled());
    }

    void savesOptionsAndRequestsMounts()
    {
        KConfig config(m_root.filePath(QStringLiteral("saves")), KConfig::SimpleConfig);
        const KConfigGroup group = config.group(QStringLiteral("RemoteMounts"));
        const RemoteProfiles::Profile remote = profile(Protocol::Smb);
        RemoteMountDialog dialog(remote, group, nullptr, all);
        auto *backend = dialog.findChild<QComboBox *>();
        backend->setCurrentIndex(backend->findData(int(Backend::Cifs)));
        findCheck(dialog, QStringLiteral("Read-only"))->setChecked(true);
        dialog.findChild<QLineEdit *>()->setText(QStringLiteral("/mnt/nas"));
        findCheck(dialog, QStringLiteral("Mount when Plasma Opus starts"))->setChecked(true);
        bool hintShown = false;
        for (QLabel *label : dialog.findChildren<QLabel *>()) {
            hintShown = hintShown || label->text().contains(QStringLiteral("//nas.example.org/share /mnt/nas cifs user,noauto,ro 0 0"));
        }
        QVERIFY(hintShown);

        QSignalSpy requested(&dialog, &RemoteMountDialog::mountRequested);
        QVERIFY(mountButton(dialog)->isEnabled());
        mountButton(dialog)->click();
        QCOMPARE(requested.size(), 1);
        const RemoteMounts::Options saved = RemoteMounts::loadOptions(group, remote);
        QCOMPARE(saved.backend, Backend::Cifs);
        QVERIFY(saved.readOnly);
        QCOMPARE(saved.location, QStringLiteral("/mnt/nas"));
        QVERIFY(saved.atStartup);

        RemoteMountDialog reopened(remote, group, nullptr, all);
        QCOMPARE(reopened.options().backend, Backend::Cifs);
        QCOMPARE(reopened.findChild<QLineEdit *>()->text(), QStringLiteral("/mnt/nas"));
    }

private:
    static QString none(const QString &)
    {
        return {};
    }

    static QString all(const QString &name)
    {
        return QStringLiteral("/usr/bin/") + name;
    }

    static RemoteProfiles::Profile profile(Protocol protocol)
    {
        RemoteProfiles::Profile remote;
        remote.id = QStringLiteral("5f0b8f0e-1c55-4d1b-8a3e-0c2d6b1f9e21");
        remote.name = QStringLiteral("Server");
        remote.protocol = protocol;
        remote.host = QStringLiteral("nas.example.org");
        return remote;
    }

    static QCheckBox *findCheck(const QDialog &dialog, const QString &text)
    {
        for (QCheckBox *box : dialog.findChildren<QCheckBox *>()) {
            if (box->text() == text) {
                return box;
            }
        }
        return nullptr;
    }

    static QPushButton *mountButton(const QDialog &dialog)
    {
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text() == QLatin1String("Save and Mount")) {
                return button;
            }
        }
        return nullptr;
    }

    static QPushButton *saveButton(const QDialog &dialog)
    {
        return dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    }

    QTemporaryDir m_root;
};

QTEST_MAIN(RemoteMountDialogTest)

#include "remotemountdialogtest.moc"
