// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudaccountsdialog.h"
#include "folderview.h"
#include "mainwindow.h"
#include "nextcloudmenu.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KConfig>
#include <KConfigGroup>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QFile>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using NextcloudAccounts::Account;

namespace
{
const QString appPassword = QStringLiteral("dialog-app-password-42");

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

bool knownProtocol(const QString &)
{
    return true;
}

QByteArray reply(const QByteArray &status, const QByteArray &body, const QByteArray &type = "application/json")
{
    return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size())
        + "\r\nConnection: close\r\n\r\n" + body;
}

/// A WebDAV response for @p href, a folder when @p size is negative.
QByteArray davResponse(const QByteArray &href, const QByteArray &permissions, int size = -1)
{
    const QByteArray type = size < 0
        ? QByteArray("<d:resourcetype><d:collection/></d:resourcetype>")
        : "<d:resourcetype/><d:getcontentlength>" + QByteArray::number(size) + "</d:getcontentlength><d:getcontenttype>text/plain</d:getcontenttype>";
    return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop>" + type
        + "<d:getlastmodified>Tue, 03 Nov 2026 10:00:00 GMT</d:getlastmodified><oc:permissions>" + permissions
        + "</oc:permissions></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
}
}

class NextcloudAccountsDialogTest : public QObject
{
    Q_OBJECT

private:
    QTcpServer m_server;
    QTemporaryDir m_dir;
    std::unique_ptr<KConfig> m_config;
    MemoryStore m_store;
    int m_polls = 0;
    int m_revocations = 0;

    QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()));
    }

    KConfigGroup group() const
    {
        return m_config->group(NextcloudAccounts::groupName());
    }

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("opusrc"));
    }

    Account savedAccount()
    {
        QList<Account> accounts;
        Account account;
        account.name = QStringLiteral("Work");
        account.server = base();
        account.user = QStringLiteral("alice");
        account.loginName = QStringLiteral("alice@example.com");
        accounts.append(account);
        NextcloudAccounts::save(group(), accounts);
        m_store.store(accounts.first(), appPassword);
        return accounts.first();
    }

    static Account onlineAccount()
    {
        Account account;
        account.id = QStringLiteral("kaccounts-7");
        account.source = NextcloudAccounts::Source::OnlineAccounts;
        account.onlineAccountId = 7;
        account.server = QUrl(QStringLiteral("https://cloud.example.org"));
        account.user = QStringLiteral("bob");
        return account;
    }

    QByteArray respond(const QByteArray &method, const QByteArray &target, const QByteArray &authorization)
    {
        const QByteArray path = target.split('?').first();
        const bool authorized = authorization == "Basic " + QByteArray("alice@example.com:" + appPassword.toUtf8()).toBase64();
        const QByteArray ok = "\"meta\":{\"status\":\"ok\",\"statuscode\":200,\"message\":\"OK\"}";
        if (method == "GET" && path == "/ocs/v2.php/cloud/capabilities") {
            QByteArray apps = authorized ? ",\"files_sharing\":{\"api_enabled\":true},\"dav\":{\"chunking\":\"1.0\"}" : QByteArray();
            return reply("200 OK",
                         "{\"ocs\":{" + ok + ",\"data\":{\"version\":{\"major\":30,\"minor\":0,\"micro\":1,\"string\":\"30.0.1\"},\"capabilities\":{"
                             + "\"theming\":{\"name\":\"Nextcloud\"}" + apps + "}}}}");
        }
        if (method == "GET" && path == "/ocs/v2.php/cloud/user") {
            if (!authorized) {
                return reply("401 Unauthorized", "{}");
            }
            return reply("200 OK", "{\"ocs\":{" + ok + ",\"data\":{\"id\":\"alice\",\"display-name\":\"Alice\"}}}");
        }
        if (method == "POST" && path == "/index.php/login/v2") {
            const QByteArray server = base().toString().toUtf8();
            return reply("200 OK", "{\"poll\":{\"token\":\"tok\",\"endpoint\":\"" + server + "/login/v2/poll\"},\"login\":\"" + server + "/login/v2/flow/x\"}");
        }
        if (method == "POST" && path == "/login/v2/poll") {
            if (++m_polls < 2) {
                return reply("404 Not Found", "[]");
            }
            return reply("200 OK",
                         "{\"server\":\"" + base().toString().toUtf8() + "\",\"loginName\":\"alice@example.com\",\"appPassword\":\"" + appPassword.toUtf8()
                             + "\"}");
        }
        if (method == "PROPFIND" && (path == "/remote.php/dav/files/alice" || path == "/remote.php/dav/files/alice/")) {
            // A shared folder that allows neither new files nor folders, with one read-only file.
            const QByteArray folder = "/remote.php/dav/files/alice/";
            return reply("207 Multi-Status",
                         "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\">" + davResponse(folder, "SGDNV")
                             + davResponse(folder + "Locked.txt", "SG", 5) + davResponse(folder + "Open.txt", "GDNVW", 4) + "</d:multistatus>",
                         "application/xml");
        }
        if (method == "DELETE" && path == "/ocs/v2.php/core/apppassword" && authorized) {
            ++m_revocations;
            return reply("200 OK", "{\"ocs\":{" + ok + ",\"data\":[]}}");
        }
        return reply("404 Not Found", "[]");
    }

    template<typename T>
    static T *child(QWidget &dialog, const char *name)
    {
        auto *widget = dialog.findChild<T *>(QLatin1String(name));
        Q_ASSERT(widget);
        return widget;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        // Keep libaccounts away from the real account database.
        qputenv("ACCOUNTS", m_dir.path().toUtf8());
        qputenv("AG_PROVIDERS", m_dir.path().toUtf8());
        qputenv("AG_SERVICES", m_dir.path().toUtf8());
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
                    const QList<QByteArray> start = lines.first().trimmed().split(' ');
                    QByteArray authorization;
                    int length = 0;
                    for (const QByteArray &line : lines.mid(1)) {
                        const int colon = line.indexOf(':');
                        const QByteArray name = line.left(colon).trimmed().toLower();
                        if (name == "authorization") {
                            authorization = line.mid(colon + 1).trimmed();
                        } else if (name == "content-length") {
                            length = line.mid(colon + 1).trimmed().toInt();
                        }
                    }
                    if (data.size() < end + 4 + length) {
                        return;
                    }
                    socket->write(respond(start.value(0), start.value(1), authorization));
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void init()
    {
        m_config.reset();
        QFile::remove(configPath());
        m_config = std::make_unique<KConfig>(configPath(), KConfig::SimpleConfig);
        m_store.passwords.clear();
        m_polls = 0;
        m_revocations = 0;
        KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        KSharedConfig::openConfig()->sync();
    }

    void emptyStateExplainsHowToAddAnAccount()
    {
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        QCOMPARE(child<QListWidget>(dialog, "nextcloudAccounts")->count(), 0);
        QVERIFY(child<QLabel>(dialog, "nextcloudStatus")->text().contains(QStringLiteral("No Nextcloud accounts")));
        QVERIFY(!child<QPushButton>(dialog, "nextcloudOpen")->isEnabled());
        QVERIFY(!child<QPushButton>(dialog, "nextcloudCheck")->isEnabled());
        QVERIFY(!child<QPushButton>(dialog, "nextcloudRemove")->isEnabled());
        QVERIFY(!child<QPushButton>(dialog, "nextcloudSignIn")->isEnabled());

        child<QLineEdit>(dialog, "nextcloudServer")->setText(QStringLiteral("http://cloud.example.com"));
        QVERIFY(child<QPushButton>(dialog, "nextcloudSignIn")->isEnabled());
        child<QPushButton>(dialog, "nextcloudSignIn")->click();
        QVERIFY(!dialog.client()->isLoggingIn());
        QVERIFY(!child<QLabel>(dialog, "nextcloudLoginStatus")->text().isEmpty());
    }

    void signInKeepsTheAppPasswordOutOfTheSettings()
    {
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        dialog.client()->setPollInterval(50);
        QUrl opened;
        dialog.setPageOpener([&opened](const QUrl &url) {
            opened = url;
        });
        QSignalSpy changed(&dialog, &NextcloudAccountsDialog::accountsChanged);
        child<QLineEdit>(dialog, "nextcloudServer")->setText(base().toString() + QStringLiteral("/index.php/apps/files/"));
        child<QPushButton>(dialog, "nextcloudSignIn")->click();
        QVERIFY(dialog.client()->isLoggingIn());
        QVERIFY(child<QPushButton>(dialog, "nextcloudCancelSignIn")->isVisibleTo(&dialog));
        QTRY_VERIFY_WITH_TIMEOUT(!opened.isEmpty(), 20000);
        QCOMPARE(opened.path(), QStringLiteral("/login/v2/flow/x"));
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 30000);

        const QList<Account> accounts = dialog.accounts();
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(accounts.first().server, base());
        QCOMPARE(accounts.first().user, QStringLiteral("alice"));
        QVERIFY(!accounts.first().id.isEmpty());
        QCOMPARE(m_store.password(accounts.first()), std::optional<QString>(appPassword));
        QCOMPARE(child<QListWidget>(dialog, "nextcloudAccounts")->currentRow(), 0);
        QVERIFY(child<QLabel>(dialog, "nextcloudStatus")->text().contains(QStringLiteral("Signed in as")));

        m_config->sync();
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(saved.contains("alice@example.com"));
        QVERIFY(!saved.contains(appPassword.toUtf8()));

        // Signing in to the same account again updates it instead of adding another.
        m_polls = 0;
        child<QLineEdit>(dialog, "nextcloudServer")->setText(base().toString());
        child<QPushButton>(dialog, "nextcloudSignIn")->click();
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 30000);
        QCOMPARE(dialog.accounts().size(), 1);
        QCOMPARE(dialog.accounts().first().id, accounts.first().id);
    }

    void signInCanBeCancelled()
    {
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        dialog.client()->setPollInterval(60000);
        dialog.setPageOpener([](const QUrl &) { });
        child<QLineEdit>(dialog, "nextcloudServer")->setText(base().toString());
        child<QPushButton>(dialog, "nextcloudSignIn")->click();
        QVERIFY(dialog.client()->isLoggingIn());
        child<QPushButton>(dialog, "nextcloudCancelSignIn")->click();
        QVERIFY(!dialog.client()->isLoggingIn());
        QVERIFY(child<QLabel>(dialog, "nextcloudLoginStatus")->text().contains(QStringLiteral("cancelled")));
        QVERIFY(dialog.accounts().isEmpty());
    }

    void checkShowsTheServerStatus()
    {
        const Account account = savedAccount();
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        QVERIFY(child<QLabel>(dialog, "nextcloudStatus")->text().contains(QStringLiteral("Not checked yet")));
        child<QPushButton>(dialog, "nextcloudCheck")->click();
        QVERIFY(!child<QPushButton>(dialog, "nextcloudCheck")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(child<QPushButton>(dialog, "nextcloudCheck")->isEnabled(), 20000);
        const QString status = child<QLabel>(dialog, "nextcloudStatus")->text();
        QVERIFY2(status.contains(QStringLiteral("30.0.1")), qPrintable(status));
        QVERIFY2(status.contains(QStringLiteral("Signed in as")), qPrintable(status));
        QVERIFY2(status.contains(QStringLiteral("Sharing: available")), qPrintable(status));

        // A revoked app password is reported as expired access.
        m_store.store(account, QStringLiteral("revoked"));
        child<QPushButton>(dialog, "nextcloudCheck")->click();
        QTRY_VERIFY_WITH_TIMEOUT(child<QPushButton>(dialog, "nextcloudCheck")->isEnabled(), 20000);
        QVERIFY(child<QLabel>(dialog, "nextcloudStatus")->text().contains(QStringLiteral("Sign in again")));
    }

    void removeAsksAndCanRevoke()
    {
        const Account account = savedAccount();
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        QSignalSpy changed(&dialog, &NextcloudAccountsDialog::accountsChanged);
        NextcloudAccountsDialog::Removal answer = NextcloudAccountsDialog::Removal::Cancel;
        int asked = 0;
        QStringList askedAbout;
        dialog.setRemovalConfirmation([&](const Account &asking) {
            ++asked;
            askedAbout.append(asking.id);
            return answer;
        });

        child<QPushButton>(dialog, "nextcloudRemove")->click();
        QCOMPARE(asked, 1);
        QCOMPARE(askedAbout, QStringList{account.id});
        QCOMPARE(dialog.accounts().size(), 1);
        QVERIFY(m_store.password(account));
        QCOMPARE(changed.count(), 0);

        answer = NextcloudAccountsDialog::Removal::RevokeAndRemove;
        child<QPushButton>(dialog, "nextcloudRemove")->click();
        QCOMPARE(asked, 2);
        QVERIFY(dialog.accounts().isEmpty());
        QVERIFY(!m_store.password(account));
        QCOMPARE(changed.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(m_revocations, 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(dialog, "nextcloudNotice")->text().contains(QStringLiteral("revoked")), 20000);

        const Account other = savedAccount();
        dialog.setOnlineAccounts({});
        QCOMPARE(dialog.accounts().size(), 1);
        answer = NextcloudAccountsDialog::Removal::Remove;
        child<QPushButton>(dialog, "nextcloudRemove")->click();
        QVERIFY(dialog.accounts().isEmpty());
        QVERIFY(!m_store.password(other));
        QCOMPARE(askedAbout, (QStringList{account.id, account.id, other.id}));
        QVERIFY(child<QLabel>(dialog, "nextcloudNotice")->text().contains(QStringLiteral("keeps working")));
        QTest::qWait(200);
        QCOMPARE(m_revocations, 1);
    }

    void onlineAccountsAreListedButNotRemoved()
    {
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        int asked = 0;
        dialog.setRemovalConfirmation([&asked](const Account &) {
            ++asked;
            return NextcloudAccountsDialog::Removal::Remove;
        });
        dialog.setOnlineAccounts({onlineAccount()}, QStringLiteral("One account could not be read."));
        auto *list = child<QListWidget>(dialog, "nextcloudAccounts");
        QCOMPARE(list->count(), 1);
        QVERIFY(list->item(0)->text().contains(QStringLiteral("KDE Online Accounts")));
        const QString status = child<QLabel>(dialog, "nextcloudStatus")->text();
        QVERIFY(status.contains(QStringLiteral("One account could not be read.")));
        QVERIFY(status.contains(QStringLiteral("System Settings")) || status.contains(QStringLiteral("KDE Online Accounts")));
        QVERIFY(!child<QPushButton>(dialog, "nextcloudRemove")->isEnabled());
        QVERIFY(child<QPushButton>(dialog, "nextcloudCheck")->isEnabled());
        child<QPushButton>(dialog, "nextcloudRemove")->click();
        QCOMPARE(asked, 0);
        QCOMPARE(dialog.accounts().size(), 1);
    }

    void openEmitsTheSelectedAccount()
    {
        const Account account = savedAccount();
        NextcloudAccountsDialog dialog(group(), &m_store, nullptr, knownProtocol);
        dialog.setOnlineAccounts({onlineAccount()});
        dialog.selectAccount(onlineAccount().id);
        dialog.setOtherPaneAvailable(false);
        QVERIFY(!child<QPushButton>(dialog, "nextcloudOpenOther")->isEnabled());
        dialog.setOtherPaneAvailable(true);
        QSignalSpy requested(&dialog, &NextcloudAccountsDialog::openRequested);
        child<QPushButton>(dialog, "nextcloudOpenOther")->click();
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first().at(0).value<Account>().id, onlineAccount().id);
        QVERIFY(requested.first().at(1).toBool());

        dialog.selectAccount(account.id);
        child<QPushButton>(dialog, "nextcloudOpen")->click();
        QCOMPARE(requested.count(), 2);
        QCOMPARE(requested.last().at(0).value<Account>().id, account.id);
        QVERIFY(!requested.last().at(1).toBool());

        NextcloudAccountsDialog missingWorker(group(), &m_store, nullptr, [](const QString &) {
            return false;
        });
        QVERIFY(!child<QPushButton>(missingWorker, "nextcloudOpen")->isEnabled());
        QVERIFY(!child<QPushButton>(missingWorker, "nextcloudOpen")->toolTip().isEmpty());
    }

    void menuListsAccountsOrExplainsThereAreNone()
    {
        QMenu menu;
        QAction manage(QStringLiteral("Manage Nextcloud Accounts…"));
        QList<QPair<QString, bool>> opened;
        auto open = [&opened](const Account &account, bool otherPane) {
            opened.append({account.id, otherPane});
        };
        NextcloudMenu::populate(&menu, {}, QString(), QString(), true, &manage, this, open);
        QCOMPARE(menu.actions().size(), 3);
        QCOMPARE(menu.actions().first()->text(), QStringLiteral("No Nextcloud accounts"));
        QVERIFY(!menu.actions().first()->isEnabled());
        QCOMPARE(menu.actions().last(), &manage);

        Account saved;
        saved.id = QStringLiteral("abc");
        saved.server = QUrl(QStringLiteral("https://cloud.example.com"));
        saved.user = QStringLiteral("alice");
        NextcloudMenu::populate(&menu, {saved, onlineAccount()}, QStringLiteral("Broken settings"), QString(), false, &manage, this, open);
        auto *submenu = menu.findChild<QMenu *>(QStringLiteral("nextcloudAccount-abc"));
        QVERIFY(submenu);
        QCOMPARE(submenu->menuAction()->toolTip(), QStringLiteral("webdavs://alice@cloud.example.com/remote.php/dav/files/alice/"));
        QCOMPARE(submenu->actions().size(), 2);
        QVERIFY(!submenu->actions().at(1)->isEnabled());
        submenu->actions().first()->trigger();
        QCOMPARE(opened, (QList<QPair<QString, bool>>{{QStringLiteral("abc"), false}}));
        QVERIFY(menu.findChild<QMenu *>(QStringLiteral("nextcloudAccount-kaccounts-7")));
        bool problemShown = false;
        for (QAction *action : menu.actions()) {
            problemShown = problemShown || action->text() == QStringLiteral("Broken settings");
        }
        QVERIFY(problemShown);

        NextcloudMenu::populate(&menu, {saved}, QString(), QStringLiteral("Install kio-extras."), true, nullptr, this, open);
        QCOMPARE(menu.actions().size(), 1);
        QVERIFY(!menu.actions().first()->isEnabled());
        QCOMPARE(menu.actions().first()->toolTip(), QStringLiteral("Install kio-extras."));
    }

    void goMenuOffersNextcloud()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        MainWindow window;
        auto *nextcloud = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("nextcloud")));
        QVERIFY(nextcloud);
        QAction *manage = window.actionCollection()->action(QStringLiteral("manage_nextcloud_accounts"));
        QVERIFY(manage);
        auto *goMenu = window.findChild<QMenu *>(QStringLiteral("go"));
        QVERIFY(goMenu);
        QVERIFY(goMenu->actions().contains(nextcloud));

        // Without accounts the menu only explains how to add one.
        Q_EMIT nextcloud->menu()->aboutToShow();
        QList<QAction *> items = nextcloud->menu()->actions();
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.first()->text(), QStringLiteral("No Nextcloud accounts"));
        QCOMPARE(items.last(), manage);

        QList<Account> accounts;
        Account account;
        account.server = QUrl(QStringLiteral("https://cloud.example.com"));
        account.user = QStringLiteral("alice");
        accounts.append(account);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        Q_EMIT nextcloud->menu()->aboutToShow();
        QVERIFY(nextcloud->menu()->findChild<QMenu *>(QStringLiteral("nextcloudAccount-") + accounts.first().id));

        manage->trigger();
        auto *dialog = window.findChild<NextcloudAccountsDialog *>();
        QVERIFY(dialog);
        QCOMPARE(dialog->accounts().size(), 1);
        dialog->close();
    }

    void browsingRespectsServerPermissions()
    {
        if (!KProtocolInfo::isKnownProtocol(QStringLiteral("webdav"))) {
            QSKIP("The KIO WebDAV worker isn't installed.");
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            KSharedConfig::openConfig()->deleteGroup(NextcloudAccounts::groupName());
        });
        QList<Account> accounts;
        Account account;
        account.server = base();
        account.user = QStringLiteral("alice");
        accounts.append(account);
        QVERIFY(NextcloudAccounts::save(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName()), accounts));
        account = NextcloudAccounts::load(KSharedConfig::openConfig()->group(NextcloudAccounts::groupName())).value(0);
        QVERIFY(!account.id.isEmpty());

        MainWindow window;
        window.openNextcloudAccount(account, false);
        const QUrl folder = NextcloudAccounts::filesUrl(account);
        QCOMPARE(window.activePane()->url().adjusted(QUrl::StripTrailingSlash), folder.adjusted(QUrl::StripTrailingSlash));
        QTRY_VERIFY_WITH_TIMEOUT(window.nextcloudPermissions(folder).has_value(), 20000);

        KActionCollection *actions = window.actionCollection();
        QAction *rename = actions->action(QStringLiteral("rename"));
        QAction *remove = actions->action(QStringLiteral("delete"));
        QAction *newFolder = actions->action(QStringLiteral("new_folder"));
        QVERIFY(!newFolder->isEnabled());
        QVERIFY(newFolder->toolTip().contains(QStringLiteral("Nextcloud server")));

        FolderView *view = window.activePane()->view();
        view->selectUrls({folder.resolved(QUrl(QStringLiteral("Locked.txt")))});
        QTRY_VERIFY(!rename->isEnabled());
        QVERIFY(!remove->isEnabled());
        QVERIFY(rename->toolTip().contains(QStringLiteral("renaming")));
        QVERIFY(remove->toolTip().contains(QStringLiteral("deleting")));

        view->selectUrls({folder.resolved(QUrl(QStringLiteral("Open.txt")))});
        QTRY_VERIFY(rename->isEnabled());
        QVERIFY(remove->isEnabled());
        QVERIFY(!rename->toolTip().contains(QStringLiteral("renaming")));
    }
};

QTEST_MAIN(NextcloudAccountsDialogTest)

#include "nextcloudaccountsdialogtest.moc"
