// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

namespace NextcloudAccounts
{
struct Account;
}

/// What a Nextcloud server reports about a file through its WebDAV interface.
namespace Nextcloud
{
/// One value of a server metadata property, such as nc:metadata-photos-exif.
struct MetadataField {
    /// The property name without its namespace, such as "metadata-photos-exif".
    QString property;
    /// The child element or JSON key within the property, or empty when the property is a single value.
    QString key;
    QString value;

    bool operator==(const MetadataField &) const = default;
};

/// The server's properties of a file or folder, as read with PROPFIND.
struct FileMetadata {
    /// The path of the item on the server, decoded, such as /remote.php/dav/files/alice/a.jpg.
    QString path;
    bool folder = false;
    /// oc:fileid, which the preview and tag interfaces need.
    QString fileId;
    QString ownerId;
    QString ownerDisplayName;
    /// oc:size for folders, d:getcontentlength for files.
    std::optional<qint64> size;
    QDateTime modified;
    QString etag;
    QString contentType;
    /// oc:permissions letters; parse them with parsePermissions().
    QString permissions;
    /// nc:mount-type, such as "shared" or "group".
    QString mountType;
    std::optional<bool> favorite;
    /// nc:has-preview: whether the server can render a preview.
    std::optional<bool> hasPreview;
    /// The server's collaborative tags of the item, from nc:system-tags or oc:tags.
    QStringList serverTags;
    /// oc:share-types: how the item is shared, as Nextcloud share type numbers.
    QList<int> shareTypes;
    /// nc:metadata-* and nc:file-metadata-* values, in the server's order.
    QList<MetadataField> metadata;
    /// Properties the server answered with an error other than "not found", such as 403.
    QStringList unavailable;
};

/// The PROPFIND body that asks for the properties of FileMetadata.
QString metadataRequest();
/**
 * Parses a PROPFIND multistatus response into one FileMetadata per response,
 * in order. Properties the server doesn't have are left empty.
 * @return nullopt, setting @p error, when @p xml isn't a multistatus response
 */
std::optional<QList<FileMetadata>> parseMetadata(const QByteArray &xml, QString *error = nullptr);
/// A name for a Nextcloud share type, such as "Public link" for 3.
QString describeShareType(int type);
/// A label for @p field, such as "Photo EXIF › Make".
QString describeMetadataField(const MetadataField &field);

/// The size the server is asked for when previewing an item at @p size: a power of two from 64 to 2048 pixels.
QSize previewRequestSize(const QSize &size);
/**
 * The URL of the server's preview of the file @p fileId, at most @p size,
 * keeping its aspect ratio, without the file type's icon in place of a
 * missing preview.
 */
QUrl previewUrl(const NextcloudAccounts::Account &account, const QString &fileId, const QSize &size);
/// The most bytes a server preview may have, the same as the panel's limit for images.
constexpr qint64 PreviewByteLimit = 8 * 1024 * 1024;
}
