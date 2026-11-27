// SPDX-License-Identifier: GPL-3.0-or-later

#include "plasmavaults.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <unistd.h>

namespace
{
constexpr int Timeout = 3000;

const bool registered = [] {
    qDBusRegisterMetaType<PlasmaVaults::Vault>();
    qDBusRegisterMetaType<QList<PlasmaVaults::Vault>>();
    return true;
}();

QString readLink(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    QByteArray buffer(4096, Qt::Uninitialized);
    const ssize_t length = ::readlink(encoded.constData(), buffer.data(), buffer.size());
    if (length <= 0) {
        return {};
    }
    return QFile::decodeName(buffer.left(length));
}
}

namespace PlasmaVaults
{
bool Vault::isOpen() const
{
    return status == Status::Opened;
}

bool Vault::isBusy() const
{
    return status == Status::Creating || status == Status::Opening || status == Status::Closing || status == Status::Dismantling;
}

QString Vault::displayName() const
{
    return name.isEmpty() ? device : name;
}

QString statusText(Status status)
{
    switch (status) {
    case Status::Opened:
        return i18nc("@info Plasma Vault status", "Open");
    case Status::Closed:
        return i18nc("@info Plasma Vault status", "Closed");
    case Status::Creating:
        return i18nc("@info Plasma Vault status", "Creating…");
    case Status::Opening:
        return i18nc("@info Plasma Vault status", "Opening…");
    case Status::Closing:
        return i18nc("@info Plasma Vault status", "Closing…");
    case Status::Dismantling:
    case Status::Dismantled:
        return i18nc("@info Plasma Vault status", "Being removed");
    case Status::DeviceMissing:
        return i18nc("@info Plasma Vault status", "Encrypted data missing");
    case Status::NotInitialized:
        return i18nc("@info Plasma Vault status", "Not set up");
    case Status::Error:
        break;
    }
    return i18nc("@info Plasma Vault status", "Error");
}

QString iconName(const Vault &vault)
{
    switch (vault.status) {
    case Status::Opened:
        return QStringLiteral("folder-decrypted");
    case Status::Closed:
    case Status::Opening:
    case Status::Closing:
        return QStringLiteral("folder-encrypted");
    case Status::Error:
    case Status::DeviceMissing:
        return QStringLiteral("dialog-error");
    default:
        return QStringLiteral("folder-gray");
    }
}

bool contains(const QString &mountPoint, const QString &path)
{
    if (mountPoint.isEmpty() || path.isEmpty()) {
        return false;
    }
    const QString folder = QDir::cleanPath(mountPoint);
    const QString cleaned = QDir::cleanPath(path);
    return cleaned == folder || cleaned.startsWith(folder.endsWith(u'/') ? folder : folder + u'/');
}

QList<User> usersOf(const QString &mountPoint, const QString &procRoot)
{
    QList<User> users;
    if (mountPoint.isEmpty()) {
        return users;
    }
    const QString canonical = QFileInfo(mountPoint).canonicalFilePath();
    const QString folder = canonical.isEmpty() ? QDir::cleanPath(mountPoint) : canonical;
    const auto inside = [&folder](const QString &target) {
        return !target.isEmpty() && target.startsWith(u'/') && contains(folder, target);
    };
    const QStringList processes = QDir(procRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : processes) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric) {
            continue;
        }
        const QString base = procRoot + u'/' + entry;
        bool using_ = inside(readLink(base + QStringLiteral("/cwd"))) || inside(readLink(base + QStringLiteral("/root")));
        if (!using_) {
            const QStringList descriptors = QDir(base + QStringLiteral("/fd")).entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
            for (const QString &descriptor : descriptors) {
                if (inside(readLink(base + QStringLiteral("/fd/") + descriptor))) {
                    using_ = true;
                    break;
                }
            }
        }
        if (!using_) {
            continue;
        }
        QFile comm(base + QStringLiteral("/comm"));
        QString name = comm.open(QIODevice::ReadOnly) ? QString::fromUtf8(comm.readAll()).trimmed() : QString();
        users.append({.pid = pid, .name = name.isEmpty() ? entry : name});
    }
    std::sort(users.begin(), users.end(), [](const User &a, const User &b) {
        return a.pid < b.pid;
    });
    return users;
}
}

QDBusArgument &operator<<(QDBusArgument &argument, const PlasmaVaults::Vault &vault)
{
    argument.beginStructure();
    argument << vault.name << vault.device << vault.mountPoint << static_cast<quint16>(vault.status) << vault.message << vault.activities << vault.offlineOnly;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PlasmaVaults::Vault &vault)
{
    quint16 status = 0;
    argument.beginStructure();
    argument >> vault.name >> vault.device >> vault.mountPoint >> status >> vault.message >> vault.activities >> vault.offlineOnly;
    argument.endStructure();
    vault.status = static_cast<PlasmaVaults::Status>(status);
    return argument;
}

DBusVaultClient::DBusVaultClient(QObject *parent)
    : VaultClient(parent)
{
    Q_UNUSED(registered)
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto *watcher = new QDBusServiceWatcher(PlasmaVaults::Service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &VaultClient::changed);
    const QString service = PlasmaVaults::Service;
    const QString path = PlasmaVaults::Path;
    const QString interface = PlasmaVaults::Interface;
    bus.connect(service, path, interface, QStringLiteral("vaultAdded"), this, SIGNAL(changed()));
    bus.connect(service, path, interface, QStringLiteral("vaultRemoved"), this, SIGNAL(changed()));
    bus.connect(service, path, interface, QStringLiteral("vaultChanged"), this, SLOT(onVaultChanged(QDBusMessage)));
}

QList<PlasmaVaults::Vault> DBusVaultClient::vaults(QString *error) const
{
    QDBusMessage message =
        QDBusMessage::createMethodCall(PlasmaVaults::Service, PlasmaVaults::Path, PlasmaVaults::Interface, QStringLiteral("availableDevices"));
    const QDBusReply<QList<PlasmaVaults::Vault>> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    if (!reply.isValid()) {
        if (error) {
            *error = i18nc("@info:status", "Plasma Vault is not available. Install Plasma Vault and make sure its background service is enabled.");
        }
        return {};
    }
    QList<PlasmaVaults::Vault> result = reply.value();
    std::sort(result.begin(), result.end(), [](const PlasmaVaults::Vault &a, const PlasmaVaults::Vault &b) {
        return a.displayName().localeAwareCompare(b.displayName()) < 0;
    });
    return result;
}

void DBusVaultClient::request(Request request, const QString &device)
{
    QString method;
    QVariantList arguments{device};
    switch (request) {
    case Request::Open:
        method = QStringLiteral("openVault");
        break;
    case Request::Close:
        method = QStringLiteral("closeVault");
        break;
    case Request::Configure:
        method = QStringLiteral("configureVault");
        break;
    case Request::Create:
        method = QStringLiteral("requestNewVault");
        arguments.clear();
        break;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(PlasmaVaults::Service, PlasmaVaults::Path, PlasmaVaults::Interface, method);
    message.setArguments(arguments);
    // Plasma Vault shows its own dialogs, so don't wait for the reply.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 60 * 60 * 1000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        if (watcher->isError()) {
            Q_EMIT failed(i18nc("@info:status", "Plasma Vault did not respond: %1", watcher->error().message()));
        }
    });
}

void DBusVaultClient::onVaultChanged(const QDBusMessage &message)
{
    const QVariantList arguments = message.arguments();
    if (arguments.isEmpty()) {
        return;
    }
    PlasmaVaults::Vault vault;
    arguments.constFirst().value<QDBusArgument>() >> vault;
    Q_EMIT vaultChanged(vault);
}
