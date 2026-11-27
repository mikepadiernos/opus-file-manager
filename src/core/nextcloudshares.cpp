// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudshares.h"

#include "nextcloudaccount.h"
#include "nextcloudclient.h"

#include <KIO/StoredTransferJob>
#include <KIO/TransferJob>
#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

namespace
{
const QString sharesPath = QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares");
const QString formContentType = QStringLiteral("Content-Type: application/x-www-form-urlencoded");
constexpr int allPermissions = 31;

QJsonValue ocsData(const QByteArray &json, bool *ok)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject ocs = document.object().value(QLatin1String("ocs")).toObject();
    *ok = parseError.error == QJsonParseError::NoError && document.isObject() && !ocs.isEmpty();
    return ocs.value(QLatin1String("data"));
}

/// The OCS status code of @p json, or -1 when it isn't an OCS response.
int ocsStatusCode(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return -1;
    }
    const QJsonValue meta = document.object().value(QLatin1String("ocs")).toObject().value(QLatin1String("meta"));
    return meta.isObject() ? meta.toObject().value(QLatin1String("statuscode")).toInt(-1) : -1;
}

bool ocsSucceeded(const QByteArray &json)
{
    const int code = ocsStatusCode(json);
    return code == 100 || code == 200;
}

QString notOcs()
{
    return i18nc("@info", "The server's response is not from Nextcloud's sharing interface.");
}

QString jsonString(const QJsonValue &value)
{
    if (value.isDouble()) {
        return QString::number(qint64(value.toDouble()));
    }
    return value.toString();
}

Nextcloud::Share shareFrom(const QJsonObject &object)
{
    Nextcloud::Share share;
    share.id = jsonString(object.value(QLatin1String("id")));
    share.type = Nextcloud::shareType(object.value(QLatin1String("share_type")).toInt(-1));
    share.path = object.value(QLatin1String("path")).toString();
    share.folder = object.value(QLatin1String("item_type")).toString() == QLatin1String("folder");
    share.owner = jsonString(object.value(QLatin1String("uid_owner")));
    share.ownerDisplayName = object.value(QLatin1String("displayname_owner")).toString();
    share.permissions = Nextcloud::SharePermissions::fromInt(object.value(QLatin1String("permissions")).toInt() & allPermissions);
    const QString expiration = object.value(QLatin1String("expiration")).toString();
    share.expiration = QDate::fromString(expiration.left(10), Qt::ISODate);
    share.hasPassword = !object.value(QLatin1String("password")).toString().isEmpty();
    if (share.type == Nextcloud::ShareType::PublicLink) {
        // Older servers report a link's password hash as share_with; it's never kept or shown.
        share.hasPassword = share.hasPassword || !object.value(QLatin1String("share_with")).toString().isEmpty();
        share.url = QUrl(object.value(QLatin1String("url")).toString());
    } else {
        share.shareWith = jsonString(object.value(QLatin1String("share_with")));
        share.shareWithDisplayName = object.value(QLatin1String("share_with_displayname")).toString();
    }
    share.label = object.value(QLatin1String("label")).toString();
    share.note = object.value(QLatin1String("note")).toString();
    share.canEdit = object.value(QLatin1String("can_edit")).toBool(true);
    share.canDelete = object.value(QLatin1String("can_delete")).toBool(true);
    return share;
}

bool hasParentSegment(const QString &path)
{
    return path.split(QLatin1Char('/')).contains(QLatin1String(".."));
}

QString filesPath(const NextcloudAccounts::Account &account)
{
    QString path = NextcloudAccounts::filesUrl(account).path(QUrl::FullyDecoded);
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
    }
    return path;
}

QUrl jsonUrl(QUrl url)
{
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}

void addField(QByteArray &body, const char *name, const QString &value)
{
    if (!body.isEmpty()) {
        body += '&';
    }
    body += name;
    body += '=';
    body += QUrl::toPercentEncoding(value);
}

/**
 * Why the request that returned @p data failed, or an empty string when it
 * succeeded. KIO doesn't pass on the response to PUT and DELETE requests, so
 * @p bodyOptional accepts their success without one.
 */
QString requestError(KJob *job, const QByteArray &data, bool bodyOptional = false)
{
    const int code = Nextcloud::httpStatus(job);
    if (!job->error() && code < 400 && (ocsSucceeded(data) || (bodyOptional && data.trimmed().isEmpty()))) {
        return {};
    }
    if (Nextcloud::isAccessExpired(job->error(), code)) {
        return Nextcloud::errorMessage(job->error(), code, job->errorString());
    }
    if (const QString message = Nextcloud::ocsMessage(data); !message.isEmpty()) {
        return message;
    }
    if (!job->error() && code < 400) {
        return notOcs();
    }
    return Nextcloud::errorMessage(job->error(), code, job->errorString());
}

/// Collects the response of a job that doesn't store it.
std::shared_ptr<QByteArray> collect(KIO::TransferJob *job)
{
    auto received = std::make_shared<QByteArray>();
    QObject::connect(job, &KIO::TransferJob::data, job, [received](KIO::Job *, const QByteArray &data) {
        received->append(data);
    });
    return received;
}
}

Nextcloud::ShareType Nextcloud::shareType(int value)
{
    switch (value) {
    case int(ShareType::User):
    case int(ShareType::Group):
    case int(ShareType::PublicLink):
    case int(ShareType::Email):
    case int(ShareType::Federated):
    case int(ShareType::Team):
    case int(ShareType::Talk):
        return ShareType(value);
    default:
        return ShareType::Other;
    }
}

QString Nextcloud::shareTypeName(ShareType type)
{
    switch (type) {
    case ShareType::User:
        return i18nc("@item share type", "User");
    case ShareType::Group:
        return i18nc("@item share type", "Group");
    case ShareType::PublicLink:
        return i18nc("@item share type", "Public link");
    case ShareType::Email:
        return i18nc("@item share type", "Email");
    case ShareType::Federated:
        return i18nc("@item share type", "Federated cloud");
    case ShareType::Team:
        return i18nc("@item share type", "Team");
    case ShareType::Talk:
        return i18nc("@item share type", "Talk conversation");
    case ShareType::Other:
        break;
    }
    return i18nc("@item share type", "Other");
}

QString Nextcloud::describePermissions(SharePermissions permissions, bool folder)
{
    if (folder && permissions == SharePermission::Create) {
        return i18nc("@item share permissions", "Upload only (file drop)");
    }
    QStringList parts;
    if (permissions.testFlag(SharePermission::Read)) {
        parts.append(i18nc("@item share permission", "view"));
    }
    if (permissions.testFlag(SharePermission::Update)) {
        parts.append(i18nc("@item share permission", "edit"));
    }
    if (folder && permissions.testFlag(SharePermission::Create)) {
        parts.append(i18nc("@item share permission", "upload"));
    }
    if (folder && permissions.testFlag(SharePermission::Delete)) {
        parts.append(i18nc("@item share permission", "delete"));
    }
    if (permissions.testFlag(SharePermission::Share)) {
        parts.append(i18nc("@item share permission", "reshare"));
    }
    if (parts.isEmpty()) {
        return i18nc("@item share permissions", "None");
    }
    QString text = parts.join(QStringLiteral(", "));
    text[0] = text.at(0).toUpper();
    return text;
}

std::optional<Nextcloud::Share> Nextcloud::parseShare(const QByteArray &json, QString *error)
{
    bool ok = false;
    const QJsonValue data = ocsData(json, &ok);
    if (!ok || !data.isObject()) {
        if (error) {
            *error = notOcs();
        }
        return std::nullopt;
    }
    return shareFrom(data.toObject());
}

std::optional<QList<Nextcloud::Share>> Nextcloud::parseShares(const QByteArray &json, QString *error)
{
    bool ok = false;
    const QJsonValue data = ocsData(json, &ok);
    if (!ok || !data.isArray()) {
        if (error) {
            *error = notOcs();
        }
        return std::nullopt;
    }
    QList<Share> shares;
    const QJsonArray array = data.toArray();
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            shares.append(shareFrom(value.toObject()));
        }
    }
    return shares;
}

std::optional<Nextcloud::ShareCapabilities> Nextcloud::parseShareCapabilities(const QByteArray &json, QString *error)
{
    bool ok = false;
    const QJsonValue data = ocsData(json, &ok);
    if (!ok || !data.isObject()) {
        if (error) {
            *error = i18nc("@info", "The server's response is not from Nextcloud. Check the server address.");
        }
        return std::nullopt;
    }
    ShareCapabilities capabilities;
    const QJsonValue sharing = data.toObject().value(QLatin1String("capabilities")).toObject().value(QLatin1String("files_sharing"));
    if (!sharing.isObject()) {
        return capabilities;
    }
    const QJsonObject object = sharing.toObject();
    capabilities.api = object.value(QLatin1String("api_enabled")).toBool();
    const QJsonObject links = object.value(QLatin1String("public")).toObject();
    capabilities.publicLinks = links.value(QLatin1String("enabled")).toBool();
    capabilities.passwordEnforced = links.value(QLatin1String("password")).toObject().value(QLatin1String("enforced")).toBool();
    const QJsonObject expiry = links.value(QLatin1String("expire_date")).toObject();
    capabilities.expireEnforced = expiry.value(QLatin1String("enabled")).toBool() && expiry.value(QLatin1String("enforced")).toBool();
    capabilities.expireDays = expiry.value(QLatin1String("enabled")).toBool() ? expiry.value(QLatin1String("days")).toInt() : 0;
    capabilities.publicUpload = links.value(QLatin1String("upload")).toBool();
    capabilities.resharing = object.value(QLatin1String("resharing")).toBool();
    capabilities.groupSharing = object.value(QLatin1String("group_sharing")).toBool(true);
    if (object.contains(QLatin1String("default_permissions"))) {
        capabilities.defaultPermissions = SharePermissions::fromInt(object.value(QLatin1String("default_permissions")).toInt() & allPermissions);
    }
    return capabilities;
}

QString Nextcloud::sharePath(const NextcloudAccounts::Account &account, const QUrl &file)
{
    const QUrl files = NextcloudAccounts::filesUrl(account);
    if (file.scheme() != files.scheme() || file.host().compare(files.host(), Qt::CaseInsensitive) != 0 || file.port() != files.port()) {
        return {};
    }
    const QString base = filesPath(account);
    QString path = file.path(QUrl::FullyDecoded);
    if (!path.startsWith(base) || hasParentSegment(path)) {
        return {};
    }
    path = path.mid(base.size());
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path.isEmpty() ? QString() : QLatin1Char('/') + path;
}

QUrl Nextcloud::shareFileUrl(const NextcloudAccounts::Account &account, const Share &share)
{
    if (!share.path.startsWith(QLatin1Char('/')) || share.path.size() < 2 || hasParentSegment(share.path)) {
        return {};
    }
    QUrl url = NextcloudAccounts::filesUrl(account);
    url.setPath(filesPath(account) + share.path.mid(1), QUrl::DecodedMode);
    return url;
}

QUrl Nextcloud::sharesUrl(const NextcloudAccounts::Account &account, const QString &id)
{
    if (id.isEmpty()) {
        return NextcloudAccounts::webUrl(account, sharesPath);
    }
    const bool numeric = std::all_of(id.cbegin(), id.cend(), [](QChar c) {
        return c >= QLatin1Char('0') && c <= QLatin1Char('9');
    });
    if (!numeric) {
        return {};
    }
    return NextcloudAccounts::webUrl(account, sharesPath + QLatin1Char('/') + id);
}

std::optional<QByteArray> Nextcloud::createShareBody(const NewShare &share, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };
    if (!share.path.startsWith(QLatin1Char('/')) || share.path.size() < 2 || hasParentSegment(share.path)) {
        return fail(i18nc("@info", "Only files and folders in your Nextcloud files can be shared, not the whole account."));
    }
    const bool withRecipient = share.type == ShareType::User || share.type == ShareType::Group;
    if (!withRecipient && share.type != ShareType::PublicLink) {
        return fail(i18nc("@info", "Plasma Opus can only create public links and shares with users and groups."));
    }
    if (withRecipient && share.shareWith.trimmed().isEmpty()) {
        return fail(share.type == ShareType::User ? i18nc("@info", "Enter the user to share with.") : i18nc("@info", "Enter the group to share with."));
    }
    QByteArray body;
    addField(body, "path", share.path);
    addField(body, "shareType", QString::number(int(share.type)));
    if (withRecipient) {
        addField(body, "shareWith", share.shareWith.trimmed());
    }
    if (share.permissions) {
        addField(body, "permissions", QString::number(share.permissions.toInt()));
    }
    if (share.expiration.isValid()) {
        addField(body, "expireDate", share.expiration.toString(Qt::ISODate));
    }
    if (!share.password.isEmpty()) {
        addField(body, "password", share.password);
    }
    return body;
}

QByteArray Nextcloud::updateShareBody(const ShareUpdate &update)
{
    QByteArray body;
    if (update.permissions) {
        addField(body, "permissions", QString::number(update.permissions->toInt()));
    }
    if (update.expiration) {
        addField(body, "expireDate", update.expiration->isValid() ? update.expiration->toString(Qt::ISODate) : QString());
    }
    if (update.password) {
        addField(body, "password", *update.password);
    }
    return body;
}

bool Nextcloud::broadensAccess(const Share &share, const ShareUpdate &update)
{
    if (update.permissions && (update.permissions->toInt() & ~share.permissions.toInt()) != 0) {
        return true;
    }
    if (update.expiration && share.expiration.isValid() && (!update.expiration->isValid() || *update.expiration > share.expiration)) {
        return true;
    }
    return update.password && update.password->isEmpty() && share.hasPassword;
}

int NextcloudClient::listShares(const NextcloudAccounts::Account &account, Nextcloud::ShareScope scope, const QString &path)
{
    const int request = ++m_lastRequest;
    QUrl url = Nextcloud::sharesUrl(account);
    QUrlQuery query;
    if (scope == Nextcloud::ShareScope::Item) {
        query.addQueryItem(QStringLiteral("path"), path);
        query.addQueryItem(QStringLiteral("reshares"), QStringLiteral("true"));
    } else if (scope == Nextcloud::ShareScope::SharedWithMe) {
        query.addQueryItem(QStringLiteral("shared_with_me"), QStringLiteral("true"));
    }
    url.setQuery(query);
    const std::optional<QString> secret = password(account);
    KIO::Job *job = ocsGet(url, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, request, scope, path] {
        const QByteArray data = static_cast<KIO::StoredTransferJob *>(job)->data();
        const int code = Nextcloud::httpStatus(job);
        Nextcloud::ShareListing listing;
        listing.scope = scope;
        listing.path = path;
        const int status = ocsStatusCode(data);
        // An unknown OCS route reports 998, or 404 without an OCS response.
        if (status == 998 || (code == 404 && status < 0)) {
            listing.unavailable = true;
            listing.error = i18nc("@info", "This server doesn't offer sharing, or its sharing app is disabled.");
        } else if (const QString error = requestError(job, data); !error.isEmpty()) {
            listing.error = error;
        } else {
            listing.shares = Nextcloud::parseShares(data, &listing.error).value_or(QList<Nextcloud::Share>());
        }
        Q_EMIT sharesListed(request, listing);
    });
    return request;
}

void NextcloudClient::fetchShareCapabilities(const NextcloudAccounts::Account &account)
{
    const std::optional<QString> secret = password(account);
    KIO::Job *job = ocsGet(NextcloudAccounts::webUrl(account, QStringLiteral("ocs/v2.php/cloud/capabilities")),
                           NextcloudAccounts::loginName(account),
                           secret.value_or(QString()));
    const QString id = account.id;
    connect(job, &KJob::result, this, [this, job, id] {
        const QByteArray data = static_cast<KIO::StoredTransferJob *>(job)->data();
        QString error = requestError(job, data);
        Nextcloud::ShareCapabilities capabilities;
        if (error.isEmpty()) {
            capabilities = Nextcloud::parseShareCapabilities(data, &error).value_or(Nextcloud::ShareCapabilities());
        }
        Q_EMIT shareCapabilitiesFetched(id, capabilities, error);
    });
}

void NextcloudClient::createShare(const NextcloudAccounts::Account &account, const Nextcloud::NewShare &share)
{
    QString error;
    const std::optional<QByteArray> body = Nextcloud::createShareBody(share, &error);
    if (!body) {
        Q_EMIT shareCreated(share.path, {}, error);
        return;
    }
    KIO::StoredTransferJob *job = KIO::storedHttpPost(*body, jsonUrl(Nextcloud::sharesUrl(account)), KIO::HideProgressInfo);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    job->addMetaData(QStringLiteral("content-type"), formContentType);
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    const QString path = share.path;
    connect(job, &KJob::result, this, [this, job, path] {
        const QByteArray data = job->data();
        QString error = requestError(job, data);
        Nextcloud::Share created;
        if (error.isEmpty()) {
            created = Nextcloud::parseShare(data, &error).value_or(Nextcloud::Share());
        }
        Q_EMIT shareCreated(path, created, error);
    });
}

void NextcloudClient::updateShare(const NextcloudAccounts::Account &account, const Nextcloud::Share &share, const Nextcloud::ShareUpdate &update)
{
    const QUrl url = share.id.isEmpty() ? QUrl() : Nextcloud::sharesUrl(account, share.id);
    if (!url.isValid() || update.isEmpty()) {
        Q_EMIT shareUpdated(share.id, share, url.isValid() ? i18nc("@info", "Nothing to change.") : i18nc("@info", "The share's ID is not valid."));
        return;
    }
    // KIO's generic WebDAV request would send the body as XML, so this is a plain PUT.
    KIO::StoredTransferJob *job = KIO::storedPut(Nextcloud::updateShareBody(update), jsonUrl(url), -1, KIO::Overwrite | KIO::HideProgressInfo);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    job->addMetaData(QStringLiteral("content-type"), formContentType);
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    connect(job, &KJob::result, this, [this, job, share, update] {
        const QByteArray data = job->data();
        QString error = requestError(job, data, true);
        Nextcloud::Share updated = share;
        if (error.isEmpty()) {
            if (const std::optional<Nextcloud::Share> parsed = Nextcloud::parseShare(data)) {
                updated = *parsed;
            } else {
                updated.permissions = update.permissions.value_or(share.permissions);
                updated.expiration = update.expiration.value_or(share.expiration);
                updated.hasPassword = update.password ? !update.password->isEmpty() : share.hasPassword;
            }
        }
        Q_EMIT shareUpdated(share.id, updated, error);
    });
}

void NextcloudClient::deleteShare(const NextcloudAccounts::Account &account, const Nextcloud::Share &share)
{
    // Never the collection of all shares.
    const QUrl url = share.id.isEmpty() ? QUrl() : Nextcloud::sharesUrl(account, share.id);
    if (!url.isValid()) {
        Q_EMIT shareDeleted(share.id, i18nc("@info", "The share's ID is not valid."));
        return;
    }
    KIO::TransferJob *job = KIO::http_delete(jsonUrl(url), KIO::HideProgressInfo);
    const std::optional<QString> secret = password(account);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    const std::shared_ptr<QByteArray> received = collect(job);
    const QString id = share.id;
    connect(job, &KJob::result, this, [this, job, id, received] {
        const int code = Nextcloud::httpStatus(job);
        // A share that no longer exists is already revoked.
        Q_EMIT shareDeleted(id, code == 404 && !Nextcloud::isAccessExpired(job->error(), code) ? QString() : requestError(job, *received, true));
    });
}
