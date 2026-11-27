// SPDX-License-Identifier: GPL-3.0-or-later

#include "filemanagerservice.h"

#include "filemanagerrequest.h"
#include "mainwindow.h"

#include <KLocalizedString>
#include <KWindowSystem>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QStatusBar>
#include <QUrl>
#include <QWindow>

FileManagerService::FileManagerService(MainWindow *window)
    : QObject(window)
    , m_window(window)
{
}

bool FileManagerService::registerOnBus(bool claimName)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    if (!bus.registerObject(FileManagerRequest::objectPath(), this, QDBusConnection::ExportScriptableSlots)) {
        return false;
    }
    return claimName && FileManagerService::claimName() == Claim::Claimed;
}

FileManagerService::Claim FileManagerService::claimName()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.objectRegisteredAt(FileManagerRequest::objectPath())) {
        return Claim::Unavailable;
    }
    const QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply = bus.interface()->registerService(FileManagerRequest::serviceName(),
                                                                                                              QDBusConnectionInterface::DontQueueService,
                                                                                                              QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid()) {
        return Claim::Unavailable;
    }
    return reply.value() == QDBusConnectionInterface::ServiceRegistered ? Claim::Claimed : Claim::OwnedElsewhere;
}

void FileManagerService::ShowFolders(const QStringList &uriList, const QString &startUpId)
{
    QStringList rejected;
    m_window->showFolders(FileManagerRequest::parseUris(uriList, &rejected));
    activate(startUpId);
    reportRejected(rejected);
}

void FileManagerService::ShowItems(const QStringList &uriList, const QString &startUpId)
{
    QStringList rejected;
    m_window->showItems(FileManagerRequest::parseUris(uriList, &rejected));
    activate(startUpId);
    reportRejected(rejected);
}

void FileManagerService::ShowItemProperties(const QStringList &uriList, const QString &startUpId)
{
    QStringList rejected;
    const QList<QUrl> urls = FileManagerRequest::parseUris(uriList, &rejected);
    // The dialog needs a visible parent to be placed and activated.
    activate(startUpId);
    m_window->showItemProperties(urls);
    reportRejected(rejected);
}

void FileManagerService::reportRejected(const QStringList &rejected) const
{
    // Reported last, so session restoration cannot replace the message.
    if (!rejected.isEmpty()) {
        m_window->statusBar()->showMessage(i18ncp("@info:status",
                                                  "Ignored %1 invalid location: %2",
                                                  "Ignored %1 invalid locations: %2",
                                                  rejected.size(),
                                                  rejected.join(QStringLiteral(", "))));
    }
}

void FileManagerService::activate(const QString &startUpId)
{
    // Also restores a window that is minimized or hidden in the tray.
    m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
    m_window->show();
    if (QWindow *handle = m_window->windowHandle()) {
        if (!startUpId.isEmpty()) {
            if (KWindowSystem::isPlatformWayland()) {
                KWindowSystem::setCurrentXdgActivationToken(startUpId);
            } else {
                qputenv("DESKTOP_STARTUP_ID", startUpId.toUtf8());
            }
        }
        KWindowSystem::updateStartupId(handle);
        KWindowSystem::activateWindow(handle);
    }
    Q_EMIT requestHandled();
}
