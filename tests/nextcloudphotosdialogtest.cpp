// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudphotosdialog.h"
#include "nextcloudaccount.h"
#include "nextcloudmenu.h"

#include <KConfigGroup>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTreeWidget>

#include <algorithm>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("photos-dialog-password-0123");

#define SKIP_WITHOUT_WEBDAV()                                                                                                                                  \
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {                                                                                           \
        QSKIP("The KIO WebDAV worker isn't installed.");                                                                                                       \
    }

QByteArray multistatus(const QByteArray &responses)
{
    return "<?xml version=\"1.0\"?>\n<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n" + responses
        + "</d:multistatus>";
}

QByteArray collection(const QByteArray &href, const QByteArray &properties = QByteArray())
{
    return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype>" + properties
        + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>\n";
}

QByteArray photo(const QByteArray &href, const QByteArray &fileId, const QByteArray &properties)
{
    return "<d:response><d:href>" + href
        + "</d:href><d:propstat><d:prop><d:resourcetype/><d:getcontenttype>image/jpeg</d:getcontenttype>"
          "<d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><oc:fileid>"
        + fileId + "</oc:fileid><nc:has-preview>false</nc:has-preview>" + properties + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>\n";
}

const QByteArray photosRoot = "/remote.php/dav/photos/alice/";

const QByteArray albumsResponse =
    multistatus(collection(photosRoot + "albums/") + collection(photosRoot + "albums/Summer%202026/", "<nc:nbItems>0</nc:nbItems>")
                + collection(photosRoot + "albums/Trip/", "<nc:nbItems>3</nc:nbItems><nc:collaborators/>"));

/// A shared album that only allows reading, and one that allows adding and removing.
const QByteArray sharedAlbumsResponse =
    multistatus(collection(photosRoot + "sharedalbums/")
                + collection(photosRoot + "sharedalbums/Family%20(bob)/", "<nc:nbItems>1</nc:nbItems><oc:permissions>G</oc:permissions>")
                + collection(photosRoot + "sharedalbums/Party%20(carol)/", "<nc:nbItems>0</nc:nbItems><oc:permissions>GCD</oc:permissions>"));

const QByteArray tripResponse = multistatus(collection(photosRoot + "albums/Trip/")
                                            + photo(photosRoot + "albums/Trip/42-Lake%20view.jpg",
                                                    "42",
                                                    "<nc:file-name>Lake view.jpg</nc:file-name><nc:realpath>/alice/files/Photos/Lake view.jpg</nc:realpath>"
                                                    "<nc:metadata-photos-original_date_time>1767268800</nc:metadata-photos-original_date_time>"
                                                    "<nc:metadata-photos-place>Konstanz</nc:metadata-photos-place>")
                                            + photo(photosRoot + "albums/Trip/43-Boat.jpg", "43", "<nc:realpath>/bob/files/Boat.jpg</nc:realpath>")
                                            + photo(photosRoot + "albums/Trip/44-Old.jpg",
                                                    "44",
                                                    "<nc:realpath>/alice/files/Old.jpg</nc:realpath>"
                                                    "<nc:metadata-photos-original_date_time>1748772000</nc:metadata-photos-original_date_time>"
                                                    "<nc:metadata-photos-place>Zürich</nc:metadata-photos-place>"));

const QByteArray locationsResponse = multistatus(collection(photosRoot + "locations/") + collection(photosRoot + "locations/Konstanz/"));

struct Request {
    QByteArray method;
    QByteArray path;
    QHash<QByteArray, QByteArray> headers;
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

template<typename T>
T *child(QObject &parent, const char *name)
{
    auto *object = parent.findChild<T *>(QLatin1String(name));
    Q_ASSERT(object);
    return object;
}

QStringList names(const QList<Nextcloud::Photo> &photos)
{
    QStringList names;
    for (const Nextcloud::Photo &photo : photos) {
        names.append(photo.name);
    }
    return names;
}
}

class NextcloudPhotosDialogTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;
    /// Whether the server has the Photos app.
    bool m_photosApp = true;

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
        return account;
    }

    QUrl fileUrl(const QString &path) const
    {
        QUrl url = NextcloudAccounts::filesUrl(account());
        url.setPath(url.path() + path);
        return url;
    }

    QList<Request> requests(const QByteArray &method) const
    {
        QList<Request> matching;
        std::copy_if(m_requests.cbegin(), m_requests.cend(), std::back_inserter(matching), [&](const Request &request) {
            return request.method == method;
        });
        return matching;
    }

    QByteArray respond(const Request &request)
    {
        auto reply = [](const QByteArray &status, const QByteArray &body = QByteArray()) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/xml; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        QByteArray path = request.path;
        if (path.endsWith('/')) {
            path.chop(1);
        }
        if (!m_photosApp && path.startsWith("/remote.php/dav/photos/")) {
            return reply("404 Not Found");
        }
        if (request.method == "PROPFIND") {
            if (path == photosRoot + "albums") {
                return reply("207 Multi-Status", albumsResponse);
            }
            if (path == photosRoot + "sharedalbums") {
                return reply("207 Multi-Status", sharedAlbumsResponse);
            }
            if (path == photosRoot + "locations") {
                return reply("207 Multi-Status", locationsResponse);
            }
            if (path == photosRoot + "albums/Trip") {
                return reply("207 Multi-Status", tripResponse);
            }
            if (path.startsWith(QByteArray(photosRoot + "albums/")) || path.startsWith(QByteArray(photosRoot + "sharedalbums/"))
                || path.startsWith(QByteArray(photosRoot + "locations/"))) {
                return reply("207 Multi-Status", multistatus(collection(request.path)));
            }
        }
        if (request.method == "DELETE" && path.startsWith(QByteArray(photosRoot + "albums/"))) {
            return reply("204 No Content");
        }
        if (request.method == "COPY") {
            return reply("201 Created");
        }
        if (request.method == "MKCOL" && path.startsWith(QByteArray(photosRoot + "albums/"))) {
            return reply("201 Created");
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
                    request.path = start.value(1).split('?').first();
                    int length = 0;
                    for (const QByteArray &line : lines.mid(1)) {
                        const int colon = line.indexOf(':');
                        const QByteArray name = line.left(colon).trimmed().toLower();
                        request.headers.insert(name, line.mid(colon + 1).trimmed());
                        if (name == "content-length") {
                            length = line.mid(colon + 1).trimmed().toInt();
                        }
                    }
                    if (data.size() < end + 4 + length) {
                        return;
                    }
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
        m_photosApp = true;
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("NextcloudPhotosDialog"));
    }

    void menusOfferPhotosOnlyWithAHandler()
    {
        QMenu menu;
        QList<QString> opened;
        NextcloudMenu::populate(
            &menu,
            {account()},
            QString(),
            QString(),
            true,
            nullptr,
            this,
            [](const Account &, bool) { },
            {},
            NextcloudMenu::AccountHandlers{[&opened](const Account &account) {
                opened.append(account.id);
            }});
        auto *photos = menu.findChild<QAction *>(QStringLiteral("nextcloudPhotos"));
        QVERIFY(photos);
        photos->trigger();
        QCOMPARE(opened, QStringList{QStringLiteral("test")});

        QMenu plain;
        NextcloudMenu::populate(&plain, {account()}, QString(), QString(), true, nullptr, this, [](const Account &, bool) { });
        QVERIFY(!plain.findChild<QAction *>(QStringLiteral("nextcloudPhotos")));

        // The item menu offers adding to an album only when it's possible.
        NextcloudMenu::ItemHandlers handlers;
        handlers.setFavorite = [](bool) { };
        handlers.editTags = [] { };
        handlers.organize = [] { };
        QMenu folderMenu;
        NextcloudMenu::addItemMenu(&folderMenu, false, this, handlers);
        QVERIFY(!folderMenu.findChild<QAction *>(QStringLiteral("nextcloudAddToAlbum")));
        bool added = false;
        handlers.addToAlbum = [&added] {
            added = true;
        };
        QMenu fileMenu;
        NextcloudMenu::addItemMenu(&fileMenu, false, this, handlers);
        auto *album = fileMenu.findChild<QAction *>(QStringLiteral("nextcloudAddToAlbum"));
        QVERIFY(album);
        album->trigger();
        QVERIFY(added);
    }

    void albumsAreListedSortedAndGrouped()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudPhotosDialog dialog(account(), &m_store);
        auto *collections = child<QListWidget>(dialog, "photosCollections");
        QTRY_COMPARE(collections->count(), 2);
        QCOMPARE(collections->item(1)->text(), QStringLiteral("Trip (3)"));
        collections->setCurrentRow(1);
        QTRY_COMPARE(dialog.shownPhotos().size(), 3);
        // Grouped by month, newest first: the server's capture dates, not the files' modification dates.
        auto *list = child<QTreeWidget>(dialog, "photosList");
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(names(dialog.shownPhotos()), (QStringList{QStringLiteral("Lake view.jpg"), QStringLiteral("Old.jpg"), QStringLiteral("Boat.jpg")}));
        QVERIFY(list->topLevelItem(2)->text(0).contains(QStringLiteral("Boat")) || list->topLevelItem(2)->child(0)->text(0).contains(QStringLiteral("Boat")));
        QVERIFY(child<QLabel>(dialog, "photosStatus")->text().contains(QStringLiteral("capture date")));

        auto *sort = child<QComboBox>(dialog, "photosSort");
        auto *group = child<QComboBox>(dialog, "photosGroup");
        group->setCurrentIndex(group->findData(int(Nextcloud::PhotoGrouping::None)));
        sort->setCurrentIndex(sort->findData(int(Nextcloud::PhotoSort::OldestFirst)));
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(names(dialog.shownPhotos()), (QStringList{QStringLiteral("Old.jpg"), QStringLiteral("Lake view.jpg"), QStringLiteral("Boat.jpg")}));
        group->setCurrentIndex(group->findData(int(Nextcloud::PhotoGrouping::Place)));
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(list->topLevelItem(0)->childCount(), 1);
        // The arrangement is remembered.
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudPhotosDialog")).readEntry("Grouping", -1),
                 int(Nextcloud::PhotoGrouping::Place));
        // Only GET-free requests: nothing was downloaded.
        QVERIFY(requests("GET").isEmpty());

        // A photo in someone else's files can't be shown in a pane.
        auto *show = child<QPushButton>(dialog, "photosShow");
        QSignalSpy shown(&dialog, &NextcloudPhotosDialog::showRequested);
        group->setCurrentIndex(group->findData(int(Nextcloud::PhotoGrouping::None)));
        list->topLevelItem(2)->setSelected(true);
        QVERIFY(!show->isEnabled());
        list->clearSelection();
        list->topLevelItem(1)->setSelected(true);
        QVERIFY(show->isEnabled());
        show->click();
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.first().first().toUrl(), fileUrl(QStringLiteral("Photos/Lake view.jpg")));
    }

    void removingAfterConfirmationDeletesOnlyTheAlbumEntry()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudPhotosDialog dialog(account(), &m_store);
        QStringList questions;
        bool accept = false;
        dialog.setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            questions.append(text);
            return accept;
        });
        auto *collections = child<QListWidget>(dialog, "photosCollections");
        QTRY_COMPARE(collections->count(), 2);
        collections->setCurrentRow(1);
        QTRY_COMPARE(dialog.shownPhotos().size(), 3);
        auto *group = child<QComboBox>(dialog, "photosGroup");
        group->setCurrentIndex(group->findData(int(Nextcloud::PhotoGrouping::None)));
        auto *list = child<QTreeWidget>(dialog, "photosList");
        auto *remove = child<QPushButton>(dialog, "photosRemove");
        list->topLevelItem(0)->setSelected(true);
        QVERIFY(remove->isEnabled());

        // Cancelled: no request at all.
        remove->click();
        QCOMPARE(questions.size(), 1);
        QVERIFY(questions.first().contains(QStringLiteral("not deleted")));
        QTest::qWait(100);
        QVERIFY(requests("DELETE").isEmpty());

        accept = true;
        remove->click();
        QTRY_COMPARE(requests("DELETE").size(), 1);
        QCOMPARE(requests("DELETE").first().path, photosRoot + "albums/Trip/42-Lake%20view.jpg");
        QTRY_VERIFY(child<QLabel>(dialog, "photosStatus")->text().contains(QStringLiteral("not deleted")));
        // Never anything in the user's files.
        for (const Request &request : std::as_const(m_requests)) {
            QVERIFY2(!request.path.startsWith("/remote.php/dav/files/") || request.method == "PROPFIND", request.path.constData());
        }
    }

    void sharedAlbumsRespectPermissions()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudPhotosDialog dialog(account(), &m_store);
        dialog.setKind(Nextcloud::PhotoCollectionKind::SharedAlbums);
        auto *collections = child<QListWidget>(dialog, "photosCollections");
        QTRY_COMPARE(collections->count(), 2);
        QCOMPARE(collections->currentRow(), 0);
        QVERIFY(collections->item(0)->toolTip().contains(QStringLiteral("doesn't allow")));
        auto *remove = child<QPushButton>(dialog, "photosRemove");
        QVERIFY(!remove->isEnabled());
        QVERIFY(remove->toolTip().contains(QStringLiteral("doesn't allow")));
    }

    void placesFallBackToLocationsAndAreReadOnly()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudPhotosDialog dialog(account(), &m_store);
        dialog.setKind(Nextcloud::PhotoCollectionKind::Places);
        auto *collections = child<QListWidget>(dialog, "photosCollections");
        QTRY_COMPARE(collections->count(), 1);
        QCOMPARE(collections->item(0)->text(), QStringLiteral("Konstanz"));
        QVERIFY(!child<QPushButton>(dialog, "photosRemove")->isEnabled());
    }

    void missingPhotosAppIsReported()
    {
        SKIP_WITHOUT_WEBDAV();
        m_photosApp = false;
        NextcloudPhotosDialog dialog(account(), &m_store);
        auto *status = child<QLabel>(dialog, "photosStatus");
        QTRY_VERIFY(!status->text().contains(QStringLiteral("Loading")));
        QVERIFY(status->text().contains(QStringLiteral("Photos")));
        QVERIFY(!child<QPushButton>(dialog, "photosNewAlbum")->isEnabled());
        QCOMPARE(child<QListWidget>(dialog, "photosCollections")->count(), 0);
    }

    void albumsAreCreatedAndSelected()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudPhotosDialog dialog(account(), &m_store);
        dialog.setNamePrompt([](QWidget *) {
            return std::optional(QStringLiteral("Trip"));
        });
        auto *collections = child<QListWidget>(dialog, "photosCollections");
        QTRY_COMPARE(collections->count(), 2);
        child<QPushButton>(dialog, "photosNewAlbum")->click();
        QTRY_COMPARE(requests("MKCOL").size(), 1);
        QCOMPARE(requests("MKCOL").first().path, photosRoot + "albums/Trip");
        QTRY_COMPARE(collections->currentRow(), 1);
    }

    void addingAfterConfirmationCopiesIntoTheAlbum()
    {
        SKIP_WITHOUT_WEBDAV();
        const QUrl file = fileUrl(QStringLiteral("Photos/Lake view.jpg"));
        NextcloudAddToAlbumDialog dialog(account(), {file}, &m_store);
        bool accept = false;
        QStringList questions;
        dialog.setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            questions.append(text);
            return accept;
        });
        QSignalSpy finished(&dialog, &NextcloudAddToAlbumDialog::finished);
        auto *albums = child<QListWidget>(dialog, "addToAlbumList");
        // The read-only shared album is left out; own albums come first.
        QTRY_COMPARE(albums->count(), 3);
        QVERIFY(albums->item(2)->text().startsWith(QStringLiteral("Party (carol)")));
        albums->setCurrentRow(1);

        dialog.addToChosen();
        QCOMPARE(questions.size(), 1);
        QTest::qWait(100);
        QVERIFY(requests("COPY").isEmpty());
        QCOMPARE(finished.size(), 0);

        accept = true;
        child<QPushButton>(dialog, "addToAlbumAdd")->click();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toString(), QStringLiteral("Trip"));
        QCOMPARE(finished.first().at(1).toInt(), 1);
        QVERIFY(finished.first().at(2).toStringList().isEmpty());
        const QList<Request> copies = requests("COPY");
        QCOMPARE(copies.size(), 1);
        QCOMPARE(copies.first().path, QByteArray("/remote.php/dav/files/alice/Photos/Lake%20view.jpg"));
        QVERIFY(copies.first().headers.value("destination").endsWith(QByteArray(photosRoot + "albums/Trip/Lake%20view.jpg")));
        QCOMPARE(copies.first().headers.value("overwrite"), QByteArray("F"));
        QVERIFY(requests("DELETE").isEmpty());
    }

    void sharedAlbumsWarnBeforeAdding()
    {
        Nextcloud::PhotoCollection album;
        album.kind = Nextcloud::PhotoCollectionKind::SharedAlbums;
        album.name = QStringLiteral("Party (carol)");
        QVERIFY(NextcloudPhotos::addQuestion(album, 2).contains(QStringLiteral("shared with")));
        album.kind = Nextcloud::PhotoCollectionKind::Albums;
        QVERIFY(!NextcloudPhotos::addQuestion(album, 2).contains(QStringLiteral("shared with")));
        album.collaborators = {QStringLiteral("Bob")};
        QVERIFY(NextcloudPhotos::addQuestion(album, 2).contains(QStringLiteral("Bob")));
        QVERIFY(NextcloudPhotos::removeQuestion(album, 1).contains(QStringLiteral("not deleted")));
    }
};

QTEST_MAIN(NextcloudPhotosDialogTest)

#include "nextcloudphotosdialogtest.moc"
