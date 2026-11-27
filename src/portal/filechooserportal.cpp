// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooserportal.h"

#include "chooserdialog.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KWindowSystem>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QPointer>

namespace
{
/// The org.freedesktop.impl.portal.Request object of one request.
class PortalRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    using QObject::QObject;

public Q_SLOTS:
    Q_SCRIPTABLE void Close()
    {
        Q_EMIT closeRequested();
    }

Q_SIGNALS:
    void closeRequested();
};
}

FileChooserPortal::FileChooserPortal(QObject *parent)
    : QObject(parent)
{
    FileChooser::registerTypes();
}

FileChooserPortal::~FileChooserPortal() = default;

QString FileChooserPortal::objectPath()
{
    return QStringLiteral("/org/freedesktop/portal/desktop");
}

int FileChooserPortal::pendingRequests() const
{
    return m_pending;
}

uint FileChooserPortal::OpenFile(const QDBusObjectPath &handle,
                                 const QString &app_id,
                                 const QString &parent_window,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results)
{
    Q_UNUSED(results)
    return start(FileChooser::Mode::Open, handle, app_id, parent_window, title, options);
}

uint FileChooserPortal::SaveFile(const QDBusObjectPath &handle,
                                 const QString &app_id,
                                 const QString &parent_window,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results)
{
    Q_UNUSED(results)
    return start(FileChooser::Mode::Save, handle, app_id, parent_window, title, options);
}

uint FileChooserPortal::SaveFiles(const QDBusObjectPath &handle,
                                  const QString &app_id,
                                  const QString &parent_window,
                                  const QString &title,
                                  const QVariantMap &options,
                                  QVariantMap &results)
{
    Q_UNUSED(results)
    return start(FileChooser::Mode::SaveFiles, handle, app_id, parent_window, title, options);
}

uint FileChooserPortal::start(FileChooser::Mode mode,
                              const QDBusObjectPath &handle,
                              const QString &appId,
                              const QString &parentWindow,
                              const QString &title,
                              const QVariantMap &options)
{
    if (!calledFromDBus()) {
        return FileChooser::Ended;
    }
    setDelayedReply(true);
    const QDBusMessage call = message();
    QDBusConnection bus = connection();
    FileChooser::Request request = FileChooser::parseRequest(mode, title, options);
    request.appId = appId;
    // Preferences may have changed while the backend runs.
    Settings::self()->load();
    const bool rememberFolder = Settings::fileChooserStartFolder() == Settings::EnumFileChooserStartFolder::LastFolder;
    if (rememberFolder) {
        const KSharedConfigPtr state = KSharedConfig::openStateConfig();
        state->reparseConfiguration();
        request.lastFolder = FileChooser::lastFolder(state->group(FileChooser::stateGroupName()), appId);
    }

    auto *dialog = new ChooserDialog(request);
    auto *closer = new PortalRequest(dialog);
    const QString handlePath = handle.path();
    const bool closable = !handlePath.isEmpty() && bus.registerObject(handlePath, closer, QDBusConnection::ExportScriptableSlots);
    auto closed = std::make_shared<bool>(false);
    // Queued, so the request object is not unregistered while it answers Close.
    connect(
        closer,
        &PortalRequest::closeRequested,
        dialog,
        [dialog, closed] {
            *closed = true;
            dialog->reject();
        },
        Qt::QueuedConnection);
    connect(dialog, &QDialog::finished, this, [this, dialog, call, bus, handlePath, closable, closed, rememberFolder](int code) mutable {
        uint response = *closed ? FileChooser::Ended : FileChooser::Cancelled;
        QVariantMap results;
        if (code == QDialog::Accepted) {
            response = FileChooser::Accepted;
            results = FileChooser::results(dialog->request(), dialog->chooserResult());
            if (rememberFolder) {
                const KSharedConfigPtr state = KSharedConfig::openStateConfig();
                state->reparseConfiguration();
                KConfigGroup group = state->group(FileChooser::stateGroupName());
                FileChooser::rememberFolder(group, dialog->request().appId, FileChooser::acceptedFolder(dialog->request(), dialog->chooserResult()));
                state->sync();
            }
        }
        if (closable) {
            bus.unregisterObject(handlePath);
        }
        bus.send(call.createReply(QVariantList{QVariant::fromValue(response), results}));
        dialog->deleteLater();
        if (--m_pending == 0) {
            Q_EMIT idle();
        }
    });

    ++m_pending;
    dialog->winId();
    if (!parentWindow.isEmpty() && dialog->windowHandle()) {
        KWindowSystem::setMainWindow(dialog->windowHandle(), parentWindow);
    }
    dialog->show();
    if (dialog->windowHandle()) {
        KWindowSystem::activateWindow(dialog->windowHandle());
    }
    Q_EMIT dialogShown(dialog);
    return FileChooser::Accepted;
}

#include "filechooserportal.moc"
