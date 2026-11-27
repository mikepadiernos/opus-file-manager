// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudaccount.h"

#include <KConfig>
#include <KConfigGroup>

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using NextcloudAccounts::Account;

class NextcloudAccountTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static Account account()
    {
        Account account;
        account.server = QUrl(QStringLiteral("https://cloud.example.com/nextcloud"));
        account.user = QStringLiteral("alice");
        account.loginName = QStringLiteral("alice@example.com");
        return account;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        QFile::remove(m_dir.filePath(QStringLiteral("accountsrc")));
    }

    void serverAddressesAreNormalized_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");
        QTest::newRow("host") << QStringLiteral(" cloud.example.com ") << QStringLiteral("https://cloud.example.com");
        QTest::newRow("trailing slash") << QStringLiteral("https://Cloud.Example.com/") << QStringLiteral("https://cloud.example.com");
        QTest::newRow("port and base") << QStringLiteral("cloud.example.com:8443/nextcloud/") << QStringLiteral("https://cloud.example.com:8443/nextcloud");
        QTest::newRow("web link") << QStringLiteral("https://example.com/nc/index.php/apps/files/?dir=/") << QStringLiteral("https://example.com/nc");
        QTest::newRow("dav link") << QStringLiteral("https://example.com/remote.php/dav/files/alice/") << QStringLiteral("https://example.com");
        QTest::newRow("loopback http") << QStringLiteral("http://127.0.0.1:8080") << QStringLiteral("http://127.0.0.1:8080");
        QTest::newRow("localhost http") << QStringLiteral("http://localhost/") << QStringLiteral("http://localhost");
    }

    void serverAddressesAreNormalized()
    {
        QFETCH(QString, text);
        QFETCH(QString, expected);
        QString error;
        QCOMPARE(NextcloudAccounts::serverUrl(text, &error).toString(), expected);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void unsafeServerAddressesAreRefused_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("message");
        QTest::newRow("empty") << QString() << QStringLiteral("Enter");
        QTest::newRow("plain http") << QStringLiteral("http://cloud.example.com") << QStringLiteral("https://");
        QTest::newRow("credentials") << QStringLiteral("https://alice:secret@cloud.example.com") << QStringLiteral("password");
        QTest::newRow("other scheme") << QStringLiteral("ftp://cloud.example.com") << QStringLiteral("https://");
    }

    void unsafeServerAddressesAreRefused()
    {
        QFETCH(QString, text);
        QFETCH(QString, message);
        QString error;
        QVERIFY(NextcloudAccounts::serverUrl(text, &error).isEmpty());
        QVERIFY2(error.contains(message), qPrintable(error));
    }

    void urlsUseTheLoginNameAndUserFolder()
    {
        const Account nextcloud = account();
        const QUrl files = NextcloudAccounts::filesUrl(nextcloud);
        QCOMPARE(files.scheme(), QStringLiteral("webdavs"));
        QCOMPARE(files.host(), QStringLiteral("cloud.example.com"));
        QCOMPARE(files.userName(), QStringLiteral("alice@example.com"));
        QVERIFY(files.password().isEmpty());
        QCOMPARE(files.path(), QStringLiteral("/nextcloud/remote.php/dav/files/alice/"));
        QCOMPARE(NextcloudAccounts::davUrl(nextcloud, QStringLiteral("photos/alice/")).path(), QStringLiteral("/nextcloud/remote.php/dav/photos/alice/"));
        QCOMPARE(NextcloudAccounts::webUrl(nextcloud, QStringLiteral("ocs/v2.php/cloud/user")).toString(),
                 QStringLiteral("https://cloud.example.com/nextcloud/ocs/v2.php/cloud/user"));
        QCOMPARE(NextcloudAccounts::title(nextcloud), QStringLiteral("alice@example.com on cloud.example.com"));
        QCOMPARE(NextcloudAccounts::realm(nextcloud), QStringLiteral("Nextcloud"));

        Account local = nextcloud;
        local.server = QUrl(QStringLiteral("http://127.0.0.1:8080"));
        QCOMPARE(NextcloudAccounts::filesUrl(local).scheme(), QStringLiteral("webdav"));
    }

    void urlsAreMatchedToTheirAccount()
    {
        Account first = account();
        first.id = QStringLiteral("first");
        Account second = account();
        second.id = QStringLiteral("second");
        second.user = QStringLiteral("bob");
        const QList<Account> accounts{first, second};
        QCOMPARE(NextcloudAccounts::accountForUrl(accounts, QUrl(QStringLiteral("webdavs://cloud.example.com/nextcloud/remote.php/dav/files/bob/Docs")))->id,
                 QStringLiteral("second"));
        QCOMPARE(NextcloudAccounts::accountForUrl(accounts, QUrl(QStringLiteral("webdavs://x@CLOUD.example.com/nextcloud/remote.php/dav/files/alice")))->id,
                 QStringLiteral("first"));
        QVERIFY(!NextcloudAccounts::accountForUrl(accounts, QUrl(QStringLiteral("webdavs://cloud.example.com/nextcloud/remote.php/dav/files/alicia/"))));
        QVERIFY(!NextcloudAccounts::accountForUrl(accounts, QUrl(QStringLiteral("webdav://cloud.example.com/nextcloud/remote.php/dav/files/alice/"))));
        QVERIFY(!NextcloudAccounts::accountForUrl(accounts, QUrl::fromLocalFile(QStringLiteral("/nextcloud/remote.php/dav/files/alice/"))));
    }

    void onlineAccountsProvideNextcloudStorage()
    {
        OnlineAccounts::Service storage;
        storage.type = QStringLiteral("dav-storage");
        storage.enabled = true;
        storage.host = QStringLiteral("cloud.example.com/nc");
        storage.storagePath = QStringLiteral("/remote.php/dav/files/alice");
        OnlineAccounts::Account nextcloud;
        nextcloud.id = 7;
        nextcloud.displayName = QStringLiteral("alice");
        nextcloud.provider = QStringLiteral("nextcloud");
        nextcloud.enabled = true;
        nextcloud.services = {storage};
        OnlineAccounts::Account google = nextcloud;
        google.id = 8;
        google.provider = QStringLiteral("google");
        OnlineAccounts::Account disabled = nextcloud;
        disabled.id = 9;
        disabled.enabled = false;

        const QList<Account> accounts = NextcloudAccounts::fromOnlineAccounts({google, nextcloud, disabled});
        QCOMPARE(accounts.size(), 1);
        const Account &account = accounts.first();
        QCOMPARE(account.id, QStringLiteral("kaccounts-7"));
        QCOMPARE(account.source, NextcloudAccounts::Source::OnlineAccounts);
        QCOMPARE(account.onlineAccountId, 7u);
        QCOMPARE(account.server.toString(), QStringLiteral("https://cloud.example.com/nc"));
        QCOMPARE(account.user, QStringLiteral("alice"));
        QCOMPARE(NextcloudAccounts::filesUrl(account).toString(), QStringLiteral("webdavs://alice@cloud.example.com/nc/remote.php/dav/files/alice/"));

        Account saved = account;
        saved.id = QStringLiteral("saved");
        saved.source = NextcloudAccounts::Source::Login;
        QCOMPARE(NextcloudAccounts::merged({saved}, accounts).size(), 1);
        QCOMPARE(NextcloudAccounts::merged({}, accounts).size(), 1);
    }

    void accountsAreSavedWithoutCredentials()
    {
        KConfig config(m_dir.filePath(QStringLiteral("accountsrc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(NextcloudAccounts::groupName());
        Account first = account();
        first.name = QStringLiteral("Work");
        first.realm = QStringLiteral("Example Cloud");
        Account second = account();
        second.user = QStringLiteral("bob");
        second.loginName.clear();
        Account online = account();
        online.id = QStringLiteral("kaccounts-3");
        online.source = NextcloudAccounts::Source::OnlineAccounts;
        QList<Account> accounts{first, second, online};
        QString error;
        QVERIFY2(NextcloudAccounts::save(group, accounts, &error), qPrintable(error));
        QVERIFY(!accounts.at(0).id.isEmpty());
        QCOMPARE(accounts.at(2).id, QStringLiteral("kaccounts-3"));

        const QList<Account> loaded = NextcloudAccounts::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded, (QList<Account>{accounts.at(0), accounts.at(1)}));

        QFile file(m_dir.filePath(QStringLiteral("accountsrc")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray contents = file.readAll().toLower();
        QVERIFY(!contents.contains("password"));
        QVERIFY(!contents.contains("token"));

        QList<Account> duplicate{first, first};
        QVERIFY(!NextcloudAccounts::save(group, duplicate, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(NextcloudAccounts::load(group).size(), 2);

        Account insecure = account();
        insecure.server = QUrl(QStringLiteral("http://cloud.example.com"));
        QList<Account> refused{insecure};
        QVERIFY(!NextcloudAccounts::save(group, refused, &error));
        QVERIFY(error.contains(QStringLiteral("https://")));
    }

    void invalidSavedAccountsAreSkipped()
    {
        KConfig config(m_dir.filePath(QStringLiteral("accountsrc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(NextcloudAccounts::groupName());
        QList<Account> accounts{account()};
        QVERIFY(NextcloudAccounts::save(group, accounts));
        KConfigGroup broken = group.group(QStringLiteral("not-a-uuid"));
        broken.writeEntry("Server", QStringLiteral("https://cloud.example.com"));
        broken.writeEntry("User", QStringLiteral("carol"));
        KConfigGroup plain = group.group(QStringLiteral("9b2f7c51-0f5d-4b6a-9d61-111111111111"));
        plain.writeEntry("Server", QStringLiteral("http://cloud.example.com"));
        plain.writeEntry("User", QStringLiteral("dave"));
        QString error;
        const QList<Account> loaded = NextcloudAccounts::load(group, &error);
        QCOMPARE(loaded.size(), 1);
        QVERIFY(error.contains(QStringLiteral("2")));
    }
};

QTEST_GUILESS_MAIN(NextcloudAccountTest)

#include "nextcloudaccounttest.moc"
