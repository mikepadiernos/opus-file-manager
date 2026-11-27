// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudassets.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QString>
#include <QUrl>

#include <optional>

namespace NextcloudAccounts
{
struct Account;
}

/**
 * The albums, shared albums and places of the Nextcloud Photos app, from its
 * WebDAV collections under remote.php/dav/photos/<user>/. An album holds
 * references to files: removing a photo from an album never deletes the file.
 */
namespace Nextcloud
{
enum class PhotoCollectionKind {
    /// The user's own albums, in albums/.
    Albums,
    /// Albums others shared with the user, in sharedalbums/.
    SharedAlbums,
    /// Photos grouped by where they were taken, in places/. Read-only.
    Places,
};

/// An album or place.
struct PhotoCollection {
    PhotoCollectionKind kind = PhotoCollectionKind::Albums;
    QString name;
    /// The collection's decoded server path, such as /remote.php/dav/photos/alice/albums/Trip.
    QString path;
    /// nc:nbItems: how many photos the server says the collection holds.
    std::optional<int> itemCount;
    /// nc:location: the place the album's owner gave it.
    QString location;
    /// nc:dateRange: when the album's photos were taken.
    QDateTime start;
    QDateTime end;
    /// oc:permissions letters, when the server reports them.
    QString permissions;
    /// The names of the album's collaborators, from nc:collaborators.
    QStringList collaborators;

    bool operator==(const PhotoCollection &) const = default;
};

/// A photo in an album or place.
struct Photo {
    /// The server's properties of the photo; file.path is the entry's path in the collection.
    FileMetadata file;
    /// The photo's file name, from nc:file-name or the entry's name.
    QString name;
    /// nc:realpath: where the file is stored, such as /alice/files/Photos/a.jpg.
    QString realPath;
    /// When the photo was taken, from the server's metadata, or invalid when the server doesn't know.
    QDateTime taken;
    /// The place the server's reverse geocoding found, from nc:metadata-photos-place.
    QString place;
    /// Latitude and longitude, from nc:metadata-photos-gps.
    std::optional<QPointF> gps;
};

/// What listing the collections of one kind returned.
struct PhotoCollectionListing {
    PhotoCollectionKind kind = PhotoCollectionKind::Albums;
    QList<PhotoCollection> collections;
    QString error;
    /// Whether the server doesn't offer these collections, such as when the Photos app is missing.
    bool unavailable = false;
};

/// What listing the photos of a collection returned.
struct PhotoListing {
    QString collectionPath;
    QList<Photo> photos;
    QString error;
    bool unavailable = false;
};

enum class PhotoSort {
    NewestFirst,
    OldestFirst,
    Name,
    Place,
};

enum class PhotoGrouping {
    None,
    Month,
    Place,
};

/// Photos that share a month or place.
struct PhotoGroup {
    /// "2026-11" or the place's name, or empty for photos without a date or place.
    QString key;
    QString title;
    QList<Photo> photos;
};

/// The server folder name of @p kind, such as "albums".
QString photoCollectionFolder(PhotoCollectionKind kind);
/// The webdav URL of the collections of @p kind; @p folder overrides the folder's name.
QUrl photoCollectionsUrl(const NextcloudAccounts::Account &account, PhotoCollectionKind kind, const QString &folder = QString());
/// The PROPFIND body that asks for the properties of collections and photos.
QString photosRequest();
/**
 * Parses a PROPFIND response of the collections of @p kind, leaving out the
 * folder itself, sorted by name.
 * @return nullopt, setting @p error, when @p xml isn't a multistatus response
 */
std::optional<QList<PhotoCollection>> parsePhotoCollections(const QByteArray &xml, PhotoCollectionKind kind, QString *error = nullptr);
/**
 * Parses a PROPFIND response of the collection at @p collectionPath into its
 * photos, in the server's order.
 */
std::optional<QList<Photo>> parsePhotos(const QByteArray &xml, const QString &collectionPath, QString *error = nullptr);
/**
 * Sorts @p photos by capture date, name or place and groups them by the
 * month they were taken or their place. Photos without a capture date or
 * place come last, in a group of their own.
 */
QList<PhotoGroup> arrangePhotos(QList<Photo> photos, PhotoSort sort, PhotoGrouping grouping);
/// How many of @p photos the server reports no capture date for.
int photosWithoutCaptureDate(const QList<Photo> &photos);

/// The webdav URL of the file behind @p photo, or an empty URL when it isn't in the files of @p account.
QUrl photoFileUrl(const NextcloudAccounts::Account &account, const Photo &photo);
/// Why @p name can't be an album's name, or an empty string.
QString albumNameError(const QString &name);
/**
 * The webdav URL of the album at the decoded server path @p albumPath, which
 * must be one of the user's albums or shared albums: never a place, and
 * never a file. Sets @p error and returns an empty URL otherwise.
 */
QUrl albumUrl(const NextcloudAccounts::Account &account, const QString &albumPath, QString *error = nullptr);
/**
 * The webdav URL of the entry at the decoded server path @p entryPath, which
 * must be a photo directly in one of the user's albums or shared albums.
 * Deleting this URL removes the photo from the album and leaves the file in
 * place. Sets @p error and returns an empty URL for anything else, such as a
 * file in the user's files, an album itself or a place.
 */
QUrl albumEntryUrl(const NextcloudAccounts::Account &account, const QString &entryPath, QString *error = nullptr);
/// Whether @p collection allows adding photos: never for places, and always when the server doesn't report permissions.
bool canAddPhotos(const PhotoCollection &collection);
/// Whether @p collection allows removing photos: never for places, and always when the server doesn't report permissions.
bool canRemovePhotos(const PhotoCollection &collection);
}
