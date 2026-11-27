// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailhandoff.h"

#include <KApplicationTrader>
#include <KLocalizedString>
#include <KService>

#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace MailHandoff
{
Selection classify(const KFileItemList &items)
{
    Selection selection;
    QSet<QString> seenFiles;
    QSet<QUrl> seenRemote;
    for (const KFileItem &item : items) {
        const QString localPath = item.localPath();
        if (localPath.isEmpty()) {
            if (item.isDir()) {
                selection.rejected.append({item.url(), Reason::Folder});
            } else if (!seenRemote.contains(item.url())) {
                seenRemote.insert(item.url());
                selection.remote.append(item.url());
            }
            continue;
        }
        const QFileInfo info(localPath);
        if (!info.exists()) {
            selection.rejected.append({item.url(), Reason::Missing});
        } else if (info.isDir()) {
            selection.rejected.append({item.url(), Reason::Folder});
        } else if (!info.isFile()) {
            selection.rejected.append({item.url(), Reason::NotRegularFile});
        } else if (!info.isReadable()) {
            selection.rejected.append({item.url(), Reason::Unreadable});
        } else {
            const QString target = info.canonicalFilePath();
            if (!seenFiles.contains(target)) {
                seenFiles.insert(target);
                selection.files.append(QUrl::fromLocalFile(info.absoluteFilePath()));
            }
        }
    }
    return selection;
}

QString reasonText(Reason reason)
{
    switch (reason) {
    case Reason::Folder:
        return i18nc("@info why an item cannot be attached", "Folders cannot be attached");
    case Reason::Missing:
        return i18nc("@info why an item cannot be attached", "The item no longer exists");
    case Reason::Unreadable:
        return i18nc("@info why an item cannot be attached", "You do not have permission to read it");
    case Reason::NotRegularFile:
        return i18nc("@info why an item cannot be attached", "Not a regular file");
    }
    return {};
}

bool ignoresAttachments(const QString &desktopId)
{
    static const QStringList ignoring{
        QStringLiteral("io.github.mikepadiernos.plasma_impresario.desktop"),
    };
    return ignoring.contains(desktopId);
}

std::optional<Client> preferredClient()
{
    const KService::Ptr service = KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/mailto"));
    if (!service) {
        return std::nullopt;
    }
    Client client;
    client.desktopId = service->storageId();
    client.name = service->name();
    client.acceptsAttachments = !ignoresAttachments(client.desktopId);
    return client;
}

QVariantMap portalOptions(const QList<QDBusUnixFileDescriptor> &attachments)
{
    QVariantMap options;
    if (!attachments.isEmpty()) {
        options.insert(QStringLiteral("attachment_fds"), QVariant::fromValue(attachments));
    }
    return options;
}

QString displayName(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toDisplayString(QUrl::RemovePassword | QUrl::PreferLocalFile);
}

namespace
{
/// Plasma Impresario, like Trojitá before it, drags only these references.
bool isPrivateMailFormat(const QString &format)
{
    return format == QLatin1String("application/x-trojita-imap-part") || format == QLatin1String("application/x-trojita-attachments-list")
        || format == QLatin1String("application/x-trojita-message-list");
}

const QLatin1String SuggestedFileName("application/x-kde-suggestedfilename");
}

DropKind classifyDrop(const QMimeData *mime)
{
    if (!mime) {
        return DropKind::None;
    }
    if (mime->hasUrls()) {
        return DropKind::Urls;
    }
    const QStringList formats = mime->formats();
    const bool hasPrivate = std::any_of(formats.cbegin(), formats.cend(), isPrivateMailFormat);
    const bool hasContent = formats.contains(SuggestedFileName) && std::any_of(formats.cbegin(), formats.cend(), [](const QString &format) {
                                return format != SuggestedFileName && !isPrivateMailFormat(format) && !format.startsWith(QLatin1String("application/x-qt-"));
                            });
    if (hasContent) {
        return DropKind::Content;
    }
    return hasPrivate ? DropKind::PrivateMailReference : DropKind::None;
}

void copyWithoutPrivateMailReferences(const QMimeData *from, QMimeData *to)
{
    const QStringList formats = from->formats();
    for (const QString &format : formats) {
        if (!isPrivateMailFormat(format)) {
            to->setData(format, from->data(format));
        }
    }
}
}
