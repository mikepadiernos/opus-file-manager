// SPDX-License-Identifier: GPL-3.0-or-later

// The XDG portal file chooser backend that D-Bus starts once the user opts
// in with opus-file-manager --install-portal.

#include "appidentity.h"
#include "filechooserportal.h"
#include "portalsetup.h"

#include <KLocalizedString>

#include <QApplication>
#include <QDBusConnection>
#include <QTimer>

#include <chrono>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Views, places and window sizes follow Plasma Opus's own configuration.
    QCoreApplication::setApplicationName(AppIdentity::componentName());
    QGuiApplication::setDesktopFileName(AppIdentity::desktopFileName());
    QApplication::setWindowIcon(AppIdentity::applicationIcon());
    QApplication::setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));

    FileChooserPortal portal;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(FileChooserPortal::objectPath(), &portal, QDBusConnection::ExportScriptableSlots)) {
        qWarning("opus-file-manager-portal: could not register %s", qPrintable(FileChooserPortal::objectPath()));
        return 1;
    }
    if (!bus.registerService(PortalSetup::dbusName())) {
        qWarning("opus-file-manager-portal: %s is already registered", qPrintable(PortalSetup::dbusName()));
        return 1;
    }

    // D-Bus starts the backend again for the next request.
    QTimer idleQuit;
    idleQuit.setSingleShot(true);
    idleQuit.setInterval(std::chrono::minutes(2));
    QObject::connect(&idleQuit, &QTimer::timeout, &app, [&portal] {
        if (portal.pendingRequests() == 0) {
            QCoreApplication::quit();
        }
    });
    QObject::connect(&portal, &FileChooserPortal::dialogShown, &idleQuit, &QTimer::stop);
    QObject::connect(&portal, &FileChooserPortal::idle, &idleQuit, qOverload<>(&QTimer::start));
    idleQuit.start();
    return app.exec();
}
