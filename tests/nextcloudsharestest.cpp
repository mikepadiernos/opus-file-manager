// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudshares.h"
#include "nextcloudclient.h"

#include <KProtocolInfo>

#include <QHash>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("shares-app-password-0123");
const QByteArray passwordHash = "$2y$10$Zm9vYmFyaGFzaGhhc2hoYXNo";
const QByteArray sharesRoot = "/ocs/v2.php/apps/files_sharing/api/v1/shares";

QByteArray ocs(const QByteArray &data, int status = 200, const QByteArray &message = "OK")
{
    return "{\"ocs\":{\"meta\":{\"status\":\"" + QByteArray(status == 200 ? "ok" : "failure") + "\",\"statuscode\":" + QByteArray::number(status)
        + ",\"message\":\"" + message + "\"},\"data\":" + data + "}}";
}

/// A password-protected public link to a folder, as Nextcloud 28 reports it.
const QByteArray linkShare =
    "{\"id\":\"7\",\"share_type\":3,\"uid_owner\":\"alice\",\"displayname_owner\":\"Alice\",\"permissions\":1,\"can_edit\":true,\"can_delete\":true,"
    "\"stime\":1767268800,\"expiration\":\"2026-12-31 00:00:00\",\"token\":\"AbCdEf123\",\"note\":\"\",\"label\":\"Family\",\"path\":\"/Photos\","
    "\"item_type\":\"folder\",\"share_with\":\""
    + passwordHash + "\",\"share_with_displayname\":\"" + passwordHash + "\",\"password\":\"" + passwordHash
    + "\",\"url\":\"https://cloud.example.com/s/AbCdEf123\",\"mail_send\":0,\"hide_download\":0}";

const QByteArray userShare =
    "{\"id\":8,\"share_type\":0,\"uid_owner\":\"alice\",\"displayname_owner\":\"Alice\",\"permissions\":19,\"can_edit\":true,\"can_delete\":true,"
    "\"expiration\":null,\"path\":\"/Photos\",\"item_type\":\"folder\",\"share_with\":\"bob\",\"share_with_displayname\":\"Bob\",\"note\":\"Hi\"}";

const QByteArray groupShare =
    "{\"id\":\"9\",\"share_type\":1,\"uid_owner\":\"alice\",\"permissions\":1,\"path\":\"/Photos\",\"item_type\":\"folder\","
    "\"share_with\":\"family\",\"share_with_displayname\":\"Family\",\"expiration\":\"2027-01-15 00:00:00\"}";

const QByteArray deckShare =
    "{\"id\":\"10\",\"share_type\":12,\"uid_owner\":\"alice\",\"permissions\":1,\"path\":\"/Photos\",\"item_type\":\"folder\","
    "\"share_with\":\"3\",\"share_with_displayname\":\"Board\"}";

const QByteArray receivedShare =
    "{\"id\":\"20\",\"share_type\":0,\"uid_owner\":\"bob\",\"displayname_owner\":\"Bob\",\"permissions\":1,"
    "\"can_edit\":false,\"can_delete\":true,\"path\":\"/Plans.odt\",\"item_type\":\"file\",\"share_with\":\"alice\"}";

const QByteArray capabilities =
    ocs("{\"version\":{\"major\":28},\"capabilities\":{\"files_sharing\":{\"api_enabled\":true,\"public\":{\"enabled\":true,"
        "\"password\":{\"enforced\":true,\"askForOptionalPassword\":false},\"expire_date\":{\"enabled\":true,\"days\":7,\"enforced\":true},"
        "\"upload\":true},\"resharing\":true,\"group_sharing\":false,\"default_permissions\":17}}}");

struct Request {
    QByteArray method;
    QByteArray path;
    QUrlQuery query;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

class MemoryStore : public Nextcloud::CredentialStore
{
public:
    std::optional<QString> password(const Account &) override
    {
        return appPassword;
    }
    void store(const Account &, const QString &) override
    {
    }
    void forget(const Account &) override
    {
    }
};

QUrlQuery form(const QByteArray &body)
{
    return QUrlQuery(QString::fromUtf8(body));
}
}

class NextcloudSharesTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;
    /// How the server answers when its sharing app is disabled: 0 for enabled, 998 for an OCS error, 404 for a plain page.
    int m_sharingMissing = 0;

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

    QUrl fileUrl(const QString &path) const
    {
        QUrl url = NextcloudAccounts::filesUrl(account());
        url.setPath(url.path() + path);
        return url;
    }

    Nextcloud::Share share(const QString &id) const
    {
        Nextcloud::Share share;
        share.id = id;
        share.type = Nextcloud::ShareType::PublicLink;
        share.permissions = Nextcloud::SharePermission::Read;
        return share;
    }

    QByteArray respond(const Request &request)
    {
        auto reply = [](const QByteArray &status, const QByteArray &body = QByteArray(), const QByteArray &type = "application/json; charset=utf-8") {
            return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        if (request.headers.value("authorization") != "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64()) {
            return reply("401 Unauthorized", ocs("[]", 997, "Current user is not logged in"));
        }
        if (request.path == "/ocs/v2.php/cloud/capabilities") {
            return reply("200 OK", capabilities);
        }
        if (!request.path.startsWith(sharesRoot)) {
            return reply("404 Not Found", "<html/>", "text/html");
        }
        if (m_sharingMissing == 998) {
            return reply("404 Not Found", ocs("[]", 998, "Invalid query, please check the syntax."));
        }
        if (m_sharingMissing == 404) {
            return reply("404 Not Found", "<html>Not found</html>", "text/html");
        }
        if (request.method == "GET" && request.path == sharesRoot) {
            if (request.query.queryItemValue(QStringLiteral("shared_with_me")) == QLatin1String("true")) {
                return reply("200 OK", ocs("[" + receivedShare + "]"));
            }
            const QString path = request.query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded);
            if (path == QLatin1String("/Photos")) {
                return reply("200 OK", ocs("[" + linkShare + "," + userShare + "," + groupShare + "," + deckShare + "]"));
            }
            if (!path.isEmpty()) {
                return reply("404 Not Found", ocs("[]", 404, "Wrong path, file/folder does not exist"));
            }
            return reply("200 OK", ocs("[" + linkShare + "," + userShare + "]"));
        }
        if (request.method == "POST" && request.path == sharesRoot) {
            const QUrlQuery fields = form(request.body);
            if (fields.queryItemValue(QStringLiteral("shareWith")) == QLatin1String("nobody")) {
                return reply("404 Not Found", ocs("[]", 404, "Please specify a valid user"));
            }
            return reply("200 OK", ocs(fields.queryItemValue(QStringLiteral("shareType")) == QLatin1String("3") ? linkShare : userShare));
        }
        if (request.method == "PUT" && request.path == sharesRoot + "/7") {
            return reply("200 OK", ocs(linkShare));
        }
        if (request.method == "PUT" && request.path == sharesRoot + "/8") {
            return reply("403 Forbidden", ocs("[]", 403, "Cannot increase permissions"));
        }
        if (request.method == "DELETE" && request.path == sharesRoot + "/7") {
            return reply("200 OK", ocs("[]"));
        }
        if (request.method == "DELETE" && request.path == sharesRoot + "/9") {
            return reply("404 Not Found", ocs("[]", 404, "Wrong share ID, share does not exist"));
        }
        return reply("404 Not Found", ocs("[]", 404, "Not found"));
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
                    const QByteArray target = start.value(1);
                    request.path = target.split('?').first();
                    request.query = QUrlQuery(QString::fromUtf8(target.mid(request.path.size() + 1)));
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
        m_sharingMissing = 0;
    }

    void sharesAreParsedWithoutPasswordHashes()
    {
        QString error;
        const auto shares = Nextcloud::parseShares(ocs("[" + linkShare + "," + userShare + "," + groupShare + "," + deckShare + "]"), &error);
        QVERIFY2(shares, qPrintable(error));
        QCOMPARE(shares->size(), 4);

        const Nextcloud::Share &link = shares->at(0);
        QCOMPARE(link.id, QStringLiteral("7"));
        QCOMPARE(link.type, Nextcloud::ShareType::PublicLink);
        QCOMPARE(link.path, QStringLiteral("/Photos"));
        QVERIFY(link.folder);
        QCOMPARE(link.ownerDisplayName, QStringLiteral("Alice"));
        QCOMPARE(link.permissions, Nextcloud::SharePermissions(Nextcloud::SharePermission::Read));
        QCOMPARE(link.expiration, QDate(2026, 12, 31));
        QVERIFY(link.hasPassword);
        QCOMPARE(link.url, QUrl(QStringLiteral("https://cloud.example.com/s/AbCdEf123")));
        QCOMPARE(link.label, QStringLiteral("Family"));
        // The password hash a link reports is never kept.
        QVERIFY(link.shareWith.isEmpty());
        QVERIFY(link.shareWithDisplayName.isEmpty());

        const Nextcloud::Share &user = shares->at(1);
        QCOMPARE(user.id, QStringLiteral("8"));
        QCOMPARE(user.type, Nextcloud::ShareType::User);
        QCOMPARE(user.shareWith, QStringLiteral("bob"));
        QCOMPARE(user.shareWithDisplayName, QStringLiteral("Bob"));
        QCOMPARE(user.permissions.toInt(), 19);
        QVERIFY(!user.expiration.isValid());
        QVERIFY(!user.hasPassword);
        QCOMPARE(user.note, QStringLiteral("Hi"));
        QCOMPARE(Nextcloud::describePermissions(user.permissions, true), QStringLiteral("View, edit, reshare"));

        QCOMPARE(shares->at(2).type, Nextcloud::ShareType::Group);
        QCOMPARE(shares->at(2).expiration, QDate(2027, 1, 15));
        QCOMPARE(shares->at(3).type, Nextcloud::ShareType::Other);
        QCOMPARE(Nextcloud::describePermissions(Nextcloud::SharePermission::Create, true), QStringLiteral("Upload only (file drop)"));

        const auto single = Nextcloud::parseShare(ocs(linkShare), &error);
        QVERIFY2(single, qPrintable(error));
        QCOMPARE(*single, link);
        QVERIFY(!Nextcloud::parseShares("<html/>", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!Nextcloud::parseShares(ocs(linkShare), &error));
    }

    void capabilitiesAreParsed()
    {
        QString error;
        const auto parsed = Nextcloud::parseShareCapabilities(capabilities, &error);
        QVERIFY2(parsed, qPrintable(error));
        QVERIFY(parsed->api);
        QVERIFY(parsed->publicLinks);
        QVERIFY(parsed->passwordEnforced);
        QVERIFY(parsed->expireEnforced);
        QCOMPARE(parsed->expireDays, 7);
        QVERIFY(parsed->publicUpload);
        QVERIFY(parsed->resharing);
        QVERIFY(!parsed->groupSharing);
        QCOMPARE(parsed->defaultPermissions.toInt(), 17);

        // Without the sharing app, sharing is reported as unavailable.
        const auto none = Nextcloud::parseShareCapabilities(ocs("{\"capabilities\":{\"core\":{}}}"), &error);
        QVERIFY(none);
        QVERIFY(!none->api);
        QVERIFY(!Nextcloud::parseShareCapabilities("nope", &error));
    }

    void pathsAndUrlsStayInTheAccount()
    {
        QCOMPARE(Nextcloud::sharePath(account(), fileUrl(QStringLiteral("Photos/"))), QStringLiteral("/Photos"));
        QCOMPARE(Nextcloud::sharePath(account(), fileUrl(QStringLiteral("Photos/Lake view.jpg"))), QStringLiteral("/Photos/Lake view.jpg"));
        QVERIFY(Nextcloud::sharePath(account(), NextcloudAccounts::filesUrl(account())).isEmpty());
        QVERIFY(Nextcloud::sharePath(account(), fileUrl(QStringLiteral("../bob/x"))).isEmpty());
        QUrl other = fileUrl(QStringLiteral("Photos"));
        other.setPort(1);
        QVERIFY(Nextcloud::sharePath(account(), other).isEmpty());

        Nextcloud::Share received;
        received.path = QStringLiteral("/Plans.odt");
        QCOMPARE(Nextcloud::shareFileUrl(account(), received), fileUrl(QStringLiteral("Plans.odt")));
        received.path = QStringLiteral("/../x");
        QVERIFY(Nextcloud::shareFileUrl(account(), received).isEmpty());

        QCOMPARE(Nextcloud::sharesUrl(account(), QStringLiteral("7")).path(), QString::fromLatin1(sharesRoot + "/7"));
        QVERIFY(!Nextcloud::sharesUrl(account(), QStringLiteral("7/../../x")).isValid());
        QVERIFY(!Nextcloud::sharesUrl(account(), QStringLiteral("ocinternal:7")).isValid());
    }

    void bodiesAndBroadening()
    {
        QString error;
        Nextcloud::NewShare link;
        link.path = QStringLiteral("/Photos");
        link.permissions = Nextcloud::SharePermission::Read;
        link.expiration = QDate(2026, 12, 31);
        link.password = QStringLiteral("s3cret p&ss=");
        const auto body = Nextcloud::createShareBody(link, &error);
        QVERIFY2(body, qPrintable(error));
        const QUrlQuery fields = form(*body);
        QCOMPARE(fields.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded), QStringLiteral("/Photos"));
        QCOMPARE(fields.queryItemValue(QStringLiteral("shareType")), QStringLiteral("3"));
        QVERIFY(!fields.hasQueryItem(QStringLiteral("shareWith")));
        QCOMPARE(fields.queryItemValue(QStringLiteral("permissions")), QStringLiteral("1"));
        QCOMPARE(fields.queryItemValue(QStringLiteral("expireDate")), QStringLiteral("2026-12-31"));
        QCOMPARE(fields.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded), link.password);

        Nextcloud::NewShare user;
        user.path = QStringLiteral("/Photos");
        user.type = Nextcloud::ShareType::User;
        QVERIFY(!Nextcloud::createShareBody(user, &error));
        user.shareWith = QStringLiteral(" bob ");
        QCOMPARE(form(*Nextcloud::createShareBody(user)).queryItemValue(QStringLiteral("shareWith")), QStringLiteral("bob"));
        user.path = QStringLiteral("/");
        QVERIFY(!Nextcloud::createShareBody(user, &error));
        user.path = QStringLiteral("/a/../b");
        QVERIFY(!Nextcloud::createShareBody(user, &error));
        user.path = QStringLiteral("/Photos");
        user.type = Nextcloud::ShareType::Federated;
        QVERIFY(!Nextcloud::createShareBody(user, &error));

        Nextcloud::ShareUpdate update;
        update.expiration = QDate();
        update.password = QString();
        const QUrlQuery changes = form(Nextcloud::updateShareBody(update));
        QVERIFY(changes.hasQueryItem(QStringLiteral("expireDate")));
        QVERIFY(changes.queryItemValue(QStringLiteral("expireDate")).isEmpty());
        QVERIFY(changes.hasQueryItem(QStringLiteral("password")));
        QVERIFY(!changes.hasQueryItem(QStringLiteral("permissions")));

        Nextcloud::Share existing = share(QStringLiteral("7"));
        existing.expiration = QDate(2026, 12, 31);
        existing.hasPassword = true;
        auto broadens = [&existing](const Nextcloud::ShareUpdate &change) {
            return Nextcloud::broadensAccess(existing, change);
        };
        QVERIFY(broadens({Nextcloud::SharePermissions(Nextcloud::SharePermission::Read | Nextcloud::SharePermission::Update), {}, {}}));
        QVERIFY(!broadens({Nextcloud::SharePermissions(Nextcloud::SharePermission::Read), {}, {}}));
        QVERIFY(broadens({{}, QDate(), {}}));
        QVERIFY(broadens({{}, QDate(2027, 1, 1), {}}));
        QVERIFY(!broadens({{}, QDate(2026, 12, 1), {}}));
        QVERIFY(broadens({{}, {}, QString()}));
        QVERIFY(!broadens({{}, {}, QStringLiteral("new")}));
        existing.expiration = QDate();
        QVERIFY(!broadens({{}, QDate(2026, 12, 1), {}}));
        existing.permissions = Nextcloud::SharePermissions::fromInt(31);
        QVERIFY(!broadens({Nextcloud::SharePermissions(Nextcloud::SharePermission::Read), {}, {}}));
    }

    void listingsUseTheirScope()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy listed(&client, &NextcloudClient::sharesListed);

        const int item = client.listShares(account(), Nextcloud::ShareScope::Item, QStringLiteral("/Photos"));
        QTRY_COMPARE(listed.size(), 1);
        QCOMPARE(listed.at(0).at(0).toInt(), item);
        auto listing = listed.at(0).at(1).value<Nextcloud::ShareListing>();
        QVERIFY2(listing.error.isEmpty(), qPrintable(listing.error));
        QCOMPARE(listing.scope, Nextcloud::ShareScope::Item);
        QCOMPARE(listing.path, QStringLiteral("/Photos"));
        QCOMPARE(listing.shares.size(), 4);
        const Request &request = m_requests.last();
        QCOMPARE(request.method, QByteArray("GET"));
        QCOMPARE(request.path, sharesRoot);
        QCOMPARE(request.query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded), QStringLiteral("/Photos"));
        QCOMPARE(request.query.queryItemValue(QStringLiteral("reshares")), QStringLiteral("true"));
        QCOMPARE(request.query.queryItemValue(QStringLiteral("format")), QStringLiteral("json"));
        QCOMPARE(request.headers.value("ocs-apirequest"), QByteArray("true"));

        client.listShares(account(), Nextcloud::ShareScope::SharedWithMe);
        QTRY_COMPARE(listed.size(), 2);
        listing = listed.at(1).at(1).value<Nextcloud::ShareListing>();
        QCOMPARE(listing.shares.size(), 1);
        QCOMPARE(listing.shares.first().ownerDisplayName, QStringLiteral("Bob"));
        QVERIFY(!listing.shares.first().canEdit);
        QCOMPARE(m_requests.last().query.queryItemValue(QStringLiteral("shared_with_me")), QStringLiteral("true"));

        client.listShares(account(), Nextcloud::ShareScope::SharedByMe);
        QTRY_COMPARE(listed.size(), 3);
        QCOMPARE(listed.at(2).at(1).value<Nextcloud::ShareListing>().shares.size(), 2);
        QVERIFY(!m_requests.last().query.hasQueryItem(QStringLiteral("path")));
        QVERIFY(!m_requests.last().query.hasQueryItem(QStringLiteral("shared_with_me")));

        QSignalSpy fetched(&client, &NextcloudClient::shareCapabilitiesFetched);
        client.fetchShareCapabilities(account());
        QTRY_COMPARE(fetched.size(), 1);
        QVERIFY(fetched.first().at(2).toString().isEmpty());
        QCOMPARE(fetched.first().at(1).value<Nextcloud::ShareCapabilities>().expireDays, 7);
    }

    void errorsAndUnavailableSharing()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy listed(&client, &NextcloudClient::sharesListed);

        // A missing item is an error, not missing sharing.
        client.listShares(account(), Nextcloud::ShareScope::Item, QStringLiteral("/Missing"));
        QTRY_COMPARE(listed.size(), 1);
        auto listing = listed.at(0).at(1).value<Nextcloud::ShareListing>();
        QCOMPARE(listing.error, QStringLiteral("Wrong path, file/folder does not exist"));
        QVERIFY(!listing.unavailable);

        m_sharingMissing = 998;
        client.listShares(account(), Nextcloud::ShareScope::SharedByMe);
        QTRY_COMPARE(listed.size(), 2);
        listing = listed.at(1).at(1).value<Nextcloud::ShareListing>();
        QVERIFY(listing.unavailable);
        QVERIFY(listing.error.contains(QStringLiteral("sharing")));

        m_sharingMissing = 404;
        client.listShares(account(), Nextcloud::ShareScope::SharedByMe);
        QTRY_COMPARE(listed.size(), 3);
        QVERIFY(listed.at(2).at(1).value<Nextcloud::ShareListing>().unavailable);

        // Without the app password, access has expired.
        m_sharingMissing = 0;
        NextcloudClient anonymous;
        QSignalSpy anonymousListed(&anonymous, &NextcloudClient::sharesListed);
        anonymous.listShares(account(), Nextcloud::ShareScope::SharedByMe);
        QTRY_COMPARE(anonymousListed.size(), 1);
        listing = anonymousListed.first().at(1).value<Nextcloud::ShareListing>();
        QVERIFY(listing.error.contains(QStringLiteral("Sign in again")));
        QVERIFY(!listing.unavailable);
    }

    void creatingSendsTheFormOnce()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("http"))) {
            QSKIP("The KIO HTTP worker isn't installed.");
        }
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy created(&client, &NextcloudClient::shareCreated);
        Nextcloud::NewShare link;
        link.path = QStringLiteral("/Photos");
        link.permissions = Nextcloud::SharePermission::Read;
        link.password = QStringLiteral("s3cret p&ss=");
        client.createShare(account(), link);
        QTRY_COMPARE(created.size(), 1);
        QVERIFY2(created.first().at(2).toString().isEmpty(), qPrintable(created.first().at(2).toString()));
        QCOMPARE(created.first().at(0).toString(), QStringLiteral("/Photos"));
        const auto share = created.first().at(1).value<Nextcloud::Share>();
        QCOMPARE(share.id, QStringLiteral("7"));
        QVERIFY(share.hasPassword);
        QCOMPARE(m_requests.size(), 1);
        const Request &request = m_requests.first();
        QCOMPARE(request.method, QByteArray("POST"));
        QCOMPARE(request.path, sharesRoot);
        QCOMPARE(request.query.queryItemValue(QStringLiteral("format")), QStringLiteral("json"));
        QVERIFY2(request.headers.value("content-type").startsWith("application/x-www-form-urlencoded"), request.headers.value("content-type").constData());
        QCOMPARE(request.headers.value("ocs-apirequest"), QByteArray("true"));
        // The password goes in the body, never the address.
        QCOMPARE(form(request.body).queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded), link.password);
        QVERIFY(!request.query.hasQueryItem(QStringLiteral("password")));

        Nextcloud::NewShare nobody;
        nobody.path = QStringLiteral("/Photos");
        nobody.type = Nextcloud::ShareType::User;
        nobody.shareWith = QStringLiteral("nobody");
        client.createShare(account(), nobody);
        QTRY_COMPARE(created.size(), 2);
        QCOMPARE(created.at(1).at(2).toString(), QStringLiteral("Please specify a valid user"));

        // An invalid share is refused without a request.
        nobody.path = QStringLiteral("/");
        client.createShare(account(), nobody);
        QCOMPARE(created.size(), 3);
        QVERIFY(!created.at(2).at(2).toString().isEmpty());
        QCOMPARE(m_requests.size(), 2);
    }

    void updatingSendsAPutToTheShare()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("http"))) {
            QSKIP("The KIO HTTP worker isn't installed.");
        }
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy updated(&client, &NextcloudClient::shareUpdated);
        Nextcloud::ShareUpdate update;
        update.permissions = Nextcloud::SharePermissions(Nextcloud::SharePermission::Read);
        update.expiration = QDate();
        update.password = QStringLiteral("n3w pass");
        client.updateShare(account(), share(QStringLiteral("7")), update);
        QTRY_COMPARE(updated.size(), 1);
        QVERIFY2(updated.first().at(2).toString().isEmpty(), qPrintable(updated.first().at(2).toString()));
        // The server's new state when KIO passes it on, otherwise the change applied.
        const auto changed = updated.first().at(1).value<Nextcloud::Share>();
        QVERIFY(changed.expiration == QDate(2026, 12, 31) || !changed.expiration.isValid());
        QVERIFY(changed.hasPassword);
        QCOMPARE(m_requests.size(), 1);
        const Request &request = m_requests.first();
        QCOMPARE(request.method, QByteArray("PUT"));
        QCOMPARE(request.path, sharesRoot + "/7");
        QVERIFY2(request.headers.value("content-type").startsWith("application/x-www-form-urlencoded"), request.headers.value("content-type").constData());
        const QUrlQuery fields = form(request.body);
        QCOMPARE(fields.queryItemValue(QStringLiteral("permissions")), QStringLiteral("1"));
        QVERIFY(fields.hasQueryItem(QStringLiteral("expireDate")));
        QCOMPARE(fields.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded), QStringLiteral("n3w pass"));

        client.updateShare(account(), share(QStringLiteral("8")), update);
        QTRY_COMPARE(updated.size(), 2);
        QVERIFY(!updated.at(1).at(2).toString().isEmpty());
        QCOMPARE(updated.at(1).at(1).value<Nextcloud::Share>(), share(QStringLiteral("8")));

        // Nothing to change, or a share ID that could reach another address: no request.
        client.updateShare(account(), share(QStringLiteral("7")), {});
        client.updateShare(account(), share(QStringLiteral("../7")), update);
        QCOMPARE(updated.size(), 4);
        QCOMPARE(m_requests.size(), 2);
    }

    void revokingDeletesOnlyTheShare()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("http"))) {
            QSKIP("The KIO HTTP worker isn't installed.");
        }
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy deleted(&client, &NextcloudClient::shareDeleted);
        client.deleteShare(account(), share(QStringLiteral("7")));
        QTRY_COMPARE(deleted.size(), 1);
        QVERIFY2(deleted.first().at(1).toString().isEmpty(), qPrintable(deleted.first().at(1).toString()));
        QCOMPARE(m_requests.size(), 1);
        QCOMPARE(m_requests.first().method, QByteArray("DELETE"));
        QCOMPARE(m_requests.first().path, sharesRoot + "/7");

        // A share that no longer exists is already revoked.
        client.deleteShare(account(), share(QStringLiteral("9")));
        QTRY_COMPARE(deleted.size(), 2);
        QVERIFY(deleted.at(1).at(1).toString().isEmpty());

        client.deleteShare(account(), share(QString()));
        client.deleteShare(account(), share(QStringLiteral("7?x")));
        QCOMPARE(deleted.size(), 4);
        QVERIFY(!deleted.at(3).at(1).toString().isEmpty());
        QCOMPARE(m_requests.size(), 2);
        for (const Request &request : std::as_const(m_requests)) {
            QVERIFY(!request.path.startsWith("/remote.php/"));
        }
    }
};

QTEST_GUILESS_MAIN(NextcloudSharesTest)

#include "nextcloudsharestest.moc"
