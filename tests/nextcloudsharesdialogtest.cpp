// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudsharesdialog.h"
#include "nextcloudaccount.h"
#include "nextcloudmenu.h"

#include <KConfigGroup>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTreeWidget>
#include <QUrlQuery>

#include <algorithm>

using Nextcloud::SharePermission;
using Nextcloud::ShareType;
using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("shares-dialog-password-0123");
const QString sharePassword = QStringLiteral("share-secret-4567");
const QByteArray sharesRoot = "/ocs/v2.php/apps/files_sharing/api/v1/shares";

#define SKIP_WITHOUT_WEBDAV()                                                                                                                                  \
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {                                                                                           \
        QSKIP("The KIO WebDAV worker isn't installed.");                                                                                                       \
    }

QByteArray ocs(const QByteArray &data, int status = 200, const QByteArray &message = "OK")
{
    return "{\"ocs\":{\"meta\":{\"status\":\"" + QByteArray(status == 200 ? "ok" : "failure") + "\",\"statuscode\":" + QByteArray::number(status)
        + ",\"message\":\"" + message + "\"},\"data\":" + data + "}}";
}

const QByteArray linkShare =
    "{\"id\":\"7\",\"share_type\":3,\"uid_owner\":\"alice\",\"displayname_owner\":\"Alice\",\"permissions\":1,\"can_edit\":true,\"can_delete\":true,"
    "\"expiration\":\"2099-12-31 00:00:00\",\"token\":\"AbCdEf123\",\"label\":\"Family\",\"path\":\"/Photos\",\"item_type\":\"folder\","
    "\"share_with\":\"$2y$10$hash\",\"password\":\"$2y$10$hash\",\"url\":\"https://cloud.example.com/s/AbCdEf123\"}";

const QByteArray userShare =
    "{\"id\":\"8\",\"share_type\":0,\"uid_owner\":\"alice\",\"permissions\":19,\"can_edit\":true,\"can_delete\":true,"
    "\"expiration\":null,\"path\":\"/Photos\",\"item_type\":\"folder\",\"share_with\":\"bob\",\"share_with_displayname\":\"Bob\"}";

const QByteArray newLinkShare =
    "{\"id\":\"11\",\"share_type\":3,\"uid_owner\":\"alice\",\"permissions\":1,\"path\":\"/Photos\",\"item_type\":\"folder\","
    "\"url\":\"https://cloud.example.com/s/New456\",\"share_with\":\"$2y$10$hash\"}";

const QByteArray receivedShare =
    "{\"id\":\"20\",\"share_type\":0,\"uid_owner\":\"bob\",\"displayname_owner\":\"Bob\",\"permissions\":1,"
    "\"can_edit\":false,\"can_delete\":true,\"path\":\"/Plans.odt\",\"item_type\":\"file\",\"share_with\":\"alice\"}";

/// @p enforcedExpiryDays is the longest a public link may last, or 0 when the server doesn't limit it.
QByteArray capabilities(bool api, int enforcedExpiryDays = 0)
{
    const QByteArray expiry = enforcedExpiryDays > 0 ? "{\"enabled\":true,\"enforced\":true,\"days\":" + QByteArray::number(enforcedExpiryDays) + "}"
                                                     : QByteArray("{\"enabled\":false}");
    return ocs("{\"capabilities\":{\"files_sharing\":{\"api_enabled\":" + QByteArray(api ? "true" : "false")
               + ",\"public\":{\"enabled\":true,\"password\":{\"enforced\":false},\"expire_date\":" + expiry + ",\"upload\":true},"
                 "\"resharing\":true,\"group_sharing\":true,\"default_permissions\":31}}}");
}

struct Request {
    QByteArray method;
    QByteArray path;
    QUrlQuery query;
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

QStringList ids(const QList<Nextcloud::Share> &shares)
{
    QStringList ids;
    for (const Nextcloud::Share &share : shares) {
        ids.append(share.id);
    }
    return ids;
}

/// Selects the share @p id in @p dialog.
bool select(NextcloudSharesDialog &dialog, const QString &id)
{
    const int row = int(ids(dialog.shownShares()).indexOf(id));
    if (row < 0) {
        return false;
    }
    auto *list = child<QTreeWidget>(dialog, "sharesList");
    list->setCurrentItem(list->topLevelItem(row));
    return true;
}

/// Whether @p text is anywhere in @p config.
bool configContains(const KSharedConfigPtr &config, const QString &text)
{
    const QStringList groups = config->groupList();
    for (const QString &group : groups) {
        const QMap<QString, QString> entries = config->group(group).entryMap();
        for (const QString &value : entries) {
            if (value.contains(text)) {
                return true;
            }
        }
    }
    return false;
}
}

class NextcloudSharesDialogTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QList<Request> m_requests;
    MemoryStore m_store;
    bool m_sharingMissing = false;
    bool m_api = true;
    int m_enforcedExpiryDays = 0;

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
            return request.method == method && request.path.startsWith(sharesRoot);
        });
        return matching;
    }

    /// How often the shares of the Photos folder were listed.
    qsizetype photosListings() const
    {
        const QList<Request> gets = requests("GET");
        return std::count_if(gets.cbegin(), gets.cend(), [](const Request &request) {
            return request.query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded) == QLatin1String("/Photos");
        });
    }

    QByteArray respond(const Request &request)
    {
        auto reply = [](const QByteArray &status, const QByteArray &body = QByteArray()) {
            return "HTTP/1.1 " + status + "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        };
        if (request.path == "/ocs/v2.php/cloud/capabilities") {
            return reply("200 OK", capabilities(m_api, m_enforcedExpiryDays));
        }
        if (m_sharingMissing && request.path.startsWith(sharesRoot)) {
            return reply("404 Not Found", ocs("[]", 998, "Invalid query, please check the syntax."));
        }
        if (request.method == "GET" && request.path == sharesRoot) {
            if (request.query.queryItemValue(QStringLiteral("shared_with_me")) == QLatin1String("true")) {
                return reply("200 OK", ocs("[" + receivedShare + "]"));
            }
            return reply("200 OK", ocs("[" + linkShare + "," + userShare + "]"));
        }
        if (request.method == "POST" && request.path == sharesRoot) {
            return reply("200 OK", ocs(newLinkShare));
        }
        if (request.method == "PUT" && request.path == sharesRoot + "/7") {
            return reply("200 OK", ocs(linkShare));
        }
        if (request.method == "PUT" && request.path == sharesRoot + "/8") {
            return reply("200 OK", ocs(userShare));
        }
        if (request.method == "DELETE" && (request.path == sharesRoot + "/7" || request.path == sharesRoot + "/8")) {
            return reply("200 OK", ocs("[]"));
        }
        return reply("404 Not Found", ocs("[]", 404, "Not found"));
    }

    /// A dialog listing the shares of the Photos folder.
    std::unique_ptr<NextcloudSharesDialog> photosDialog()
    {
        auto dialog = std::make_unique<NextcloudSharesDialog>(account(), &m_store, fileUrl(QStringLiteral("Photos")), true);
        return dialog;
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
                    int length = 0;
                    for (const QByteArray &line : lines.mid(1)) {
                        const int colon = line.indexOf(':');
                        if (colon > 0 && line.left(colon).trimmed().toLower() == "content-length") {
                            length = line.mid(colon + 1).trimmed().toInt();
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
        m_sharingMissing = false;
        m_api = true;
        m_enforcedExpiryDays = 0;
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("NextcloudSharesDialog"));
    }

    void menusOfferSharesOnlyWithAHandler()
    {
        QMenu menu;
        QStringList opened;
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
            NextcloudMenu::AccountHandlers{{}, [&opened](const Account &account) {
                                               opened.append(account.id);
                                           }});
        QVERIFY(!menu.findChild<QAction *>(QStringLiteral("nextcloudPhotos")));
        auto *shares = menu.findChild<QAction *>(QStringLiteral("nextcloudShares"));
        QVERIFY(shares);
        shares->trigger();
        QCOMPARE(opened, QStringList{QStringLiteral("test")});

        QMenu plain;
        NextcloudMenu::populate(&plain, {account()}, QString(), QString(), true, nullptr, this, [](const Account &, bool) { });
        QVERIFY(!plain.findChild<QAction *>(QStringLiteral("nextcloudShares")));

        NextcloudMenu::ItemHandlers handlers;
        handlers.setFavorite = [](bool) { };
        handlers.editTags = [] { };
        handlers.organize = [] { };
        QMenu several;
        NextcloudMenu::addItemMenu(&several, false, this, handlers);
        QVERIFY(!several.findChild<QAction *>(QStringLiteral("nextcloudItemShares")));
        bool asked = false;
        handlers.shares = [&asked] {
            asked = true;
        };
        QMenu one;
        NextcloudMenu::addItemMenu(&one, false, this, handlers);
        auto *item = one.findChild<QAction *>(QStringLiteral("nextcloudItemShares"));
        QVERIFY(item);
        item->trigger();
        QVERIFY(asked);
    }

    void itemSharesAndReceivedSharesAreListed()
    {
        SKIP_WITHOUT_WEBDAV();
        auto dialog = photosDialog();
        auto *scope = child<QComboBox>(*dialog, "sharesScope");
        QCOMPARE(scope->count(), 3);
        QCOMPARE(dialog->scope(), Nextcloud::ShareScope::Item);
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        QCOMPARE(photosListings(), 1);
        auto *list = child<QTreeWidget>(*dialog, "sharesList");
        QVERIFY(list->topLevelItem(0)->text(1).contains(QStringLiteral("Family")));
        QCOMPARE(list->topLevelItem(0)->text(4), QStringLiteral("Yes"));
        QVERIFY(list->topLevelItem(1)->text(1).contains(QStringLiteral("Bob")));
        // No password hash is ever shown.
        for (int column = 0; column < list->columnCount(); ++column) {
            QVERIFY(!list->topLevelItem(0)->text(column).contains(QStringLiteral("$2y$")));
        }

        QVERIFY(select(*dialog, QStringLiteral("8")));
        QVERIFY(child<QPushButton>(*dialog, "sharesEdit")->isEnabled());
        QVERIFY(child<QPushButton>(*dialog, "sharesRevoke")->isEnabled());
        QVERIFY(!child<QPushButton>(*dialog, "sharesCopyLink")->isEnabled());
        QSignalSpy shown(dialog.get(), &NextcloudSharesDialog::showRequested);
        child<QPushButton>(*dialog, "sharesShow")->click();
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.first().at(0).toUrl(), fileUrl(QStringLiteral("Photos")));
        QCOMPARE(shown.first().at(1).toBool(), true);

        // Received shares are read-only, and the scope is remembered.
        dialog->setScope(Nextcloud::ShareScope::SharedWithMe);
        QTRY_COMPARE(ids(dialog->shownShares()), QStringList{QStringLiteral("20")});
        QVERIFY(list->topLevelItem(0)->text(1).contains(QStringLiteral("Bob")));
        QVERIFY(select(*dialog, QStringLiteral("20")));
        QVERIFY(!child<QPushButton>(*dialog, "sharesEdit")->isEnabled());
        QVERIFY(!child<QPushButton>(*dialog, "sharesRevoke")->isEnabled());
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudSharesDialog")).readEntry("Scope", -1),
                 int(Nextcloud::ShareScope::SharedWithMe));

        // Without an item, the remembered scope is listed and nothing can be created.
        NextcloudSharesDialog overview(account(), &m_store);
        QCOMPARE(child<QComboBox>(overview, "sharesScope")->count(), 2);
        QCOMPARE(overview.scope(), Nextcloud::ShareScope::SharedWithMe);
        QVERIFY(child<QPushButton>(overview, "sharesCreateLink")->isHidden());
        QVERIFY(child<QPushButton>(overview, "sharesCreate")->isHidden());
    }

    void creatingAsksFirstAndNeverKeepsThePassword()
    {
        SKIP_WITHOUT_WEBDAV();
        auto dialog = photosDialog();
        QStringList questions;
        bool accept = false;
        dialog->setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            questions.append(text);
            return accept;
        });
        QList<NextcloudShares::EditorContext> contexts;
        dialog->setShareEditor([&](QWidget *, const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context) {
            contexts.append(context);
            NextcloudShares::ShareDraft chosen = draft;
            chosen.password = sharePassword;
            return std::optional(chosen);
        });
        QSignalSpy fetched(dialog->client(), &NextcloudClient::shareCapabilitiesFetched);
        QTRY_COMPARE(fetched.size(), 1);
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        auto *createLink = child<QPushButton>(*dialog, "sharesCreateLink");
        QTRY_VERIFY(createLink->isEnabled());

        // Cancelled: nothing is sent.
        createLink->click();
        QCOMPARE(contexts.size(), 1);
        QVERIFY(contexts.first().creating);
        QVERIFY(contexts.first().folder);
        QVERIFY(contexts.first().capabilities);
        QCOMPARE(questions.size(), 1);
        QVERIFY(questions.first().contains(QStringLiteral("public link")));
        QVERIFY(!questions.first().contains(sharePassword));
        QTest::qWait(100);
        QVERIFY(requests("POST").isEmpty());

        accept = true;
        createLink->click();
        QTRY_COMPARE(requests("POST").size(), 1);
        const QUrlQuery form(QString::fromUtf8(requests("POST").first().body));
        QCOMPARE(form.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded), QStringLiteral("/Photos"));
        QCOMPARE(form.queryItemValue(QStringLiteral("shareType")), QStringLiteral("3"));
        QCOMPARE(form.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded), sharePassword);
        auto *status = child<QLabel>(*dialog, "sharesStatus");
        QTRY_VERIFY(status->text().contains(QStringLiteral("New456")));
        QTRY_COMPARE(photosListings(), 2);

        // The password went to the server only.
        KSharedConfig::openStateConfig()->sync();
        QVERIFY(!configContains(KSharedConfig::openStateConfig(), sharePassword));
        QVERIFY(!configContains(KSharedConfig::openConfig(), sharePassword));
        const QList<QLineEdit *> edits = dialog->findChildren<QLineEdit *>();
        for (const QLineEdit *edit : edits) {
            QVERIFY(!edit->text().contains(sharePassword));
        }
        QVERIFY(!status->text().contains(sharePassword));
    }

    void linksStartWithTheDefaultExpiry()
    {
        SKIP_WITHOUT_WEBDAV();
        QList<NextcloudShares::ShareDraft> drafts;
        const auto editor = [&drafts](QWidget *, const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &) {
            drafts.append(draft);
            return std::optional<NextcloudShares::ShareDraft>();
        };

        // Without a default, links never expire unless the server says so.
        auto dialog = photosDialog();
        dialog->setShareEditor(editor);
        auto *createLink = child<QPushButton>(*dialog, "sharesCreateLink");
        QTRY_VERIFY(createLink->isEnabled());
        createLink->click();
        QCOMPARE(drafts.size(), 1);
        QVERIFY(!drafts.last().expiration.isValid());

        // The default of Preferences › Remote and Cloud › Nextcloud applies to links only.
        dialog->setDefaultLinkExpiryDays(10);
        createLink->click();
        QCOMPARE(drafts.size(), 2);
        QCOMPARE(drafts.last().expiration, QDate::currentDate().addDays(10));
        auto *create = child<QPushButton>(*dialog, "sharesCreate");
        QVERIFY(create->isEnabled());
        create->click();
        QCOMPARE(drafts.size(), 3);
        QVERIFY(!drafts.last().expiration.isValid());
        QTest::qWait(100);
        QVERIFY(requests("POST").isEmpty());

        // A server that enforces an earlier expiry wins, and its own default applies without one.
        m_enforcedExpiryDays = 7;
        auto enforced = photosDialog();
        enforced->setShareEditor(editor);
        QSignalSpy fetched(enforced->client(), &NextcloudClient::shareCapabilitiesFetched);
        QTRY_COMPARE(fetched.size(), 1);
        auto *enforcedLink = child<QPushButton>(*enforced, "sharesCreateLink");
        QTRY_VERIFY(enforcedLink->isEnabled());
        enforced->setDefaultLinkExpiryDays(30);
        enforcedLink->click();
        QCOMPARE(drafts.last().expiration, QDate::currentDate().addDays(7));
        enforced->setDefaultLinkExpiryDays(3);
        enforcedLink->click();
        QCOMPARE(drafts.last().expiration, QDate::currentDate().addDays(3));
        enforced->setDefaultLinkExpiryDays(0);
        enforcedLink->click();
        QCOMPARE(drafts.last().expiration, QDate::currentDate().addDays(7));
    }

    void broadeningAsksButNarrowingDoesNot()
    {
        SKIP_WITHOUT_WEBDAV();
        auto dialog = photosDialog();
        QStringList questions;
        bool accept = false;
        dialog->setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            questions.append(text);
            return accept;
        });
        Nextcloud::SharePermissions permissions = SharePermission::Read;
        dialog->setShareEditor([&](QWidget *, const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context) {
            Q_ASSERT(!context.creating);
            NextcloudShares::ShareDraft chosen = draft;
            chosen.permissions = permissions;
            return std::optional(chosen);
        });
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        QVERIFY(select(*dialog, QStringLiteral("8")));
        auto *edit = child<QPushButton>(*dialog, "sharesEdit");
        auto *status = child<QLabel>(*dialog, "sharesStatus");

        // Less access: no question.
        edit->click();
        QCOMPARE(questions.size(), 0);
        QTRY_COMPARE(requests("PUT").size(), 1);
        QCOMPARE(requests("PUT").first().path, sharesRoot + "/8");
        QCOMPARE(QUrlQuery(QString::fromUtf8(requests("PUT").first().body)).queryItemValue(QStringLiteral("permissions")), QStringLiteral("1"));
        QTRY_VERIFY(status->text().contains(QStringLiteral("Changed")));
        QTRY_VERIFY(edit->isEnabled());

        // More access: asked, and cancelled.
        permissions = Nextcloud::SharePermissions::fromInt(31);
        QVERIFY(select(*dialog, QStringLiteral("8")));
        edit->click();
        QCOMPARE(questions.size(), 1);
        QVERIFY(questions.first().contains(QStringLiteral("more access")));
        QTest::qWait(100);
        QCOMPARE(requests("PUT").size(), 1);

        accept = true;
        edit->click();
        QTRY_COMPARE(requests("PUT").size(), 2);
        QCOMPARE(QUrlQuery(QString::fromUtf8(requests("PUT").last().body)).queryItemValue(QStringLiteral("permissions")), QStringLiteral("31"));

        // Removing a link's password gives more access too.
        QTRY_COMPARE(photosListings(), 3);
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        QVERIFY(select(*dialog, QStringLiteral("7")));
        QVERIFY(edit->isEnabled());
        dialog->setShareEditor([](QWidget *, const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context) {
            Q_ASSERT(context.hadPassword);
            NextcloudShares::ShareDraft chosen = draft;
            chosen.removePassword = true;
            return std::optional(chosen);
        });
        accept = false;
        edit->click();
        QCOMPARE(questions.size(), 3);
        QVERIFY(questions.last().contains(QStringLiteral("password")));
        QTest::qWait(100);
        QCOMPARE(requests("PUT").size(), 2);
    }

    void revokingAsksFirst()
    {
        SKIP_WITHOUT_WEBDAV();
        auto dialog = photosDialog();
        QStringList questions;
        bool accept = false;
        dialog->setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            questions.append(text);
            return accept;
        });
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        QVERIFY(select(*dialog, QStringLiteral("7")));
        auto *revoke = child<QPushButton>(*dialog, "sharesRevoke");
        revoke->click();
        QCOMPARE(questions.size(), 1);
        QVERIFY(questions.first().contains(QStringLiteral("stops working")));
        QTest::qWait(100);
        QVERIFY(requests("DELETE").isEmpty());

        accept = true;
        revoke->click();
        QTRY_COMPARE(requests("DELETE").size(), 1);
        QCOMPARE(requests("DELETE").first().path, sharesRoot + "/7");
        QTRY_VERIFY(child<QLabel>(*dialog, "sharesStatus")->text().contains(QStringLiteral("Revoked")));
        // Never anything in the files themselves.
        for (const Request &request : std::as_const(m_requests)) {
            QVERIFY2(!request.path.startsWith("/remote.php/"), request.path.constData());
        }
    }

    void sendingALinkIsGatedAndReviewed()
    {
        SKIP_WITHOUT_WEBDAV();
        auto dialog = photosDialog();
        auto *email = child<QPushButton>(*dialog, "sharesEmail");
        QVERIFY(email->isHidden());
        QStringList reviews;
        bool accept = false;
        dialog->setConfirmation([&](QWidget *, const QString &, const QString &text, const QString &) {
            reviews.append(text);
            return accept;
        });
        QList<std::pair<QString, QString>> messages;
        dialog->setMailComposer([&](const QString &subject, const QString &body) {
            messages.append({subject, body});
        });
        QTRY_COMPARE(dialog->shownShares().size(), 2);
        QVERIFY(select(*dialog, QStringLiteral("7")));
        email->click();
        QVERIFY(reviews.isEmpty());

        dialog->setSendByEmailEnabled(true);
        QVERIFY(!email->isHidden());
        QVERIFY(select(*dialog, QStringLiteral("8")));
        QVERIFY(!email->isEnabled());
        QVERIFY(select(*dialog, QStringLiteral("7")));
        QVERIFY(email->isEnabled());

        email->click();
        QCOMPARE(reviews.size(), 1);
        QVERIFY(reviews.first().contains(QStringLiteral("https://cloud.example.com/s/AbCdEf123")));
        QVERIFY(reviews.first().contains(QStringLiteral("View")));
        QVERIFY(reviews.first().contains(QStringLiteral("separately")));
        QVERIFY(messages.isEmpty());

        accept = true;
        email->click();
        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.first().first.contains(QStringLiteral("Photos")));
        QVERIFY(messages.first().second.contains(QStringLiteral("https://cloud.example.com/s/AbCdEf123")));
        QVERIFY(messages.first().second.contains(QStringLiteral("separately")));
        QVERIFY(!messages.first().second.contains(QStringLiteral("$2y$")));
    }

    void unavailableSharingIsReported()
    {
        SKIP_WITHOUT_WEBDAV();
        m_sharingMissing = true;
        auto dialog = photosDialog();
        auto *status = child<QLabel>(*dialog, "sharesStatus");
        QTRY_VERIFY(!status->text().contains(QStringLiteral("Loading")));
        QVERIFY(status->text().contains(QStringLiteral("sharing")));
        QVERIFY(!child<QPushButton>(*dialog, "sharesCreateLink")->isEnabled());
        QVERIFY(!child<QPushButton>(*dialog, "sharesCreate")->isEnabled());

        m_sharingMissing = false;
        m_api = false;
        auto disabled = photosDialog();
        QTRY_VERIFY(child<QLabel>(*disabled, "sharesStatus")->text().contains(QStringLiteral("turned off")));
        QVERIFY(!child<QPushButton>(*disabled, "sharesCreate")->isEnabled());
    }

    void editorEnforcesTheServerRules()
    {
        Nextcloud::ShareCapabilities capabilities;
        capabilities.api = true;
        capabilities.publicLinks = true;
        capabilities.passwordEnforced = true;
        capabilities.expireEnforced = true;
        capabilities.expireDays = 7;
        capabilities.publicUpload = false;
        NextcloudShares::EditorContext context;
        context.folder = true;
        context.itemName = QStringLiteral("Photos");
        context.capabilities = capabilities;

        NextcloudShareEditor editor(NextcloudShares::ShareDraft{}, context);
        QVERIFY(editor.problem().contains(QStringLiteral("password")));
        // The expiry is required and limited.
        QVERIFY(child<QCheckBox>(editor, "shareExpires")->isChecked());
        QVERIFY(!child<QCheckBox>(editor, "shareExpires")->isEnabled());
        child<QLineEdit>(editor, "sharePassword")->setText(sharePassword);
        QCOMPARE(editor.problem(), QString());
        QCOMPARE(editor.draft().password, sharePassword);
        QVERIFY(editor.draft().expiration.isValid());
        QVERIFY(editor.draft().expiration <= QDate::currentDate().addDays(7));
        QCOMPARE(child<QLineEdit>(editor, "sharePassword")->echoMode(), QLineEdit::Password);
        // No uploads through links on this server.
        child<QCheckBox>(editor, "shareCreate")->setChecked(true);
        QVERIFY(editor.problem().contains(QStringLiteral("upload")));
        child<QCheckBox>(editor, "shareCreate")->setChecked(false);

        // User shares always allow viewing, need a recipient and have no password.
        auto *type = child<QComboBox>(editor, "shareType");
        type->setCurrentIndex(type->findData(int(ShareType::User)));
        QVERIFY(editor.problem().contains(QStringLiteral("user")));
        child<QLineEdit>(editor, "shareWith")->setText(QStringLiteral("bob"));
        QCOMPARE(editor.problem(), QString());
        QVERIFY(editor.draft().permissions.testFlag(SharePermission::Read));
        QVERIFY(editor.draft().password.isEmpty());
        QCOMPARE(editor.draft().shareWith, QStringLiteral("bob"));

        // An existing password can't be removed when the server requires one.
        context.creating = false;
        context.hadPassword = true;
        NextcloudShares::ShareDraft existing;
        existing.expiration = QDate::currentDate().addDays(3);
        NextcloudShareEditor change(existing, context);
        QCOMPARE(change.problem(), QString());
        QVERIFY(!child<QCheckBox>(change, "shareRemovePassword")->isEnabled());
        QVERIFY(!child<QComboBox>(change, "shareType")->isEnabled());
    }
};

QTEST_MAIN(NextcloudSharesDialogTest)

#include "nextcloudsharesdialogtest.moc"
