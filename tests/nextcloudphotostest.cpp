// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudphotos.h"
#include "nextcloudclient.h"

#include <QHash>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("photos-app-password-0123");

QByteArray multistatus(const QByteArray &responses)
{
    return "<?xml version=\"1.0\"?>\n<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n" + responses
        + "</d:multistatus>";
}

QByteArray collection(const QByteArray &href, const QByteArray &properties = QByteArray())
{
    return "<d:response><d:href>" + href
        + "</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype>" + properties
        + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>"
          "<d:propstat><d:prop><oc:fileid/><nc:realpath/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response>\n";
}

QByteArray photo(const QByteArray &href, const QByteArray &fileId, const QByteArray &properties)
{
    return "<d:response><d:href>" + href
        + "</d:href><d:propstat><d:prop><d:resourcetype/><d:getcontenttype>image/jpeg</d:getcontenttype>"
          "<d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><oc:fileid>"
        + fileId + "</oc:fileid><nc:has-preview>true</nc:has-preview>" + properties + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>\n";
}

/// The user's albums, in the format of Nextcloud Photos 2.x.
const QByteArray albumsResponse =
    multistatus(collection("/remote.php/dav/photos/alice/albums/")
                + collection("/remote.php/dav/photos/alice/albums/Trip/",
                             "<nc:nbItems>3</nc:nbItems><nc:location>Lake Constance</nc:location>"
                             "<nc:dateRange>{\"start\":1748772000,\"end\":1767268800}</nc:dateRange><nc:collaborators/>")
                + collection("/remote.php/dav/photos/alice/albums/Summer%202026/", "<nc:nbItems>0</nc:nbItems><nc:dateRange>[]</nc:dateRange>"));

const QByteArray sharedAlbumsResponse = multistatus(collection("/remote.php/dav/photos/alice/sharedalbums/")
                                                    + collection("/remote.php/dav/photos/alice/sharedalbums/Family%20(bob)/",
                                                                 "<nc:nbItems>1</nc:nbItems><oc:permissions>G</oc:permissions><nc:collaborators>"
                                                                 "<nc:collaborator><id>alice</id><label>Alice</label><type>0</type></nc:collaborator>"
                                                                 "<nc:collaborator><id>carol</id><type>0</type></nc:collaborator></nc:collaborators>"));

/// Photos in the album Trip: one with full metadata, one from another user without a capture date and one dated only by EXIF.
const QByteArray tripResponse =
    multistatus(collection("/remote.php/dav/photos/alice/albums/Trip/")
                + photo("/remote.php/dav/photos/alice/albums/Trip/42-Lake%20view.jpg",
                        "42",
                        "<nc:file-name>Lake view.jpg</nc:file-name><nc:realpath>/alice/files/Photos/Lake view.jpg</nc:realpath>"
                        "<nc:metadata-photos-original_date_time>1767268800</nc:metadata-photos-original_date_time>"
                        "<nc:metadata-photos-place>Konstanz</nc:metadata-photos-place>"
                        "<nc:metadata-photos-gps><latitude>47.66</latitude><longitude>9.17</longitude></nc:metadata-photos-gps>")
                + photo("/remote.php/dav/photos/alice/albums/Trip/43-Boat.jpg", "43", "<nc:realpath>/bob/files/Boat.jpg</nc:realpath>")
                + photo("/remote.php/dav/photos/alice/albums/Trip/44-Old.jpg",
                        "44",
                        "<nc:realpath>/alice/files/Old.jpg</nc:realpath><nc:metadata-photos-exif><DateTimeOriginal>2025:06:01 10:00:00</DateTimeOriginal>"
                        "</nc:metadata-photos-exif><nc:metadata-photos-place>Zürich</nc:metadata-photos-place>"));

const QByteArray locationsResponse = multistatus(collection("/remote.php/dav/photos/alice/locations/")
                                                 + collection("/remote.php/dav/photos/alice/locations/Konstanz/", "<nc:nbItems>1</nc:nbItems>"));

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

class NextcloudPhotosTest : public QObject
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

    Account account(const QString &user = QStringLiteral("alice")) const
    {
        Account account;
        account.id = QStringLiteral("test");
        account.server = base();
        account.user = user;
        account.loginName = user + QStringLiteral("@example.com");
        return account;
    }

    QUrl fileUrl(const QString &path) const
    {
        QUrl url = NextcloudAccounts::filesUrl(account());
        url.setPath(url.path() + path);
        return url;
    }

    Nextcloud::Photo entry(const QString &path) const
    {
        Nextcloud::Photo photo;
        photo.file.path = path;
        return photo;
    }

    QByteArray respond(const Request &request)
    {
        const QByteArray path = request.target.split('?').first();
        auto reply = [](const QByteArray &status, const QByteArray &body = QByteArray()) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/xml; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        const QByteArray login = request.headers.value("authorization");
        if (login != "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64()
            && login != "Basic " + QByteArray("carol@example.com:" + appPassword.toUtf8()).toBase64()) {
            return reply("401 Unauthorized");
        }
        if (request.method == "PROPFIND") {
            if (path == "/remote.php/dav/photos/alice/albums/") {
                return reply("207 Multi-Status", albumsResponse);
            }
            if (path == "/remote.php/dav/photos/alice/sharedalbums/") {
                return reply("207 Multi-Status", sharedAlbumsResponse);
            }
            if (path == "/remote.php/dav/photos/alice/locations/") {
                return reply("207 Multi-Status", locationsResponse);
            }
            if (path == "/remote.php/dav/photos/alice/albums/Trip/") {
                return reply("207 Multi-Status", tripResponse);
            }
            if (path == "/remote.php/dav/photos/alice/sharedalbums/Family%20(bob)/") {
                return reply("403 Forbidden");
            }
        }
        if (request.method == "COPY" && path == "/remote.php/dav/files/alice/Photos/Lake%20view.jpg") {
            const QByteArray destination = request.headers.value("destination");
            if (destination.contains("/sharedalbums/")) {
                return reply("403 Forbidden");
            }
            return reply(destination.endsWith("/Present/Lake%20view.jpg") ? "412 Precondition Failed" : "201 Created");
        }
        if (request.method == "DELETE" && path.startsWith("/remote.php/dav/photos/alice/albums/Trip/")) {
            return reply("204 No Content");
        }
        if (request.method == "DELETE" && path.startsWith("/remote.php/dav/photos/alice/sharedalbums/")) {
            return reply("403 Forbidden");
        }
        if (request.method == "MKCOL" && path == "/remote.php/dav/photos/alice/albums/New%20album") {
            return reply("201 Created");
        }
        if (request.method == "MKCOL" && path == "/remote.php/dav/photos/alice/albums/Trip") {
            return reply("405 Method Not Allowed");
        }
        return reply("404 Not Found");
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
    }

    void collectionsAreParsed()
    {
        QString error;
        const auto albums = Nextcloud::parsePhotoCollections(albumsResponse, Nextcloud::PhotoCollectionKind::Albums, &error);
        QVERIFY2(albums, qPrintable(error));
        QCOMPARE(albums->size(), 2);
        // Sorted by name, with the folder itself left out.
        QCOMPARE(albums->at(0).name, QStringLiteral("Summer 2026"));
        QCOMPARE(albums->at(0).path, QStringLiteral("/remote.php/dav/photos/alice/albums/Summer 2026"));
        QCOMPARE(albums->at(0).itemCount, 0);
        QVERIFY(!albums->at(0).start.isValid());
        const Nextcloud::PhotoCollection &trip = albums->at(1);
        QCOMPARE(trip.name, QStringLiteral("Trip"));
        QCOMPARE(trip.itemCount, 3);
        QCOMPARE(trip.location, QStringLiteral("Lake Constance"));
        QCOMPARE(trip.start, QDateTime::fromSecsSinceEpoch(1748772000, QTimeZone::UTC));
        QCOMPARE(trip.end, QDateTime::fromSecsSinceEpoch(1767268800, QTimeZone::UTC));
        QVERIFY(trip.collaborators.isEmpty());
        QVERIFY(Nextcloud::canAddPhotos(trip));
        QVERIFY(Nextcloud::canRemovePhotos(trip));

        const auto shared = Nextcloud::parsePhotoCollections(sharedAlbumsResponse, Nextcloud::PhotoCollectionKind::SharedAlbums, &error);
        QVERIFY2(shared, qPrintable(error));
        QCOMPARE(shared->size(), 1);
        QCOMPARE(shared->first().name, QStringLiteral("Family (bob)"));
        QCOMPARE(shared->first().collaborators, (QStringList{QStringLiteral("Alice"), QStringLiteral("carol")}));
        // A read-only shared album allows neither adding nor removing.
        QVERIFY(!Nextcloud::canAddPhotos(shared->first()));
        QVERIFY(!Nextcloud::canRemovePhotos(shared->first()));
        Nextcloud::PhotoCollection place;
        place.kind = Nextcloud::PhotoCollectionKind::Places;
        QVERIFY(!Nextcloud::canAddPhotos(place));
        QVERIFY(!Nextcloud::canRemovePhotos(place));

        QVERIFY(!Nextcloud::parsePhotoCollections("<html/>", Nextcloud::PhotoCollectionKind::Albums, &error));
        QVERIFY(!error.isEmpty());
    }

    void photosAreParsedWithCaptureDateAndPlace()
    {
        QString error;
        const auto photos = Nextcloud::parsePhotos(tripResponse, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"), &error);
        QVERIFY2(photos, qPrintable(error));
        QCOMPARE(photos->size(), 3);
        const Nextcloud::Photo &lake = photos->at(0);
        QCOMPARE(lake.name, QStringLiteral("Lake view.jpg"));
        QCOMPARE(lake.file.path, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/42-Lake view.jpg"));
        QCOMPARE(lake.file.fileId, QStringLiteral("42"));
        QCOMPARE(lake.taken, QDateTime::fromSecsSinceEpoch(1767268800, QTimeZone::UTC));
        QCOMPARE(lake.place, QStringLiteral("Konstanz"));
        QVERIFY(lake.gps);
        QCOMPARE(lake.gps->x(), 47.66);
        QCOMPARE(lake.gps->y(), 9.17);
        QCOMPARE(Nextcloud::photoFileUrl(account(), lake), fileUrl(QStringLiteral("Photos/Lake view.jpg")));

        const Nextcloud::Photo &boat = photos->at(1);
        // Without nc:file-name the entry's name loses its file ID.
        QCOMPARE(boat.name, QStringLiteral("Boat.jpg"));
        QVERIFY(!boat.taken.isValid());
        QVERIFY(boat.file.modified.isValid());
        // Another user's file has no URL in the user's files.
        QVERIFY(Nextcloud::photoFileUrl(account(), boat).isEmpty());

        const Nextcloud::Photo &old = photos->at(2);
        QCOMPARE(old.taken, QDateTime(QDate(2025, 6, 1), QTime(10, 0), QTimeZone::UTC));
        QCOMPARE(old.place, QStringLiteral("Zürich"));
        QCOMPARE(Nextcloud::photosWithoutCaptureDate(*photos), 1);

        Nextcloud::Photo escape = lake;
        escape.realPath = QStringLiteral("/alice/files/../bob/files/x.jpg");
        QVERIFY(Nextcloud::photoFileUrl(account(), escape).isEmpty());
    }

    void photosAreSortedAndGrouped()
    {
        const QList<Nextcloud::Photo> photos = *Nextcloud::parsePhotos(tripResponse, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"));
        auto names = [](const QList<Nextcloud::Photo> &list) {
            QStringList result;
            for (const Nextcloud::Photo &photo : list) {
                result.append(photo.name);
            }
            return result;
        };
        QList<Nextcloud::PhotoGroup> groups = Nextcloud::arrangePhotos(photos, Nextcloud::PhotoSort::NewestFirst, Nextcloud::PhotoGrouping::None);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(names(groups.first().photos), (QStringList{QStringLiteral("Lake view.jpg"), QStringLiteral("Old.jpg"), QStringLiteral("Boat.jpg")}));
        groups = Nextcloud::arrangePhotos(photos, Nextcloud::PhotoSort::OldestFirst, Nextcloud::PhotoGrouping::None);
        QCOMPARE(names(groups.first().photos), (QStringList{QStringLiteral("Old.jpg"), QStringLiteral("Lake view.jpg"), QStringLiteral("Boat.jpg")}));

        groups = Nextcloud::arrangePhotos(photos, Nextcloud::PhotoSort::NewestFirst, Nextcloud::PhotoGrouping::Month);
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups.at(0).key, QStringLiteral("2026-01"));
        QCOMPARE(groups.at(1).key, QStringLiteral("2025-06"));
        // Photos without a capture date are grouped last, not by their modification date.
        QVERIFY(groups.at(2).key.isEmpty());
        QCOMPARE(names(groups.at(2).photos), QStringList{QStringLiteral("Boat.jpg")});

        groups = Nextcloud::arrangePhotos(photos, Nextcloud::PhotoSort::Name, Nextcloud::PhotoGrouping::Place);
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups.at(0).title, QStringLiteral("Konstanz"));
        QCOMPARE(groups.at(1).title, QStringLiteral("Zürich"));
        QVERIFY(groups.at(2).key.isEmpty());
        QVERIFY(Nextcloud::arrangePhotos({}, Nextcloud::PhotoSort::Name, Nextcloud::PhotoGrouping::None).isEmpty());
    }

    void onlyAlbumEntriesCanBeRemoved()
    {
        const Account alice = account();
        QString error;
        const QUrl entry = Nextcloud::albumEntryUrl(alice, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/42-Lake view.jpg"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entry.path(), QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/42-Lake view.jpg"));
        QVERIFY(Nextcloud::albumEntryUrl(alice, QStringLiteral("/remote.php/dav/photos/alice/sharedalbums/Family (bob)/7-a.jpg")).isValid());
        const QStringList forbidden{
            QStringLiteral("/remote.php/dav/files/alice/Photos/Lake view.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/"),
            QStringLiteral("/remote.php/dav/photos/alice/albums"),
            QStringLiteral("/remote.php/dav/photos/alice/places/Konstanz/42-Lake view.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/../../../files/alice/x.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/sub/42-x.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums//42-x.jpg"),
            QStringLiteral("/remote.php/dav/photos/bob/albums/Trip/42-x.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/42-x.jpg/"),
            QString(),
        };
        for (const QString &path : forbidden) {
            QVERIFY2(Nextcloud::albumEntryUrl(alice, path, &error).isEmpty(), qPrintable(path));
            QVERIFY(!error.isEmpty());
        }
        QVERIFY(Nextcloud::albumUrl(alice, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip")).isValid());
        QVERIFY(Nextcloud::albumUrl(alice, QStringLiteral("/remote.php/dav/photos/alice/places/Konstanz")).isEmpty());
        QVERIFY(Nextcloud::albumUrl(alice, QStringLiteral("/remote.php/dav/files/alice/Photos")).isEmpty());

        QVERIFY(!Nextcloud::albumNameError(QStringLiteral(" ")).isEmpty());
        QVERIFY(!Nextcloud::albumNameError(QStringLiteral("a/b")).isEmpty());
        QVERIFY(!Nextcloud::albumNameError(QStringLiteral("..")).isEmpty());
        QVERIFY(Nextcloud::albumNameError(QStringLiteral("Trip 2026")).isEmpty());
    }

    void collectionsAreListed()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy spy(&client, &NextcloudClient::photoCollectionsListed);
        const int albums = client.listPhotoCollections(account(), Nextcloud::PhotoCollectionKind::Albums);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QCOMPARE(spy.first().at(0).toInt(), albums);
        auto listing = spy.first().at(1).value<Nextcloud::PhotoCollectionListing>();
        QVERIFY2(listing.error.isEmpty(), qPrintable(listing.error));
        QCOMPARE(listing.collections.size(), 2);
        QCOMPARE(m_requests.first().method, QByteArray("PROPFIND"));
        QCOMPARE(m_requests.first().headers.value("depth"), QByteArray("1"));
        QVERIFY(m_requests.first().body.contains("metadata-photos-original_date_time"));
        QVERIFY(m_requests.first().body.contains("metadata-photos-place"));

        // Without places/, the older locations/ collection is read.
        m_requests.clear();
        spy.clear();
        client.listPhotoCollections(account(), Nextcloud::PhotoCollectionKind::Places);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        listing = spy.first().at(1).value<Nextcloud::PhotoCollectionListing>();
        QVERIFY2(listing.error.isEmpty(), qPrintable(listing.error));
        QCOMPARE(listing.kind, Nextcloud::PhotoCollectionKind::Places);
        QCOMPARE(listing.collections.size(), 1);
        QCOMPARE(listing.collections.first().name, QStringLiteral("Konstanz"));
        QCOMPARE(m_requests.size(), 2);
        QCOMPARE(m_requests.at(0).target, QByteArray("/remote.php/dav/photos/alice/places/"));
        QCOMPARE(m_requests.at(1).target, QByteArray("/remote.php/dav/photos/alice/locations/"));

        QSignalSpy photos(&client, &NextcloudClient::photosListed);
        const auto trip = Nextcloud::parsePhotoCollections(albumsResponse, Nextcloud::PhotoCollectionKind::Albums)->at(1);
        client.listPhotos(account(), trip);
        QTRY_COMPARE_WITH_TIMEOUT(photos.size(), 1, 20000);
        const auto photoListing = photos.first().at(1).value<Nextcloud::PhotoListing>();
        QVERIFY2(photoListing.error.isEmpty(), qPrintable(photoListing.error));
        QCOMPARE(photoListing.photos.size(), 3);

        // A shared album its owner no longer shares reports why.
        photos.clear();
        const auto family = Nextcloud::parsePhotoCollections(sharedAlbumsResponse, Nextcloud::PhotoCollectionKind::SharedAlbums)->first();
        client.listPhotos(account(), family);
        QTRY_COMPARE_WITH_TIMEOUT(photos.size(), 1, 20000);
        QVERIFY(photos.first().at(1).value<Nextcloud::PhotoListing>().error.contains(QStringLiteral("owner")));
    }

    void missingPhotosAppIsReported()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy spy(&client, &NextcloudClient::photoCollectionsListed);
        client.listPhotoCollections(account(QStringLiteral("carol")), Nextcloud::PhotoCollectionKind::Albums);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        const auto listing = spy.first().at(1).value<Nextcloud::PhotoCollectionListing>();
        QVERIFY(listing.unavailable);
        QVERIFY(listing.error.contains(QStringLiteral("Photos")));
        QVERIFY(listing.collections.isEmpty());
    }

    void addingCopiesIntoTheAlbum()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy spy(&client, &NextcloudClient::photoAdded);
        const QUrl file = fileUrl(QStringLiteral("Photos/Lake view.jpg"));
        client.addToAlbum(account(), file, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY2(spy.first().at(2).toString().isEmpty(), qPrintable(spy.first().at(2).toString()));
        QCOMPARE(m_requests.size(), 1);
        const Request &copy = m_requests.first();
        QCOMPARE(copy.method, QByteArray("COPY"));
        QCOMPARE(copy.target, QByteArray("/remote.php/dav/files/alice/Photos/Lake%20view.jpg"));
        QCOMPARE(copy.headers.value("destination"), base().toEncoded() + "/remote.php/dav/photos/alice/albums/Trip/Lake%20view.jpg");
        QCOMPARE(copy.headers.value("overwrite"), QByteArray("F"));

        // A photo that is already in the album isn't an error.
        spy.clear();
        client.addToAlbum(account(), file, QStringLiteral("/remote.php/dav/photos/alice/albums/Present"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY(spy.first().at(2).toString().isEmpty());

        spy.clear();
        client.addToAlbum(account(), file, QStringLiteral("/remote.php/dav/photos/alice/sharedalbums/Family (bob)"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY(spy.first().at(2).toString().contains(QStringLiteral("owner")));
        QCOMPARE(m_requests.last().headers.value("destination"),
                 base().toEncoded() + "/remote.php/dav/photos/alice/sharedalbums/Family%20(bob)/Lake%20view.jpg");

        // Places, files and other users' files are refused without a request.
        m_requests.clear();
        spy.clear();
        client.addToAlbum(account(), file, QStringLiteral("/remote.php/dav/photos/alice/places/Konstanz"));
        client.addToAlbum(account(), file, QStringLiteral("/remote.php/dav/files/alice/Photos"));
        QUrl other = file;
        other.setPath(QStringLiteral("/remote.php/dav/files/bob/x.jpg"));
        client.addToAlbum(account(), other, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"));
        QCOMPARE(spy.size(), 3);
        for (const QList<QVariant> &signal : std::as_const(spy)) {
            QVERIFY(!signal.at(2).toString().isEmpty());
        }
        QTest::qWait(100);
        QVERIFY(m_requests.isEmpty());
    }

    void removingDeletesOnlyTheAlbumEntry()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy spy(&client, &NextcloudClient::photoRemoved);
        const auto photos = *Nextcloud::parsePhotos(tripResponse, QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"));
        client.removeFromAlbum(account(), photos.first());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY2(spy.first().at(1).toString().isEmpty(), qPrintable(spy.first().at(1).toString()));
        QCOMPARE(m_requests.size(), 1);
        QCOMPARE(m_requests.first().method, QByteArray("DELETE"));
        // The album's entry, never the file in /files/ that it references.
        QCOMPARE(m_requests.first().target, QByteArray("/remote.php/dav/photos/alice/albums/Trip/42-Lake%20view.jpg"));

        spy.clear();
        client.removeFromAlbum(account(), entry(QStringLiteral("/remote.php/dav/photos/alice/sharedalbums/Family (bob)/7-a.jpg")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY(spy.first().at(1).toString().contains(QStringLiteral("owner")));

        m_requests.clear();
        spy.clear();
        const QStringList forbidden{
            QStringLiteral("/remote.php/dav/files/alice/Photos/Lake view.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip"),
            QStringLiteral("/remote.php/dav/photos/alice/places/Konstanz/42-Lake view.jpg"),
            QStringLiteral("/remote.php/dav/photos/alice/albums/Trip/../../../files/alice/Photos/Lake view.jpg"),
        };
        for (const QString &path : forbidden) {
            client.removeFromAlbum(account(), entry(path));
        }
        QCOMPARE(spy.size(), forbidden.size());
        for (const QList<QVariant> &signal : std::as_const(spy)) {
            QVERIFY(!signal.at(1).toString().isEmpty());
        }
        QTest::qWait(100);
        QVERIFY(m_requests.isEmpty());
    }

    void albumsAreCreated()
    {
        NextcloudClient client;
        client.setCredentialStore(&m_store);
        QSignalSpy spy(&client, &NextcloudClient::albumCreated);
        client.createAlbum(account(), QStringLiteral(" New album "));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY2(spy.first().at(2).toString().isEmpty(), qPrintable(spy.first().at(2).toString()));
        QCOMPARE(spy.first().at(1).toString(), QStringLiteral("New album"));
        QCOMPARE(m_requests.last().method, QByteArray("MKCOL"));
        QCOMPARE(m_requests.last().target, QByteArray("/remote.php/dav/photos/alice/albums/New%20album"));

        spy.clear();
        client.createAlbum(account(), QStringLiteral("Trip"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 20000);
        QVERIFY(spy.first().at(2).toString().contains(QStringLiteral("exists")));

        m_requests.clear();
        spy.clear();
        client.createAlbum(account(), QStringLiteral("../files"));
        QCOMPARE(spy.size(), 1);
        QVERIFY(!spy.first().at(2).toString().isEmpty());
        QVERIFY(m_requests.isEmpty());
    }
};

QTEST_GUILESS_MAIN(NextcloudPhotosTest)
#include "nextcloudphotostest.moc"
