// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudorganize.h"
#include "nextcloudclient.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("organize-app-password-0123");

/// A PROPFIND response of remote.php/dav/systemtags in Nextcloud 29's format.
const QByteArray tagsResponse =
    "<?xml version=\"1.0\"?>\n"
    "<d:multistatus xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
    " <d:response><d:href>/remote.php/dav/systemtags/</d:href>\n"
    "  <d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>\n"
    "  <d:propstat><d:prop><oc:id/><oc:display-name/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>\n"
    " </d:response>\n"
    " <d:response><d:href>/remote.php/dav/systemtags/5</d:href>\n"
    "  <d:propstat><d:prop><oc:id>5</oc:id><oc:display-name>Lake</oc:display-name><oc:user-visible>true</oc:user-visible>"
    "<oc:user-assignable>false</oc:user-assignable><oc:can-assign>false</oc:can-assign></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>\n"
    " </d:response>\n"
    " <d:response><d:href>/remote.php/dav/systemtags/2</d:href>\n"
    "  <d:propstat><d:prop><oc:id>2</oc:id><oc:display-name>Holiday</oc:display-name><oc:user-visible>true</oc:user-visible>"
    "<oc:user-assignable>true</oc:user-assignable><oc:can-assign>true</oc:can-assign></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>\n"
    " </d:response>\n"
    " <d:response><d:href>/remote.php/dav/systemtags/8</d:href>\n"
    "  <d:propstat><d:prop><oc:id>8</oc:id><oc:display-name>archive</oc:display-name><oc:user-visible>false</oc:user-visible>"
    "<oc:user-assignable>false</oc:user-assignable></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>\n"
    " </d:response>\n"
    "</d:multistatus>";

/// The tags of file 42, from remote.php/dav/systemtags-relations/files/42.
const QByteArray fileTagsResponse =
    "<?xml version=\"1.0\"?>\n"
    "<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\">\n"
    " <d:response><d:href>/remote.php/dav/systemtags-relations/files/42/</d:href>\n"
    "  <d:propstat><d:prop><oc:id/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>\n"
    " </d:response>\n"
    " <d:response><d:href>/remote.php/dav/systemtags-relations/files/42/2</d:href>\n"
    "  <d:propstat><d:prop><oc:id>2</oc:id><oc:display-name>Holiday</oc:display-name><oc:user-visible>true</oc:user-visible>"
    "<oc:user-assignable>true</oc:user-assignable><oc:can-assign>true</oc:can-assign></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>\n"
    " </d:response>\n"
    "</d:multistatus>";

QByteArray propPatchResponse(const QByteArray &href, const QByteArray &status)
{
    return "<?xml version=\"1.0\"?>\n"
           "<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\">\n"
           " <d:response><d:href>"
        + href + "</d:href>\n  <d:propstat><d:prop><oc:favorite/></d:prop><d:status>HTTP/1.1 " + status
        + "</d:status></d:propstat>\n </d:response>\n</d:multistatus>";
}

QByteArray fileResponse(const QByteArray &href, const QByteArray &fileId, const QByteArray &type, const QByteArray &extra = QByteArray())
{
    return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop>"
        + (type == "httpd/unix-directory" ? QByteArray("<d:resourcetype><d:collection/></d:resourcetype>") : QByteArray("<d:resourcetype/>"))
        + "<d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><d:getcontenttype>" + type + "</d:getcontenttype>"
        + (fileId.isEmpty() ? QByteArray() : "<oc:fileid>" + fileId + "</oc:fileid>") + "<oc:favorite>1</oc:favorite>"
        + "<nc:system-tags><nc:system-tag oc:id=\"2\">Holiday</nc:system-tag></nc:system-tags>" + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>"
        + extra + "</d:response>\n";
}

QByteArray multistatus(const QByteArray &responses)
{
    return "<?xml version=\"1.0\"?>\n<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n" + responses
        + "</d:multistatus>";
}

/// Favourites as REPORT oc:filter-files returns them, with one item whose ID is withheld and one with a forbidden property.
const QByteArray favoritesResponse =
    multistatus(fileResponse("/remote.php/dav/files/alice/Photos/Lake%20view.jpg", "42", "image/jpeg")
                + fileResponse("/remote.php/dav/files/alice/Shared/",
                               "51",
                               "httpd/unix-directory",
                               "<d:propstat><d:prop><oc:owner-display-name/></d:prop><d:status>HTTP/1.1 403 Forbidden</d:status></d:propstat>")
                + fileResponse("/remote.php/dav/files/alice/Locked.txt", QByteArray(), "text/plain"));

struct Request {
    QByteArray method;
    QByteArray target;
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
}

class NextcloudOrganizeTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;
    QList<QByteArray> m_createdTags;

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

    QByteArray respond(const Request &request)
    {
        const QByteArray path = request.target.split('?').first();
        auto reply = [](const QByteArray &status, const QByteArray &body, const QByteArray &headers = QByteArray()) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/xml; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n"
                + headers + "Connection: close\r\n\r\n" + body;
        };
        if (request.headers.value("authorization") != "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64()) {
            return reply("401 Unauthorized", QByteArray());
        }
        if (request.method == "PROPFIND" && path == "/remote.php/dav/systemtags/") {
            return reply("207 Multi-Status", tagsResponse);
        }
        if (request.method == "PROPFIND" && path.startsWith("/remote.php/dav/systemtags-relations/files/42")) {
            return reply("207 Multi-Status", fileTagsResponse);
        }
        if (request.method == "PROPPATCH") {
            return reply("207 Multi-Status", propPatchResponse(path, path.endsWith("Shared") ? "403 Forbidden" : "200 OK"));
        }
        if (request.method == "PUT" && path.startsWith("/remote.php/dav/systemtags-relations/files/42/")) {
            const QByteArray tag = path.mid(path.lastIndexOf('/') + 1);
            return reply(tag == "5" ? "409 Conflict" : tag == "8" ? "403 Forbidden" : "201 Created", QByteArray());
        }
        if (request.method == "DELETE" && path.startsWith("/remote.php/dav/systemtags-relations/files/42/")) {
            return reply(path.endsWith("/9") ? "404 Not Found" : "204 No Content", QByteArray());
        }
        if (request.method == "POST" && path == "/remote.php/dav/systemtags") {
            const QString name = QJsonDocument::fromJson(request.body).object().value(QStringLiteral("name")).toString();
            m_createdTags.append(name.toUtf8());
            // Like Nextcloud on MySQL, names differing only in case conflict.
            if (name.compare(QLatin1String("Holiday"), Qt::CaseInsensitive) == 0) {
                return reply("409 Conflict", QByteArray());
            }
            if (name == QLatin1String("Forbidden")) {
                return reply("403 Forbidden", QByteArray());
            }
            return reply("201 Created", QByteArray(), "Content-Location: /remote.php/dav/systemtags/12\r\n");
        }
        if (request.method == "REPORT" && path == "/remote.php/dav/files/alice/") {
            if (request.body.contains("<oc:systemtag>2</oc:systemtag>")) {
                return reply("207 Multi-Status", multistatus(fileResponse("/remote.php/dav/files/alice/Photos/Lake%20view.jpg", "42", "image/jpeg")));
            }
            if (request.body.contains("<oc:systemtag>")) {
                return reply("400 Bad Request", "<d:error xmlns:d=\"DAV:\"/>");
            }
            return reply("207 Multi-Status", favoritesResponse);
        }
        if (request.method == "SEARCH" && path == "/remote.php/dav/") {
            if (request.body.contains("unsupported")) {
                return reply("501 Not Implemented", QByteArray());
            }
            QByteArray responses;
            for (int i = 1; i <= 3; ++i) {
                responses +=
                    fileResponse("/remote.php/dav/files/alice/Photos/Lake%20" + QByteArray::number(i) + ".jpg", QByteArray::number(100 + i), "image/jpeg");
            }
            return reply("207 Multi-Status", multistatus(responses));
        }
        return reply("404 Not Found", QByteArray());
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
        m_createdTags.clear();
    }

    void tagsAreParsedAndSorted()
    {
        QString error;
        const auto tags = Nextcloud::parseSystemTags(tagsResponse, &error);
        QVERIFY2(tags, qPrintable(error));
        QCOMPARE(tags->size(), 3);
        QCOMPARE(tags->at(0).name, QStringLiteral("archive"));
        QCOMPARE(tags->at(1), (Nextcloud::SystemTag{QStringLiteral("2"), QStringLiteral("Holiday"), true, true, true}));
        QCOMPARE(tags->at(2).id, QStringLiteral("5"));
        QVERIFY(!Nextcloud::canAssign(tags->at(0)));
        QVERIFY(Nextcloud::canAssign(tags->at(1)));
        QVERIFY(!Nextcloud::canAssign(tags->at(2)));
        QCOMPARE(Nextcloud::describeTag(tags->at(0)), QStringLiteral("archive (invisible)"));
        QCOMPARE(Nextcloud::describeTag(tags->at(2)), QStringLiteral("Lake (restricted)"));
        QCOMPARE(Nextcloud::describeTag(tags->at(1)), QStringLiteral("Holiday"));
        // Servers before Nextcloud 11 don't report can-assign.
        QVERIFY(Nextcloud::canAssign(Nextcloud::SystemTag{QStringLiteral("3"), QStringLiteral("Old"), true, true, std::nullopt}));
        QVERIFY(!Nextcloud::parseSystemTags("<html/>", &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(Nextcloud::tagNameError(QStringLiteral("  ")).contains(QStringLiteral("name")));
        QVERIFY(!Nextcloud::tagNameError(QString(65, QLatin1Char('x'))).isEmpty());
        QVERIFY(Nextcloud::tagNameError(QStringLiteral("Trip")).isEmpty());
        const QJsonObject create = QJsonDocument::fromJson(Nextcloud::createTagRequest(QStringLiteral(" Trip "))).object();
        QCOMPARE(create.value(QStringLiteral("name")).toString(), QStringLiteral("Trip"));
        QVERIFY(create.value(QStringLiteral("userAssignable")).toBool());
        QCOMPARE(Nextcloud::createdTagId(QStringLiteral("HTTP/1.1 201 Created\nContent-Location: /remote.php/dav/systemtags/12\n")), QStringLiteral("12"));
        QVERIFY(Nextcloud::createdTagId(QStringLiteral("Content-Type: text/html")).isEmpty());
    }

    void requestsAreWellFormed()
    {
        QVERIFY(Nextcloud::favoriteRequest(true).contains(QStringLiteral("<oc:favorite>1</oc:favorite>")));
        QVERIFY(Nextcloud::favoriteRequest(false).contains(QStringLiteral("<oc:favorite>0</oc:favorite>")));
        QVERIFY(Nextcloud::filterFilesRequest().contains(QStringLiteral("<oc:favorite>1</oc:favorite>")));
        QVERIFY(Nextcloud::filterFilesRequest(QStringLiteral("2")).contains(QStringLiteral("<oc:systemtag>2</oc:systemtag>")));

        Nextcloud::SearchQuery query;
        query.text = QStringLiteral("lake & <view>");
        query.type = Nextcloud::AssetType::Images;
        query.favoritesOnly = true;
        query.limit = 50;
        const QString search = Nextcloud::searchRequest(account(), query);
        QVERIFY(search.contains(QStringLiteral("<d:href>/files/alice</d:href>")));
        QVERIFY(search.contains(QStringLiteral("<d:literal>%lake &amp; &lt;view&gt;%</d:literal>")));
        QVERIFY(search.contains(QStringLiteral("<d:literal>image/%</d:literal>")));
        QVERIFY(search.contains(QStringLiteral("<oc:favorite/></d:prop><d:literal>1</d:literal>")));
        QVERIFY(search.contains(QStringLiteral("<d:and>")));
        QVERIFY(search.contains(QStringLiteral("<d:nresults>51</d:nresults>")));
        const QString any = Nextcloud::searchRequest(account(), Nextcloud::SearchQuery{QStringLiteral("x"), Nextcloud::AssetType::Any, false, 10});
        QVERIFY(!any.contains(QStringLiteral("<d:and>")));
        QVERIFY(Nextcloud::searchRequest(account(), {QString(), Nextcloud::AssetType::Documents, false, 10}).contains(QStringLiteral("application/pdf")));

        QCOMPARE(Nextcloud::itemUrl(account(), QStringLiteral("/remote.php/dav/files/alice/Photos/Lake view.jpg")),
                 fileUrl(QStringLiteral("Photos/Lake view.jpg")));
        QVERIFY(!Nextcloud::itemUrl(account(), QStringLiteral("/remote.php/dav/files/bob/x.jpg")).isValid());
        QCOMPARE(Nextcloud::itemUrl(account(), QStringLiteral("/remote.php/dav/files/alice")), NextcloudAccounts::filesUrl(account()));
    }

    void resultsReportTruncationAndIncompleteItems()
    {
        Nextcloud::AssetResults results = Nextcloud::parseAssetResults(favoritesResponse, account(), 10);
        QVERIFY(results.error.isEmpty());
        QCOMPARE(results.items.size(), 2);
        QCOMPARE(results.incomplete, 2);
        QVERIFY(!results.truncated);
        QCOMPARE(results.items.first().fileId, QStringLiteral("42"));
        QVERIFY(results.items.at(1).folder);

        results = Nextcloud::parseAssetResults(favoritesResponse, account(), 1);
        QCOMPARE(results.items.size(), 1);
        QVERIFY(results.truncated);

        results = Nextcloud::parseAssetResults("<html/>", account(), 10);
        QVERIFY(!results.error.isEmpty());

        bool unavailable = false;
        QVERIFY(!Nextcloud::listingError(0, 501, QString(), &unavailable).isEmpty());
        QVERIFY(unavailable);
        Nextcloud::listingError(0, 500, QString(), &unavailable);
        QVERIFY(!unavailable);
    }

    void favoritesAreSetWithPropPatch()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        std::optional<QString> error;
        bool favorite = false;
        connect(&client, &NextcloudClient::favoriteSet, this, [&](const QUrl &, bool value, const QString &message) {
            favorite = value;
            error = message;
        });
        client.setFavorite(account(), fileUrl(QStringLiteral("Photos/Lake view.jpg")), true);
        QTRY_VERIFY_WITH_TIMEOUT(error.has_value(), 20000);
        QVERIFY2(error->isEmpty(), qPrintable(*error));
        QVERIFY(favorite);
        QCOMPARE(m_requests.last().method, QByteArray("PROPPATCH"));
        QVERIFY(m_requests.last().body.startsWith("<?xml"));
        QVERIFY(m_requests.last().body.endsWith("</d:propertyupdate>"));
        QVERIFY(m_requests.last().body.contains("<oc:favorite>1</oc:favorite>"));

        error.reset();
        client.setFavorite(account(), fileUrl(QStringLiteral("Shared")), false);
        QTRY_VERIFY_WITH_TIMEOUT(error.has_value(), 20000);
        QVERIFY2(error->contains(QStringLiteral("refused")), qPrintable(*error));
        QVERIFY(!favorite);
    }

    void tagsAreListedAssignedAndRemoved()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        std::optional<QList<Nextcloud::SystemTag>> tags;
        QString error;
        connect(&client, &NextcloudClient::systemTagsFetched, this, [&](const QString &, const QList<Nextcloud::SystemTag> &result, const QString &message) {
            tags = result;
            error = message;
        });
        connect(&client, &NextcloudClient::fileTagsFetched, this, [&](const QString &, const QList<Nextcloud::SystemTag> &result, const QString &message) {
            tags = result;
            error = message;
        });
        client.fetchSystemTags(account());
        QTRY_VERIFY_WITH_TIMEOUT(tags.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(tags->size(), 3);
        QCOMPARE(m_requests.last().headers.value("depth"), QByteArray("1"));

        tags.reset();
        client.fetchFileTags(account(), QStringLiteral("42"));
        QTRY_VERIFY_WITH_TIMEOUT(tags.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(tags->size(), 1);
        QCOMPARE(tags->first().name, QStringLiteral("Holiday"));

        QList<std::tuple<QString, bool, QString>> changes;
        connect(&client, &NextcloudClient::fileTagSet, this, [&](const QString &, const QString &tag, bool assigned, const QString &message) {
            changes.append({tag, assigned, message});
        });
        client.setFileTag(account(), QStringLiteral("42"), QStringLiteral("2"), true);
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 1, 20000);
        QCOMPARE(m_requests.last().method, QByteArray("PUT"));
        QCOMPARE(m_requests.last().target, QByteArray("/remote.php/dav/systemtags-relations/files/42/2"));
        // Already assigned.
        client.setFileTag(account(), QStringLiteral("42"), QStringLiteral("5"), true);
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 2, 20000);
        client.setFileTag(account(), QStringLiteral("42"), QStringLiteral("8"), true);
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 3, 20000);
        client.setFileTag(account(), QStringLiteral("42"), QStringLiteral("2"), false);
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 4, 20000);
        QCOMPARE(m_requests.last().method, QByteArray("DELETE"));
        // Already removed.
        client.setFileTag(account(), QStringLiteral("42"), QStringLiteral("9"), false);
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 5, 20000);
        QVERIFY2(std::get<2>(changes.at(0)).isEmpty(), qPrintable(std::get<2>(changes.at(0))));
        QVERIFY2(std::get<2>(changes.at(1)).isEmpty(), qPrintable(std::get<2>(changes.at(1))));
        QVERIFY2(std::get<2>(changes.at(2)).contains(QStringLiteral("doesn't allow")), qPrintable(std::get<2>(changes.at(2))));
        QVERIFY2(std::get<2>(changes.at(3)).isEmpty(), qPrintable(std::get<2>(changes.at(3))));
        QVERIFY2(std::get<2>(changes.at(4)).isEmpty(), qPrintable(std::get<2>(changes.at(4))));
        QVERIFY(!std::get<1>(changes.at(4)));

        client.setFileTag(account(), QString(), QStringLiteral("2"), true);
        QCOMPARE(changes.size(), 6);
        QVERIFY(!std::get<2>(changes.last()).isEmpty());
    }

    void tagsAreCreated()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        std::optional<Nextcloud::SystemTag> tag;
        QString error;
        connect(&client, &NextcloudClient::tagCreated, this, [&](const QString &, const Nextcloud::SystemTag &result, const QString &message) {
            tag = result;
            error = message;
        });
        client.createTag(account(), QStringLiteral(" Trip "));
        QTRY_VERIFY_WITH_TIMEOUT(tag.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(tag->id, QStringLiteral("12"));
        QCOMPARE(tag->name, QStringLiteral("Trip"));
        QCOMPARE(m_createdTags, QList<QByteArray>{"Trip"});
        QVERIFY(m_requests.last().headers.value("content-type").contains("application/json"));

        // An existing tag is found by name instead.
        tag.reset();
        client.createTag(account(), QStringLiteral("holiday"));
        QTRY_VERIFY_WITH_TIMEOUT(tag.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(tag->id, QStringLiteral("2"));

        tag.reset();
        client.createTag(account(), QStringLiteral("Forbidden"));
        QTRY_VERIFY_WITH_TIMEOUT(tag.has_value(), 20000);
        QVERIFY(error.contains(QStringLiteral("create tags")));
        QVERIFY(tag->id.isEmpty());

        tag.reset();
        client.createTag(account(), QStringLiteral(" "));
        QVERIFY(tag && tag->id.isEmpty());
        QCOMPARE(m_createdTags.size(), 3);
    }

    void favoritesAndTaggedFilesAreListed()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QHash<int, Nextcloud::AssetResults> listed;
        connect(&client, &NextcloudClient::assetsListed, this, [&](int request, const Nextcloud::AssetResults &results) {
            listed.insert(request, results);
        });
        const int favorites = client.listFavorites(account(), 100);
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(favorites), 20000);
        QCOMPARE(m_requests.last().method, QByteArray("REPORT"));
        QVERIFY(m_requests.last().body.contains("<oc:filter-rules><oc:favorite>1</oc:favorite></oc:filter-rules>"));
        QCOMPARE(listed.value(favorites).items.size(), 2);
        QCOMPARE(listed.value(favorites).incomplete, 2);

        const int tagged = client.listTagged(account(), QStringLiteral("2"), 100);
        QVERIFY(tagged != favorites);
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(tagged), 20000);
        QCOMPARE(listed.value(tagged).items.size(), 1);
        QCOMPARE(Nextcloud::itemUrl(account(), listed.value(tagged).items.first().path), fileUrl(QStringLiteral("Photos/Lake view.jpg")));

        const int unknown = client.listTagged(account(), QStringLiteral("99"), 100);
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(unknown), 20000);
        QVERIFY(!listed.value(unknown).error.isEmpty());

        const int none = client.listTagged(account(), QString(), 100);
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(none), 20000);
        QVERIFY(!listed.value(none).error.isEmpty());
    }

    void searchRunsOnTheServer()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QHash<int, Nextcloud::AssetResults> listed;
        connect(&client, &NextcloudClient::assetsListed, this, [&](int request, const Nextcloud::AssetResults &results) {
            listed.insert(request, results);
        });
        const int request = client.search(account(), {QStringLiteral("Lake"), Nextcloud::AssetType::Images, false, 2});
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(request), 20000);
        const Request &sent = m_requests.last();
        QCOMPARE(sent.method, QByteArray("SEARCH"));
        QCOMPARE(sent.target, QByteArray("/remote.php/dav/"));
        QVERIFY(sent.body.startsWith("<?xml"));
        QVERIFY(sent.body.endsWith("</d:searchrequest>"));
        QVERIFY(sent.body.contains("<d:nresults>3</d:nresults>"));
        const Nextcloud::AssetResults results = listed.value(request);
        QVERIFY2(results.error.isEmpty(), qPrintable(results.error));
        QCOMPARE(results.items.size(), 2);
        QVERIFY(results.truncated);
        QCOMPARE(results.items.first().fileId, QStringLiteral("101"));

        const int unsupported = client.search(account(), {QStringLiteral("unsupported"), Nextcloud::AssetType::Any, false, 2});
        QTRY_VERIFY_WITH_TIMEOUT(listed.contains(unsupported), 20000);
        QVERIFY(listed.value(unsupported).unavailable);
        QVERIFY(!listed.value(unsupported).error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(NextcloudOrganizeTest)

#include "nextcloudorganizetest.moc"
