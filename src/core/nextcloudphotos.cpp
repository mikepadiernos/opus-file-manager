// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudphotos.h"

#include "nextcloudaccount.h"
#include "nextcloudclient.h"

#include <KIO/DavJob>
#include <KIO/MkdirJob>

#include <KIO/SimpleJob>
#include <KIO/SpecialJob>
#include <KIO/TransferJob>
#include <KLocalizedString>

#include <QCollator>
#include <QDataStream>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimeZone>
#include <QXmlStreamReader>

#include <algorithm>
#include <memory>

namespace
{
const QString davNamespace = QStringLiteral("DAV:");

/// KIO::DAV_COPY, from KIO's private httpmethod_p.h, which the HTTP worker's generic WebDAV command takes.
constexpr int davCopyMethod = 9;

/// The properties of one d:response that FileMetadata doesn't hold.
struct PhotoResponse {
    /// The path as parseMetadata() reports it, to skip the same responses.
    QString prettyPath;
    /// The fully decoded path.
    QString path;
    QHash<QString, QString> properties;
    QStringList collaborators;
};

QString normalizedPath(QString path)
{
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

QStringList readCollaborators(QXmlStreamReader &reader)
{
    QStringList names;
    QString id;
    QString label;
    int depth = 1;
    while (!reader.atEnd() && depth > 0) {
        reader.readNext();
        if (reader.isStartElement()) {
            ++depth;
            if (reader.name() == QLatin1String("id")) {
                id = reader.readElementText().trimmed();
                --depth;
            } else if (reader.name() == QLatin1String("label")) {
                label = reader.readElementText().trimmed();
                --depth;
            } else if (reader.name() == QLatin1String("collaborator")) {
                id.clear();
                label.clear();
            }
        } else if (reader.isEndElement()) {
            --depth;
            if (reader.name() == QLatin1String("collaborator")) {
                const QString name = label.isEmpty() ? id : label;
                if (!name.isEmpty()) {
                    names.append(name);
                }
            }
        }
    }
    return names;
}

std::optional<QList<PhotoResponse>> parseResponses(const QByteArray &xml)
{
    QList<PhotoResponse> responses;
    PhotoResponse response;
    QHash<QString, QString> properties;
    QStringList collaborators;
    QString status;
    bool inProp = false;
    bool multistatus = false;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (inProp) {
                const QString name = reader.name().toString();
                if (name == QLatin1String("collaborators")) {
                    collaborators = readCollaborators(reader);
                } else {
                    properties.insert(name, reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed());
                }
                continue;
            }
            if (reader.namespaceUri() != davNamespace) {
                continue;
            }
            if (reader.name() == QLatin1String("multistatus")) {
                multistatus = true;
            } else if (reader.name() == QLatin1String("response")) {
                response = PhotoResponse();
            } else if (reader.name() == QLatin1String("propstat")) {
                properties.clear();
                collaborators.clear();
                status.clear();
            } else if (reader.name() == QLatin1String("prop")) {
                inProp = true;
            } else if (reader.name() == QLatin1String("href")) {
                const QUrl href = QUrl::fromEncoded(reader.readElementText().trimmed().toUtf8());
                response.prettyPath = normalizedPath(href.path());
                response.path = normalizedPath(href.path(QUrl::FullyDecoded));
            } else if (reader.name() == QLatin1String("status")) {
                status = reader.readElementText();
            }
        } else if (reader.isEndElement() && reader.namespaceUri() == davNamespace) {
            if (reader.name() == QLatin1String("prop")) {
                inProp = false;
            } else if (reader.name() == QLatin1String("propstat")) {
                if (status.contains(QLatin1String(" 200"))) {
                    response.properties.insert(properties);
                    response.collaborators.append(collaborators);
                }
            } else if (reader.name() == QLatin1String("response") && !response.prettyPath.isEmpty()) {
                responses.append(response);
            }
        }
    }
    if (reader.hasError() || !multistatus) {
        return std::nullopt;
    }
    return responses;
}

QString lastSegment(const QString &path)
{
    return path.section(QLatin1Char('/'), -1);
}

QDateTime unixTime(const QString &text)
{
    bool ok = false;
    const qint64 seconds = text.toLongLong(&ok);
    return ok && seconds > 0 ? QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::UTC) : QDateTime();
}

QDateTime captureDate(const QList<Nextcloud::MetadataField> &metadata)
{
    for (const Nextcloud::MetadataField &field : metadata) {
        if (field.property == QLatin1String("metadata-photos-original_date_time") && field.key.isEmpty()) {
            if (const QDateTime date = unixTime(field.value); date.isValid()) {
                return date;
            }
            const QDateTime date = QDateTime::fromString(field.value, Qt::ISODate);
            if (date.isValid()) {
                return date;
            }
        }
    }
    for (const Nextcloud::MetadataField &field : metadata) {
        if (field.property == QLatin1String("metadata-photos-exif") && field.key == QLatin1String("DateTimeOriginal")) {
            // EXIF dates have no time zone; they are shown as they were recorded.
            QDateTime date = QDateTime::fromString(field.value, QStringLiteral("yyyy:MM:dd HH:mm:ss"));
            if (date.isValid()) {
                date.setTimeZone(QTimeZone::UTC);
                return date;
            }
        }
    }
    return {};
}

QString metadataValue(const QList<Nextcloud::MetadataField> &metadata, const QString &property, const QString &key = QString())
{
    for (const Nextcloud::MetadataField &field : metadata) {
        if (field.property == property && field.key == key) {
            return field.value;
        }
    }
    return {};
}

QString photosHomePath(const NextcloudAccounts::Account &account)
{
    return NextcloudAccounts::davUrl(account, QStringLiteral("photos/") + account.user + QLatin1Char('/')).path();
}

/// The segments of @p path below the user's Photos home, or nothing when it isn't below it or has empty or relative segments.
std::optional<QStringList> photoSegments(const NextcloudAccounts::Account &account, const QString &path)
{
    const QString home = photosHomePath(account);
    if (account.user.isEmpty() || !path.startsWith(home) || path.endsWith(QLatin1Char('/'))) {
        return std::nullopt;
    }
    const QStringList segments = path.mid(home.size()).split(QLatin1Char('/'));
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String(".") || segment == QLatin1String("..") || segment.contains(QLatin1Char('\\'))) {
            return std::nullopt;
        }
    }
    return segments;
}

bool isAlbumFolder(const QString &folder)
{
    return folder == QLatin1String("albums") || folder == QLatin1String("sharedalbums");
}

QUrl davPathUrl(const NextcloudAccounts::Account &account, const QString &path)
{
    QUrl url = NextcloudAccounts::davUrl(account, QString());
    url.setPath(path, QUrl::DecodedMode);
    return url;
}

QString photosUnavailable()
{
    return i18nc("@info", "Nextcloud Photos isn't available on this server. Its Photos app may be disabled or not installed.");
}

bool notFound(int kioError, int code)
{
    return code == 404 || code == 405 || code == 501 || (code == 0 && kioError == KIO::ERR_DOES_NOT_EXIST);
}

QString sortKey(const Nextcloud::Photo &photo)
{
    return photo.name.isEmpty() ? lastSegment(photo.file.path) : photo.name;
}
}

QString Nextcloud::photoCollectionFolder(PhotoCollectionKind kind)
{
    switch (kind) {
    case PhotoCollectionKind::Albums:
        return QStringLiteral("albums");
    case PhotoCollectionKind::SharedAlbums:
        return QStringLiteral("sharedalbums");
    case PhotoCollectionKind::Places:
        return QStringLiteral("places");
    }
    return {};
}

QUrl Nextcloud::photoCollectionsUrl(const NextcloudAccounts::Account &account, PhotoCollectionKind kind, const QString &folder)
{
    return NextcloudAccounts::davUrl(account,
                                     QStringLiteral("photos/") + account.user + QLatin1Char('/') + (folder.isEmpty() ? photoCollectionFolder(kind) : folder)
                                         + QLatin1Char('/'));
}

QString Nextcloud::photosRequest()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\"><d:prop>"
        "<d:resourcetype/><d:displayname/><d:getlastmodified/><d:getetag/><d:getcontenttype/><d:getcontentlength/>"
        "<oc:fileid/><oc:permissions/><oc:favorite/><nc:has-preview/>"
        "<nc:file-name/><nc:realpath/><nc:nbItems/><nc:location/><nc:dateRange/><nc:collaborators/>"
        "<nc:metadata-photos-original_date_time/><nc:metadata-photos-exif/><nc:metadata-photos-place/><nc:metadata-photos-gps/>"
        "<nc:metadata-photos-size/>"
        "</d:prop></d:propfind>");
}

std::optional<QList<Nextcloud::PhotoCollection>> Nextcloud::parsePhotoCollections(const QByteArray &xml, PhotoCollectionKind kind, QString *error)
{
    const std::optional<QList<PhotoResponse>> responses = parseResponses(xml);
    const std::optional<QList<FileMetadata>> metadata = parseMetadata(xml);
    if (!responses || !metadata || responses->size() != metadata->size()) {
        if (error) {
            *error = i18nc("@info", "The server's albums could not be read.");
        }
        return std::nullopt;
    }
    QList<PhotoCollection> collections;
    for (qsizetype i = 0; i < responses->size(); ++i) {
        const PhotoResponse &response = responses->at(i);
        // The first response is the folder that holds the collections.
        if (i == 0 || !metadata->at(i).folder) {
            continue;
        }
        PhotoCollection collection;
        collection.kind = kind;
        collection.path = response.path;
        collection.name = lastSegment(response.path);
        bool ok = false;
        const int count = response.properties.value(QStringLiteral("nbItems")).toInt(&ok);
        if (ok) {
            collection.itemCount = count;
        }
        collection.location = response.properties.value(QStringLiteral("location"));
        const QJsonObject range = QJsonDocument::fromJson(response.properties.value(QStringLiteral("dateRange")).toUtf8()).object();
        collection.start = unixTime(QString::number(range.value(QLatin1String("start")).toVariant().toLongLong()));
        collection.end = unixTime(QString::number(range.value(QLatin1String("end")).toVariant().toLongLong()));
        collection.permissions = metadata->at(i).permissions;
        collection.collaborators = response.collaborators;
        collections.append(collection);
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(collections.begin(), collections.end(), [&collator](const PhotoCollection &a, const PhotoCollection &b) {
        return collator.compare(a.name, b.name) < 0;
    });
    if (error) {
        error->clear();
    }
    return collections;
}

std::optional<QList<Nextcloud::Photo>> Nextcloud::parsePhotos(const QByteArray &xml, const QString &collectionPath, QString *error)
{
    const std::optional<QList<PhotoResponse>> responses = parseResponses(xml);
    const std::optional<QList<FileMetadata>> metadata = parseMetadata(xml);
    if (!responses || !metadata || responses->size() != metadata->size()) {
        if (error) {
            *error = i18nc("@info", "The server's photos could not be read.");
        }
        return std::nullopt;
    }
    const QString collection = normalizedPath(collectionPath);
    QList<Photo> photos;
    for (qsizetype i = 0; i < responses->size(); ++i) {
        const PhotoResponse &response = responses->at(i);
        const FileMetadata &file = metadata->at(i);
        if (response.path == collection || file.folder || !response.path.startsWith(collection + QLatin1Char('/'))) {
            continue;
        }
        Photo photo;
        photo.file = file;
        photo.file.path = response.path;
        photo.realPath = response.properties.value(QStringLiteral("realpath"));
        photo.name = response.properties.value(QStringLiteral("file-name"));
        if (photo.name.isEmpty()) {
            photo.name = lastSegment(response.path);
            // Album entries are named "<file id>-<file name>".
            if (!file.fileId.isEmpty() && photo.name.startsWith(file.fileId + QLatin1Char('-'))) {
                photo.name = photo.name.mid(file.fileId.size() + 1);
            }
        }
        photo.taken = captureDate(file.metadata);
        photo.place = metadataValue(file.metadata, QStringLiteral("metadata-photos-place"));
        bool latitudeOk = false;
        bool longitudeOk = false;
        const double latitude = metadataValue(file.metadata, QStringLiteral("metadata-photos-gps"), QStringLiteral("latitude")).toDouble(&latitudeOk);
        const double longitude = metadataValue(file.metadata, QStringLiteral("metadata-photos-gps"), QStringLiteral("longitude")).toDouble(&longitudeOk);
        if (latitudeOk && longitudeOk) {
            photo.gps = QPointF(latitude, longitude);
        }
        photos.append(photo);
    }
    if (error) {
        error->clear();
    }
    return photos;
}

QList<Nextcloud::PhotoGroup> Nextcloud::arrangePhotos(QList<Photo> photos, PhotoSort sort, PhotoGrouping grouping)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const auto byName = [&collator](const Photo &a, const Photo &b) {
        return collator.compare(sortKey(a), sortKey(b)) < 0;
    };
    std::stable_sort(photos.begin(), photos.end(), [&](const Photo &a, const Photo &b) {
        switch (sort) {
        case PhotoSort::NewestFirst:
        case PhotoSort::OldestFirst:
            if (a.taken.isValid() != b.taken.isValid()) {
                return a.taken.isValid();
            }
            if (a.taken != b.taken) {
                return sort == PhotoSort::NewestFirst ? a.taken > b.taken : a.taken < b.taken;
            }
            return byName(a, b);
        case PhotoSort::Place:
            if (a.place.isEmpty() != b.place.isEmpty()) {
                return !a.place.isEmpty();
            }
            if (const int order = collator.compare(a.place, b.place); order != 0) {
                return order < 0;
            }
            return byName(a, b);
        case PhotoSort::Name:
            break;
        }
        return byName(a, b);
    });

    QList<PhotoGroup> groups;
    if (grouping == PhotoGrouping::None) {
        if (!photos.isEmpty()) {
            groups.append({QString(), QString(), photos});
        }
        return groups;
    }
    QHash<QString, qsizetype> index;
    PhotoGroup unknown;
    unknown.title =
        grouping == PhotoGrouping::Month ? i18nc("@title:group photos without a date", "No date") : i18nc("@title:group photos without a place", "No place");
    for (const Photo &photo : std::as_const(photos)) {
        QString key;
        QString title;
        if (grouping == PhotoGrouping::Month && photo.taken.isValid()) {
            const QDate date = photo.taken.date();
            key = date.toString(QStringLiteral("yyyy-MM"));
            title = QLocale().toString(date, QStringLiteral("MMMM yyyy"));
        } else if (grouping == PhotoGrouping::Place && !photo.place.isEmpty()) {
            key = photo.place;
            title = photo.place;
        }
        if (key.isEmpty()) {
            unknown.photos.append(photo);
            continue;
        }
        if (!index.contains(key)) {
            index.insert(key, groups.size());
            groups.append({key, title, {}});
        }
        groups[index.value(key)].photos.append(photo);
    }
    if (grouping == PhotoGrouping::Place) {
        std::stable_sort(groups.begin(), groups.end(), [&collator](const PhotoGroup &a, const PhotoGroup &b) {
            return collator.compare(a.title, b.title) < 0;
        });
    }
    if (!unknown.photos.isEmpty()) {
        groups.append(unknown);
    }
    return groups;
}

int Nextcloud::photosWithoutCaptureDate(const QList<Photo> &photos)
{
    return static_cast<int>(std::count_if(photos.cbegin(), photos.cend(), [](const Photo &photo) {
        return !photo.taken.isValid();
    }));
}

QUrl Nextcloud::photoFileUrl(const NextcloudAccounts::Account &account, const Photo &photo)
{
    const QString prefix = QLatin1Char('/') + account.user + QStringLiteral("/files/");
    if (account.user.isEmpty() || !photo.realPath.startsWith(prefix) || photo.realPath.size() == prefix.size()) {
        return {};
    }
    const QStringList segments = photo.realPath.mid(prefix.size()).split(QLatin1Char('/'));
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String(".") || segment == QLatin1String("..")) {
            return {};
        }
    }
    QUrl url = NextcloudAccounts::filesUrl(account);
    url.setPath(url.path(QUrl::FullyDecoded) + segments.join(QLatin1Char('/')), QUrl::DecodedMode);
    return url;
}

QString Nextcloud::albumNameError(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return i18nc("@info", "Enter a name for the album.");
    }
    if (trimmed == QLatin1String(".") || trimmed == QLatin1String("..") || trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\'))) {
        return i18nc("@info", "An album's name can't contain slashes or be \".\" or \"..\".");
    }
    if (std::any_of(trimmed.cbegin(), trimmed.cend(), [](QChar character) {
            return character.category() == QChar::Other_Control;
        })) {
        return i18nc("@info", "An album's name can't contain control characters.");
    }
    if (trimmed.size() > 250) {
        return i18nc("@info", "The album's name is too long.");
    }
    return {};
}

QUrl Nextcloud::albumUrl(const NextcloudAccounts::Account &account, const QString &albumPath, QString *error)
{
    const std::optional<QStringList> segments = photoSegments(account, albumPath);
    if (!segments || segments->size() != 2 || !isAlbumFolder(segments->first())) {
        if (error) {
            *error = i18nc("@info", "Photos can only be added to your albums and albums shared with you.");
        }
        return {};
    }
    if (error) {
        error->clear();
    }
    return davPathUrl(account, albumPath);
}

QUrl Nextcloud::albumEntryUrl(const NextcloudAccounts::Account &account, const QString &entryPath, QString *error)
{
    const std::optional<QStringList> segments = photoSegments(account, entryPath);
    if (!segments || segments->size() != 3 || !isAlbumFolder(segments->first())) {
        if (error) {
            *error = i18nc("@info", "Only photos in your albums and albums shared with you can be removed from them.");
        }
        return {};
    }
    if (error) {
        error->clear();
    }
    return davPathUrl(account, entryPath);
}

bool Nextcloud::canAddPhotos(const PhotoCollection &collection)
{
    if (collection.kind == PhotoCollectionKind::Places) {
        return false;
    }
    return collection.permissions.isEmpty() || parsePermissions(collection.permissions).testFlag(Permission::CreateFile);
}

bool Nextcloud::canRemovePhotos(const PhotoCollection &collection)
{
    if (collection.kind == PhotoCollectionKind::Places) {
        return false;
    }
    return collection.permissions.isEmpty() || parsePermissions(collection.permissions).testFlag(Permission::Delete);
}

int NextcloudClient::listPhotoCollections(const NextcloudAccounts::Account &account, Nextcloud::PhotoCollectionKind kind)
{
    const int request = ++m_lastRequest;
    listPhotoCollectionsIn(account, kind, request, QString());
    return request;
}

void NextcloudClient::listPhotoCollectionsIn(const NextcloudAccounts::Account &account, Nextcloud::PhotoCollectionKind kind, int request, const QString &folder)
{
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job =
        KIO::davPropFind(Nextcloud::photoCollectionsUrl(account, kind, folder), Nextcloud::photosRequest(), QStringLiteral("1"), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, account, kind, request, folder] {
        const int code = Nextcloud::httpStatus(job);
        Nextcloud::PhotoCollectionListing listing;
        listing.kind = kind;
        if (notFound(job->error(), code)) {
            if (kind == Nextcloud::PhotoCollectionKind::Places && folder.isEmpty()) {
                // Some versions of Photos name the places collection "locations".
                listPhotoCollectionsIn(account, kind, request, QStringLiteral("locations"));
                return;
            }
            listing.unavailable = true;
            listing.error = kind == Nextcloud::PhotoCollectionKind::Places
                ? i18nc("@info", "This server's Photos app doesn't group photos by place, or Photos isn't available.")
                : photosUnavailable();
        } else if (code == 403) {
            listing.error = i18nc("@info", "The server doesn't allow you to see these albums.");
        } else if (job->error() || code >= 400) {
            listing.error = Nextcloud::errorMessage(job->error(), code, job->errorString());
        } else {
            listing.collections = Nextcloud::parsePhotoCollections(job->responseData(), kind, &listing.error).value_or(QList<Nextcloud::PhotoCollection>());
        }
        Q_EMIT photoCollectionsListed(request, listing);
    });
}

int NextcloudClient::listPhotos(const NextcloudAccounts::Account &account, const Nextcloud::PhotoCollection &collection)
{
    const int request = ++m_lastRequest;
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job =
        KIO::davPropFind(davPathUrl(account, collection.path + QLatin1Char('/')), Nextcloud::photosRequest(), QStringLiteral("1"), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    const QString path = collection.path;
    connect(job, &KJob::result, this, [this, job, request, path] {
        const int code = Nextcloud::httpStatus(job);
        Nextcloud::PhotoListing listing;
        listing.collectionPath = path;
        if (code == 403) {
            listing.error = i18nc("@info", "The album's owner or the server doesn't allow you to see its photos.");
        } else if (code == 404) {
            listing.error = i18nc("@info", "The album no longer exists.");
        } else if (job->error() || code >= 400) {
            listing.error = Nextcloud::errorMessage(job->error(), code, job->errorString());
        } else {
            listing.photos = Nextcloud::parsePhotos(job->responseData(), path, &listing.error).value_or(QList<Nextcloud::Photo>());
        }
        Q_EMIT photosListed(request, listing);
    });
    return request;
}

void NextcloudClient::addToAlbum(const NextcloudAccounts::Account &account, const QUrl &file, const QString &albumPath)
{
    QString error;
    const QUrl album = Nextcloud::albumUrl(account, albumPath, &error);
    const QUrl files = NextcloudAccounts::filesUrl(account);
    const QString filePath = file.path(QUrl::FullyDecoded);
    if (error.isEmpty()
        && (file.scheme() != files.scheme() || file.host() != files.host() || file.port() != files.port() || !filePath.startsWith(files.path())
            || filePath.endsWith(QLatin1Char('/')) || filePath.size() == files.path().size())) {
        error = i18nc("@info", "Only files in your Nextcloud files can be added to an album.");
    }
    if (!error.isEmpty()) {
        Q_EMIT photoAdded(file, albumPath, error);
        return;
    }
    // COPY into an album adds a reference to the file; the file stays where it is.
    QUrl destination = account.server;
    destination.setPath(albumPath + QLatin1Char('/') + lastSegment(filePath), QUrl::DecodedMode);
    QByteArray arguments;
    QDataStream stream(&arguments, QIODevice::WriteOnly);
    stream << int(7) << file << davCopyMethod << static_cast<qint64>(0);
    auto *job = new KIO::SpecialJob(file, arguments);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    const QString headers = job->outgoingMetaData().value(QStringLiteral("customHTTPHeader"));
    job->addMetaData(QStringLiteral("customHTTPHeader"),
                     headers + QStringLiteral("\r\nDestination: ") + QString::fromLatin1(destination.toEncoded()) + QStringLiteral("\r\nOverwrite: F"));
    connect(job, &KIO::TransferJob::dataReq, this, [](KIO::Job *, QByteArray &data) {
        data.clear();
    });
    connect(job, &KJob::result, this, [this, job, file, albumPath] {
        const int code = Nextcloud::httpStatus(job);
        QString error;
        // 412 means the photo is already in the album.
        if (code == 412 || code == 201 || code == 204 || job->error() == KIO::ERR_NO_CONTENT) {
            error.clear();
        } else if (code == 403) {
            error = i18nc("@info", "The album's owner or the server doesn't allow you to add photos to this album.");
        } else if (code == 404 || code == 409) {
            error = i18nc("@info", "The photo or the album no longer exists.");
        } else if (code == 415) {
            error = i18nc("@info", "Only photos and videos can be added to an album.");
        } else if (job->error() || code >= 400) {
            error = Nextcloud::errorMessage(job->error(), code, job->errorString());
        }
        Q_EMIT photoAdded(file, albumPath, error);
    });
}

void NextcloudClient::removeFromAlbum(const NextcloudAccounts::Account &account, const Nextcloud::Photo &photo)
{
    const QString entryPath = photo.file.path;
    QString error;
    const QUrl entry = Nextcloud::albumEntryUrl(account, entryPath, &error);
    if (!entry.isValid()) {
        // Never send a request that could delete the file itself.
        Q_EMIT photoRemoved(entryPath, error);
        return;
    }
    KIO::TransferJob *job = KIO::http_delete(entry, KIO::HideProgressInfo);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, entryPath] {
        const int code = Nextcloud::httpStatus(job);
        QString error;
        // 404 means the photo already isn't in the album.
        if (code == 404 || code == 200 || code == 204 || job->error() == KIO::ERR_NO_CONTENT) {
            error.clear();
        } else if (code == 403) {
            error = i18nc("@info", "The album's owner or the server doesn't allow you to remove this photo from the album.");
        } else if (job->error() || code >= 400) {
            error = Nextcloud::errorMessage(job->error(), code, job->errorString());
        }
        Q_EMIT photoRemoved(entryPath, error);
    });
}

void NextcloudClient::createAlbum(const NextcloudAccounts::Account &account, const QString &name)
{
    const QString id = account.id;
    const QString trimmed = name.trimmed();
    if (const QString invalid = Nextcloud::albumNameError(name); !invalid.isEmpty()) {
        Q_EMIT albumCreated(id, trimmed, invalid);
        return;
    }
    QUrl url = Nextcloud::photoCollectionsUrl(account, Nextcloud::PhotoCollectionKind::Albums);
    url.setPath(url.path(QUrl::FullyDecoded) + trimmed, QUrl::DecodedMode);
    KIO::SimpleJob *job = KIO::mkdir(url);
    job->setUiDelegate(nullptr);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, id, trimmed] {
        const int code = Nextcloud::httpStatus(job);
        QString error;
        if (code == 405 || job->error() == KIO::ERR_DIR_ALREADY_EXIST) {
            error = i18nc("@info", "An album with this name already exists.");
        } else if (code == 404) {
            error = photosUnavailable();
        } else if (code == 403) {
            error = i18nc("@info", "The server doesn't allow you to create albums.");
        } else if (job->error() || (code >= 400)) {
            error = Nextcloud::errorMessage(job->error(), code, job->errorString());
        }
        Q_EMIT albumCreated(id, trimmed, error);
    });
}
