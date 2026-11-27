// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudorganize.h"

#include "nextcloudaccount.h"
#include "nextcloudclient.h"

#include <KIO/DavJob>
#include <KIO/SpecialJob>
#include <KIO/StoredTransferJob>
#include <KIO/TransferJob>
#include <KLocalizedString>

#include <QCollator>
#include <QDataStream>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>
#include <memory>

namespace
{
const QString davNamespace = QStringLiteral("DAV:");

/// Properties shown in result lists; fewer than metadataRequest() asks for, to keep large listings small.
const QString listingProperties = QStringLiteral(
    "<d:resourcetype/><d:getlastmodified/><d:getetag/><d:getcontenttype/><d:getcontentlength/>"
    "<oc:fileid/><oc:size/><oc:permissions/><oc:favorite/><oc:owner-display-name/><oc:share-types/>"
    "<nc:has-preview/><nc:system-tags/>");

/// One d:response of a multistatus response: the properties the server returned and those it refused.
struct DavResponse {
    QString path;
    QHash<QString, QString> properties;
    QStringList refused;
    /// A d:status of the whole response, such as "HTTP/1.1 403 Forbidden".
    QString status;
};

/// Reads the text of the current element, ignoring its child elements' names.
QString elementText(QXmlStreamReader &reader)
{
    return reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

std::optional<QList<DavResponse>> parseResponses(const QByteArray &xml)
{
    QList<DavResponse> responses;
    DavResponse response;
    QHash<QString, QString> properties;
    QStringList names;
    QString status;
    bool inPropstat = false;
    bool inProp = false;
    bool multistatus = false;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (inProp) {
                const QString name = reader.name().toString();
                names.append(name);
                properties.insert(name, elementText(reader));
                continue;
            }
            if (reader.namespaceUri() != davNamespace) {
                continue;
            }
            if (reader.name() == QLatin1String("multistatus")) {
                multistatus = true;
            } else if (reader.name() == QLatin1String("response")) {
                response = DavResponse();
            } else if (reader.name() == QLatin1String("propstat")) {
                inPropstat = true;
                properties.clear();
                names.clear();
                status.clear();
            } else if (reader.name() == QLatin1String("prop")) {
                inProp = true;
            } else if (reader.name() == QLatin1String("href")) {
                QString path = QUrl::fromEncoded(reader.readElementText().trimmed().toUtf8()).path();
                while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
                    path.chop(1);
                }
                response.path = path;
            } else if (reader.name() == QLatin1String("status")) {
                (inPropstat ? status : response.status) = reader.readElementText();
            }
        } else if (reader.isEndElement() && reader.namespaceUri() == davNamespace) {
            if (reader.name() == QLatin1String("prop")) {
                inProp = false;
            } else if (reader.name() == QLatin1String("propstat")) {
                inPropstat = false;
                if (status.contains(QLatin1String(" 200"))) {
                    response.properties.insert(properties);
                } else {
                    response.refused.append(names);
                }
            } else if (reader.name() == QLatin1String("response")) {
                responses.append(response);
            }
        }
    }
    if (reader.hasError() || !multistatus) {
        return std::nullopt;
    }
    return responses;
}

bool isTrue(const QString &value)
{
    return value == QLatin1String("true") || value == QLatin1String("1");
}

QString like(const QString &property, const QString &pattern)
{
    return QStringLiteral("<d:like><d:prop>%1</d:prop><d:literal>%2</d:literal></d:like>").arg(property, pattern.toHtmlEscaped());
}

QString equals(const QString &property, const QString &value)
{
    return QStringLiteral("<d:eq><d:prop>%1</d:prop><d:literal>%2</d:literal></d:eq>").arg(property, value.toHtmlEscaped());
}

QString typeCondition(Nextcloud::AssetType type)
{
    const QString contentType = QStringLiteral("<d:getcontenttype/>");
    switch (type) {
    case Nextcloud::AssetType::Any:
        return {};
    case Nextcloud::AssetType::Images:
        return like(contentType, QStringLiteral("image/%"));
    case Nextcloud::AssetType::Videos:
        return like(contentType, QStringLiteral("video/%"));
    case Nextcloud::AssetType::Audio:
        return like(contentType, QStringLiteral("audio/%"));
    case Nextcloud::AssetType::Folders:
        return equals(contentType, QStringLiteral("httpd/unix-directory"));
    case Nextcloud::AssetType::Documents:
        return QStringLiteral("<d:or>") + like(contentType, QStringLiteral("text/%")) + equals(contentType, QStringLiteral("application/pdf"))
            + like(contentType, QStringLiteral("application/vnd.oasis.opendocument.%"))
            + like(contentType, QStringLiteral("application/vnd.openxmlformats-officedocument.%")) + equals(contentType, QStringLiteral("application/msword"))
            + equals(contentType, QStringLiteral("application/vnd.ms-excel")) + equals(contentType, QStringLiteral("application/vnd.ms-powerpoint"))
            + QStringLiteral("</d:or>");
    }
    return {};
}

int boundedLimit(int limit)
{
    return std::clamp(limit, 1, 10000);
}

/// KIO::DAV_SEARCH, from KIO's private httpmethod_p.h, which the HTTP worker's generic WebDAV command takes.
constexpr int davSearchMethod = 13;
}

bool Nextcloud::canAssign(const SystemTag &tag)
{
    return tag.canAssign.value_or(tag.userVisible && tag.userAssignable);
}

QString Nextcloud::describeTag(const SystemTag &tag)
{
    if (!tag.userVisible) {
        return i18nc("@item Nextcloud tag name", "%1 (invisible)", tag.name);
    }
    if (!canAssign(tag)) {
        return i18nc("@item Nextcloud tag name", "%1 (restricted)", tag.name);
    }
    return tag.name;
}

QString Nextcloud::systemTagsRequest()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\"><d:prop>"
        "<oc:id/><oc:display-name/><oc:user-visible/><oc:user-assignable/><oc:can-assign/>"
        "</d:prop></d:propfind>");
}

std::optional<QList<Nextcloud::SystemTag>> Nextcloud::parseSystemTags(const QByteArray &xml, QString *error)
{
    const std::optional<QList<DavResponse>> responses = parseResponses(xml);
    if (!responses) {
        if (error) {
            *error = i18nc("@info", "The server's tags could not be read.");
        }
        return std::nullopt;
    }
    QList<SystemTag> tags;
    for (const DavResponse &response : *responses) {
        const QString id = response.properties.value(QStringLiteral("id"));
        if (id.isEmpty()) {
            // The collection itself.
            continue;
        }
        SystemTag tag;
        tag.id = id;
        tag.name = response.properties.value(QStringLiteral("display-name"));
        tag.userVisible = !response.properties.contains(QStringLiteral("user-visible")) || isTrue(response.properties.value(QStringLiteral("user-visible")));
        tag.userAssignable =
            !response.properties.contains(QStringLiteral("user-assignable")) || isTrue(response.properties.value(QStringLiteral("user-assignable")));
        if (response.properties.contains(QStringLiteral("can-assign"))) {
            tag.canAssign = isTrue(response.properties.value(QStringLiteral("can-assign")));
        }
        tags.append(tag);
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::stable_sort(tags.begin(), tags.end(), [&collator](const SystemTag &a, const SystemTag &b) {
        return collator.compare(a.name, b.name) < 0;
    });
    if (error) {
        error->clear();
    }
    return tags;
}

QByteArray Nextcloud::createTagRequest(const QString &name)
{
    const QJsonObject object{
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("userVisible"), true},
        {QStringLiteral("userAssignable"), true},
        {QStringLiteral("canAssign"), true},
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString Nextcloud::tagNameError(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return i18nc("@info", "Enter a name for the tag.");
    }
    // Nextcloud stores tag names in a column of 64 characters.
    if (trimmed.size() > 64) {
        return i18nc("@info", "Tag names can have at most 64 characters.");
    }
    return {};
}

QString Nextcloud::createdTagId(const QString &headers)
{
    const QStringList lines = headers.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!line.startsWith(QLatin1String("content-location:"), Qt::CaseInsensitive)) {
            continue;
        }
        QString location = line.mid(17).trimmed();
        while (location.endsWith(QLatin1Char('/'))) {
            location.chop(1);
        }
        const QString id = location.section(QLatin1Char('/'), -1);
        bool numeric = false;
        id.toULongLong(&numeric);
        return numeric ? id : QString();
    }
    return {};
}

QString Nextcloud::favoriteRequest(bool favorite)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<d:propertyupdate xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\"><d:set><d:prop>"
               "<oc:favorite>%1</oc:favorite>"
               "</d:prop></d:set></d:propertyupdate>")
        .arg(favorite ? 1 : 0);
}

bool Nextcloud::parsePropPatch(const QByteArray &xml, QString *error)
{
    const std::optional<QList<DavResponse>> responses = parseResponses(xml);
    QString message;
    if (!responses || responses->isEmpty()) {
        message = i18nc("@info", "The server's answer could not be read.");
    } else {
        for (const DavResponse &response : *responses) {
            if (!response.refused.isEmpty() || (!response.status.isEmpty() && !response.status.contains(QLatin1String(" 2")))) {
                message = i18nc("@info", "The server refused the change.");
            }
        }
    }
    if (error) {
        *error = message;
    }
    return message.isEmpty();
}

QString Nextcloud::filterFilesRequest(const QString &tagId)
{
    const QString rule =
        tagId.isEmpty() ? QStringLiteral("<oc:favorite>1</oc:favorite>") : QStringLiteral("<oc:systemtag>%1</oc:systemtag>").arg(tagId.toHtmlEscaped());
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<oc:filter-files xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">"
               "<d:prop>%1</d:prop><oc:filter-rules>%2</oc:filter-rules></oc:filter-files>")
        .arg(listingProperties, rule);
}

QString Nextcloud::searchRequest(const NextcloudAccounts::Account &account, const SearchQuery &query)
{
    QStringList conditions;
    const QString text = query.text.trimmed();
    if (!text.isEmpty()) {
        conditions.append(like(QStringLiteral("<d:displayname/>"), QLatin1Char('%') + text + QLatin1Char('%')));
    }
    if (const QString type = typeCondition(query.type); !type.isEmpty()) {
        conditions.append(type);
    }
    if (query.favoritesOnly) {
        conditions.append(equals(QStringLiteral("<oc:favorite/>"), QStringLiteral("1")));
    }
    if (conditions.isEmpty()) {
        conditions.append(like(QStringLiteral("<d:displayname/>"), QStringLiteral("%")));
    }
    const QString where = conditions.size() == 1 ? conditions.first() : QStringLiteral("<d:and>") + conditions.join(QString()) + QStringLiteral("</d:and>");
    // One more result than shown tells whether the server had more.
    const int requested = boundedLimit(query.limit) + 1;
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<d:searchrequest xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\"><d:basicsearch>"
               "<d:select><d:prop>%1</d:prop></d:select>"
               "<d:from><d:scope><d:href>/files/%2</d:href><d:depth>infinity</d:depth></d:scope></d:from>"
               "<d:where>%3</d:where>"
               "<d:orderby><d:order><d:prop><d:getlastmodified/></d:prop><d:descending/></d:order></d:orderby>"
               "<d:limit><d:nresults>%4</d:nresults></d:limit>"
               "</d:basicsearch></d:searchrequest>")
        .arg(listingProperties, account.user.toHtmlEscaped(), where, QString::number(requested));
}

QUrl Nextcloud::itemUrl(const NextcloudAccounts::Account &account, const QString &path)
{
    const QUrl files = NextcloudAccounts::filesUrl(account);
    const QString root = files.path();
    if (path == root.chopped(1)) {
        return files;
    }
    if (!path.startsWith(root) || path.size() == root.size()) {
        return {};
    }
    QUrl url = files;
    url.setPath(path);
    return url;
}

Nextcloud::AssetResults Nextcloud::parseAssetResults(const QByteArray &xml, const NextcloudAccounts::Account &account, int limit)
{
    AssetResults results;
    QString error;
    const std::optional<QList<FileMetadata>> parsed = parseMetadata(xml, &error);
    if (!parsed) {
        results.error = error;
        return results;
    }
    limit = boundedLimit(limit);
    const QUrl files = NextcloudAccounts::filesUrl(account);
    for (const FileMetadata &item : *parsed) {
        const QUrl url = itemUrl(account, item.path);
        if (url == files) {
            continue;
        }
        // Items without an ID can't be tagged or previewed, and items outside the user's files can't be opened.
        if (item.fileId.isEmpty() || !url.isValid()) {
            ++results.incomplete;
            continue;
        }
        if (results.items.size() == limit) {
            results.truncated = true;
            break;
        }
        if (!item.unavailable.isEmpty()) {
            ++results.incomplete;
        }
        results.items.append(item);
    }
    return results;
}

QString Nextcloud::listingError(int kioError, int httpStatus, const QString &errorText, bool *unavailable)
{
    const bool notOffered = httpStatus == 405 || httpStatus == 501 || httpStatus == 400 || kioError == KIO::ERR_UNSUPPORTED_ACTION;
    if (unavailable) {
        *unavailable = notOffered;
    }
    if (notOffered) {
        return i18nc("@info", "This server doesn't offer this search, or not with these criteria.");
    }
    return errorMessage(kioError, httpStatus, errorText);
}

void NextcloudClient::setFavorite(const NextcloudAccounts::Account &account, const QUrl &item, bool favorite)
{
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davPropPatch(item, Nextcloud::favoriteRequest(favorite), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, item, favorite] {
        const int code = Nextcloud::httpStatus(job);
        if (job->error() || code >= 400) {
            Q_EMIT favoriteSet(item, favorite, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        Nextcloud::parsePropPatch(job->responseData(), &error);
        Q_EMIT favoriteSet(item, favorite, error);
    });
}

void NextcloudClient::fetchSystemTags(const NextcloudAccounts::Account &account)
{
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davPropFind(NextcloudAccounts::davUrl(account, QStringLiteral("systemtags/")),
                                        Nextcloud::systemTagsRequest(),
                                        QStringLiteral("1"),
                                        KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    const QString id = account.id;
    connect(job, &KJob::result, this, [this, job, id] {
        const int code = Nextcloud::httpStatus(job);
        if (code == 404 || code == 405 || (code == 0 && job->error() == KIO::ERR_DOES_NOT_EXIST)) {
            Q_EMIT systemTagsFetched(id, {}, i18nc("@info", "Collaborative tags aren't available on this server."));
            return;
        }
        if (job->error() || code >= 400) {
            Q_EMIT systemTagsFetched(id, {}, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<QList<Nextcloud::SystemTag>> tags = Nextcloud::parseSystemTags(job->responseData(), &error);
        Q_EMIT systemTagsFetched(id, tags.value_or(QList<Nextcloud::SystemTag>()), error);
    });
}

void NextcloudClient::fetchFileTags(const NextcloudAccounts::Account &account, const QString &fileId)
{
    if (fileId.isEmpty()) {
        Q_EMIT fileTagsFetched(fileId, {}, i18nc("@info", "The server didn't report the file's ID, which its tags need."));
        return;
    }
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davPropFind(NextcloudAccounts::davUrl(account, QStringLiteral("systemtags-relations/files/") + fileId + QLatin1Char('/')),
                                        Nextcloud::systemTagsRequest(),
                                        QStringLiteral("1"),
                                        KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, fileId] {
        const int code = Nextcloud::httpStatus(job);
        if (job->error() || code >= 400) {
            Q_EMIT fileTagsFetched(fileId, {}, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<QList<Nextcloud::SystemTag>> tags = Nextcloud::parseSystemTags(job->responseData(), &error);
        Q_EMIT fileTagsFetched(fileId, tags.value_or(QList<Nextcloud::SystemTag>()), error);
    });
}

void NextcloudClient::setFileTag(const NextcloudAccounts::Account &account, const QString &fileId, const QString &tagId, bool assign)
{
    if (fileId.isEmpty() || tagId.isEmpty()) {
        Q_EMIT fileTagSet(fileId, tagId, assign, i18nc("@info", "The server didn't report the file's ID, which its tags need."));
        return;
    }
    const std::optional<QString> secret = password(account);
    const QUrl url = NextcloudAccounts::webUrl(account, QStringLiteral("remote.php/dav/systemtags-relations/files/%1/%2").arg(fileId, tagId));
    KIO::Job *job = assign ? static_cast<KIO::Job *>(KIO::storedPut(QByteArray(), url, -1, KIO::Overwrite | KIO::HideProgressInfo))
                           : static_cast<KIO::Job *>(KIO::http_delete(url, KIO::HideProgressInfo));
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, fileId, tagId, assign] {
        const int code = Nextcloud::httpStatus(job);
        // 409 means the tag was already assigned, and 404 on removal that it already wasn't. KIO reports 204 as having no content.
        if ((assign && code == 409) || (!assign && code == 404) || job->error() == KIO::ERR_NO_CONTENT) {
            Q_EMIT fileTagSet(fileId, tagId, assign, QString());
            return;
        }
        if (code == 403) {
            Q_EMIT fileTagSet(fileId,
                              tagId,
                              assign,
                              assign ? i18nc("@info", "The server doesn't allow you to assign this tag to this file.")
                                     : i18nc("@info", "The server doesn't allow you to remove this tag from this file."));
            return;
        }
        if (job->error() || code >= 400) {
            Q_EMIT fileTagSet(fileId, tagId, assign, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        Q_EMIT fileTagSet(fileId, tagId, assign, QString());
    });
}

void NextcloudClient::createTag(const NextcloudAccounts::Account &account, const QString &name)
{
    const QString id = account.id;
    if (const QString invalid = Nextcloud::tagNameError(name); !invalid.isEmpty()) {
        Q_EMIT tagCreated(id, {}, invalid);
        return;
    }
    const QString trimmed = name.trimmed();
    const std::optional<QString> secret = password(account);
    KIO::StoredTransferJob *job = KIO::storedHttpPost(Nextcloud::createTagRequest(trimmed),
                                                      NextcloudAccounts::webUrl(account, QStringLiteral("remote.php/dav/systemtags")),
                                                      KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    job->addMetaData(QStringLiteral("content-type"), QStringLiteral("Content-Type: application/json"));
    job->addMetaData(QStringLiteral("PropagateHttpHeader"), QStringLiteral("true"));
    connect(job, &KJob::result, this, [this, job, account, id, trimmed] {
        const int code = Nextcloud::httpStatus(job);
        const bool exists = code == 409;
        if (!exists && (job->error() || code >= 400)) {
            Q_EMIT tagCreated(id,
                              {},
                              code == 403 ? i18nc("@info", "The server doesn't allow you to create tags.")
                                          : Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        // A conflict means the tag exists; its answer has no location to read.
        const QString created = exists ? QString() : Nextcloud::createdTagId(job->queryMetaData(QStringLiteral("HTTP-Headers")));
        if (!created.isEmpty()) {
            Q_EMIT tagCreated(id, Nextcloud::SystemTag{created, trimmed, true, true, true}, QString());
            return;
        }
        // An existing tag, or a server that didn't say where the new one is: find it by name.
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(this,
                              &NextcloudClient::systemTagsFetched,
                              this,
                              [this, connection, id, trimmed](const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error) {
                                  if (accountId != id) {
                                      return;
                                  }
                                  disconnect(*connection);
                                  const auto match = std::find_if(tags.cbegin(), tags.cend(), [&trimmed](const Nextcloud::SystemTag &tag) {
                                      return tag.name.compare(trimmed, Qt::CaseInsensitive) == 0;
                                  });
                                  if (match == tags.cend()) {
                                      Q_EMIT tagCreated(id, {}, error.isEmpty() ? i18nc("@info", "The server didn't report the new tag.") : error);
                                      return;
                                  }
                                  Q_EMIT tagCreated(id, *match, QString());
                              });
        fetchSystemTags(account);
    });
}

int NextcloudClient::listFavorites(const NextcloudAccounts::Account &account, int limit)
{
    return listFiles(account, QString(), limit);
}

int NextcloudClient::listTagged(const NextcloudAccounts::Account &account, const QString &tagId, int limit)
{
    if (tagId.isEmpty()) {
        const int request = ++m_lastRequest;
        QTimer::singleShot(0, this, [this, request] {
            Nextcloud::AssetResults results;
            results.error = i18nc("@info", "Choose a tag.");
            Q_EMIT assetsListed(request, results);
        });
        return request;
    }
    return listFiles(account, tagId, limit);
}

int NextcloudClient::listFiles(const NextcloudAccounts::Account &account, const QString &tagId, int limit)
{
    const int request = ++m_lastRequest;
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davReport(NextcloudAccounts::filesUrl(account), Nextcloud::filterFilesRequest(tagId), QStringLiteral("0"), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, account, request, limit] {
        const int code = Nextcloud::httpStatus(job);
        Nextcloud::AssetResults results;
        if (job->error() || code >= 400) {
            results.error = Nextcloud::listingError(job->error(), code, job->errorString(), &results.unavailable);
        } else {
            results = Nextcloud::parseAssetResults(job->responseData(), account, limit);
        }
        Q_EMIT assetsListed(request, results);
    });
    return request;
}

int NextcloudClient::search(const NextcloudAccounts::Account &account, const Nextcloud::SearchQuery &query)
{
    const int request = ++m_lastRequest;
    // KIO::davSearch() can't send a basicsearch query, so the SEARCH request is sent like DavJob sends it.
    const QUrl url = NextcloudAccounts::davUrl(account, QString());
    const QByteArray body = Nextcloud::searchRequest(account, query).toUtf8();
    QByteArray arguments;
    QDataStream stream(&arguments, QIODevice::WriteOnly);
    stream << int(7) << url << davSearchMethod << static_cast<qint64>(body.size());
    auto *job = new KIO::SpecialJob(url, arguments);
    job->addMetaData(QStringLiteral("content-type"), QStringLiteral("Content-Type: text/xml; charset=utf-8"));
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    auto sent = std::make_shared<bool>(false);
    connect(job, &KIO::TransferJob::dataReq, this, [body, sent](KIO::Job *, QByteArray &data) {
        if (!*sent) {
            data = body;
            *sent = true;
        }
    });
    auto response = std::make_shared<QByteArray>();
    connect(job, &KIO::TransferJob::data, this, [response](KIO::Job *, const QByteArray &chunk) {
        response->append(chunk);
    });
    const int limit = query.limit;
    connect(job, &KJob::result, this, [this, job, account, request, response, limit] {
        const int code = Nextcloud::httpStatus(job);
        Nextcloud::AssetResults results;
        if (job->error() || code >= 400) {
            results.error = Nextcloud::listingError(job->error(), code, job->errorString(), &results.unavailable);
        } else {
            results = Nextcloud::parseAssetResults(*response, account, limit);
        }
        Q_EMIT assetsListed(request, results);
    });
    return request;
}
