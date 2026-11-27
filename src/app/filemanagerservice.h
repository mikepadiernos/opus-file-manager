// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>

class MainWindow;

/**
 * The `org.freedesktop.FileManager1` D-Bus service, which applications use
 * to show folders, reveal files ("Show in folder") and show their properties.
 *
 * Plasma Opus claims the service name only while it is the default folder
 * handler, and never replaces or queues behind another owner.
 */
class FileManagerService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
    explicit FileManagerService(MainWindow *window);

    /**
     * Exports the service object and, if @p claimName, requests the
     * well-known name without replacing its current owner.
     * @returns whether Plasma Opus now owns the name
     */
    bool registerOnBus(bool claimName);

    enum class Claim {
        /// Plasma Opus owns the name.
        Claimed,
        /// Another application, such as another file manager, owns the name.
        OwnedElsewhere,
        /// The session bus or the exported service object is not available.
        Unavailable,
    };

    /**
     * Requests the well-known name for the service object this process has
     * exported, without replacing its current owner, such as after Plasma
     * Opus becomes the default file manager.
     */
    static Claim claimName();

public Q_SLOTS:
    Q_SCRIPTABLE void ShowFolders(const QStringList &uriList, const QString &startUpId);
    Q_SCRIPTABLE void ShowItems(const QStringList &uriList, const QString &startUpId);
    Q_SCRIPTABLE void ShowItemProperties(const QStringList &uriList, const QString &startUpId);

Q_SIGNALS:
    /// Emitted after a request has been handled and the window shown.
    void requestHandled();

private:
    void reportRejected(const QStringList &rejected) const;
    void activate(const QString &startUpId);

    MainWindow *m_window;
};
