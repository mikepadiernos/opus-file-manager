// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "kdeconnectshare.h"

#include <QList>
#include <QObject>
#include <QUrl>

class QDBusServiceWatcher;

/// The KDE Connect daemon, as far as sending files needs it.
class KdeConnectClient : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Whether the KDE Connect daemon is running on the session bus.
    virtual bool isAvailable() const = 0;
    /// The paired devices, reachable or not.
    virtual QList<KdeConnectShare::Device> devices() const = 0;
    /// Asks KDE Connect to send @p files to @p deviceId. A successful reply
    /// means KDE Connect accepted the request, not that the transfer finished.
    /// @return an empty string, or why the request failed
    virtual QString share(const QString &deviceId, const QList<QUrl> &files) = 0;

Q_SIGNALS:
    /// The daemon started or stopped, or its devices changed.
    void changed();
};

/// Talks to the KDE Connect daemon over D-Bus. It reads devices and calls the
/// share plugin only; it never pairs devices or changes plugin settings.
class DBusKdeConnectClient : public KdeConnectClient
{
    Q_OBJECT

public:
    explicit DBusKdeConnectClient(QObject *parent = nullptr);

    bool isAvailable() const override;
    QList<KdeConnectShare::Device> devices() const override;
    QString share(const QString &deviceId, const QList<QUrl> &files) override;

private:
    QDBusServiceWatcher *m_watcher;
};
