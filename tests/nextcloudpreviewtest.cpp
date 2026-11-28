// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "informationpanel.h"
#include "mainwindow.h"
#include "nextcloudaccount.h"
#include "pane.h"
#include "previewpanel.h"
#include "settings.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KFileItem>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QBuffer>
#include <QDockWidget>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QToolButton>
#include <QUrlQuery>

#include <algorithm>

namespace
{
QByteArray png(int width, int height, Qt::GlobalColor color)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

QByteArray reply(const QByteArray &status, const QByteArray &body, const QByteArray &type)
{
    return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n"
        + body;
}

const QByteArray davHeader = "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">";

QByteArray davFile(const QByteArray &href, const QByteArray &type, int size, const QByteArray &extra = {})
{
    return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop><d:resourcetype/><d:getcontentlength>" + QByteArray::number(size)
        + "</d:getcontentlength><d:getcontenttype>" + type
        + "</d:getcontenttype><d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><oc:permissions>GDNVW</oc:permissions>" + extra
        + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
}
}

class NextcloudPreviewTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    /// The requests received, as "METHOD path?query".
    QList<QByteArray> m_requests;
    /// Whether the server renders previews; otherwise it answers 404.
    bool m_previews = true;

    QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()));
    }

    QUrl url(const QString &path) const
    {
        return base().resolved(QUrl(path));
    }

    qsizetype requestCount(const QByteArray &prefix) const
    {
        return std::count_if(m_requests.cbegin(), m_requests.cend(), [&prefix](const QByteArray &request) {
            return request.startsWith(prefix);
        });
    }

    QByteArray respond(const QByteArray &method, const QByteArray &target)
    {
        m_requests.append(method + ' ' + target);
        const QByteArray path = target.split('?').first();
        if (method == "GET" && path == "/image.png") {
            return reply("200 OK", png(20, 10, Qt::red), "image/png");
        }
        if (method == "GET" && path == "/index.php/core/preview" && m_previews) {
            return reply("200 OK", png(30, 20, Qt::green), "image/png");
        }
        const QByteArray folder = "/remote.php/dav/files/alice/";
        if (method == "PROPFIND" && (path == "/remote.php/dav/files/alice" || path == folder)) {
            return reply("207 Multi-Status",
                         davHeader + "<d:response><d:href>" + folder
                             + "</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype><oc:permissions>SGDNVCK</oc:permissions>"
                               "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
                             + davFile(folder + "photo.png", "image/png", 300) + "</d:multistatus>",
                         "application/xml");
        }
        if (method == "PROPFIND" && path == folder + "photo.png") {
            return reply("207 Multi-Status",
                         davHeader
                             + davFile(folder + "photo.png",
                                       "image/png",
                                       300,
                                       "<oc:fileid>42</oc:fileid><oc:favorite>1</oc:favorite><nc:has-preview>true</nc:has-preview>"
                                       "<oc:owner-display-name>Alice</oc:owner-display-name><oc:owner-id>alice</oc:owner-id>"
                                       "<nc:system-tags><nc:system-tag>Holiday</nc:system-tag></nc:system-tags>"
                                       "<nc:metadata-photos-ifd0><Make>Canon</Make></nc:metadata-photos-ifd0>")
                             + "</d:multistatus>",
                         "application/xml");
        }
        if (method == "GET" && path == folder + "photo.png") {
            return reply("200 OK", png(40, 40, Qt::blue), "image/png");
        }
        return reply("404 Not Found", "missing", "text/plain");
    }

    template<typename T>
    static T *child(QWidget &parent, const char *name)
    {
        auto *widget = parent.findChild<T *>(QLatin1String(name));
        Q_ASSERT(widget);
        return widget;
    }

    static bool rowShown(QWidget *field)
    {
        auto *layout = qobject_cast<QFormLayout *>(field->parentWidget()->layout());
        return layout && layout->isRowVisible(field);
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
                    const QList<QByteArray> lines = data.left(end).split('\n');
                    int length = 0;
                    for (const QByteArray &line : lines.mid(1)) {
                        const int colon = line.indexOf(':');
                        if (line.left(colon).trimmed().toLower() == "content-length") {
                            length = line.mid(colon + 1).trimmed().toInt();
                        }
                    }
                    if (data.size() < end + 4 + length) {
                        return;
                    }
                    const QList<QByteArray> start = lines.first().trimmed().split(' ');
                    socket->write(respond(start.value(0), start.value(1)));
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void init()
    {
        m_requests.clear();
        m_previews = true;
    }

    void serverPreviewIsShownInsteadOfTheFile()
    {
        PreviewPanel panel;
        panel.resize(400, 400);
        panel.show();
        QList<QPair<QUrl, QSize>> asked;
        panel.setServerPreviewHandler([&asked](const KFileItem &item, const QSize &size) {
            asked.append({item.url(), size});
            return true;
        });
        auto *image = child<QLabel>(panel, "previewImage");
        auto *footer = child<QLabel>(panel, "previewFooter");
        auto *downloadFull = child<QToolButton>(panel, "previewDownloadFull");
        const QUrl remote = url(QStringLiteral("image.png"));

        panel.setItems({KFileItem(remote)});
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first, remote);
        QVERIFY(!asked.first().second.isEmpty());
        QVERIFY(!panel.isShowingServerPreview());

        // An answer for another item is ignored.
        QImage server(30, 20, QImage::Format_RGB32);
        server.fill(Qt::green);
        panel.showServerPreview(url(QStringLiteral("other.png")), server);
        QVERIFY(!panel.isShowingServerPreview());

        panel.showServerPreview(remote, server);
        QVERIFY(panel.isShowingServerPreview());
        QVERIFY(image->isVisible() && !image->pixmap().isNull());
        QVERIFY(footer->text().contains(QStringLiteral("Preview from the server")));
        QVERIFY(downloadFull->isVisible());
        QTest::qWait(100);
        QCOMPARE(requestCount("GET /image.png"), 0);

        // The whole file can still be downloaded within the usual limits.
        downloadFull->click();
        QVERIFY(!panel.isShowingServerPreview());
        QVERIFY(!downloadFull->isVisible());
        QTRY_COMPARE(requestCount("GET /image.png"), 1);
        QTRY_VERIFY(image->isVisible() && !image->pixmap().isNull());
        QVERIFY(!footer->text().contains(QStringLiteral("Preview from the server")));
    }

    void missingServerPreviewDownloadsTheFile()
    {
        PreviewPanel panel;
        panel.resize(400, 400);
        panel.show();
        bool handled = true;
        int asked = 0;
        panel.setServerPreviewHandler([&](const KFileItem &, const QSize &) {
            ++asked;
            return handled;
        });
        auto *image = child<QLabel>(panel, "previewImage");
        const QUrl remote = url(QStringLiteral("image.png"));

        panel.setItems({KFileItem(remote)});
        panel.showServerPreview(remote, {});
        QVERIFY(!panel.isShowingServerPreview());
        QTRY_COMPARE(requestCount("GET /image.png"), 1);
        QTRY_VERIFY(image->isVisible() && !image->pixmap().isNull());

        // Items that aren't on a Nextcloud server are downloaded as before.
        handled = false;
        panel.clear();
        panel.setItems({KFileItem(remote)});
        QCOMPARE(asked, 2);
        QTRY_COMPARE(requestCount("GET /image.png"), 2);
        QVERIFY(!panel.isShowingServerPreview());
    }

    void informationPanelShowsServerMetadata()
    {
        InformationPanel panel;
        panel.setFields({QStringLiteral("size")});
        panel.show();
        const QUrl remote(QStringLiteral("webdavs://alice@cloud.example.com/remote.php/dav/files/alice/photo.jpg"));
        auto *heading = child<QLabel>(panel, "informationNextcloudHeading");
        auto *status = child<QLabel>(panel, "informationNextcloud_status");
        auto *fileId = child<QLabel>(panel, "informationNextcloud_fileId");
        auto *favorite = child<QLabel>(panel, "informationNextcloud_favorite");
        auto *tags = child<QLabel>(panel, "informationNextcloud_tags");
        auto *shares = child<QLabel>(panel, "informationNextcloud_shares");
        auto *metadataLabel = child<QLabel>(panel, "informationNextcloud_metadata");
        QVERIFY(!rowShown(heading));

        panel.setItem(KFileItem(remote));
        QVERIFY(!rowShown(heading));
        // A late answer about another item is ignored.
        panel.setServerMetadata(QUrl(QStringLiteral("webdavs://cloud.example.com/remote.php/dav/files/alice/")), std::nullopt, QStringLiteral("Folder"));
        QVERIFY(!rowShown(heading));
        panel.setServerMetadata(remote, std::nullopt, QStringLiteral("Loading server metadata…"));
        QVERIFY(rowShown(heading));
        QVERIFY(rowShown(status));
        QVERIFY(!rowShown(fileId));

        Nextcloud::FileMetadata metadata;
        metadata.fileId = QStringLiteral("42");
        metadata.favorite = true;
        metadata.serverTags = {QStringLiteral("Holiday"), QStringLiteral("Family")};
        metadata.shareTypes = {3};
        metadata.metadata = {{QStringLiteral("metadata-photos-ifd0"), QStringLiteral("Make"), QStringLiteral("Canon")}};
        metadata.unavailable = {QStringLiteral("owner-id")};
        // User info in the item's URL doesn't make it a different item.
        panel.setServerMetadata(remote.adjusted(QUrl::RemoveUserInfo), metadata);
        QVERIFY(rowShown(heading));
        QCOMPARE(fileId->text(), QStringLiteral("42"));
        QCOMPARE(favorite->text(), QStringLiteral("Yes"));
        QCOMPARE(tags->text(), QStringLiteral("Holiday, Family"));
        QVERIFY(tags->toolTip().contains(QStringLiteral("separate from Plasma Opus tags")));
        QCOMPARE(shares->text(), QStringLiteral("Public link"));
        QCOMPARE(metadataLabel->text(), QStringLiteral("Camera › Make: Canon"));
        QVERIFY(status->text().contains(QStringLiteral("withheld: owner-id")));
        QVERIFY(!rowShown(child<QLabel>(panel, "informationNextcloud_owner")));

        // The section belongs to its item only.
        panel.setItem(KFileItem(QUrl(QStringLiteral("webdavs://cloud.example.com/remote.php/dav/files/alice/other.jpg"))));
        QVERIFY(!rowShown(heading));
        QVERIFY(!rowShown(fileId));
        panel.setFields({QStringLiteral("owner")});
        panel.setItem(KFileItem(remote));
        QVERIFY(rowShown(child<QLabel>(panel, "informationNextcloudHeading")));
        QCOMPARE(child<QLabel>(panel, "informationNextcloud_fileId")->text(), QStringLiteral("42"));
    }

    void mainWindowUsesTheServerForPreviewsAndMetadata()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {
            QSKIP("The KIO WebDAV worker isn't installed.");
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        QList<NextcloudAccounts::Account> accounts;
        NextcloudAccounts::Account account;
        account.server = base();
        account.user = QStringLiteral("alice");
        accounts.append(account);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        account = NextcloudAccounts::load(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName())).value(0);

        MainWindow window;
        window.resize(1000, 700);
        window.show();
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->trigger();
        window.actionCollection()->action(QStringLiteral("show_information_panel"))->trigger();
        window.openNextcloudAccount(account, false);
        const QUrl photo = NextcloudAccounts::filesUrl(account).resolved(QUrl(QStringLiteral("photo.png")));
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY_WITH_TIMEOUT(!view->itemsInViewOrder().isEmpty(), 20000);
        view->selectUrls({photo});

        auto *preview = window.findChild<PreviewPanel *>();
        QVERIFY(preview);
        QTRY_VERIFY_WITH_TIMEOUT(preview->isShowingServerPreview(), 20000);

        const auto previewRequestIt = std::find_if(m_requests.cbegin(), m_requests.cend(), [](const QByteArray &request) {
            return request.startsWith("GET /index.php/core/preview");
        });
        QVERIFY(previewRequestIt != m_requests.cend());
        const QUrlQuery query(QUrl(QString::fromUtf8(previewRequestIt->mid(4))));
        QCOMPARE(query.queryItemValue(QStringLiteral("fileId")), QStringLiteral("42"));
        QCOMPARE(requestCount("GET /remote.php/dav/files/alice/photo.png"), 0);

        auto *information = window.findChild<InformationPanel *>();
        QVERIFY(information);
        QTRY_COMPARE_WITH_TIMEOUT(child<QLabel>(*information, "informationNextcloud_fileId")->text(), QStringLiteral("42"), 20000);
        QCOMPARE(child<QLabel>(*information, "informationNextcloud_favorite")->text(), QStringLiteral("Yes"));
        QCOMPARE(child<QLabel>(*information, "informationNextcloud_tags")->text(), QStringLiteral("Holiday"));
        // The preview and the information panel share one metadata request.
        QCOMPARE(requestCount("PROPFIND /remote.php/dav/files/alice/photo.png"), 1);
    }

    void mainWindowDownloadsWhenTheServerHasNoPreview()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {
            QSKIP("The KIO WebDAV worker isn't installed.");
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        m_previews = false;
        QList<NextcloudAccounts::Account> accounts;
        NextcloudAccounts::Account account;
        account.server = base();
        account.user = QStringLiteral("alice");
        accounts.append(account);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        account = NextcloudAccounts::load(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName())).value(0);

        MainWindow window;
        window.resize(1000, 700);
        window.show();
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->trigger();
        window.openNextcloudAccount(account, false);
        const QUrl photo = NextcloudAccounts::filesUrl(account).resolved(QUrl(QStringLiteral("photo.png")));
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY_WITH_TIMEOUT(!view->itemsInViewOrder().isEmpty(), 20000);
        view->selectUrls({photo});

        QTRY_COMPARE_WITH_TIMEOUT(requestCount("GET /remote.php/dav/files/alice/photo.png"), 1, 20000);
        QCOMPARE(requestCount("GET /index.php/core/preview"), 1);
        QVERIFY(!window.findChild<PreviewPanel *>()->isShowingServerPreview());
    }

    void mainWindowFollowsThePreviewAndMetadataSettings()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {
            QSKIP("The KIO WebDAV worker isn't installed.");
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        QList<NextcloudAccounts::Account> accounts;
        NextcloudAccounts::Account account;
        account.server = base();
        account.user = QStringLiteral("alice");
        accounts.append(account);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        account = NextcloudAccounts::load(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName())).value(0);
        Settings::setNextcloudServerPreviews(false);
        Settings::setNextcloudServerMetadata(false);

        MainWindow window;
        window.resize(1000, 700);
        window.show();
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->trigger();
        window.actionCollection()->action(QStringLiteral("show_information_panel"))->trigger();
        window.openNextcloudAccount(account, false);
        const QUrl photo = NextcloudAccounts::filesUrl(account).resolved(QUrl(QStringLiteral("photo.png")));
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY_WITH_TIMEOUT(!view->itemsInViewOrder().isEmpty(), 20000);
        view->selectUrls({photo});

        // The preview panel downloads the file, as in other remote folders, without asking the server for a preview.
        QTRY_COMPARE_WITH_TIMEOUT(requestCount("GET /remote.php/dav/files/alice/photo.png"), 1, 20000);
        QCOMPARE(requestCount("GET /index.php/core/preview"), 0);
        QVERIFY(!window.findChild<PreviewPanel *>()->isShowingServerPreview());
        // The information panel shows no server metadata.
        auto *information = window.findChild<InformationPanel *>();
        QVERIFY(information);
        QTest::qWait(200);
        QVERIFY(!rowShown(child<QLabel>(*information, "informationNextcloudHeading")));
        QVERIFY(child<QLabel>(*information, "informationNextcloud_fileId")->text() != QStringLiteral("42"));
    }
};

QTEST_MAIN(NextcloudPreviewTest)

#include "nextcloudpreviewtest.moc"
