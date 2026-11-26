// SPDX-License-Identifier: GPL-3.0-or-later

#include "kdeconnectclient.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

namespace
{
const QString Service = QStringLiteral("org.kde.kdeconnect");
const QString DaemonPath = QStringLiteral("/modules/kdeconnect");
const QString DaemonInterface = QStringLiteral("org.kde.kdeconnect.daemon");
const QString DeviceInterface = QStringLiteral("org.kde.kdeconnect.device");
const QString ShareInterface = QStringLiteral("org.kde.kdeconnect.device.share");
const QString SharePlugin = QStringLiteral("kdeconnect_share");
constexpr int Timeout = 2000;

QString devicePath(const QString &id)
{
    return DaemonPath + QStringLiteral("/devices/") + id;
}

QVariantMap deviceProperties(const QString &id)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, devicePath(id), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << DeviceInterface;
    const QDBusReply<QVariantMap> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    return reply.isValid() ? reply.value() : QVariantMap();
}

bool hasSharePlugin(const QString &id)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, devicePath(id), DeviceInterface, QStringLiteral("hasPlugin"));
    message << SharePlugin;
    const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    return reply.isValid() && reply.value();
}
}

DBusKdeConnectClient::DBusKdeConnectClient(QObject *parent)
    : KdeConnectClient(parent)
    , m_watcher(new QDBusServiceWatcher(Service, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this))
{
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &KdeConnectClient::changed);
    // QtDBus follows the service's owner, so the signal survives a daemon restart.
    QDBusConnection::sessionBus().connect(Service, DaemonPath, DaemonInterface, QStringLiteral("deviceListChanged"), this, SIGNAL(changed()));
}

bool DBusKdeConnectClient::isAvailable() const
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(Service);
}

QList<KdeConnectShare::Device> DBusKdeConnectClient::devices() const
{
    if (!isAvailable()) {
        return {};
    }
    QDBusMessage message = QDBusMessage::createMethodCall(Service, DaemonPath, DaemonInterface, QStringLiteral("devices"));
    message << false << true;
    const QDBusReply<QStringList> ids = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    if (!ids.isValid()) {
        return {};
    }
    QList<KdeConnectShare::Device> result;
    for (const QString &id : ids.value()) {
        const QVariantMap properties = deviceProperties(id);
        KdeConnectShare::Device device;
        device.id = id;
        device.name = properties.value(QStringLiteral("name"), id).toString();
        device.reachable = properties.value(QStringLiteral("isReachable")).toBool();
        // KDE Connect before 24.02 called pairing "trusted".
        device.paired = properties.contains(QStringLiteral("isPaired")) ? properties.value(QStringLiteral("isPaired")).toBool()
                                                                        : properties.value(QStringLiteral("isTrusted"), true).toBool();
        device.canShare = device.reachable && hasSharePlugin(id);
        result.append(device);
    }
    return result;
}

QString DBusKdeConnectClient::share(const QString &deviceId, const QList<QUrl> &files)
{
    if (!isAvailable()) {
        return i18nc("@info", "KDE Connect is not running.");
    }
    QStringList urls;
    for (const QUrl &file : files) {
        urls.append(file.toString(QUrl::FullyEncoded));
    }
    QDBusMessage message =
        QDBusMessage::createMethodCall(Service, devicePath(deviceId) + QStringLiteral("/share"), ShareInterface, QStringLiteral("shareUrls"));
    message << urls;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, Timeout);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return reply.errorMessage().isEmpty() ? reply.errorName() : reply.errorMessage();
    }
    return {};
}
