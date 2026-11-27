// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filechooser.h"

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

class ChooserDialog;

/**
 * The org.freedesktop.impl.portal.FileChooser backend. Each request shows
 * a ChooserDialog and replies when it closes. The request's
 * org.freedesktop.impl.portal.Request object at its handle closes the
 * dialog when xdg-desktop-portal cancels the request.
 */
class FileChooserPortal : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")

public:
    explicit FileChooserPortal(QObject *parent = nullptr);
    ~FileChooserPortal() override;

    /// The object path xdg-desktop-portal calls backends on.
    static QString objectPath();
    int pendingRequests() const;

public Q_SLOTS:
    Q_SCRIPTABLE uint OpenFile(const QDBusObjectPath &handle,
                               const QString &app_id,
                               const QString &parent_window,
                               const QString &title,
                               const QVariantMap &options,
                               QVariantMap &results);
    Q_SCRIPTABLE uint SaveFile(const QDBusObjectPath &handle,
                               const QString &app_id,
                               const QString &parent_window,
                               const QString &title,
                               const QVariantMap &options,
                               QVariantMap &results);
    Q_SCRIPTABLE uint SaveFiles(const QDBusObjectPath &handle,
                                const QString &app_id,
                                const QString &parent_window,
                                const QString &title,
                                const QVariantMap &options,
                                QVariantMap &results);

Q_SIGNALS:
    /// A request's dialog was shown.
    void dialogShown(ChooserDialog *dialog);
    /// The last pending request finished.
    void idle();

private:
    uint start(FileChooser::Mode mode, const QDBusObjectPath &handle, const QString &parentWindow, const QString &title, const QVariantMap &options);

    int m_pending = 0;
};
