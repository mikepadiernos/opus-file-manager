// SPDX-License-Identifier: GPL-3.0-or-later

#include "plasmavaults.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

/// Stands in for Plasma Vault's kded module on a private session bus.
class FakePlasmaVault : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.plasmavault")

public:
    QList<PlasmaVaults::Vault> vaults;
    QStringList calls;

public Q_SLOTS:
    QList<PlasmaVaults::Vault> availableDevices() const
    {
        return vaults;
    }
    void openVault(const QString &device)
    {
        calls.append(QStringLiteral("open ") + device);
    }
    void closeVault(const QString &device)
    {
        calls.append(QStringLiteral("close ") + device);
    }
    void configureVault(const QString &device)
    {
        calls.append(QStringLiteral("configure ") + device);
    }
    void requestNewVault()
    {
        calls.append(QStringLiteral("create"));
    }
};

class PlasmaVaultsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static PlasmaVaults::Vault vault(const QString &name, PlasmaVaults::Status status)
    {
        return {.name = name,
                .device = QStringLiteral("/home/user/.local/share/plasma-vault/") + name + QStringLiteral(".enc"),
                .mountPoint = QStringLiteral("/home/user/Vaults/") + name,
                .status = status,
                .message = {},
                .activities = {QStringLiteral("activity")},
                .offlineOnly = true};
    }

    void makeProcess(const QString &proc, const QString &pid, const QString &name, const QString &cwd, const QStringList &files)
    {
        const QString base = proc + u'/' + pid;
        QVERIFY(QDir().mkpath(base + QStringLiteral("/fd")));
        QFile comm(base + QStringLiteral("/comm"));
        QVERIFY(comm.open(QIODevice::WriteOnly));
        comm.write(name.toUtf8() + '\n');
        QVERIFY(QFile::link(cwd, base + QStringLiteral("/cwd")));
        QVERIFY(QFile::link(QStringLiteral("/"), base + QStringLiteral("/root")));
        for (int i = 0; i < files.size(); ++i) {
            QVERIFY(QFile::link(files.at(i), base + QStringLiteral("/fd/") + QString::number(i)));
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void containsPaths()
    {
        QVERIFY(PlasmaVaults::contains(QStringLiteral("/v/a"), QStringLiteral("/v/a")));
        QVERIFY(PlasmaVaults::contains(QStringLiteral("/v/a/"), QStringLiteral("/v/a/b")));
        QVERIFY(!PlasmaVaults::contains(QStringLiteral("/v/a"), QStringLiteral("/v/ab")));
        QVERIFY(!PlasmaVaults::contains(QString(), QStringLiteral("/v")));
    }

    void statusAndIcons()
    {
        QCOMPARE(PlasmaVaults::iconName(vault(QStringLiteral("a"), PlasmaVaults::Status::Opened)), QStringLiteral("folder-decrypted"));
        QCOMPARE(PlasmaVaults::iconName(vault(QStringLiteral("a"), PlasmaVaults::Status::Closed)), QStringLiteral("folder-encrypted"));
        QCOMPARE(PlasmaVaults::iconName(vault(QStringLiteral("a"), PlasmaVaults::Status::Error)), QStringLiteral("dialog-error"));
        QVERIFY(vault(QStringLiteral("a"), PlasmaVaults::Status::Closing).isBusy());
        QVERIFY(!vault(QStringLiteral("a"), PlasmaVaults::Status::Closed).isBusy());
        QVERIFY(!PlasmaVaults::statusText(PlasmaVaults::Status::Opened).isEmpty());
        PlasmaVaults::Vault unnamed;
        unnamed.device = QStringLiteral("/data/x.enc");
        QCOMPARE(unnamed.displayName(), QStringLiteral("/data/x.enc"));
    }

    void lockStatesAndHiddenVaults()
    {
        using PlasmaVaults::Status;
        for (Status status : {Status::Opened, Status::Closed, Status::Opening, Status::Closing}) {
            QVERIFY(PlasmaVaults::isLockState(status));
        }
        for (Status status : {Status::NotInitialized, Status::Creating, Status::Dismantling, Status::Dismantled, Status::DeviceMissing, Status::Error}) {
            QVERIFY(!PlasmaVaults::isLockState(status));
        }

        const QList<PlasmaVaults::Vault> all{vault(QStringLiteral("a"), Status::Opened),
                                             vault(QStringLiteral("b"), Status::Closed),
                                             vault(QStringLiteral("c"), Status::Error)};
        const auto devices = [](const QList<PlasmaVaults::Vault> &vaults) {
            QStringList result;
            for (const PlasmaVaults::Vault &each : vaults) {
                result.append(each.device);
            }
            return result;
        };
        QCOMPARE(devices(PlasmaVaults::shownVaults(all, {})), devices(all));
        QCOMPARE(devices(PlasmaVaults::shownVaults(all, {all.at(1).device, QStringLiteral("/gone.enc")})), QStringList({all.at(0).device, all.at(2).device}));
        // Names don't hide vaults; only the encrypted data folder identifies one.
        QCOMPARE(PlasmaVaults::shownVaults(all, {QStringLiteral("a")}).size(), 3);
    }

    void findsProcessesUsingAMountPoint()
    {
        const QString root = QDir(m_dir.path()).canonicalPath();
        const QString mount = root + QStringLiteral("/mount");
        QVERIFY(QDir().mkpath(mount + QStringLiteral("/docs")));
        const QString proc = root + QStringLiteral("/proc");
        makeProcess(proc, QStringLiteral("42"), QStringLiteral("shell"), mount + QStringLiteral("/docs"), {});
        makeProcess(proc, QStringLiteral("7"), QStringLiteral("editor"), root, {QStringLiteral("socket:[1]"), mount + QStringLiteral("/docs/a.txt")});
        makeProcess(proc, QStringLiteral("9"), QStringLiteral("idle"), root, {root + QStringLiteral("/mounted")});
        QVERIFY(QDir().mkpath(proc + QStringLiteral("/self")));

        const QList<PlasmaVaults::User> users = PlasmaVaults::usersOf(mount, proc);
        QCOMPARE(users.size(), 2);
        QCOMPARE(users.at(0).pid, 7);
        QCOMPARE(users.at(0).name, QStringLiteral("editor"));
        QCOMPARE(users.at(1).pid, 42);
        QCOMPARE(users.at(1).name, QStringLiteral("shell"));
        QVERIFY(PlasmaVaults::usersOf(QString(), proc).isEmpty());
    }

    void talksToPlasmaVault()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("No session bus");
        }
        // Register before calling, so the bus doesn't start a real kded6.
        if (!bus.registerService(PlasmaVaults::Service)) {
            QSKIP("kded6 is running on this bus");
        }
        DBusVaultClient client;
        QString error;
        FakePlasmaVault fake;
        fake.vaults = {vault(QStringLiteral("Work"), PlasmaVaults::Status::Opened), vault(QStringLiteral("Archive"), PlasmaVaults::Status::Closed)};
        fake.vaults[1].message = QStringLiteral("Device busy");
        QVERIFY(bus.registerObject(PlasmaVaults::Path, &fake, QDBusConnection::ExportAllSlots));

        error.clear();
        const QList<PlasmaVaults::Vault> vaults = client.vaults(&error);
        QVERIFY(error.isEmpty());
        QCOMPARE(vaults.size(), 2);
        QCOMPARE(vaults.at(0).name, QStringLiteral("Archive"));
        QCOMPARE(vaults.at(0).status, PlasmaVaults::Status::Closed);
        QCOMPARE(vaults.at(0).message, QStringLiteral("Device busy"));
        QCOMPARE(vaults.at(1).mountPoint, QStringLiteral("/home/user/Vaults/Work"));
        QCOMPARE(vaults.at(1).activities, QStringList{QStringLiteral("activity")});
        QVERIFY(vaults.at(1).offlineOnly);

        client.request(VaultClient::Request::Open, fake.vaults.at(1).device);
        client.request(VaultClient::Request::Create);
        QTRY_COMPARE(fake.calls, (QStringList{QStringLiteral("open ") + fake.vaults.at(1).device, QStringLiteral("create")}));

        QSignalSpy changed(&client, &VaultClient::vaultChanged);
        QDBusMessage signal = QDBusMessage::createSignal(PlasmaVaults::Path, PlasmaVaults::Interface, QStringLiteral("vaultChanged"));
        signal << QVariant::fromValue(fake.vaults.at(0));
        QVERIFY(bus.send(signal));
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(0).value<PlasmaVaults::Vault>().device, fake.vaults.at(0).device);
        QCOMPARE(changed.at(0).at(0).value<PlasmaVaults::Vault>().status, PlasmaVaults::Status::Opened);

        QSignalSpy listChanged(&client, &VaultClient::changed);
        QDBusMessage removed = QDBusMessage::createSignal(PlasmaVaults::Path, PlasmaVaults::Interface, QStringLiteral("vaultRemoved"));
        removed << fake.vaults.at(0).device;
        QVERIFY(bus.send(removed));
        QTRY_VERIFY(!listChanged.isEmpty());
        bus.unregisterObject(PlasmaVaults::Path);
        bus.unregisterService(PlasmaVaults::Service);
    }
};

QTEST_GUILESS_MAIN(PlasmaVaultsTest)

#include "plasmavaultstest.moc"
