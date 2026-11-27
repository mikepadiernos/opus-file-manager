// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "informationpanel.h"
#include "mainwindow.h"
#include "nextcloudaccount.h"
#include "nextcloudassetsdialog.h"
#include "nextcloudmenu.h"
#include "nextcloudtagsdialog.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTreeWidget>

#include <algorithm>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("organize-dialog-password-0123");

#define SKIP_WITHOUT_WEBDAV()                                                                                                                                  \
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {                                                                                           \
        QSKIP("The KIO WebDAV worker isn't installed.");                                                                                                       \
    }

QByteArray multistatus(const QByteArray &responses)
{
    return "<?xml version=\"1.0\"?>\n<d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">\n" + responses
        + "</d:multistatus>";
}

QByteArray fileResponse(const QByteArray &href, const QByteArray &fileId, const QByteArray &type, bool favorite, const QByteArray &extra = QByteArray())
{
    const bool folder = type == "httpd/unix-directory";
    return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop>"
        + (folder ? QByteArray("<d:resourcetype><d:collection/></d:resourcetype>")
                  : QByteArray("<d:resourcetype/><d:getcontentlength>300</d:getcontentlength>"))
        + "<d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><d:getcontenttype>" + type + "</d:getcontenttype>"
        + (fileId.isEmpty() ? QByteArray() : "<oc:fileid>" + fileId + "</oc:fileid>") + "<oc:favorite>" + (favorite ? "1" : "0") + "</oc:favorite>"
        + "<oc:permissions>GDNVW</oc:permissions><nc:system-tags><nc:system-tag oc:id=\"2\">Holiday</nc:system-tag></nc:system-tags>"
        + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>" + extra + "</d:response>\n";
}

QByteArray tagResponse(const QByteArray &href, const QByteArray &id, const QByteArray &name, bool visible, bool assignable)
{
    const QByteArray flag = assignable ? "true" : "false";
    return "<d:response><d:href>" + href + id + "</d:href><d:propstat><d:prop><oc:id>" + id + "</oc:id><oc:display-name>" + name
        + "</oc:display-name><oc:user-visible>" + (visible ? "true" : "false") + "</oc:user-visible><oc:user-assignable>" + flag
        + "</oc:user-assignable><oc:can-assign>" + flag + "</oc:can-assign></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>\n";
}

/// The server's tags in Nextcloud 29's format: an assignable one, a restricted one and an invisible one.
const QByteArray tagsResponse =
    multistatus(tagResponse("/remote.php/dav/systemtags/", "5", "Lake", true, false) + tagResponse("/remote.php/dav/systemtags/", "2", "Holiday", true, true)
                + tagResponse("/remote.php/dav/systemtags/", "8", "archive", false, false));

struct Request {
    QByteArray method;
    QByteArray path;
    QByteArray authorization;
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

template<typename T>
T *child(QObject &parent, const char *name)
{
    auto *object = parent.findChild<T *>(QLatin1String(name));
    Q_ASSERT(object);
    return object;
}

bool comboItemEnabled(QComboBox *combo, int index)
{
    return qobject_cast<QStandardItemModel *>(combo->model())->item(index)->isEnabled();
}
}

class NextcloudOrganizeDialogTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;
    /// Whether photo.png is a favourite on the server.
    bool m_favorite = true;

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

    QUrl fileUrl(const Account &account, const QString &path) const
    {
        QUrl url = NextcloudAccounts::filesUrl(account);
        url.setPath(url.path() + path);
        return url;
    }

    QList<Request> requests(const QByteArray &method, const QByteArray &prefix = QByteArray()) const
    {
        QList<Request> matching;
        std::copy_if(m_requests.cbegin(), m_requests.cend(), std::back_inserter(matching), [&](const Request &request) {
            return request.method == method && request.path.startsWith(prefix);
        });
        return matching;
    }

    QByteArray respond(const Request &request)
    {
        const QByteArray &path = request.path;
        auto reply = [](const QByteArray &status, const QByteArray &body = QByteArray(), const QByteArray &headers = QByteArray()) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/xml; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n"
                + headers + "Connection: close\r\n\r\n" + body;
        };
        const QByteArray files = "/remote.php/dav/files/alice/";
        if (request.method == "PROPFIND" && path == "/remote.php/dav/systemtags/") {
            return reply("207 Multi-Status", tagsResponse);
        }
        if (request.method == "PROPFIND" && path.startsWith("/remote.php/dav/systemtags-relations/files/")) {
            const QByteArray folder = path.endsWith('/') ? path : path + '/';
            QByteArray responses = "<d:response><d:href>" + folder
                + "</d:href><d:propstat><d:prop><oc:id/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response>";
            if (folder.endsWith("/42/")) {
                responses += tagResponse(folder, "2", "Holiday", true, true);
            }
            return reply("207 Multi-Status", multistatus(responses));
        }
        if (request.method == "PUT" && path.startsWith("/remote.php/dav/systemtags-relations/files/")) {
            return reply(path.endsWith("/13") ? "403 Forbidden" : "201 Created");
        }
        if (request.method == "DELETE" && path.startsWith("/remote.php/dav/systemtags-relations/files/")) {
            return reply("204 No Content");
        }
        if (request.method == "POST" && path == "/remote.php/dav/systemtags") {
            const QString name = QJsonDocument::fromJson(request.body).object().value(QStringLiteral("name")).toString();
            return reply("201 Created",
                         QByteArray(),
                         "Content-Location: /remote.php/dav/systemtags/" + QByteArray(name == QLatin1String("Broken") ? "13" : "12") + "\r\n");
        }
        if (request.method == "PROPPATCH") {
            const bool forbidden = path.contains("Shared");
            if (!forbidden && path == files + "photo.png") {
                m_favorite = request.body.contains("<oc:favorite>1</oc:favorite>");
            }
            return reply("207 Multi-Status",
                         multistatus("<d:response><d:href>" + path + "</d:href><d:propstat><d:prop><oc:favorite/></d:prop><d:status>HTTP/1.1 "
                                     + (forbidden ? "403 Forbidden" : "200 OK") + "</d:status></d:propstat></d:response>"));
        }
        if (request.method == "REPORT" && path == files) {
            if (request.body.contains("<oc:systemtag>2</oc:systemtag>")) {
                return reply("207 Multi-Status", multistatus(fileResponse(files + "Photos/Lake%20view.jpg", "42", "image/jpeg", true)));
            }
            // Favourites: one whose owner is withheld, and one without an ID.
            return reply(
                "207 Multi-Status",
                multistatus(fileResponse(files + "Photos/Lake%20view.jpg", "42", "image/jpeg", true)
                            + fileResponse(files + "Shared/",
                                           "51",
                                           "httpd/unix-directory",
                                           true,
                                           "<d:propstat><d:prop><oc:owner-display-name/></d:prop><d:status>HTTP/1.1 403 Forbidden</d:status></d:propstat>")
                            + fileResponse(files + "Locked.txt", QByteArray(), "text/plain", true)));
        }
        if (request.method == "SEARCH" && path == "/remote.php/dav/") {
            if (request.body.contains("unsupported")) {
                return reply("501 Not Implemented");
            }
            // One more than the dialog shows, as the dialog asks for.
            QByteArray responses;
            for (int i = 1; i <= NextcloudAssetsDialog::ResultLimit + 1; ++i) {
                responses += fileResponse(files + "Photos/Lake%20" + QByteArray::number(i) + ".jpg", QByteArray::number(1000 + i), "image/jpeg", false);
            }
            return reply("207 Multi-Status", multistatus(responses));
        }
        if (request.method == "PROPFIND" && (path == files || path == files.chopped(1))) {
            return reply(
                "207 Multi-Status",
                multistatus(fileResponse(files, "1", "httpd/unix-directory", false) + fileResponse(files + "photo.png", "42", "image/png", m_favorite)));
        }
        if (request.method == "PROPFIND" && path == files + "photo.png") {
            return reply("207 Multi-Status", multistatus(fileResponse(path, "42", "image/png", m_favorite)));
        }
        if (request.method == "PROPFIND" && path == files + "notes.txt") {
            return reply("207 Multi-Status", multistatus(fileResponse(path, "43", "text/plain", false)));
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
                        if (name == "content-length") {
                            length = line.mid(colon + 1).trimmed().toInt();
                        } else if (name == "authorization") {
                            request.authorization = line.mid(colon + 1).trimmed();
                        }
                    }
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
        m_favorite = true;
    }

    void menusOfferSearchAndOrganize()
    {
        QMenu menu;
        QList<QString> organized;
        NextcloudMenu::populate(
            &menu,
            {account()},
            QString(),
            QString(),
            true,
            nullptr,
            this,
            [](const Account &, bool) { },
            [&organized](const Account &account) {
                organized.append(account.id);
            });
        auto *organize = menu.findChild<QMenu *>(QStringLiteral("nextcloudAccount-test"))->findChild<QAction *>(QStringLiteral("nextcloudOrganize"));
        QVERIFY(organize);
        organize->trigger();
        QCOMPARE(organized, QList<QString>{QStringLiteral("test")});

        QList<bool> favorites;
        int tags = 0;
        int organizeCalls = 0;
        NextcloudMenu::ItemHandlers handlers;
        handlers.setFavorite = [&favorites](bool favorite) {
            favorites.append(favorite);
        };
        handlers.editTags = [&tags] {
            ++tags;
        };
        handlers.organize = [&organizeCalls] {
            ++organizeCalls;
        };
        QMenu context;
        QMenu *item = NextcloudMenu::addItemMenu(&context, true, this, handlers);
        QCOMPARE(item->objectName(), QStringLiteral("nextcloudItemMenu"));
        QVERIFY(!item->findChild<QAction *>(QStringLiteral("nextcloudAddFavorite")));
        item->findChild<QAction *>(QStringLiteral("nextcloudRemoveFavorite"))->trigger();
        item->findChild<QAction *>(QStringLiteral("nextcloudEditTags"))->trigger();
        item->findChild<QAction *>(QStringLiteral("nextcloudItemOrganize"))->trigger();
        QCOMPARE(favorites, QList<bool>{false});
        QCOMPARE(tags, 1);
        QCOMPARE(organizeCalls, 1);
        QVERIFY(item->findChild<QAction *>(QStringLiteral("nextcloudEditTags"))->toolTip().contains(QStringLiteral("separate from Plasma Opus tags")));

        // Unknown or mixed favourites offer both.
        QMenu mixed;
        QMenu *both = NextcloudMenu::addItemMenu(&mixed, std::nullopt, this, handlers);
        both->findChild<QAction *>(QStringLiteral("nextcloudAddFavorite"))->trigger();
        QVERIFY(both->findChild<QAction *>(QStringLiteral("nextcloudRemoveFavorite")));
        QCOMPARE(favorites, (QList<bool>{false, true}));

        // Without favourite and tag handlers, as Preferences can set, only Search and Organize is offered.
        NextcloudMenu::ItemHandlers organizeOnly;
        organizeOnly.organize = handlers.organize;
        QMenu plain;
        QMenu *reduced = NextcloudMenu::addItemMenu(&plain, std::nullopt, this, organizeOnly);
        QVERIFY(!reduced->findChild<QAction *>(QStringLiteral("nextcloudAddFavorite")));
        QVERIFY(!reduced->findChild<QAction *>(QStringLiteral("nextcloudRemoveFavorite")));
        QVERIFY(!reduced->findChild<QAction *>(QStringLiteral("nextcloudEditTags")));
        QVERIFY(!reduced->actions().constFirst()->isSeparator());
        reduced->findChild<QAction *>(QStringLiteral("nextcloudItemOrganize"))->trigger();
        QCOMPARE(organizeCalls, 2);
    }

    void searchReportsTruncatedResults()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudAssetsDialog dialog(account(), &m_store);
        dialog.setCapabilities({QStringLiteral("29.0.0"), 29, QStringLiteral("Nextcloud"), true, true, true, true, true});
        dialog.setMode(NextcloudAssetsDialog::Mode::Search);
        auto *mode = child<QComboBox>(dialog, "nextcloudAssetsMode");
        QVERIFY(comboItemEnabled(mode, 0));
        child<QLineEdit>(dialog, "nextcloudAssetsText")->setText(QStringLiteral("Lake"));
        auto *type = child<QComboBox>(dialog, "nextcloudAssetsType");
        type->setCurrentIndex(type->findData(int(Nextcloud::AssetType::Images)));
        child<QPushButton>(dialog, "nextcloudAssetsRun")->click();

        auto *results = child<QTreeWidget>(dialog, "nextcloudAssetsResults");
        auto *status = child<QLabel>(dialog, "nextcloudAssetsStatus");
        QTRY_COMPARE_WITH_TIMEOUT(results->topLevelItemCount(), NextcloudAssetsDialog::ResultLimit, 20000);
        QVERIFY2(status->text().contains(QStringLiteral("Only the first 500 results are listed")), qPrintable(status->text()));
        QCOMPARE(results->topLevelItem(0)->text(0), QStringLiteral("Lake 1.jpg"));
        QCOMPARE(results->topLevelItem(0)->text(1), QStringLiteral("/Photos"));
        QCOMPARE(results->topLevelItem(0)->text(3), QString());
        QCOMPARE(results->topLevelItem(0)->text(4), QStringLiteral("Holiday"));
        QVERIFY(results->headerItem()->text(4).contains(QStringLiteral("server")));

        const QList<Request> searches = requests("SEARCH");
        QCOMPARE(searches.size(), 1);
        QVERIFY(searches.first().body.contains("Lake"));
        QVERIFY(searches.first().body.contains("image/%"));
        QCOMPARE(searches.first().authorization, "Basic " + QByteArray("alice:" + appPassword.toUtf8()).toBase64());

        // The mode and kind are remembered as state, not as settings.
        const KConfigGroup state = KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudAssetsDialog"));
        QCOMPARE(state.readEntry("Mode", -1), int(NextcloudAssetsDialog::Mode::Search));
        QCOMPARE(state.readEntry("Type", -1), int(Nextcloud::AssetType::Images));

        // Preferences › Remote and Cloud › Nextcloud sets a smaller limit for later searches.
        QCOMPARE(dialog.resultLimit(), NextcloudAssetsDialog::ResultLimit);
        dialog.setResultLimit(100);
        child<QPushButton>(dialog, "nextcloudAssetsRun")->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("Only the first 100 results are listed")), 20000);
        QCOMPARE(results->topLevelItemCount(), 100);
        QVERIFY(requests("SEARCH").last().body.contains("<d:nresults>101</d:nresults>"));

        // A server that refuses the query says so.
        child<QLineEdit>(dialog, "nextcloudAssetsText")->setText(QStringLiteral("unsupported"));
        child<QPushButton>(dialog, "nextcloudAssetsRun")->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("doesn't offer this search")), 20000);
        QCOMPARE(results->topLevelItemCount(), 0);
    }

    void favouritesAreListedAndChanged()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudAssetsDialog dialog(account(), &m_store);
        QSignalSpy changed(&dialog, &NextcloudAssetsDialog::itemsChanged);
        QSignalSpy shown(&dialog, &NextcloudAssetsDialog::showRequested);
        dialog.setMode(NextcloudAssetsDialog::Mode::Search);
        dialog.setMode(NextcloudAssetsDialog::Mode::Favorites);
        auto *results = child<QTreeWidget>(dialog, "nextcloudAssetsResults");
        auto *status = child<QLabel>(dialog, "nextcloudAssetsStatus");
        QTRY_COMPARE_WITH_TIMEOUT(results->topLevelItemCount(), 2, 20000);
        QVERIFY2(status->text().contains(QStringLiteral("2 results are missing or incomplete")), qPrintable(status->text()));
        QVERIFY(!status->text().contains(QStringLiteral("Only the first")));
        QCOMPARE(results->topLevelItem(0)->text(0), QStringLiteral("Lake view.jpg"));
        QCOMPARE(results->topLevelItem(0)->text(3), QStringLiteral("Yes"));
        QCOMPARE(results->topLevelItem(1)->text(0), QStringLiteral("Shared"));
        QCOMPARE(results->topLevelItem(1)->text(1), QStringLiteral("/"));

        auto *show = child<QPushButton>(dialog, "nextcloudAssetsShow");
        auto *favorite = child<QPushButton>(dialog, "nextcloudAssetsFavorite");
        QVERIFY(!favorite->isEnabled());
        results->topLevelItem(1)->setSelected(true);
        show->click();
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.first().at(0).toUrl().adjusted(QUrl::StripTrailingSlash), fileUrl(account(), QStringLiteral("Shared")));
        QVERIFY(shown.first().at(1).toBool());

        // Removing a favourite the server refuses to change reports it.
        QCOMPARE(favorite->text(), QStringLiteral("Remove from Favourites"));
        favorite->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("couldn't be changed")), 20000);
        QCOMPARE(results->topLevelItem(1)->text(3), QStringLiteral("Yes"));
        QCOMPARE(changed.size(), 0);

        results->clearSelection();
        results->topLevelItem(0)->setSelected(true);
        favorite->click();
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 20000);
        QCOMPARE(changed.first().first().value<QList<QUrl>>(), QList<QUrl>{fileUrl(account(), QStringLiteral("Photos/Lake view.jpg"))});
        QCOMPARE(results->topLevelItem(0)->text(3), QString());
        QCOMPARE(favorite->text(), QStringLiteral("Add to Favourites"));
        const QList<Request> patches = requests("PROPPATCH", "/remote.php/dav/files/alice/Photos/");
        QCOMPARE(patches.size(), 1);
        QVERIFY(patches.first().body.contains("<oc:favorite>0</oc:favorite>"));
    }

    void tagModeListsTaggedFiles()
    {
        SKIP_WITHOUT_WEBDAV();
        NextcloudAssetsDialog dialog(account(), &m_store);
        dialog.setMode(NextcloudAssetsDialog::Mode::Tag);
        auto *tag = child<QComboBox>(dialog, "nextcloudAssetsTag");
        QTRY_COMPARE_WITH_TIMEOUT(tag->count(), 3, 20000);
        QCOMPARE(tag->itemText(0), QStringLiteral("archive (invisible)"));
        tag->setCurrentIndex(tag->findData(QStringLiteral("2")));
        child<QPushButton>(dialog, "nextcloudAssetsRun")->click();
        auto *results = child<QTreeWidget>(dialog, "nextcloudAssetsResults");
        QTRY_COMPARE_WITH_TIMEOUT(results->topLevelItemCount(), 1, 20000);
        QVERIFY(requests("REPORT").last().body.contains("<oc:systemtag>2</oc:systemtag>"));

        // The tags dialog opened from the results starts from the listed file ID.
        results->topLevelItem(0)->setSelected(true);
        child<QPushButton>(dialog, "nextcloudAssetsTags")->click();
        QVERIFY(dialog.tagsDialog());
        auto *list = child<QListWidget>(*dialog.tagsDialog(), "nextcloudTagList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 3, 20000);
        QVERIFY(requests("PROPFIND", "/remote.php/dav/files/alice/Photos/").isEmpty());
        QCOMPARE(list->findItems(QStringLiteral("Holiday"), Qt::MatchExactly).first()->checkState(), Qt::Checked);
    }

    void unavailableFeaturesAreDisabled()
    {
        SKIP_WITHOUT_WEBDAV();
        KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudAssetsDialog")).writeEntry("Mode", int(NextcloudAssetsDialog::Mode::Search));
        NextcloudAssetsDialog dialog(account(), &m_store);
        Nextcloud::Capabilities capabilities;
        capabilities.major = 14;
        capabilities.authenticated = true;
        capabilities.dav = true;
        dialog.setCapabilities(capabilities);
        auto *mode = child<QComboBox>(dialog, "nextcloudAssetsMode");
        QCOMPARE(dialog.mode(), NextcloudAssetsDialog::Mode::Favorites);
        QVERIFY(!comboItemEnabled(mode, 0));
        QVERIFY(!comboItemEnabled(mode, 2));
        QVERIFY(comboItemEnabled(mode, 1));
        auto *notice = child<QLabel>(dialog, "nextcloudAssetsUnavailable");
        QVERIFY(!notice->isHidden());
        QVERIFY(notice->text().contains(QStringLiteral("Searching isn't available")));
        QVERIFY(notice->text().contains(QStringLiteral("Collaborative tags app is disabled")));

        auto *results = child<QTreeWidget>(dialog, "nextcloudAssetsResults");
        QTRY_COMPARE_WITH_TIMEOUT(results->topLevelItemCount(), 2, 20000);
        results->topLevelItem(0)->setSelected(true);
        QVERIFY(child<QPushButton>(dialog, "nextcloudAssetsFavorite")->isEnabled());
        QVERIFY(!child<QPushButton>(dialog, "nextcloudAssetsTags")->isEnabled());
        dialog.run();
        QTRY_COMPARE_WITH_TIMEOUT(requests("REPORT").size(), 2, 20000);
        QVERIFY(requests("SEARCH").isEmpty());
    }

    void tagsDialogAppliesOnlyChanges()
    {
        SKIP_WITHOUT_WEBDAV();
        const QUrl photo = fileUrl(account(), QStringLiteral("photo.png"));
        const QUrl notes = fileUrl(account(), QStringLiteral("notes.txt"));
        NextcloudTagsDialog dialog(account(), {{photo, QStringLiteral("42")}, {notes, QString()}}, &m_store);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Nextcloud Tags (Server)"));
        QSignalSpy changed(&dialog, &NextcloudTagsDialog::tagsChanged);
        auto *list = child<QListWidget>(dialog, "nextcloudTagList");
        auto *ok = child<QPushButton>(dialog, "nextcloudTagsOk");
        QVERIFY(!ok->isEnabled());
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 3, 20000);
        QVERIFY(ok->isEnabled());
        QCOMPARE(list->item(0)->text(), QStringLiteral("archive (invisible)"));
        QListWidgetItem *holiday = list->item(1);
        QListWidgetItem *lake = list->item(2);
        QCOMPARE(holiday->text(), QStringLiteral("Holiday"));
        QCOMPARE(holiday->checkState(), Qt::PartiallyChecked);
        QVERIFY(holiday->flags().testFlag(Qt::ItemIsUserTristate));
        QCOMPARE(lake->text(), QStringLiteral("Lake (restricted)"));
        QVERIFY(!lake->flags().testFlag(Qt::ItemIsEnabled));
        QVERIFY(!lake->toolTip().isEmpty());
        // The file without a known ID was read once, for its ID.
        QCOMPARE(requests("PROPFIND", "/remote.php/dav/files/alice/notes.txt").size(), 1);
        QCOMPARE(requests("PROPFIND", "/remote.php/dav/systemtags-relations/files/43").size(), 1);

        holiday->setCheckState(Qt::Checked);
        ok->click();
        QTRY_COMPARE_WITH_TIMEOUT(dialog.result(), int(QDialog::Accepted), 20000);
        QCOMPARE(requests("PUT").size(), 1);
        QCOMPARE(requests("PUT").first().path, QByteArray("/remote.php/dav/systemtags-relations/files/43/2"));
        QVERIFY(requests("DELETE").isEmpty());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().first().value<QList<QUrl>>(), QList<QUrl>{notes});

        // Unchecking removes the tag from the files that have it.
        m_requests.clear();
        NextcloudTagsDialog removing(account(), {{photo, QStringLiteral("42")}}, &m_store);
        auto *removingList = child<QListWidget>(removing, "nextcloudTagList");
        QTRY_COMPARE_WITH_TIMEOUT(removingList->count(), 3, 20000);
        QCOMPARE(removingList->item(1)->checkState(), Qt::Checked);
        removingList->item(1)->setCheckState(Qt::Unchecked);
        child<QPushButton>(removing, "nextcloudTagsOk")->click();
        QTRY_COMPARE_WITH_TIMEOUT(removing.result(), int(QDialog::Accepted), 20000);
        QCOMPARE(requests("DELETE").size(), 1);
        QCOMPARE(requests("DELETE").first().path, QByteArray("/remote.php/dav/systemtags-relations/files/42/2"));
    }

    void tagsAreCreatedOnlyWhenConfirmed()
    {
        SKIP_WITHOUT_WEBDAV();
        const QUrl photo = fileUrl(account(), QStringLiteral("photo.png"));
        NextcloudTagsDialog dialog(account(), {{photo, QStringLiteral("42")}}, &m_store);
        QStringList asked;
        bool confirm = false;
        dialog.setCreationConfirmation([&asked, &confirm](const QString &name) {
            asked.append(name);
            return confirm;
        });
        auto *list = child<QListWidget>(dialog, "nextcloudTagList");
        auto *name = child<QLineEdit>(dialog, "nextcloudNewTag");
        auto *create = child<QPushButton>(dialog, "nextcloudCreateTag");
        auto *status = child<QLabel>(dialog, "nextcloudTagsStatus");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 3, 20000);
        QVERIFY(!create->isEnabled());

        // An existing tag is checked without asking or creating anything.
        list->item(1)->setCheckState(Qt::Unchecked);
        name->setText(QStringLiteral("holiday"));
        create->click();
        QVERIFY(asked.isEmpty());
        QCOMPARE(list->item(1)->checkState(), Qt::Checked);
        QVERIFY(status->text().contains(QStringLiteral("already has")));

        name->setText(QStringLiteral(" Trip "));
        create->click();
        QCOMPARE(asked, QStringList{QStringLiteral("Trip")});
        QVERIFY(status->text().contains(QStringLiteral("wasn't created")));
        QTest::qWait(100);
        QVERIFY(requests("POST").isEmpty());

        confirm = true;
        create->click();
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 4, 20000);
        QCOMPARE(requests("POST").size(), 1);
        QCOMPARE(QJsonDocument::fromJson(requests("POST").first().body).object().value(QStringLiteral("name")).toString(), QStringLiteral("Trip"));
        const QList<QListWidgetItem *> trip = list->findItems(QStringLiteral("Trip"), Qt::MatchExactly);
        QCOMPARE(trip.size(), 1);
        QCOMPARE(trip.first()->checkState(), Qt::Checked);
        QVERIFY(name->text().isEmpty());

        // A tag the server won't assign keeps the dialog open with the reason.
        name->setText(QStringLiteral("Broken"));
        create->click();
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 5, 20000);
        child<QPushButton>(dialog, "nextcloudTagsOk")->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("couldn't be saved")), 20000);
        QVERIFY(status->text().contains(QStringLiteral("Broken")));
        QVERIFY(dialog.result() != int(QDialog::Accepted));
        const QList<Request> puts = requests("PUT");
        QCOMPARE(puts.size(), 2);
        QVERIFY(std::any_of(puts.cbegin(), puts.cend(), [](const Request &request) {
            return request.path == "/remote.php/dav/systemtags-relations/files/42/12";
        }));
        // What was saved is now the starting point; what failed is still unchecked on the server.
        QCOMPARE(list->findItems(QStringLiteral("Trip"), Qt::MatchExactly).first()->checkState(), Qt::Checked);
        QCOMPARE(list->findItems(QStringLiteral("Broken"), Qt::MatchExactly).first()->checkState(), Qt::Unchecked);
    }

    void mainWindowChangesFavouritesAndRereadsMetadata()
    {
        SKIP_WITHOUT_WEBDAV();
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        QList<Account> accounts;
        Account saved;
        saved.server = base();
        saved.user = QStringLiteral("alice");
        accounts.append(saved);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        const Account account = NextcloudAccounts::load(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName())).value(0);

        MainWindow window;
        window.resize(1000, 700);
        window.show();
        window.actionCollection()->action(QStringLiteral("show_information_panel"))->trigger();
        window.openNextcloudAccount(account, false);
        const QUrl photo = fileUrl(account, QStringLiteral("photo.png"));
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY_WITH_TIMEOUT(!view->itemsInViewOrder().isEmpty(), 20000);
        view->selectUrls({photo});

        auto *information = window.findChild<InformationPanel *>();
        QVERIFY(information);
        auto *favorite = child<QLabel>(*information, "informationNextcloud_favorite");
        QTRY_COMPARE_WITH_TIMEOUT(favorite->text(), QStringLiteral("Yes"), 20000);
        const qsizetype reads = requests("PROPFIND", "/remote.php/dav/files/alice/photo.png").size();

        window.setNextcloudFavorite({photo}, false);
        QTRY_COMPARE_WITH_TIMEOUT(favorite->text(), QStringLiteral("No"), 20000);
        const QList<Request> patches = requests("PROPPATCH");
        QCOMPARE(patches.size(), 1);
        QCOMPARE(patches.first().path, QByteArray("/remote.php/dav/files/alice/photo.png"));
        QVERIFY(patches.first().body.contains("<oc:favorite>0</oc:favorite>"));
        QCOMPARE(requests("PROPFIND", "/remote.php/dav/files/alice/photo.png").size(), reads + 1);
    }
};

QTEST_MAIN(NextcloudOrganizeDialogTest)

#include "nextcloudorganizedialogtest.moc"
