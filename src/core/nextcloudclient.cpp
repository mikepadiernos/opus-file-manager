// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudclient.h"

#include <KIO/DavJob>
#include <KIO/StoredTransferJob>
#include <KIO/TransferJob>
#include <KLocalizedString>
#include <KPasswdServerClient>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace
{
const QString davNamespace = QStringLiteral("DAV:");
const QString ownCloudNamespace = QStringLiteral("http://owncloud.org/ns");

QJsonObject ocsObject(const QByteArray &json, bool *ok)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    *ok = parseError.error == QJsonParseError::NoError && document.isObject() && document.object().value(QLatin1String("ocs")).isObject();
    return document.object().value(QLatin1String("ocs")).toObject();
}

bool ocsSucceeded(const QJsonObject &ocs)
{
    const int code = ocs.value(QLatin1String("meta")).toObject().value(QLatin1String("statuscode")).toInt();
    // OCS v2 reports the HTTP status; v1 reports 100.
    return code == 100 || code == 200;
}

QString notNextcloud()
{
    return i18nc("@info", "The server's response is not from Nextcloud. Check the server address.");
}

QString normalizedPath(QString path)
{
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

bool sameHost(const QUrl &a, const QUrl &b)
{
    return a.host().compare(b.host(), Qt::CaseInsensitive) == 0;
}

QString userAgent()
{
    // Nextcloud names the app password after the client, so the user can
    // recognize and revoke it in the server's security settings.
    return QStringLiteral("Plasma Opus (KDE)");
}

int responseCode(KJob *job)
{
    auto *kioJob = qobject_cast<KIO::Job *>(job);
    return kioJob ? kioJob->queryMetaData(QStringLiteral("responsecode")).toInt() : 0;
}

QUrl withJsonFormat(QUrl url)
{
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}
}

std::optional<Nextcloud::Capabilities> Nextcloud::parseCapabilities(const QByteArray &json, QString *error)
{
    bool ok = false;
    const QJsonObject ocs = ocsObject(json, &ok);
    const QJsonObject data = ocs.value(QLatin1String("data")).toObject();
    const QJsonObject version = data.value(QLatin1String("version")).toObject();
    if (!ok || !ocsSucceeded(ocs) || version.value(QLatin1String("string")).toString().isEmpty()) {
        if (error) {
            const QString message = ok ? ocsMessage(json) : QString();
            *error = message.isEmpty() ? notNextcloud() : message;
        }
        return std::nullopt;
    }
    const QJsonObject capabilities = data.value(QLatin1String("capabilities")).toObject();
    Capabilities result;
    result.version = version.value(QLatin1String("string")).toString();
    result.major = version.value(QLatin1String("major")).toInt();
    result.productName = capabilities.value(QLatin1String("theming")).toObject().value(QLatin1String("name")).toString();
    const QJsonValue sharing = capabilities.value(QLatin1String("files_sharing"));
    result.sharing = sharing.isObject() && sharing.toObject().value(QLatin1String("api_enabled")).toBool(true);
    result.systemTags = capabilities.value(QLatin1String("systemtags")).toObject().value(QLatin1String("enabled")).toBool(false);
    result.dav = capabilities.value(QLatin1String("dav")).isObject();
    result.davSearch = result.dav && result.major >= 15;
    if (error) {
        error->clear();
    }
    return result;
}

std::optional<Nextcloud::User> Nextcloud::parseUser(const QByteArray &json, QString *error)
{
    bool ok = false;
    const QJsonObject ocs = ocsObject(json, &ok);
    const QJsonObject data = ocs.value(QLatin1String("data")).toObject();
    User user;
    user.id = data.value(QLatin1String("id")).toString();
    user.displayName = data.value(QLatin1String("display-name")).toString();
    if (user.displayName.isEmpty()) {
        user.displayName = data.value(QLatin1String("displayname")).toString();
    }
    if (!ok || !ocsSucceeded(ocs) || user.id.isEmpty()) {
        if (error) {
            const QString message = ok ? ocsMessage(json) : QString();
            *error = message.isEmpty() ? notNextcloud() : message;
        }
        return std::nullopt;
    }
    return user;
}

QString Nextcloud::ocsMessage(const QByteArray &json)
{
    bool ok = false;
    const QJsonObject ocs = ocsObject(json, &ok);
    if (!ok || ocsSucceeded(ocs)) {
        return {};
    }
    return ocs.value(QLatin1String("meta")).toObject().value(QLatin1String("message")).toString().trimmed();
}

std::optional<Nextcloud::LoginFlow> Nextcloud::parseLoginFlow(const QByteArray &json, const QUrl &server, QString *error)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QJsonObject poll = object.value(QLatin1String("poll")).toObject();
    LoginFlow flow;
    flow.loginUrl = QUrl(object.value(QLatin1String("login")).toString(), QUrl::StrictMode);
    flow.pollUrl = QUrl(poll.value(QLatin1String("endpoint")).toString(), QUrl::StrictMode);
    flow.token = poll.value(QLatin1String("token")).toString();
    const QString scheme = server.scheme() == QLatin1String("http") ? QStringLiteral("http") : QStringLiteral("https");
    auto acceptable = [&scheme](const QUrl &url) {
        return url.isValid() && !url.host().isEmpty() && (url.scheme() == QLatin1String("https") || url.scheme() == scheme);
    };
    QString problem;
    if (flow.token.isEmpty() || !flow.loginUrl.isValid() || !flow.pollUrl.isValid()) {
        problem = i18nc("@info", "The server does not support Nextcloud Login Flow. Check the server address, or update the server.");
    } else if (!acceptable(flow.loginUrl) || !acceptable(flow.pollUrl)) {
        problem = i18nc("@info", "The server asked to sign in over an unencrypted connection. Check the server's https configuration.");
    } else if (!sameHost(flow.pollUrl, server)) {
        problem = i18nc("@info %1 is a server name",
                        "The server asked to complete the sign-in on another server, %1. Check the server's address configuration.",
                        flow.pollUrl.host());
    }
    if (error) {
        *error = problem;
    }
    if (!problem.isEmpty()) {
        return std::nullopt;
    }
    return flow;
}

std::optional<Nextcloud::LoginResult> Nextcloud::parseLoginResult(const QByteArray &json, QString *error)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    LoginResult result;
    result.server = QUrl(object.value(QLatin1String("server")).toString());
    result.loginName = object.value(QLatin1String("loginName")).toString();
    result.appPassword = object.value(QLatin1String("appPassword")).toString();
    if (result.loginName.isEmpty() || result.appPassword.isEmpty()) {
        if (error) {
            *error = i18nc("@info", "The server's sign-in response is incomplete.");
        }
        return std::nullopt;
    }
    if (error) {
        error->clear();
    }
    return result;
}

Nextcloud::Permissions Nextcloud::parsePermissions(const QString &letters)
{
    Permissions permissions;
    for (const QChar letter : letters) {
        switch (letter.unicode()) {
        case 'S':
            permissions |= Permission::Shared;
            break;
        case 'R':
            permissions |= Permission::Shareable;
            break;
        case 'M':
            permissions |= Permission::Mounted;
            break;
        case 'G':
            permissions |= Permission::Readable;
            break;
        case 'D':
            permissions |= Permission::Delete;
            break;
        case 'N':
            permissions |= Permission::Rename;
            break;
        case 'V':
            permissions |= Permission::Move;
            break;
        case 'W':
            permissions |= Permission::Write;
            break;
        case 'C':
            permissions |= Permission::CreateFile;
            break;
        case 'K':
            permissions |= Permission::CreateFolder;
            break;
        default:
            break;
        }
    }
    return permissions;
}

std::optional<Nextcloud::FolderPermissions> Nextcloud::parseFolderPermissions(const QByteArray &xml, const QUrl &folder, QString *error)
{
    const QString folderPath = normalizedPath(folder.path());
    FolderPermissions result;
    QXmlStreamReader reader(xml);
    QString href;
    std::optional<QString> responseValue;
    std::optional<QString> propstatValue;
    QString status;
    bool multistatus = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (reader.namespaceUri() == davNamespace) {
                if (reader.name() == QLatin1String("multistatus")) {
                    multistatus = true;
                } else if (reader.name() == QLatin1String("response")) {
                    href.clear();
                    responseValue.reset();
                } else if (reader.name() == QLatin1String("propstat")) {
                    propstatValue.reset();
                    status.clear();
                } else if (reader.name() == QLatin1String("href")) {
                    href = reader.readElementText();
                } else if (reader.name() == QLatin1String("status")) {
                    status = reader.readElementText();
                }
            } else if (reader.namespaceUri() == ownCloudNamespace && reader.name() == QLatin1String("permissions")) {
                propstatValue = reader.readElementText();
            }
        } else if (reader.isEndElement() && reader.namespaceUri() == davNamespace) {
            if (reader.name() == QLatin1String("propstat")) {
                if (propstatValue && status.contains(QLatin1String(" 200"))) {
                    responseValue = propstatValue;
                }
            } else if (reader.name() == QLatin1String("response") && responseValue && !href.isEmpty()) {
                const QString path = normalizedPath(QUrl::fromEncoded(href.trimmed().toUtf8()).path());
                const Permissions permissions = parsePermissions(*responseValue);
                if (path == folderPath) {
                    result.folder = permissions;
                } else if (path.section(QLatin1Char('/'), 0, -2) == folderPath || (folderPath == QLatin1String("/") && path.count(QLatin1Char('/')) == 1)) {
                    result.items.insert(path.section(QLatin1Char('/'), -1), permissions);
                }
            }
        }
    }
    if (reader.hasError() || !multistatus) {
        if (error) {
            *error = i18nc("@info", "The server's list of permissions could not be read.");
        }
        return std::nullopt;
    }
    if (error) {
        error->clear();
    }
    return result;
}

QString Nextcloud::permissionsRequest()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\"><d:prop><oc:permissions/></d:prop></d:propfind>");
}

QString Nextcloud::deniedReason(Operation operation, Permissions permissions)
{
    switch (operation) {
    case Operation::Rename:
        return permissions.testFlag(Permission::Rename) ? QString() : i18nc("@info:tooltip", "The Nextcloud server doesn't allow renaming this item.");
    case Operation::Move:
        return permissions.testFlag(Permission::Move) ? QString() : i18nc("@info:tooltip", "The Nextcloud server doesn't allow moving this item.");
    case Operation::Delete:
        return permissions.testFlag(Permission::Delete) ? QString() : i18nc("@info:tooltip", "The Nextcloud server doesn't allow deleting this item.");
    case Operation::Create:
        return permissions & (Permission::CreateFile | Permission::CreateFolder)
            ? QString()
            : i18nc("@info:tooltip", "The Nextcloud server doesn't allow adding items to this folder.");
    case Operation::CreateFolder:
        return permissions.testFlag(Permission::CreateFolder) ? QString() : i18nc("@info:tooltip", "The Nextcloud server doesn't allow creating folders here.");
    }
    return {};
}

QString Nextcloud::deniedReason(Operation operation, const FolderPermissions &permissions, const QStringList &names)
{
    if (operation == Operation::Create || operation == Operation::CreateFolder) {
        return permissions.folder ? deniedReason(operation, *permissions.folder) : QString();
    }
    for (const QString &name : names) {
        const auto item = permissions.items.constFind(name);
        if (item == permissions.items.cend()) {
            continue;
        }
        const QString reason = deniedReason(operation, *item);
        if (!reason.isEmpty()) {
            return names.size() > 1 ? i18nc("@info:tooltip %1 is a file name, %2 the reason", "%1: %2", name, reason) : reason;
        }
    }
    return {};
}

bool Nextcloud::isAccessExpired(int kioError, int httpStatus)
{
    if (httpStatus != 0) {
        return httpStatus == 401;
    }
    return kioError == KIO::ERR_ACCESS_DENIED || kioError == KIO::ERR_CANNOT_AUTHENTICATE || kioError == KIO::ERR_CANNOT_LOGIN;
}

int Nextcloud::httpStatus(KJob *job)
{
    return responseCode(job);
}

QString Nextcloud::errorMessage(int kioError, int httpStatus, const QString &errorText)
{
    if (isAccessExpired(kioError, httpStatus)) {
        return i18nc("@info", "Access to the account expired or was revoked. Sign in again.");
    }
    if (httpStatus == 403) {
        return i18nc("@info", "The server refused the request.");
    }
    if (httpStatus == 503) {
        return i18nc("@info", "The server is unavailable, perhaps for maintenance. Try again later.");
    }
    if (httpStatus == 404) {
        return i18nc("@info", "The server has no Nextcloud at this address, or the requested item does not exist.");
    }
    if (httpStatus >= 500 || kioError == KIO::ERR_INTERNAL_SERVER) {
        return i18nc("@info", "The server reported an internal error. Try again later.");
    }
    switch (kioError) {
    case KIO::ERR_UNKNOWN_HOST:
        return i18nc("@info", "The server was not found. Check the server address and the network connection.");
    case KIO::ERR_CANNOT_CONNECT:
    case KIO::ERR_CONNECTION_BROKEN:
    case KIO::ERR_CANNOT_CREATE_SOCKET:
        return i18nc("@info %1 is the error",
                     "Could not connect securely to the server. Check the address, the network connection and that the server's certificate is "
                     "valid: %1",
                     errorText);
    case KIO::ERR_SERVER_TIMEOUT:
        return i18nc("@info", "The server did not answer in time. Try again later.");
    case KIO::ERR_DOES_NOT_EXIST:
        return i18nc("@info", "The server has no Nextcloud at this address, or the requested item does not exist.");
    case KIO::ERR_UNSUPPORTED_PROTOCOL:
        return i18nc("@info", "No KIO worker handles WebDAV or HTTP. Install the KIO HTTP worker.");
    default:
        break;
    }
    return errorText.isEmpty() ? i18nc("@info", "The request failed.") : errorText;
}

QStringList Nextcloud::describe(const Status &status)
{
    QStringList lines;
    if (!status.error.isEmpty() && !status.accessExpired) {
        lines.append(status.error);
    }
    if (status.capabilitiesKnown) {
        const QString product = status.capabilities.productName.isEmpty() ? QStringLiteral("Nextcloud") : status.capabilities.productName;
        lines.append(i18nc("@info %1 is a product name, %2 a version", "Server: %1 %2", product, status.capabilities.version));
    }
    if (status.accessExpired) {
        lines.append(i18nc("@info", "Access expired or was revoked. Sign in again."));
    } else if (status.signedIn) {
        lines.append(i18nc("@info %1 is a user name", "Signed in as %1.", status.user.displayName.isEmpty() ? status.user.id : status.user.displayName));
    } else if (status.capabilitiesKnown) {
        lines.append(i18nc("@info", "No saved sign-in was found, so the server only reported its version. Sign in to check its apps."));
    }
    if (status.signedIn && status.capabilities.authenticated) {
        const Capabilities &capabilities = status.capabilities;
        lines.append(capabilities.sharing ? i18nc("@info", "Sharing: available") : i18nc("@info", "Sharing: not available"));
        lines.append(capabilities.systemTags ? i18nc("@info", "Collaborative tags: available") : i18nc("@info", "Collaborative tags: not available"));
        if (!status.photos) {
            lines.append(i18nc("@info", "Photos: unknown"));
        } else {
            lines.append(*status.photos ? i18nc("@info", "Photos: available") : i18nc("@info", "Photos: not available"));
        }
        lines.append(capabilities.davSearch ? i18nc("@info", "File search: available") : i18nc("@info", "File search: not available"));
    }
    return lines;
}

KIO::AuthInfo Nextcloud::authInfo(const NextcloudAccounts::Account &account)
{
    KIO::AuthInfo info;
    info.url = NextcloudAccounts::filesUrl(account);
    info.username = NextcloudAccounts::loginName(account);
    info.realmValue = NextcloudAccounts::realm(account);
    info.keepPassword = true;
    info.verifyPath = false;
    return info;
}

QString Nextcloud::authorizationHeader(const QString &loginName, const QString &password)
{
    return QStringLiteral("Authorization: Basic ") + QString::fromLatin1((loginName + QLatin1Char(':') + password).toUtf8().toBase64());
}

Nextcloud::PasswordServerStore::PasswordServerStore(qlonglong windowId)
    : m_windowId(windowId)
{
}

std::optional<QString> Nextcloud::PasswordServerStore::password(const NextcloudAccounts::Account &account)
{
    KIO::AuthInfo info = authInfo(account);
    KPasswdServerClient client;
    if (client.checkAuthInfo(&info, m_windowId, 0) && info.username == NextcloudAccounts::loginName(account) && !info.password.isEmpty()) {
        return info.password;
    }
    return std::nullopt;
}

void Nextcloud::PasswordServerStore::store(const NextcloudAccounts::Account &account, const QString &password)
{
    KIO::AuthInfo info = authInfo(account);
    info.password = password;
    KPasswdServerClient client;
    client.addAuthInfo(info, m_windowId);
}

void Nextcloud::PasswordServerStore::forget(const NextcloudAccounts::Account &account)
{
    const QUrl url = NextcloudAccounts::filesUrl(account);
    KPasswdServerClient client;
    client.removeAuthInfo(url.host(), url.scheme(), NextcloudAccounts::loginName(account));
}

struct NextcloudClient::Check {
    NextcloudAccounts::Account account;
    std::optional<QString> password;
    Nextcloud::Status status;
};

NextcloudClient::NextcloudClient(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &NextcloudClient::poll);
}

NextcloudClient::~NextcloudClient()
{
    if (m_loginJob) {
        m_loginJob->kill(KJob::Quietly);
    }
}

void NextcloudClient::setCredentialStore(Nextcloud::CredentialStore *store)
{
    m_store = store;
}

void NextcloudClient::setPollInterval(int milliseconds)
{
    m_pollInterval = milliseconds;
}

void NextcloudClient::setLoginTimeout(int milliseconds)
{
    m_loginTimeout = milliseconds;
}

std::optional<QString> NextcloudClient::password(const NextcloudAccounts::Account &account) const
{
    return m_store ? m_store->password(account) : std::nullopt;
}

void NextcloudClient::prepare(KIO::Job *job, const QString &loginName, const QString &password)
{
    QStringList headers{QStringLiteral("OCS-APIRequest: true")};
    if (!password.isEmpty()) {
        headers.append(Nextcloud::authorizationHeader(loginName, password));
    }
    job->addMetaData(QStringLiteral("customHTTPHeader"), headers.join(QStringLiteral("\r\n")));
    // Fail on an invalid certificate instead of asking in the background,
    // and never ask for a password: requests either carry the app password
    // or are anonymous.
    job->addMetaData(QStringLiteral("ssl_no_ui"), QStringLiteral("true"));
    job->addMetaData(QStringLiteral("no-www-auth"), QStringLiteral("true"));
    job->addMetaData(QStringLiteral("cookies"), QStringLiteral("none"));
    job->addMetaData(QStringLiteral("UserAgent"), userAgent());
}

KIO::Job *NextcloudClient::ocsGet(const QUrl &url, const QString &loginName, const QString &password)
{
    KIO::StoredTransferJob *job = KIO::storedGet(withJsonFormat(url), KIO::Reload, KIO::HideProgressInfo);
    prepare(job, loginName, password);
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    return job;
}

void NextcloudClient::check(const NextcloudAccounts::Account &account)
{
    auto state = std::make_shared<Check>();
    state->account = account;
    state->password = password(account);
    runCheck(state);
}

void NextcloudClient::runCheck(const std::shared_ptr<Check> &state)
{
    const NextcloudAccounts::Account &account = state->account;
    const QString login = NextcloudAccounts::loginName(account);
    KIO::Job *capabilities =
        ocsGet(NextcloudAccounts::webUrl(account, QStringLiteral("ocs/v2.php/cloud/capabilities")), login, state->password.value_or(QString()));
    connect(capabilities, &KJob::result, this, [this, state, login](KJob *job) {
        const int code = responseCode(job);
        if (job->error() || code >= 400) {
            if (state->password && Nextcloud::isAccessExpired(job->error(), code)) {
                // The server still reports its version without the sign-in.
                state->status.accessExpired = true;
                state->password.reset();
                runCheck(state);
                return;
            }
            state->status.error = Nextcloud::errorMessage(job->error(), code, job->errorString());
            Q_EMIT checked(state->account.id, state->status);
            return;
        }
        QString error;
        const std::optional<Nextcloud::Capabilities> parsed = Nextcloud::parseCapabilities(static_cast<KIO::StoredTransferJob *>(job)->data(), &error);
        if (!parsed) {
            state->status.error = error;
            Q_EMIT checked(state->account.id, state->status);
            return;
        }
        state->status.capabilities = *parsed;
        state->status.capabilitiesKnown = true;
        if (!state->password) {
            Q_EMIT checked(state->account.id, state->status);
            return;
        }
        KIO::Job *user = ocsGet(NextcloudAccounts::webUrl(state->account, QStringLiteral("ocs/v2.php/cloud/user")), login, *state->password);
        connect(user, &KJob::result, this, [this, state, login](KJob *job) {
            const int code = responseCode(job);
            if (job->error() || code >= 400) {
                state->status.accessExpired = Nextcloud::isAccessExpired(job->error(), code);
                state->status.error = Nextcloud::errorMessage(job->error(), code, job->errorString());
                Q_EMIT checked(state->account.id, state->status);
                return;
            }
            QString error;
            const std::optional<Nextcloud::User> parsed = Nextcloud::parseUser(static_cast<KIO::StoredTransferJob *>(job)->data(), &error);
            if (!parsed) {
                state->status.error = error;
                Q_EMIT checked(state->account.id, state->status);
                return;
            }
            state->status.user = *parsed;
            state->status.signedIn = true;
            state->status.capabilities.authenticated = true;
            // Photos has no capability, so ask for the user's photos collection.
            const QUrl photos = NextcloudAccounts::davUrl(state->account, QStringLiteral("photos/") + state->account.user + QLatin1Char('/'));
            KIO::DavJob *probe =
                KIO::davPropFind(photos,
                                 QStringLiteral("<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\"><d:prop><d:resourcetype/></d:prop></d:propfind>"),
                                 QStringLiteral("0"),
                                 KIO::HideProgressInfo);
            prepare(probe, login, *state->password);
            connect(probe, &KJob::result, this, [this, state](KJob *job) {
                const int code = responseCode(job);
                if (!job->error() && code < 400) {
                    state->status.photos = true;
                } else if (code == 404 || code == 405 || job->error() == KIO::ERR_DOES_NOT_EXIST) {
                    state->status.photos = false;
                }
                Q_EMIT checked(state->account.id, state->status);
            });
        });
    });
}

void NextcloudClient::startLogin(const QUrl &server)
{
    cancelLogin();
    const int generation = ++m_loginGeneration;
    m_loginServer = server;
    QUrl url = server;
    url.setPath(server.path() + QStringLiteral("/index.php/login/v2"));
    KIO::StoredTransferJob *job = KIO::storedHttpPost(QByteArray(), url, KIO::HideProgressInfo);
    prepare(job, QString(), QString());
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    m_loginJob = job;
    connect(job, &KJob::result, this, [this, job, generation] {
        if (generation != m_loginGeneration) {
            return;
        }
        m_loginJob = nullptr;
        const int code = responseCode(job);
        if (job->error() || code >= 400) {
            failLogin(code == 404 ? i18nc("@info", "The server does not support Nextcloud Login Flow. Check the server address, or update the server.")
                                  : Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        m_login = Nextcloud::parseLoginFlow(job->data(), m_loginServer, &error);
        if (!m_login) {
            failLogin(error);
            return;
        }
        m_loginDeadline = QDateTime::currentMSecsSinceEpoch() + m_loginTimeout;
        Q_EMIT loginStarted(m_login->loginUrl);
        m_pollTimer->start(m_pollInterval);
    });
}

void NextcloudClient::poll()
{
    if (!m_login) {
        return;
    }
    if (QDateTime::currentMSecsSinceEpoch() > m_loginDeadline) {
        failLogin(i18nc("@info", "Signing in took too long. Try again."));
        return;
    }
    const int generation = m_loginGeneration;
    const QByteArray body = "token=" + QUrl::toPercentEncoding(m_login->token);
    KIO::StoredTransferJob *job = KIO::storedHttpPost(body, m_login->pollUrl, KIO::HideProgressInfo);
    prepare(job, QString(), QString());
    job->addMetaData(QStringLiteral("content-type"), QStringLiteral("Content-Type: application/x-www-form-urlencoded"));
    job->addMetaData(QStringLiteral("accept"), QStringLiteral("application/json"));
    m_loginJob = job;
    connect(job, &KJob::result, this, [this, job, generation] {
        if (generation != m_loginGeneration) {
            return;
        }
        m_loginJob = nullptr;
        const int code = responseCode(job);
        if (code == 404 || (job->error() == KIO::ERR_DOES_NOT_EXIST && code == 0)) {
            // The user hasn't granted access yet.
            m_pollTimer->start(m_pollInterval);
            return;
        }
        if (job->error() || code >= 400) {
            failLogin(Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<Nextcloud::LoginResult> result = Nextcloud::parseLoginResult(job->data(), &error);
        if (!result) {
            failLogin(error);
            return;
        }
        finishLogin(*result);
    });
}

void NextcloudClient::finishLogin(const Nextcloud::LoginResult &result)
{
    m_login.reset();
    const int generation = m_loginGeneration;
    // The address the user entered is kept: a server behind a proxy can
    // report its own address with the wrong scheme or host.
    auto account = std::make_shared<NextcloudAccounts::Account>();
    account->server = m_loginServer;
    account->loginName = result.loginName;
    const QString password = result.appPassword;
    KIO::Job *user = ocsGet(NextcloudAccounts::webUrl(*account, QStringLiteral("ocs/v2.php/cloud/user")), result.loginName, password);
    m_loginJob = user;
    connect(user, &KJob::result, this, [this, account, password, generation](KJob *job) {
        if (generation != m_loginGeneration) {
            return;
        }
        m_loginJob = nullptr;
        const int code = responseCode(job);
        if (job->error() || code >= 400) {
            failLogin(Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<Nextcloud::User> user = Nextcloud::parseUser(static_cast<KIO::StoredTransferJob *>(job)->data(), &error);
        if (!user) {
            failLogin(error);
            return;
        }
        account->user = user->id;
        if (account->loginName == account->user) {
            account->loginName.clear();
        }
        KIO::Job *capabilities =
            ocsGet(NextcloudAccounts::webUrl(*account, QStringLiteral("ocs/v2.php/cloud/capabilities")), NextcloudAccounts::loginName(*account), password);
        m_loginJob = capabilities;
        connect(capabilities, &KJob::result, this, [this, account, password, generation, user = *user](KJob *job) {
            if (generation != m_loginGeneration) {
                return;
            }
            m_loginJob = nullptr;
            Nextcloud::Status status;
            status.user = user;
            status.signedIn = true;
            const int code = responseCode(job);
            if (!job->error() && code < 400) {
                if (const auto parsed = Nextcloud::parseCapabilities(static_cast<KIO::StoredTransferJob *>(job)->data())) {
                    status.capabilities = *parsed;
                    status.capabilities.authenticated = true;
                    status.capabilitiesKnown = true;
                    account->realm = parsed->productName;
                }
            }
            if (account->realm == NextcloudAccounts::defaultRealm()) {
                account->realm.clear();
            }
            Q_EMIT loginFinished(*account, password, status);
        });
    });
}

void NextcloudClient::failLogin(const QString &error)
{
    m_login.reset();
    m_pollTimer->stop();
    Q_EMIT loginFailed(error);
}

void NextcloudClient::cancelLogin()
{
    ++m_loginGeneration;
    m_login.reset();
    m_pollTimer->stop();
    if (m_loginJob) {
        m_loginJob->kill(KJob::Quietly);
    }
    m_loginJob = nullptr;
}

bool NextcloudClient::isLoggingIn() const
{
    return m_login.has_value() || m_loginJob;
}

void NextcloudClient::fetchPermissions(const NextcloudAccounts::Account &account, const QUrl &folder)
{
    const std::optional<QString> secret = password(account);
    KIO::DavJob *job = KIO::davPropFind(folder, Nextcloud::permissionsRequest(), QStringLiteral("1"), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), secret.value_or(QString()));
    connect(job, &KJob::result, this, [this, job, folder] {
        const int code = responseCode(job);
        if (job->error() || code >= 400) {
            Q_EMIT permissionsFetched(folder, {}, Nextcloud::errorMessage(job->error(), code, job->errorString()));
            return;
        }
        QString error;
        const std::optional<Nextcloud::FolderPermissions> permissions = Nextcloud::parseFolderPermissions(job->responseData(), folder, &error);
        Q_EMIT permissionsFetched(folder, permissions.value_or(Nextcloud::FolderPermissions()), error);
    });
}

void NextcloudClient::revokeAppPassword(const NextcloudAccounts::Account &account)
{
    const std::optional<QString> secret = password(account);
    if (!secret) {
        Q_EMIT revoked(account.id, i18nc("@info", "No saved app password was found to revoke."));
        return;
    }
    KIO::TransferJob *job =
        KIO::http_delete(withJsonFormat(NextcloudAccounts::webUrl(account, QStringLiteral("ocs/v2.php/core/apppassword"))), KIO::HideProgressInfo);
    prepare(job, NextcloudAccounts::loginName(account), *secret);
    const QString id = account.id;
    connect(job, &KJob::result, this, [this, id](KJob *job) {
        const int code = responseCode(job);
        Q_EMIT revoked(id, job->error() || code >= 400 ? Nextcloud::errorMessage(job->error(), code, job->errorString()) : QString());
    });
}
