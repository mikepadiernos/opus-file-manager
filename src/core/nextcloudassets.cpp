// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudassets.h"

#include "nextcloudaccount.h"
#include "nextcloudclient.h"

#include <KIO/DavJob>
#include <KIO/TransferJob>
#include <KLocalizedString>

#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace
{
const QString davNamespace = QStringLiteral("DAV:");
const QString ownCloudNamespace = QStringLiteral("http://owncloud.org/ns");
const QString nextcloudNamespace = QStringLiteral("http://nextcloud.org/ns");

using Leaves = QList<QPair<QString, QString>>;

/**
 * Reads the rest of the current element: its own text and the text of each
 * element without children below it, named by their path from the element,
 * such as "gps/latitude".
 */
void readTree(QXmlStreamReader &reader, const QString &prefix, QString &text, Leaves &leaves)
{
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isCharacters()) {
            text += reader.text();
        } else if (reader.isStartElement()) {
            const QString name = prefix.isEmpty() ? reader.name().toString() : prefix + QLatin1Char('/') + reader.name();
            QString childText;
            Leaves childLeaves;
            readTree(reader, name, childText, childLeaves);
            if (childLeaves.isEmpty()) {
                leaves.append({name, childText.trimmed()});
            } else {
                leaves.append(childLeaves);
            }
        } else if (reader.isEndElement()) {
            return;
        }
    }
}

QString jsonText(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 15);
    }
    if (value.isNull() || value.isUndefined()) {
        return {};
    }
    const QJsonDocument document = value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject());
    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

/// Parses an HTTP date such as "Tue, 03 Nov 2026 10:00:00 GMT", which Qt's RFC 2822 parser rejects for its zone name.
QDateTime httpDate(QString text)
{
    if (text.endsWith(QLatin1String(" GMT")) || text.endsWith(QLatin1String(" UTC"))) {
        text = text.chopped(3) + QStringLiteral("+0000");
    }
    return QDateTime::fromString(text, Qt::RFC2822Date);
}

bool isMetadataProperty(const QString &name)
{
    return name.startsWith(QLatin1String("metadata-")) || name.startsWith(QLatin1String("file-metadata-"));
}

/// Properties read in one propstat, applied to the item when the propstat's status is 200.
struct Properties {
    Nextcloud::FileMetadata values;
    QStringList names;
};

void readProperty(QXmlStreamReader &reader, Properties &properties)
{
    const QString ns = reader.namespaceUri().toString();
    const QString name = reader.name().toString();
    QString text;
    Leaves leaves;
    readTree(reader, QString(), text, leaves);
    text = text.trimmed();
    properties.names.append(name);
    Nextcloud::FileMetadata &values = properties.values;
    if (ns == davNamespace) {
        if (name == QLatin1String("getlastmodified")) {
            values.modified = httpDate(text);
        } else if (name == QLatin1String("getetag")) {
            values.etag = text;
        } else if (name == QLatin1String("getcontenttype")) {
            values.contentType = text;
        } else if (name == QLatin1String("getcontentlength")) {
            bool ok = false;
            const qint64 size = text.toLongLong(&ok);
            if (ok && !values.size) {
                values.size = size;
            }
        } else if (name == QLatin1String("resourcetype")) {
            values.folder = std::any_of(leaves.cbegin(), leaves.cend(), [](const auto &leaf) {
                return leaf.first == QLatin1String("collection");
            });
        }
    } else if (ns == ownCloudNamespace) {
        if (name == QLatin1String("fileid")) {
            values.fileId = text;
        } else if (name == QLatin1String("size")) {
            bool ok = false;
            const qint64 size = text.toLongLong(&ok);
            if (ok) {
                values.size = size;
            }
        } else if (name == QLatin1String("permissions")) {
            values.permissions = text;
        } else if (name == QLatin1String("favorite")) {
            values.favorite = text == QLatin1String("1");
        } else if (name == QLatin1String("owner-id")) {
            values.ownerId = text;
        } else if (name == QLatin1String("owner-display-name")) {
            values.ownerDisplayName = text;
        } else if (name == QLatin1String("share-types")) {
            for (const auto &[key, value] : std::as_const(leaves)) {
                bool ok = false;
                const int type = value.toInt(&ok);
                if (ok && !values.shareTypes.contains(type)) {
                    values.shareTypes.append(type);
                }
            }
        } else if (name == QLatin1String("tags")) {
            for (const auto &[key, value] : std::as_const(leaves)) {
                if (!value.isEmpty() && !values.serverTags.contains(value)) {
                    values.serverTags.append(value);
                }
            }
        }
    } else if (ns == nextcloudNamespace) {
        if (name == QLatin1String("has-preview")) {
            values.hasPreview = text == QLatin1String("true") || text == QLatin1String("1");
        } else if (name == QLatin1String("mount-type")) {
            values.mountType = text;
        } else if (name == QLatin1String("system-tags")) {
            for (const auto &[key, value] : std::as_const(leaves)) {
                if (!value.isEmpty() && !values.serverTags.contains(value)) {
                    values.serverTags.append(value);
                }
            }
        } else if (isMetadataProperty(name)) {
            if (!leaves.isEmpty()) {
                for (const auto &[key, value] : std::as_const(leaves)) {
                    values.metadata.append({name, key, value});
                }
            } else if (text.startsWith(QLatin1Char('{'))) {
                // Nextcloud 25 to 27 report metadata as a JSON object.
                const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
                for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                    values.metadata.append({name, it.key(), jsonText(it.value())});
                }
            } else if (!text.isEmpty()) {
                values.metadata.append({name, QString(), text});
            }
        }
    }
}

void merge(Nextcloud::FileMetadata &target, const Nextcloud::FileMetadata &source, const QStringList &names)
{
    for (const QString &name : names) {
        if (name == QLatin1String("getlastmodified")) {
            target.modified = source.modified;
        } else if (name == QLatin1String("getetag")) {
            target.etag = source.etag;
        } else if (name == QLatin1String("getcontenttype")) {
            target.contentType = source.contentType;
        } else if (name == QLatin1String("getcontentlength")) {
            if (!target.size) {
                target.size = source.size;
            }
        } else if (name == QLatin1String("size")) {
            if (source.size) {
                target.size = source.size;
            }
        } else if (name == QLatin1String("resourcetype")) {
            target.folder = source.folder;
        } else if (name == QLatin1String("fileid")) {
            target.fileId = source.fileId;
        } else if (name == QLatin1String("permissions")) {
            target.permissions = source.permissions;
        } else if (name == QLatin1String("favorite")) {
            target.favorite = source.favorite;
        } else if (name == QLatin1String("owner-id")) {
            target.ownerId = source.ownerId;
        } else if (name == QLatin1String("owner-display-name")) {
            target.ownerDisplayName = source.ownerDisplayName;
        } else if (name == QLatin1String("share-types")) {
            target.shareTypes = source.shareTypes;
        } else if (name == QLatin1String("has-preview")) {
            target.hasPreview = source.hasPreview;
        } else if (name == QLatin1String("mount-type")) {
            target.mountType = source.mountType;
        }
    }
    for (const QString &tag : source.serverTags) {
        if (!target.serverTags.contains(tag)) {
            target.serverTags.append(tag);
        }
    }
    target.metadata.append(source.metadata);
}

QString normalizedPath(QString path)
{
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}
}

QString Nextcloud::metadataRequest()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\"><d:prop>"
        "<d:resourcetype/><d:getlastmodified/><d:getetag/><d:getcontenttype/><d:getcontentlength/>"
        "<oc:fileid/><oc:size/><oc:permissions/><oc:favorite/><oc:owner-id/><oc:owner-display-name/><oc:share-types/><oc:tags/>"
        "<nc:has-preview/><nc:mount-type/><nc:system-tags/>"
        "<nc:metadata-photos-size/><nc:metadata-photos-original_date_time/><nc:metadata-photos-ifd0/><nc:metadata-photos-exif/>"
        "<nc:metadata-photos-gps/><nc:metadata-photos-place/><nc:metadata-files-live-photo/><nc:metadata-blurhash/>"
        "<nc:file-metadata-size/><nc:file-metadata-gps/>"
        "</d:prop></d:propfind>");
}

std::optional<QList<Nextcloud::FileMetadata>> Nextcloud::parseMetadata(const QByteArray &xml, QString *error)
{
    QList<FileMetadata> result;
    QXmlStreamReader reader(xml);
    FileMetadata item;
    Properties properties;
    QString status;
    bool inProp = false;
    bool multistatus = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (inProp) {
                readProperty(reader, properties);
                continue;
            }
            if (reader.namespaceUri() != davNamespace) {
                continue;
            }
            if (reader.name() == QLatin1String("multistatus")) {
                multistatus = true;
            } else if (reader.name() == QLatin1String("response")) {
                item = FileMetadata();
            } else if (reader.name() == QLatin1String("propstat")) {
                properties = Properties();
                status.clear();
            } else if (reader.name() == QLatin1String("prop")) {
                inProp = true;
            } else if (reader.name() == QLatin1String("href")) {
                item.path = normalizedPath(QUrl::fromEncoded(reader.readElementText().trimmed().toUtf8()).path());
            } else if (reader.name() == QLatin1String("status")) {
                status = reader.readElementText();
            }
        } else if (reader.isEndElement() && reader.namespaceUri() == davNamespace) {
            if (reader.name() == QLatin1String("prop")) {
                inProp = false;
            } else if (reader.name() == QLatin1String("propstat")) {
                if (status.contains(QLatin1String(" 200"))) {
                    merge(item, properties.values, properties.names);
                } else if (!status.contains(QLatin1String(" 404"))) {
                    // Missing properties are normal; others, such as 403, mean the server withheld them.
                    item.unavailable.append(properties.names);
                }
            } else if (reader.name() == QLatin1String("response") && !item.path.isEmpty()) {
                result.append(item);
            }
        }
    }
    if (reader.hasError() || !multistatus) {
        if (error) {
            *error = i18nc("@info", "The server's file properties could not be read.");
        }
        return std::nullopt;
    }
    if (error) {
        error->clear();
    }
    return result;
}

QString Nextcloud::describeShareType(int type)
{
    switch (type) {
    case 0:
        return i18nc("@item Nextcloud share type", "User");
    case 1:
        return i18nc("@item Nextcloud share type", "Group");
    case 3:
        return i18nc("@item Nextcloud share type", "Public link");
    case 4:
        return i18nc("@item Nextcloud share type", "Email");
    case 6:
        return i18nc("@item Nextcloud share type", "Federated cloud");
    case 7:
        return i18nc("@item Nextcloud share type", "Team");
    case 9:
        return i18nc("@item Nextcloud share type", "Federated group");
    case 10:
        return i18nc("@item Nextcloud share type", "Talk conversation");
    case 12:
        return i18nc("@item Nextcloud share type", "Deck board");
    default:
        return i18nc("@item Nextcloud share type %1 is its number", "Share type %1", type);
    }
}

QString Nextcloud::describeMetadataField(const MetadataField &field)
{
    QString property = field.property;
    if (property == QLatin1String("metadata-photos-exif")) {
        property = i18nc("@label Nextcloud metadata", "Photo EXIF");
    } else if (property == QLatin1String("metadata-photos-ifd0")) {
        property = i18nc("@label Nextcloud metadata", "Camera");
    } else if (property == QLatin1String("metadata-photos-gps") || property == QLatin1String("file-metadata-gps")) {
        property = i18nc("@label Nextcloud metadata", "Location");
    } else if (property == QLatin1String("metadata-photos-size") || property == QLatin1String("file-metadata-size")) {
        property = i18nc("@label Nextcloud metadata", "Dimensions");
    } else if (property == QLatin1String("metadata-photos-original_date_time")) {
        property = i18nc("@label Nextcloud metadata", "Taken");
    } else if (property == QLatin1String("metadata-photos-place")) {
        property = i18nc("@label Nextcloud metadata", "Place");
    } else if (property == QLatin1String("metadata-files-live-photo")) {
        property = i18nc("@label Nextcloud metadata", "Live photo");
    } else if (property == QLatin1String("metadata-blurhash")) {
        property = i18nc("@label Nextcloud metadata", "Blurhash");
    } else if (property.startsWith(QLatin1String("file-metadata-"))) {
        property = property.mid(14);
    } else if (property.startsWith(QLatin1String("metadata-"))) {
        property = property.mid(9);
    }
    if (field.key.isEmpty()) {
        return property;
    }
    QString key = field.key;
    key.replace(QLatin1Char('/'), QStringLiteral(" › "));
    return i18nc("@label Nextcloud metadata property and field", "%1 › %2", property, key);
}

QSize Nextcloud::previewRequestSize(const QSize &size)
{
    const int longest = std::max(size.width(), size.height());
    int side = 64;
    while (side < longest && side < 2048) {
        side *= 2;
    }
    return {side, side};
}

QUrl Nextcloud::previewUrl(const NextcloudAccounts::Account &account, const QString &fileId, const QSize &size)
{
    QUrl url = NextcloudAccounts::webUrl(account, QStringLiteral("index.php/core/preview"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fileId"), fileId);
    query.addQueryItem(QStringLiteral("x"), QString::number(size.width()));
    query.addQueryItem(QStringLiteral("y"), QString::number(size.height()));
    query.addQueryItem(QStringLiteral("a"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("forceIcon"), QStringLiteral("0"));
    url.setQuery(query);
    return url;
}

void NextcloudClient::fetchMetadata(const NextcloudAccounts::Account &account, const QUrl &item)
{
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davPropFind(item, Nextcloud::metadataRequest(), QStringLiteral("0"), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, item] {
        const int code = Nextcloud::httpStatus(job);
        if (job->error() || code >= 400) {
            Q_EMIT metadataFetched(item, {}, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<QList<Nextcloud::FileMetadata>> parsed = Nextcloud::parseMetadata(job->responseData(), &error);
        if (!parsed || parsed->isEmpty()) {
            Q_EMIT metadataFetched(item, {}, parsed ? i18nc("@info", "The server reported no properties for this item.") : error);
            return;
        }
        const QString path = normalizedPath(item.path());
        const auto match = std::find_if(parsed->cbegin(), parsed->cend(), [&path](const Nextcloud::FileMetadata &metadata) {
            return metadata.path == path;
        });
        Q_EMIT metadataFetched(item, match != parsed->cend() ? *match : parsed->first(), QString());
    });
}

void NextcloudClient::fetchPreview(const NextcloudAccounts::Account &account, const QString &fileId, const QSize &size, const QUrl &item)
{
    if (fileId.isEmpty()) {
        Q_EMIT previewFetched(item, {}, i18nc("@info", "The server didn't report the file's ID, which its previews need."));
        return;
    }
    const std::optional<QString> secret = password(account);
    KIO::TransferJob *job = KIO::get(Nextcloud::previewUrl(account, fileId, size), KIO::Reload, KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    job->addMetaData(QStringLiteral("errorPage"), QStringLiteral("false"));
    auto data = std::make_shared<QByteArray>();
    auto failure = std::make_shared<QString>();
    const QPointer<KIO::TransferJob> transfer(job);
    connect(job, &KIO::TransferJob::data, this, [data, failure, transfer](KIO::Job *, const QByteArray &chunk) {
        if (!failure->isEmpty() || !transfer) {
            return;
        }
        if (data->size() + chunk.size() > Nextcloud::PreviewByteLimit) {
            *failure = i18nc("@info", "The server's preview is too large.");
            data->clear();
            transfer->kill(KJob::EmitResult);
            return;
        }
        data->append(chunk);
    });
    connect(job, &KJob::result, this, [this, job, data, failure, item] {
        if (!failure->isEmpty()) {
            Q_EMIT previewFetched(item, {}, *failure);
            return;
        }
        const int code = Nextcloud::httpStatus(job);
        if (code == 404 || (code == 0 && job->error() == KIO::ERR_DOES_NOT_EXIST)) {
            Q_EMIT previewFetched(item, {}, i18nc("@info", "The server has no preview of this file."));
            return;
        }
        if (job->error() || code >= 400) {
            Q_EMIT previewFetched(item, {}, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        const QImage image = QImage::fromData(*data);
        Q_EMIT previewFetched(item, image, image.isNull() ? i18nc("@info", "The server's preview could not be read.") : QString());
    });
}
