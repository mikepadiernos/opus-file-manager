// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QDBusUnixFileDescriptor>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <optional>

/// Decides which selected items can be attached to a new message and which
/// mail client composes it. Plasma Opus only opens the user's preferred
/// composer with the attachments; it never sends mail.
namespace MailHandoff
{
/// Why a selected item cannot be attached.
enum class Reason {
    Folder,
    Missing,
    Unreadable,
    NotRegularFile,
};

struct Rejected {
    QUrl url;
    Reason reason;
};

/// The selection split into attachable files and items that cannot be
/// attached.
struct Selection {
    /// Readable local regular files, following symbolic links.
    QList<QUrl> files;
    /// Remote files, which need private local copies before they can be
    /// attached.
    QList<QUrl> remote;
    QList<Rejected> rejected;
};

/// Splits @p items into local files, remote files and items that cannot be
/// attached. Folders, missing, unreadable and special files are rejected.
/// Duplicates are attached once.
Selection classify(const KFileItemList &items);

/// A short, localized description of why an item cannot be attached.
QString reasonText(Reason reason);

/// The registered handler for new messages.
struct Client {
    /// The desktop file ID, such as `org.kde.kmail2.desktop`.
    QString desktopId;
    QString name;
    /// Whether the client accepts attachments from `mailto:` URLs or the
    /// command line.
    bool acceptsAttachments = true;
};

/// Whether the client with @p desktopId is known to ignore attachments from
/// other applications. Plasma Impresario currently ignores `mailto:` `attach`
/// parameters.
bool ignoresAttachments(const QString &desktopId);

/// The preferred `x-scheme-handler/mailto` application, or nothing when no
/// mail client is registered.
std::optional<Client> preferredClient();

/// The options for `org.freedesktop.portal.Email.ComposeEmail` that attach
/// @p attachments. Register `QList<QDBusUnixFileDescriptor>` with
/// `qDBusRegisterMetaType` before sending them.
QVariantMap portalOptions(const QList<QDBusUnixFileDescriptor> &attachments);

/// @p url for display, without any password it contains.
QString displayName(const QUrl &url);
}
