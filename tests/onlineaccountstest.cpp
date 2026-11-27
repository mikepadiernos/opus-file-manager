// SPDX-License-Identifier: GPL-3.0-or-later

#include "onlineaccounts.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#ifdef OPUSFM_HAVE_ACCOUNTS
#include <Accounts/Account>
#include <Accounts/Manager>
#include <Accounts/Service>
#endif

using namespace OnlineAccounts;

namespace
{
bool allProtocols(const QString &)
{
    return true;
}

bool noGdrive(const QString &protocol)
{
    return protocol != QLatin1String("gdrive");
}

Account googleAccount()
{
    Account account;
    account.id = 1;
    account.displayName = QStringLiteral("someone@gmail.com");
    account.provider = QStringLiteral("google");
    account.providerDisplayName = QStringLiteral("Google");
    account.iconName = QStringLiteral("im-google");
    account.enabled = true;
    account.services = {
        {QStringLiteral("google-youtube"), QStringLiteral("google-youtube"), QStringLiteral("YouTube"), {}, true, {}, {}},
        {QStringLiteral("google-drive"), QStringLiteral("google-drive"), QStringLiteral("Drive"), QStringLiteral("folder-gdrive"), true, {}, {}},
    };
    return account;
}

Account nextcloudAccount()
{
    Account account;
    account.id = 2;
    account.displayName = QStringLiteral("alex");
    account.provider = QStringLiteral("nextcloud");
    account.providerDisplayName = QStringLiteral("Nextcloud");
    account.iconName = QStringLiteral("kaccounts-nextcloud");
    account.enabled = true;
    account.services = {
        {QStringLiteral("nextcloud-contacts"), QStringLiteral("dav-contacts"), QStringLiteral("Contacts"), {}, true, {}, {}},
        {QStringLiteral("nextcloud-storage"),
         QStringLiteral("dav-storage"),
         QStringLiteral("Storage"),
         QStringLiteral("kaccounts-nextcloud"),
         true,
         QStringLiteral("cloud.example.org"),
         QStringLiteral("/remote.php/dav/files/alex")},
    };
    return account;
}
}

class OnlineAccountsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fileServices()
    {
        QVERIFY(isFileService(QStringLiteral("google-drive")));
        QVERIFY(isFileService(QStringLiteral("dav-storage")));
        QVERIFY(!isFileService(QStringLiteral("dav-contacts")));
        QVERIFY(!isFileService(QStringLiteral("google-youtube")));
        QVERIFY(!isFileService(QString()));
    }

    void davUrls_data()
    {
        QTest::addColumn<QString>("host");
        QTest::addColumn<QString>("storagePath");
        QTest::addColumn<QString>("accountName");
        QTest::addColumn<QString>("expected");

        QTest::newRow("bare host") << QStringLiteral("cloud.example.org") << QStringLiteral("/remote.php/dav/files/alex") << QStringLiteral("alex")
                                   << QStringLiteral("webdavs://alex@cloud.example.org/remote.php/dav/files/alex/");
        QTest::newRow("user from path wins") << QStringLiteral("cloud.example.org") << QStringLiteral("/remote.php/dav/files/alex/")
                                             << QStringLiteral("Alex Example") << QStringLiteral("webdavs://alex@cloud.example.org/remote.php/dav/files/alex/");
        QTest::newRow("https with port") << QStringLiteral("https://cloud.example.org:8443") << QStringLiteral("remote.php/webdav")
                                         << QStringLiteral("alex@cloud.example.org")
                                         << QStringLiteral("webdavs://alex@cloud.example.org:8443/remote.php/webdav/");
        QTest::newRow("plain http") << QStringLiteral("http://nas.local") << QStringLiteral("/remote.php/webdav") << QStringLiteral("alex")
                                    << QStringLiteral("webdav://alex@nas.local/remote.php/webdav/");
        QTest::newRow("subfolder install") << QStringLiteral("https://example.org/nextcloud/") << QStringLiteral("/remote.php/dav/files/alex")
                                           << QStringLiteral("alex") << QStringLiteral("webdavs://alex@example.org/nextcloud/remote.php/dav/files/alex/");
        QTest::newRow("subfolder already in path") << QStringLiteral("https://example.org/nextcloud") << QStringLiteral("/nextcloud/remote.php/dav/files/alex")
                                                   << QStringLiteral("alex")
                                                   << QStringLiteral("webdavs://alex@example.org/nextcloud/remote.php/dav/files/alex/");
        QTest::newRow("email user") << QStringLiteral("cloud.example.org") << QStringLiteral("/remote.php/dav/files/a@b.org") << QString()
                                    << QStringLiteral("webdavs://a%40b.org@cloud.example.org/remote.php/dav/files/a@b.org/");
        QTest::newRow("no user") << QStringLiteral("cloud.example.org") << QString() << QString() << QStringLiteral("webdavs://cloud.example.org/");
        QTest::newRow("credentials dropped") << QStringLiteral("https://bob:secret@cloud.example.org") << QString() << QStringLiteral("alex")
                                             << QStringLiteral("webdavs://alex@cloud.example.org/");
        QTest::newRow("empty host") << QString() << QStringLiteral("/remote.php/webdav") << QStringLiteral("alex") << QString();
        QTest::newRow("unsupported scheme") << QStringLiteral("ftp://cloud.example.org") << QString() << QStringLiteral("alex") << QString();
    }

    void davUrls()
    {
        QFETCH(QString, host);
        QFETCH(QString, storagePath);
        QFETCH(QString, accountName);
        QFETCH(QString, expected);

        const QUrl url = davUrl(host, storagePath, accountName);
        QCOMPARE(url.toString(QUrl::FullyEncoded), expected);
        QVERIFY(url.password().isEmpty());
    }

    void googleDrive()
    {
        const QList<Location> result = locations(googleAccount(), allProtocols);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().title, QStringLiteral("Drive"));
        QCOMPARE(result.first().iconName, QStringLiteral("folder-gdrive"));
        QCOMPARE(result.first().url, QUrl(QStringLiteral("gdrive:/someone@gmail.com/")));
        QVERIFY(result.first().isAvailable());
    }

    void googleDriveNeedsKioGdrive()
    {
        const QList<Location> result = locations(googleAccount(), noGdrive);
        QCOMPARE(result.size(), 1);
        QVERIFY(!result.first().isAvailable());
        QVERIFY(result.first().url.isEmpty());
        QVERIFY(result.first().unavailableReason.contains(QLatin1String("kio-gdrive")));
    }

    void nextcloudStorage()
    {
        const QList<Location> result = locations(nextcloudAccount(), allProtocols);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().title, QStringLiteral("Storage"));
        QCOMPARE(result.first().url, QUrl(QStringLiteral("webdavs://alex@cloud.example.org/remote.php/dav/files/alex/")));
        QVERIFY(result.first().isAvailable());
    }

    void missingWebDavWorker()
    {
        const QList<Location> result = locations(nextcloudAccount(), [](const QString &protocol) {
            return protocol != QLatin1String("webdavs");
        });
        QCOMPARE(result.size(), 1);
        QVERIFY(!result.first().isAvailable());
        QVERIFY(!result.first().unavailableReason.isEmpty());
    }

    void missingHost()
    {
        Account account = nextcloudAccount();
        account.services[1].host.clear();
        const QList<Location> result = locations(account, allProtocols);
        QCOMPARE(result.size(), 1);
        QVERIFY(!result.first().isAvailable());
        QVERIFY(result.first().url.isEmpty());
    }

    void disabledAccountOrService()
    {
        Account account = nextcloudAccount();
        account.services[1].enabled = false;
        QList<Location> result = locations(account, allProtocols);
        QCOMPARE(result.size(), 1);
        QVERIFY(!result.first().isAvailable());
        const QString serviceReason = result.first().unavailableReason;

        account.enabled = false;
        result = locations(account, allProtocols);
        QVERIFY(!result.first().isAvailable());
        QVERIFY(!result.first().unavailableReason.isEmpty());
        QVERIFY(result.first().unavailableReason != serviceReason);
        QVERIFY(result.first().url.isEmpty());
    }

    void noFileService()
    {
        Account account = googleAccount();
        account.services.removeLast();
        const QList<Location> result = locations(account, allProtocols);
        QCOMPARE(result.size(), 1);
        QVERIFY(!result.first().isAvailable());
        QCOMPARE(result.first().iconName, QStringLiteral("im-google"));
        QVERIFY(!result.first().unavailableReason.isEmpty());
    }

    void severalFileServices()
    {
        Account account = nextcloudAccount();
        Service second = account.services[1];
        second.name = QStringLiteral("owncloud-storage");
        second.displayName = QString();
        second.iconName = QString();
        second.storagePath = QStringLiteral("/remote.php/webdav");
        account.services.append(second);
        const QList<Location> result = locations(account, allProtocols);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result.at(1).title, QStringLiteral("owncloud-storage"));
        QCOMPARE(result.at(1).iconName, QStringLiteral("kaccounts-nextcloud"));
        QCOMPARE(result.at(1).url, QUrl(QStringLiteral("webdavs://alex@cloud.example.org/remote.php/webdav/")));
    }

    void showsOnlyAccountsWithFileStorageThatAreNotHidden()
    {
        Account contactsOnly = googleAccount();
        contactsOnly.id = 3;
        contactsOnly.services.removeLast();
        QVERIFY(hasFileStorage(googleAccount()));
        QVERIFY(hasFileStorage(nextcloudAccount()));
        QVERIFY(!hasFileStorage(contactsOnly));

        // Unavailable storage is still storage that can be shown.
        Account turnedOff = nextcloudAccount();
        turnedOff.enabled = false;
        QVERIFY(hasFileStorage(turnedOff));

        QCOMPARE(settingsId(nextcloudAccount()), QStringLiteral("2"));
        const QList<Account> all{googleAccount(), contactsOnly, nextcloudAccount()};
        QCOMPARE(shownAccounts(all, {}), (QList<Account>{googleAccount(), nextcloudAccount()}));
        QCOMPARE(shownAccounts(all, {QStringLiteral("1"), QStringLiteral("42")}), QList<Account>{nextcloudAccount()});
        QVERIFY(shownAccounts(all, {QStringLiteral("1"), QStringLiteral("2")}).isEmpty());
    }

    void unsupportedBuildExplains()
    {
        if (isSupported()) {
            QSKIP("Built with KDE Online Accounts support");
        }
        QString error = QStringLiteral("stale");
        QVERIFY(readAccounts(&error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(error != QLatin1String("stale"));
    }

    void readsAccountDatabase()
    {
#ifndef OPUSFM_HAVE_ACCOUNTS
        QSKIP("Built without KDE Online Accounts support");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto write = [&dir](const QString &name, const QByteArray &content) {
            QFile file(dir.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
        };
        QVERIFY(write(QStringLiteral("nextcloud.provider"), "<?xml version=\"1.0\"?><provider id=\"nextcloud\"><name>Nextcloud</name></provider>"));
        QVERIFY(write(QStringLiteral("google.provider"), "<?xml version=\"1.0\"?><provider id=\"google\"><name>Google</name></provider>"));
        QVERIFY(write(QStringLiteral("nextcloud-storage.service"),
                      "<?xml version=\"1.0\"?><service id=\"nextcloud-storage\"><type>dav-storage</type><name>Storage</name>"
                      "<provider>nextcloud</provider></service>"));
        QVERIFY(write(QStringLiteral("google-drive.service"),
                      "<?xml version=\"1.0\"?><service id=\"google-drive\"><type>google-drive</type><name>Drive</name>"
                      "<icon>folder-gdrive</icon><provider>google</provider></service>"));
        qputenv("ACCOUNTS", dir.path().toUtf8());
        qputenv("AG_PROVIDERS", dir.path().toUtf8());
        qputenv("AG_SERVICES", dir.path().toUtf8());

        {
            Accounts::Manager manager(Accounts::Manager::DisableNotifications);
            Accounts::Account *nextcloud = manager.createAccount(QStringLiteral("nextcloud"));
            QVERIFY(nextcloud);
            nextcloud->setDisplayName(QStringLiteral("alex"));
            nextcloud->setEnabled(true);
            nextcloud->setValue(QStringLiteral("dav/host"), QStringLiteral("cloud.example.org"));
            nextcloud->setValue(QStringLiteral("dav/storagePath"), QStringLiteral("/remote.php/dav/files/alex"));
            nextcloud->selectService(manager.service(QStringLiteral("nextcloud-storage")));
            nextcloud->setEnabled(true);
            nextcloud->selectService();
            QVERIFY(nextcloud->syncAndBlock());

            Accounts::Account *google = manager.createAccount(QStringLiteral("google"));
            QVERIFY(google);
            google->setDisplayName(QStringLiteral("someone@gmail.com"));
            google->setEnabled(false);
            QVERIFY(google->syncAndBlock());
        }

        QString error = QStringLiteral("stale");
        const QList<Account> accounts = readAccounts(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(accounts.size(), 2);

        const Account &google = accounts.at(0);
        QCOMPARE(google.displayName, QStringLiteral("someone@gmail.com"));
        QCOMPARE(google.provider, QStringLiteral("google"));
        QCOMPARE(google.providerDisplayName, QStringLiteral("Google"));
        QVERIFY(!google.enabled);
        QCOMPARE(google.services.size(), 1);
        QCOMPARE(google.services.first().type, QStringLiteral("google-drive"));
        QCOMPARE(google.services.first().iconName, QStringLiteral("folder-gdrive"));
        QVERIFY(!locations(google, allProtocols).first().isAvailable());

        const Account &nextcloud = accounts.at(1);
        QCOMPARE(nextcloud.displayName, QStringLiteral("alex"));
        QVERIFY(nextcloud.enabled);
        QCOMPARE(nextcloud.services.size(), 1);
        const Service &storage = nextcloud.services.first();
        QCOMPARE(storage.type, QStringLiteral("dav-storage"));
        QCOMPARE(storage.displayName, QStringLiteral("Storage"));
        QVERIFY(storage.enabled);
        QCOMPARE(storage.host, QStringLiteral("cloud.example.org"));
        QCOMPARE(storage.storagePath, QStringLiteral("/remote.php/dav/files/alex"));
        QCOMPARE(locations(nextcloud, allProtocols).first().url, QUrl(QStringLiteral("webdavs://alex@cloud.example.org/remote.php/dav/files/alex/")));
#endif
    }
};

QTEST_GUILESS_MAIN(OnlineAccountsTest)
#include "onlineaccountstest.moc"
