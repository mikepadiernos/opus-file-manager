// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudassets.h"
#include "nextcloudclient.h"

#include <QBuffer>
#include <QHash>
#include <QImage>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("assets-app-password-0123");

/// A PROPFIND response recorded from Nextcloud 29 for a photo, with the server's address replaced.
const QByteArray photoResponse =
    "<?xml version=\"1.0\"?>\n"
    "<d:multistatus xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
    " <d:response>\n"
    "  <d:href>/remote.php/dav/files/alice/Photos/Lake%20view.jpg</d:href>\n"
    "  <d:propstat>\n"
    "   <d:prop>\n"
    "    <d:resourcetype/>\n"
    "    <d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified>\n"
    "    <d:getetag>&quot;6543a1b2c3d4e&quot;</d:getetag>\n"
    "    <d:getcontenttype>image/jpeg</d:getcontenttype>\n"
    "    <d:getcontentlength>2483922</d:getcontentlength>\n"
    "    <oc:fileid>42</oc:fileid>\n"
    "    <oc:size>2483922</oc:size>\n"
    "    <oc:permissions>RGDNVW</oc:permissions>\n"
    "    <oc:favorite>1</oc:favorite>\n"
    "    <oc:owner-id>alice</oc:owner-id>\n"
    "    <oc:owner-display-name>Alice Liddell</oc:owner-display-name>\n"
    "    <oc:share-types>\n"
    "     <oc:share-type>0</oc:share-type>\n"
    "     <oc:share-type>3</oc:share-type>\n"
    "    </oc:share-types>\n"
    "    <oc:tags/>\n"
    "    <nc:has-preview>true</nc:has-preview>\n"
    "    <nc:mount-type></nc:mount-type>\n"
    "    <nc:system-tags>\n"
    "     <nc:system-tag oc:id=\"2\" nc:can-assign=\"true\" nc:checked=\"true\">Holiday</nc:system-tag>\n"
    "     <nc:system-tag oc:id=\"5\" nc:can-assign=\"false\" nc:checked=\"true\">Lake</nc:system-tag>\n"
    "    </nc:system-tags>\n"
    "    <nc:metadata-photos-size><width>4032</width><height>3024</height></nc:metadata-photos-size>\n"
    "    <nc:metadata-photos-ifd0><Make>Canon</Make><Model>EOS R6</Model></nc:metadata-photos-ifd0>\n"
    "    <nc:metadata-photos-exif><ExposureTime>1/250</ExposureTime><FNumber>8/1</FNumber></nc:metadata-photos-exif>\n"
    "    <nc:metadata-photos-gps><latitude>46.5</latitude><longitude>6.6</longitude></nc:metadata-photos-gps>\n"
    "    <nc:metadata-photos-original_date_time>1717236000</nc:metadata-photos-original_date_time>\n"
    "   </d:prop>\n"
    "   <d:status>HTTP/1.1 200 OK</d:status>\n"
    "  </d:propstat>\n"
    "  <d:propstat>\n"
    "   <d:prop>\n"
    "    <nc:metadata-photos-place/>\n"
    "    <nc:metadata-blurhash/>\n"
    "    <nc:file-metadata-size/>\n"
    "   </d:prop>\n"
    "   <d:status>HTTP/1.1 404 Not Found</d:status>\n"
    "  </d:propstat>\n"
    " </d:response>\n"
    "</d:multistatus>";

/// A folder on Nextcloud 26, which reports metadata as JSON and withholds the owner.
const QByteArray folderResponse =
    "<?xml version=\"1.0\"?>\n"
    "<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n"
    " <d:response>\n"
    "  <d:href>/remote.php/dav/files/alice/Shared/</d:href>\n"
    "  <d:propstat>\n"
    "   <d:prop>\n"
    "    <d:resourcetype><d:collection/></d:resourcetype>\n"
    "    <oc:size>1048576</oc:size>\n"
    "    <oc:favorite>0</oc:favorite>\n"
    "    <oc:tags><oc:tag>personal</oc:tag></oc:tags>\n"
    "    <nc:has-preview>false</nc:has-preview>\n"
    "    <nc:mount-type>shared</nc:mount-type>\n"
    "    <nc:file-metadata-size>{\"width\":800,\"height\":600}</nc:file-metadata-size>\n"
    "   </d:prop>\n"
    "   <d:status>HTTP/1.1 200 OK</d:status>\n"
    "  </d:propstat>\n"
    "  <d:propstat>\n"
    "   <d:prop><oc:owner-id/><oc:owner-display-name/></d:prop>\n"
    "   <d:status>HTTP/1.1 403 Forbidden</d:status>\n"
    "  </d:propstat>\n"
    " </d:response>\n"
    "</d:multistatus>";

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

QByteArray png(const QSize &size)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}
}

class NextcloudAssetsTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;

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

    bool authorized(const Request &request) const
    {
        return request.headers.value("authorization") == "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64();
    }

    QByteArray respond(const Request &request)
    {
        const QByteArray path = request.target.split('?').first();
        const QUrlQuery query(QString::fromUtf8(request.target.mid(path.size() + 1)));
        auto reply = [](const QByteArray &status, const QByteArray &type, const QByteArray &body) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        const QByteArray xml("application/xml; charset=utf-8");
        if (!authorized(request)) {
            return reply("401 Unauthorized", xml, QByteArray());
        }
        if (request.method == "PROPFIND" && path == "/remote.php/dav/files/alice/Photos/Lake%20view.jpg") {
            return reply("207 Multi-Status", xml, photoResponse);
        }
        if (request.method == "PROPFIND" && path == "/remote.php/dav/files/alice/Shared") {
            return reply("207 Multi-Status", xml, folderResponse);
        }
        if (request.method == "GET" && path == "/index.php/core/preview") {
            const QString id = query.queryItemValue(QStringLiteral("fileId"));
            if (id == QLatin1String("42")) {
                const int side = query.queryItemValue(QStringLiteral("x")).toInt();
                return reply("200 OK", "image/png", png(QSize(side, side * 3 / 4)));
            }
            if (id == QLatin1String("44")) {
                return reply("200 OK", "image/png", QByteArray(Nextcloud::PreviewByteLimit + 1024, 'x'));
            }
            if (id == QLatin1String("45")) {
                return reply("403 Forbidden", "text/html", "<html>Forbidden</html>");
            }
            return reply("404 Not Found", "text/html", "<html>Not found</html>");
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
        m_store.passwords.clear();
        m_store.passwords.insert(QStringLiteral("alice@example.com"), appPassword);
    }

    void metadataIsParsed()
    {
        QString error;
        const auto parsed = Nextcloud::parseMetadata(photoResponse, &error);
        QVERIFY2(parsed, qPrintable(error));
        QCOMPARE(parsed->size(), 1);
        const Nextcloud::FileMetadata &photo = parsed->first();
        QCOMPARE(photo.path, QStringLiteral("/remote.php/dav/files/alice/Photos/Lake view.jpg"));
        QVERIFY(!photo.folder);
        QCOMPARE(photo.fileId, QStringLiteral("42"));
        QCOMPARE(photo.ownerId, QStringLiteral("alice"));
        QCOMPARE(photo.ownerDisplayName, QStringLiteral("Alice Liddell"));
        QCOMPARE(photo.size, std::optional<qint64>(2483922));
        QCOMPARE(photo.modified, QDateTime(QDate(2026, 11, 3), QTime(10, 0), QTimeZone::UTC));
        QCOMPARE(photo.etag, QStringLiteral("\"6543a1b2c3d4e\""));
        QCOMPARE(photo.contentType, QStringLiteral("image/jpeg"));
        QCOMPARE(photo.permissions, QStringLiteral("RGDNVW"));
        QCOMPARE(photo.favorite, std::optional<bool>(true));
        QCOMPARE(photo.hasPreview, std::optional<bool>(true));
        QCOMPARE(photo.serverTags, QStringList({QStringLiteral("Holiday"), QStringLiteral("Lake")}));
        QCOMPARE(photo.shareTypes, QList<int>({0, 3}));
        QVERIFY(photo.mountType.isEmpty());
        QVERIFY(photo.unavailable.isEmpty());
        QCOMPARE(photo.metadata.size(), 9);
        QCOMPARE(photo.metadata.at(0), (Nextcloud::MetadataField{QStringLiteral("metadata-photos-size"), QStringLiteral("width"), QStringLiteral("4032")}));
        QCOMPARE(photo.metadata.at(2), (Nextcloud::MetadataField{QStringLiteral("metadata-photos-ifd0"), QStringLiteral("Make"), QStringLiteral("Canon")}));
        QCOMPARE(photo.metadata.last(),
                 (Nextcloud::MetadataField{QStringLiteral("metadata-photos-original_date_time"), QString(), QStringLiteral("1717236000")}));
        QCOMPARE(Nextcloud::describeMetadataField(photo.metadata.at(2)), QStringLiteral("Camera › Make"));
        QCOMPARE(Nextcloud::describeMetadataField(photo.metadata.last()), QStringLiteral("Taken"));
        QCOMPARE(Nextcloud::describeShareType(3), QStringLiteral("Public link"));
        QCOMPARE(Nextcloud::describeShareType(99), QStringLiteral("Share type 99"));

        const auto folder = Nextcloud::parseMetadata(folderResponse, &error);
        QVERIFY2(folder, qPrintable(error));
        const Nextcloud::FileMetadata &shared = folder->first();
        QCOMPARE(shared.path, QStringLiteral("/remote.php/dav/files/alice/Shared"));
        QVERIFY(shared.folder);
        QCOMPARE(shared.size, std::optional<qint64>(1048576));
        QCOMPARE(shared.favorite, std::optional<bool>(false));
        QCOMPARE(shared.hasPreview, std::optional<bool>(false));
        QCOMPARE(shared.mountType, QStringLiteral("shared"));
        QCOMPARE(shared.serverTags, QStringList(QStringLiteral("personal")));
        QVERIFY(shared.ownerId.isEmpty());
        QCOMPARE(shared.unavailable, QStringList({QStringLiteral("owner-id"), QStringLiteral("owner-display-name")}));
        QCOMPARE(shared.metadata.size(), 2);
        QCOMPARE(shared.metadata.at(1), (Nextcloud::MetadataField{QStringLiteral("file-metadata-size"), QStringLiteral("width"), QStringLiteral("800")}));
        QCOMPARE(Nextcloud::describeMetadataField(shared.metadata.at(1)), QStringLiteral("Dimensions › width"));

        QVERIFY(!Nextcloud::parseMetadata("<html>Login</html>", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(Nextcloud::metadataRequest().contains(QStringLiteral("<nc:has-preview/>")));
        QVERIFY(Nextcloud::metadataRequest().contains(QStringLiteral("<oc:fileid/>")));
    }

    void previewUrlsAskForAScaledImage()
    {
        QCOMPARE(Nextcloud::previewRequestSize(QSize(10, 10)), QSize(64, 64));
        QCOMPARE(Nextcloud::previewRequestSize(QSize(300, 200)), QSize(512, 512));
        QCOMPARE(Nextcloud::previewRequestSize(QSize(9000, 100)), QSize(2048, 2048));
        Account cloud = account();
        cloud.server = QUrl(QStringLiteral("https://example.com/nextcloud"));
        const QUrl url = Nextcloud::previewUrl(cloud, QStringLiteral("42"), QSize(512, 512));
        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.path(), QStringLiteral("/nextcloud/index.php/core/preview"));
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QStringLiteral("fileId")), QStringLiteral("42"));
        QCOMPARE(query.queryItemValue(QStringLiteral("x")), QStringLiteral("512"));
        QCOMPARE(query.queryItemValue(QStringLiteral("a")), QStringLiteral("1"));
        QCOMPARE(query.queryItemValue(QStringLiteral("forceIcon")), QStringLiteral("0"));
        QVERIFY(url.userInfo().isEmpty());
    }

    void metadataIsFetched()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        std::optional<Nextcloud::FileMetadata> metadata;
        QString error;
        QUrl fetched;
        connect(&client, &NextcloudClient::metadataFetched, this, [&](const QUrl &item, const Nextcloud::FileMetadata &result, const QString &message) {
            fetched = item;
            metadata = result;
            error = message;
        });
        const QUrl photo = fileUrl(QStringLiteral("Photos/Lake view.jpg"));
        client.fetchMetadata(account(), photo);
        QTRY_VERIFY_WITH_TIMEOUT(metadata.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(fetched, photo);
        QCOMPARE(metadata->fileId, QStringLiteral("42"));
        const Request &request = m_requests.last();
        QCOMPARE(request.method, QByteArray("PROPFIND"));
        QCOMPARE(request.headers.value("depth"), QByteArray("0"));
        QVERIFY(request.body.contains("nc:metadata-photos-exif"));
        QVERIFY(!request.target.contains(appPassword.toUtf8()));

        m_store.passwords.clear();
        metadata.reset();
        client.fetchMetadata(account(), photo);
        QTRY_VERIFY_WITH_TIMEOUT(metadata.has_value(), 20000);
        QVERIFY(error.contains(QStringLiteral("Sign in again")));
        QVERIFY(metadata->fileId.isEmpty());
    }

    void previewsAreFetchedWithinTheLimit()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        std::optional<QImage> image;
        QString error;
        connect(&client, &NextcloudClient::previewFetched, this, [&](const QUrl &, const QImage &result, const QString &message) {
            image = result;
            error = message;
        });
        const QUrl photo = fileUrl(QStringLiteral("Photos/Lake view.jpg"));
        client.fetchPreview(account(), QStringLiteral("42"), QSize(256, 256), photo);
        QTRY_VERIFY_WITH_TIMEOUT(image.has_value(), 20000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(image->size(), QSize(256, 192));
        const Request &request = m_requests.last();
        QCOMPARE(request.method, QByteArray("GET"));
        QVERIFY(request.target.startsWith("/index.php/core/preview?fileId=42&x=256&y=256&a=1"));
        QVERIFY(authorized(request));

        for (const auto &[id, expected] : {std::pair{QStringLiteral("43"), QStringLiteral("no preview")},
                                           std::pair{QStringLiteral("44"), QStringLiteral("too large")},
                                           std::pair{QStringLiteral("45"), QStringLiteral("refused")}}) {
            image.reset();
            client.fetchPreview(account(), id, QSize(256, 256), photo);
            QTRY_VERIFY_WITH_TIMEOUT(image.has_value(), 20000);
            QVERIFY(image->isNull());
            QVERIFY2(error.contains(expected), qPrintable(error));
        }

        image.reset();
        client.fetchPreview(account(), QString(), QSize(256, 256), photo);
        QVERIFY(image && image->isNull());
        QVERIFY(error.contains(QStringLiteral("ID")));
    }
};

QTEST_GUILESS_MAIN(NextcloudAssetsTest)

#include "nextcloudassetstest.moc"
