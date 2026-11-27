// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudclient.h"

#include <KIO/Global>

#include <QHash>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using Nextcloud::Permission;
using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("app-password-0123456789");

QByteArray capabilities(bool authenticated)
{
    QByteArray apps;
    if (authenticated) {
        apps = ",\"files_sharing\":{\"api_enabled\":true},\"systemtags\":{\"enabled\":true},\"dav\":{\"chunking\":\"1.0\",\"bulkupload\":\"1.0\"}";
    }
    return "{\"ocs\":{\"meta\":{\"status\":\"ok\",\"statuscode\":200,\"message\":\"OK\"},\"data\":{\"version\":{\"major\":29,\"minor\":0,\"micro\":4,"
           "\"string\":\"29.0.4\",\"edition\":\"\"},\"capabilities\":{\"core\":{\"pollinterval\":60},\"theming\":{\"name\":\"Example Cloud\"}"
        + apps + "}}}}";
}

QByteArray unauthorized()
{
    return "{\"ocs\":{\"meta\":{\"status\":\"failure\",\"statuscode\":997,\"message\":\"Current user is not logged in\"},\"data\":[]}}";
}

QByteArray multistatus(const QList<QPair<QByteArray, QByteArray>> &entries)
{
    QByteArray body = "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">";
    for (const auto &[href, permissions] : entries) {
        body += "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop><oc:permissions>" + permissions
            + "</oc:permissions></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
    }
    return body + "</d:multistatus>";
}

struct Request {
    QByteArray method;
    QByteArray target;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

class MemoryStore : public Nextcloud::CredentialStore
{
public:
    std::optional<QString> password(const Account &account) override
    {
        if (!passwords.contains(NextcloudAccounts::loginName(account))) {
            return std::nullopt;
        }
        return passwords.value(NextcloudAccounts::loginName(account));
    }
    void store(const Account &account, const QString &password) override
    {
        passwords.insert(NextcloudAccounts::loginName(account), password);
    }
    void forget(const Account &account) override
    {
        passwords.remove(NextcloudAccounts::loginName(account));
    }
    QHash<QString, QString> passwords;
};
}

class NextcloudClientTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    int m_polls = 0;
    bool m_photos = true;

    QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()));
    }

    Account account() const
    {
        Account account;
        account.id = QStringLiteral("test");
        account.server = base();
        account.user = QStringLiteral("alice");
        account.loginName = QStringLiteral("alice@example.com");
        return account;
    }

    bool authorized(const Request &request) const
    {
        return request.headers.value("authorization") == "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64();
    }

    QByteArray respond(const Request &request)
    {
        const QByteArray path = request.target.split('?').first();
        auto reply = [](const QByteArray &status, const QByteArray &type, const QByteArray &body) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        const QByteArray json("application/json; charset=utf-8");
        const QByteArray xml("application/xml; charset=utf-8");
        const bool hasAuthorization = request.headers.contains("authorization");
        if (request.method == "GET" && path == "/ocs/v2.php/cloud/capabilities") {
            if (hasAuthorization && !authorized(request)) {
                return reply("401 Unauthorized", json, unauthorized());
            }
            return reply("200 OK", json, capabilities(hasAuthorization));
        }
        if (request.method == "GET" && path == "/ocs/v2.php/cloud/user") {
            if (!authorized(request)) {
                return reply("401 Unauthorized", json, unauthorized());
            }
            return reply("200 OK",
                         json,
                         "{\"ocs\":{\"meta\":{\"status\":\"ok\",\"statuscode\":200,\"message\":\"OK\"},\"data\":{\"id\":\"alice\",\"display-name\":\"Alice "
                         "Liddell\"}}}");
        }
        if (request.method == "PROPFIND" && path == "/remote.php/dav/photos/alice/") {
            if (!authorized(request)) {
                return reply("401 Unauthorized", xml, QByteArray());
            }
            return m_photos ? reply("207 Multi-Status", xml, multistatus({{"/remote.php/dav/photos/alice/", "RGDNVCK"}}))
                            : reply("404 Not Found", xml, QByteArray());
        }
        if (request.method == "PROPFIND" && path.startsWith("/remote.php/dav/files/alice/Shared")) {
            if (!authorized(request)) {
                return reply("401 Unauthorized", xml, QByteArray());
            }
            return reply("207 Multi-Status",
                         xml,
                         multistatus({{"/remote.php/dav/files/alice/Shared/", "SRMG"},
                                      {"/remote.php/dav/files/alice/Shared/Read%20only.txt", "SRMG"},
                                      {"http://127.0.0.1/remote.php/dav/files/alice/Shared/Editable.odt", "SRMGDNVW"},
                                      {"/remote.php/dav/files/alice/Shared/Sub/", "SRMGDNVCK"},
                                      {"/remote.php/dav/files/alice/Shared/Sub/Deep.txt", "G"}}));
        }
        if (request.method == "POST" && path == "/index.php/login/v2") {
            const QByteArray server = base().toString().toUtf8();
            return reply("200 OK",
                         json,
                         "{\"poll\":{\"token\":\"poll token/1\",\"endpoint\":\"" + server + "/login/v2/poll\"},\"login\":\"" + server
                             + "/login/v2/flow/abc\"}");
        }
        if (request.method == "POST" && path == "/login/v2/poll") {
            if (++m_polls < 3) {
                return reply("404 Not Found", json, "[]");
            }
            return reply("200 OK",
                         json,
                         "{\"server\":\"http://wrong.example.com\",\"loginName\":\"alice@example.com\",\"appPassword\":\"" + appPassword.toUtf8() + "\"}");
        }
        if (request.method == "DELETE" && path == "/ocs/v2.php/core/apppassword") {
            if (!authorized(request)) {
                return reply("401 Unauthorized", json, unauthorized());
            }
            return reply("200 OK", json, "{\"ocs\":{\"meta\":{\"status\":\"ok\",\"statuscode\":200,\"message\":\"OK\"},\"data\":[]}}");
        }
        return reply("404 Not Found", "text/plain", "missing");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_server.listen(QHostAddress::LocalHost));
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, data = QByteArray()]() mutable {
                    data += socket->readAll();
                    const int end = data.indexOf("\r\n\r\n");
                    if (end < 0) {
                        return;
                    }
                    Request request;
                    const QList<QByteArray> lines = data.left(end).split('\n');
                    const QList<QByteArray> start = lines.first().trimmed().split(' ');
                    request.method = start.value(0);
                    request.target = start.value(1);
                    for (const QByteArray &line : lines.mid(1)) {
                        const int colon = line.indexOf(':');
                        if (colon > 0) {
                            request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
                        }
                    }
                    const int length = request.headers.value("content-length").toInt();
                    if (data.size() < end + 4 + length) {
                        return;
                    }
                    request.body = data.mid(end + 4, length);
                    m_requests.append(request);
                    socket->write(respond(request));
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void init()
    {
        m_requests.clear();
        m_polls = 0;
        m_photos = true;
    }

    void capabilitiesAreParsed()
    {
        QString error;
        const auto anonymous = Nextcloud::parseCapabilities(capabilities(false), &error);
        QVERIFY2(anonymous, qPrintable(error));
        QCOMPARE(anonymous->version, QStringLiteral("29.0.4"));
        QCOMPARE(anonymous->major, 29);
        QCOMPARE(anonymous->productName, QStringLiteral("Example Cloud"));
        QVERIFY(!anonymous->sharing && !anonymous->systemTags && !anonymous->dav && !anonymous->davSearch);

        const auto full = Nextcloud::parseCapabilities(capabilities(true));
        QVERIFY(full);
        QVERIFY(full->sharing && full->systemTags && full->dav && full->davSearch);

        QVERIFY(!Nextcloud::parseCapabilities("<html>Welcome</html>", &error));
        QVERIFY(error.contains(QStringLiteral("not from Nextcloud")));
        QVERIFY(!Nextcloud::parseCapabilities(unauthorized(), &error));
        QCOMPARE(error, QStringLiteral("Current user is not logged in"));
    }

    void loginFlowResponsesAreValidated()
    {
        const QUrl server(QStringLiteral("https://cloud.example.com"));
        QString error;
        const auto flow = Nextcloud::parseLoginFlow(
            "{\"poll\":{\"token\":\"t\",\"endpoint\":\"https://cloud.example.com/login/v2/poll\"},\"login\":\"https://cloud.example.com/login/v2/flow/x\"}",
            server,
            &error);
        QVERIFY2(flow, qPrintable(error));
        QCOMPARE(flow->token, QStringLiteral("t"));

        QVERIFY(!Nextcloud::parseLoginFlow(
            "{\"poll\":{\"token\":\"t\",\"endpoint\":\"http://cloud.example.com/login/v2/poll\"},\"login\":\"https://cloud.example.com/login/v2/flow/x\"}",
            server,
            &error));
        QVERIFY(error.contains(QStringLiteral("unencrypted")));
        QVERIFY(!Nextcloud::parseLoginFlow(
            "{\"poll\":{\"token\":\"t\",\"endpoint\":\"https://evil.example.net/poll\"},\"login\":\"https://cloud.example.com/login/v2/flow/x\"}",
            server,
            &error));
        QVERIFY(error.contains(QStringLiteral("evil.example.net")));
        QVERIFY(!Nextcloud::parseLoginFlow("{}", server, &error));
        QVERIFY(!Nextcloud::parseLoginResult("{\"loginName\":\"alice\"}", &error));
    }

    void permissionsAreParsed()
    {
        QCOMPARE(Nextcloud::parsePermissions(QStringLiteral("RGDNVCK")),
                 Nextcloud::Permissions(Permission::Shareable | Permission::Readable | Permission::Delete | Permission::Rename | Permission::Move
                                        | Permission::CreateFile | Permission::CreateFolder));
        QCOMPARE(Nextcloud::parsePermissions(QString()), Nextcloud::Permissions());

        const QUrl folder(QStringLiteral("webdavs://cloud.example.com/remote.php/dav/files/alice/Shared"));
        QString error;
        const auto parsed = Nextcloud::parseFolderPermissions(multistatus({{"/remote.php/dav/files/alice/Shared/", "SRMG"},
                                                                           {"/remote.php/dav/files/alice/Shared/A%20B.txt", "GW"},
                                                                           {"/remote.php/dav/files/alice/Shared/Sub/", "GDNVCK"},
                                                                           {"/remote.php/dav/files/alice/Shared/Sub/Deep.txt", "G"}}),
                                                              folder,
                                                              &error);
        QVERIFY2(parsed, qPrintable(error));
        QVERIFY(parsed->folder);
        QVERIFY(!(*parsed->folder & (Permission::CreateFile | Permission::CreateFolder)));
        QCOMPARE(parsed->items.size(), 2);
        QCOMPARE(parsed->items.value(QStringLiteral("A B.txt")), Nextcloud::Permissions(Permission::Readable | Permission::Write));
        QVERIFY(parsed->items.value(QStringLiteral("Sub")).testFlag(Permission::Rename));

        QVERIFY(!Nextcloud::parseFolderPermissions("not xml", folder, &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(Nextcloud::deniedReason(Nextcloud::Operation::Rename, Permission::Readable).contains(QStringLiteral("renaming")));
        QVERIFY(Nextcloud::deniedReason(Nextcloud::Operation::Rename, Permission::Rename).isEmpty());
        QVERIFY(Nextcloud::deniedReason(Nextcloud::Operation::Create, Permission::CreateFile).isEmpty());
        QVERIFY(!Nextcloud::deniedReason(Nextcloud::Operation::CreateFolder, Permission::CreateFile).isEmpty());
        QVERIFY(!Nextcloud::deniedReason(Nextcloud::Operation::Delete, Permission::Readable).isEmpty());
    }

    void errorsAreMapped()
    {
        QVERIFY(Nextcloud::isAccessExpired(KIO::ERR_ACCESS_DENIED, 0));
        QVERIFY(Nextcloud::isAccessExpired(KIO::ERR_DOES_NOT_EXIST, 401));
        QVERIFY(!Nextcloud::isAccessExpired(KIO::ERR_ACCESS_DENIED, 403));
        QVERIFY(Nextcloud::errorMessage(KIO::ERR_ACCESS_DENIED, 0, QString()).contains(QStringLiteral("Sign in again")));
        QVERIFY(Nextcloud::errorMessage(KIO::ERR_CANNOT_CONNECT, 0, QStringLiteral("SSL handshake failed")).contains(QStringLiteral("certificate")));
        QVERIFY(Nextcloud::errorMessage(KIO::ERR_INTERNAL_SERVER, 503, QString()).contains(QStringLiteral("maintenance")));
        QVERIFY(Nextcloud::errorMessage(KIO::ERR_UNKNOWN_HOST, 0, QString()).contains(QStringLiteral("not found")));
    }

    void authInfoMatchesTheWebDavRealm()
    {
        Account nextcloud = account();
        nextcloud.server = QUrl(QStringLiteral("https://cloud.example.com"));
        nextcloud.realm = QStringLiteral("Example Cloud");
        const KIO::AuthInfo info = Nextcloud::authInfo(nextcloud);
        QCOMPARE(info.url.toString(), QStringLiteral("webdavs://alice%40example.com@cloud.example.com/remote.php/dav/files/alice/"));
        QCOMPARE(info.username, QStringLiteral("alice@example.com"));
        QCOMPARE(info.realmValue, QStringLiteral("Example Cloud"));
        QVERIFY(info.keepPassword);
        QVERIFY(info.password.isEmpty());
    }

    void checkReportsCapabilitiesUserAndPhotos()
    {
        MemoryStore store;
        store.passwords.insert(QStringLiteral("alice@example.com"), appPassword);
        NextcloudClient client;
        client.setCredentialStore(&store);
        std::optional<Nextcloud::Status> status;
        connect(&client, &NextcloudClient::checked, this, [&status](const QString &id, const Nextcloud::Status &result) {
            QCOMPARE(id, QStringLiteral("test"));
            status = result;
        });
        client.check(account());
        QTRY_VERIFY_WITH_TIMEOUT(status.has_value(), 20000);
        QVERIFY2(status->error.isEmpty(), qPrintable(status->error));
        QVERIFY(status->signedIn);
        QVERIFY(!status->accessExpired);
        QCOMPARE(status->user.displayName, QStringLiteral("Alice Liddell"));
        QVERIFY(status->capabilities.authenticated);
        QVERIFY(status->capabilities.sharing);
        QCOMPARE(status->photos, std::optional<bool>(true));
        const QStringList lines = Nextcloud::describe(*status);
        QVERIFY(lines.contains(QStringLiteral("Server: Example Cloud 29.0.4")));
        QVERIFY(lines.contains(QStringLiteral("Photos: available")));
        QVERIFY(lines.contains(QStringLiteral("File search: available")));

        // Requests identify the client, ask for JSON, and never carry the password in the URL.
        for (const Request &request : std::as_const(m_requests)) {
            QVERIFY(!request.target.contains(appPassword.toUtf8()));
            QCOMPARE(request.headers.value("user-agent"), QByteArray("Plasma Opus (KDE)"));
        }
        QCOMPARE(m_requests.first().headers.value("ocs-apirequest"), QByteArray("true"));
        QVERIFY(m_requests.first().target.contains("format=json"));

        m_photos = false;
        status.reset();
        client.check(account());
        QTRY_VERIFY_WITH_TIMEOUT(status.has_value(), 20000);
        QCOMPARE(status->photos, std::optional<bool>(false));
        QVERIFY(Nextcloud::describe(*status).contains(QStringLiteral("Photos: not available")));
    }

    void checkReportsExpiredAccessAndMissingSignIn()
    {
        MemoryStore store;
        store.passwords.insert(QStringLiteral("alice@example.com"), QStringLiteral("revoked"));
        NextcloudClient client;
        client.setCredentialStore(&store);
        std::optional<Nextcloud::Status> status;
        connect(&client, &NextcloudClient::checked, this, [&status](const QString &, const Nextcloud::Status &result) {
            status = result;
        });
        client.check(account());
        QTRY_VERIFY_WITH_TIMEOUT(status.has_value(), 20000);
        QVERIFY(status->accessExpired);
        QVERIFY(!status->signedIn);
        QVERIFY(status->capabilitiesKnown);
        QCOMPARE(status->capabilities.version, QStringLiteral("29.0.4"));
        QVERIFY(Nextcloud::describe(*status).contains(QStringLiteral("Access expired or was revoked. Sign in again.")));

        store.passwords.clear();
        status.reset();
        client.check(account());
        QTRY_VERIFY_WITH_TIMEOUT(status.has_value(), 20000);
        QVERIFY(!status->accessExpired);
        QVERIFY(!status->signedIn);
        QVERIFY(!status->capabilities.authenticated);
        QVERIFY(Nextcloud::describe(*status).join(QLatin1Char('\n')).contains(QStringLiteral("Sign in to check its apps")));
    }

    void unreachableServersAreReported()
    {
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost));
        const quint16 port = closed.serverPort();
        closed.close();
        Account unreachable = account();
        unreachable.server = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));
        NextcloudClient client;
        std::optional<Nextcloud::Status> status;
        connect(&client, &NextcloudClient::checked, this, [&status](const QString &, const Nextcloud::Status &result) {
            status = result;
        });
        client.check(unreachable);
        QTRY_VERIFY_WITH_TIMEOUT(status.has_value(), 20000);
        QVERIFY(!status->capabilitiesKnown);
        QVERIFY(!status->error.isEmpty());
    }

    void loginFlowPollsUntilAccessIsGranted()
    {
        NextcloudClient client;
        client.setPollInterval(50);
        QUrl loginPage;
        std::optional<Account> added;
        QString password;
        QString failure;
        connect(&client, &NextcloudClient::loginStarted, this, [&loginPage](const QUrl &url) {
            loginPage = url;
        });
        connect(&client, &NextcloudClient::loginFinished, this, [&](const Account &account, const QString &appPassword, const Nextcloud::Status &status) {
            added = account;
            password = appPassword;
            QVERIFY(status.signedIn);
            QVERIFY(status.capabilities.sharing);
        });
        connect(&client, &NextcloudClient::loginFailed, this, [&failure](const QString &error) {
            failure = error;
        });
        client.startLogin(base());
        QVERIFY(client.isLoggingIn());
        QTRY_VERIFY_WITH_TIMEOUT(!loginPage.isEmpty(), 20000);
        QCOMPARE(loginPage.path(), QStringLiteral("/login/v2/flow/abc"));
        QTRY_VERIFY_WITH_TIMEOUT(added.has_value() || !failure.isEmpty(), 30000);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QCOMPARE(m_polls, 3);
        QCOMPARE(password, appPassword);
        QCOMPARE(added->server, base());
        QCOMPARE(added->user, QStringLiteral("alice"));
        QCOMPARE(added->loginName, QStringLiteral("alice@example.com"));
        QCOMPARE(added->realm, QStringLiteral("Example Cloud"));
        QVERIFY(added->id.isEmpty());
        QVERIFY(!client.isLoggingIn());

        const auto poll = std::find_if(m_requests.cbegin(), m_requests.cend(), [](const Request &request) {
            return request.target == "/login/v2/poll";
        });
        QVERIFY(poll != m_requests.cend());
        QCOMPARE(poll->body, QByteArray("token=poll%20token%2F1"));
        QVERIFY(poll->headers.value("content-type").startsWith("application/x-www-form-urlencoded"));
    }

    void loginFlowCanBeCancelledAndTimesOut()
    {
        NextcloudClient client;
        client.setPollInterval(50);
        client.setLoginTimeout(0);
        QString failure;
        bool started = false;
        connect(&client, &NextcloudClient::loginStarted, this, [&started] {
            started = true;
        });
        connect(&client, &NextcloudClient::loginFailed, this, [&failure](const QString &error) {
            failure = error;
        });
        client.startLogin(base());
        QTRY_VERIFY_WITH_TIMEOUT(!failure.isEmpty(), 20000);
        QVERIFY(started);
        QVERIFY(failure.contains(QStringLiteral("too long")));

        failure.clear();
        started = false;
        client.setLoginTimeout(60000);
        client.startLogin(base());
        client.cancelLogin();
        QVERIFY(!client.isLoggingIn());
        QTest::qWait(500);
        QVERIFY(!started);
        QVERIFY(failure.isEmpty());

        client.startLogin(QUrl(QStringLiteral("http://127.0.0.1:%1/missing").arg(m_server.serverPort())));
        QTRY_VERIFY_WITH_TIMEOUT(!failure.isEmpty(), 20000);
        QVERIFY(failure.contains(QStringLiteral("Login Flow")));
    }

    void folderPermissionsAreFetched()
    {
        MemoryStore store;
        store.passwords.insert(QStringLiteral("alice@example.com"), appPassword);
        NextcloudClient client;
        client.setCredentialStore(&store);
        std::optional<Nextcloud::FolderPermissions> permissions;
        QString error;
        QUrl fetched;
        connect(&client,
                &NextcloudClient::permissionsFetched,
                this,
                [&](const QUrl &folder, const Nextcloud::FolderPermissions &result, const QString &message) {
                    fetched = folder;
                    permissions = result;
                    error = message;
                });
        QUrl folder = NextcloudAccounts::filesUrl(account());
        folder.setPath(folder.path() + QStringLiteral("Shared"));
        client.fetchPermissions(account(), folder);
        QTRY_VERIFY_WITH_TIMEOUT(permissions.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(fetched, folder);
        QVERIFY(permissions->folder);
        QVERIFY(!Nextcloud::deniedReason(Nextcloud::Operation::Create, *permissions->folder).isEmpty());
        QCOMPARE(permissions->items.size(), 3);
        QVERIFY(!permissions->items.value(QStringLiteral("Read only.txt")).testFlag(Permission::Delete));
        QVERIFY(permissions->items.value(QStringLiteral("Editable.odt")).testFlag(Permission::Rename));
        const Request &request = m_requests.last();
        QCOMPARE(request.method, QByteArray("PROPFIND"));
        QCOMPARE(request.headers.value("depth"), QByteArray("1"));
        QVERIFY(request.body.contains("oc:permissions"));

        store.passwords.clear();
        permissions.reset();
        client.fetchPermissions(account(), folder);
        QTRY_VERIFY_WITH_TIMEOUT(permissions.has_value(), 20000);
        QVERIFY(error.contains(QStringLiteral("Sign in again")));
        QVERIFY(!permissions->folder);
    }

    void appPasswordsCanBeRevoked()
    {
        MemoryStore store;
        NextcloudClient client;
        client.setCredentialStore(&store);
        std::optional<QString> result;
        connect(&client, &NextcloudClient::revoked, this, [&result](const QString &id, const QString &error) {
            QCOMPARE(id, QStringLiteral("test"));
            result = error;
        });
        client.revokeAppPassword(account());
        QVERIFY(result && !result->isEmpty());

        store.passwords.insert(QStringLiteral("alice@example.com"), appPassword);
        result.reset();
        client.revokeAppPassword(account());
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 20000);
        QVERIFY2(result->isEmpty(), qPrintable(*result));
        QCOMPARE(m_requests.last().method, QByteArray("DELETE"));
        QVERIFY(authorized(m_requests.last()));
    }
};

QTEST_GUILESS_MAIN(NextcloudClientTest)

#include "nextcloudclienttest.moc"
