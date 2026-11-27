// SPDX-License-Identifier: GPL-3.0-or-later

// A D-Bus service that KRunner starts on demand to offer saved Plasma Opus
// workspaces and Favorites.

#include "appidentity.h"
#include "opusrunner.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDBusConnection>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Settings and Favorites are read from Plasma Opus's own configuration.
    QCoreApplication::setApplicationName(AppIdentity::componentName());
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));

    OpusRunner runner;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/runner"), &runner, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportAllSlots)) {
        qWarning("opus-file-manager-krunner: could not register /runner");
        return 1;
    }
    if (!bus.registerService(QStringLiteral("org.kde.opusfilemanager.krunner"))) {
        qWarning("opus-file-manager-krunner: org.kde.opusfilemanager.krunner is already registered");
        return 1;
    }
    return app.exec();
}
