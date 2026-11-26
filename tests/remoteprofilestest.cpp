// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofiles.h"

#include <KSharedConfig>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using RemoteProfiles::Authentication;
using RemoteProfiles::Profile;
using RemoteProfiles::Protocol;

namespace
{
Profile sftpProfile()
{
    Profile profile;
    profile.name = QStringLiteral("Build server");
    profile.protocol = Protocol::Sftp;
    profile.host = QStringLiteral("build.example.org");
    profile.port = 2222;
    profile.user = QStringLiteral("deploy");
    profile.path = QStringLiteral("/srv/www");
    profile.authentication = Authentication::KeyOrAgent;
    profile.identityFile = QStringLiteral("/home/user/.ssh/id_ed25519");
    return profile;
}
}

class RemoteProfilesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void buildsUrlsWithoutPasswords()
    {
        Profile profile = sftpProfile();
        QCOMPARE(RemoteProfiles::url(profile), QUrl(QStringLiteral("sftp://deploy@build.example.org:2222/srv/www")));
        QVERIFY(RemoteProfiles::url(profile).password().isEmpty());

        profile.path.clear();
        profile.port = 0;
        profile.user.clear();
        QCOMPARE(RemoteProfiles::url(profile), QUrl(QStringLiteral("sftp://build.example.org")));

        profile.protocol = Protocol::WebDavs;
        profile.authentication = Authentication::Password;
        QCOMPARE(RemoteProfiles::url(profile), QUrl(QStringLiteral("webdavs://build.example.org/")));

        profile.host = QStringLiteral("::1");
        profile.path = QStringLiteral("/dav//files/");
        QCOMPARE(RemoteProfiles::url(profile), QUrl(QStringLiteral("webdavs://[::1]/dav/files")));
    }

    void buildsSmbAndFtpUrls()
    {
        Profile profile;
        profile.name = QStringLiteral("NAS");
        profile.protocol = Protocol::Smb;
        profile.host = QStringLiteral("nas.local");
        profile.user = QStringLiteral("WORKGROUP;me");
        profile.path = QStringLiteral("/photos");
        QVERIFY(RemoteProfiles::validationError(profile).isEmpty());
        QCOMPARE(RemoteProfiles::url(profile).scheme(), QStringLiteral("smb"));
        QCOMPARE(RemoteProfiles::url(profile).userName(), QStringLiteral("WORKGROUP;me"));
        QCOMPARE(RemoteProfiles::url(profile).path(), QStringLiteral("/photos"));
        profile.path.clear();
        QCOMPARE(RemoteProfiles::url(profile).path(), QStringLiteral("/"));

        profile.protocol = Protocol::Ftp;
        profile.user = QStringLiteral("anonymous");
        QCOMPARE(RemoteProfiles::url(profile), QUrl(QStringLiteral("ftp://anonymous@nas.local")));
        QVERIFY(!RemoteProfiles::securityWarning(profile).isEmpty());
        QVERIFY(!RemoteProfiles::supportsSsh(Protocol::Ftp));
        profile.protocol = Protocol::Ftps;
        QCOMPARE(RemoteProfiles::url(profile).scheme(), QStringLiteral("ftps"));
        QVERIFY(RemoteProfiles::securityWarning(profile).isEmpty());
        QCOMPARE(RemoteProfiles::defaultPort(Protocol::Smb), 445);
        QCOMPARE(RemoteProfiles::defaultPort(Protocol::Ftps), 21);
        QCOMPARE(RemoteProfiles::protocols().size(), 6);
    }

    void findsTheNetworkLocation()
    {
        QCOMPARE(RemoteProfiles::networkUrl([](const QString &) {
                     return true;
                 }),
                 QUrl(QStringLiteral("network:/")));
        QCOMPARE(RemoteProfiles::networkUrl([](const QString &scheme) {
                     return scheme == QLatin1String("remote");
                 }),
                 QUrl(QStringLiteral("remote:/")));
        QVERIFY(RemoteProfiles::networkUrl([](const QString &) {
                    return false;
                }).isEmpty());
    }

    void buildsSshArguments()
    {
        const Profile profile = sftpProfile();
        QCOMPARE(RemoteProfiles::sshArguments(profile),
                 (QStringList{QStringLiteral("-p"),
                              QStringLiteral("2222"),
                              QStringLiteral("-l"),
                              QStringLiteral("deploy"),
                              QStringLiteral("-i"),
                              QStringLiteral("/home/user/.ssh/id_ed25519"),
                              QStringLiteral("--"),
                              QStringLiteral("build.example.org")}));

        Profile minimal;
        minimal.name = QStringLiteral("Local");
        minimal.host = QStringLiteral("[::1]");
        minimal.identityFile = QStringLiteral("/ignored/without/key/sign-in");
        QCOMPARE(RemoteProfiles::sshArguments(minimal), (QStringList{QStringLiteral("--"), QStringLiteral("::1")}));
        QVERIFY(RemoteProfiles::supportsSsh(Protocol::Sftp));
        QVERIFY(!RemoteProfiles::supportsSsh(Protocol::WebDavs));
    }

    void validatesProfiles_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("valid");

        QTest::newRow("name empty") << "name" << "  " << false;
        QTest::newRow("name newline") << "name" << "a\nb" << false;
        QTest::newRow("host empty") << "host" << "" << false;
        QTest::newRow("host option") << "host" << "-oProxyCommand=x" << false;
        QTest::newRow("host space") << "host" << "a b" << false;
        QTest::newRow("host slash") << "host" << "a/b" << false;
        QTest::newRow("host user") << "host" << "me@a" << false;
        QTest::newRow("host ipv4") << "host" << "192.0.2.4" << true;
        QTest::newRow("host ipv6") << "host" << "[2001:db8::1]" << true;
        QTest::newRow("user option") << "user" << "-x" << false;
        QTest::newRow("user colon") << "user" << "a:b" << false;
        QTest::newRow("user empty") << "user" << "" << true;
        QTest::newRow("user domain") << "user" << "DOMAIN;me" << true;
        QTest::newRow("path relative") << "path" << "srv" << false;
        QTest::newRow("path empty") << "path" << "" << true;
        QTest::newRow("identity relative") << "identity" << "id_rsa" << false;
        QTest::newRow("identity empty") << "identity" << "" << true;
    }

    void validatesProfiles()
    {
        QFETCH(QString, field);
        QFETCH(QString, value);
        QFETCH(bool, valid);
        Profile profile = sftpProfile();
        if (field == QLatin1String("name")) {
            profile.name = value;
        } else if (field == QLatin1String("host")) {
            profile.host = value;
        } else if (field == QLatin1String("user")) {
            profile.user = value;
        } else if (field == QLatin1String("path")) {
            profile.path = value;
        } else {
            profile.identityFile = value;
        }
        QCOMPARE(RemoteProfiles::validationError(profile).isEmpty(), valid);
    }

    void validatesPortsAndAuthentication()
    {
        Profile profile = sftpProfile();
        profile.port = 65536;
        QVERIFY(!RemoteProfiles::validationError(profile).isEmpty());
        profile.port = -1;
        QVERIFY(!RemoteProfiles::validationError(profile).isEmpty());
        profile.port = 0;
        QVERIFY(RemoteProfiles::validationError(profile).isEmpty());

        profile.protocol = Protocol::WebDavs;
        QVERIFY(!RemoteProfiles::validationError(profile).isEmpty());
        profile.authentication = Authentication::Password;
        QVERIFY(RemoteProfiles::validationError(profile).isEmpty());
    }

    void warnsAboutUnencryptedWebDav()
    {
        Profile profile = sftpProfile();
        QVERIFY(RemoteProfiles::securityWarning(profile).isEmpty());
        profile.protocol = Protocol::WebDavs;
        QVERIFY(RemoteProfiles::securityWarning(profile).isEmpty());
        profile.protocol = Protocol::WebDav;
        QVERIFY(!RemoteProfiles::securityWarning(profile).isEmpty());
    }

    void reportsMissingWorkers()
    {
        QStringList asked;
        const auto missing = [&asked](const QString &scheme) {
            asked.append(scheme);
            return false;
        };
        QVERIFY(RemoteProfiles::unavailableReason(Protocol::Sftp, missing).contains(QLatin1String("kio-extras")));
        QVERIFY(!RemoteProfiles::unavailableReason(Protocol::WebDavs, missing).isEmpty());
        QVERIFY(RemoteProfiles::unavailableReason(Protocol::Smb, missing).contains(QLatin1String("kio-extras")));
        QVERIFY(RemoteProfiles::unavailableReason(Protocol::Ftps, missing).contains(QLatin1String("FTPS")));
        QVERIFY(!RemoteProfiles::unavailableReason(Protocol::Ftp, missing).isEmpty());
        QCOMPARE(asked, (QStringList{QStringLiteral("sftp"), QStringLiteral("webdavs"), QStringLiteral("smb"), QStringLiteral("ftps"), QStringLiteral("ftp")}));
        QVERIFY(RemoteProfiles::unavailableReason(Protocol::WebDav, [](const QString &) {
                    return true;
                }).isEmpty());
    }

    void roundTripsProfilesInOrder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("remotesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("RemoteProfiles"));

        Profile webdav;
        webdav.name = QStringLiteral("Nextcloud");
        webdav.protocol = Protocol::WebDavs;
        webdav.host = QStringLiteral("cloud.example.org");
        webdav.path = QStringLiteral("/remote.php/dav/files/me");
        QList<Profile> profiles{sftpProfile(), webdav};
        QString error;
        QVERIFY2(RemoteProfiles::save(group, profiles, &error), qPrintable(error));
        QVERIFY(!profiles.at(0).id.isEmpty());
        QVERIFY(profiles.at(0).id != profiles.at(1).id);

        config->reparseConfiguration();
        QList<Profile> loaded = RemoteProfiles::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(0).id, profiles.at(0).id);
        QCOMPARE(loaded.at(0).name, QStringLiteral("Build server"));
        QCOMPARE(loaded.at(0).port, 2222);
        QCOMPARE(loaded.at(0).authentication, Authentication::KeyOrAgent);
        QCOMPARE(loaded.at(0).identityFile, QStringLiteral("/home/user/.ssh/id_ed25519"));
        QCOMPARE(loaded.at(1).protocol, Protocol::WebDavs);
        QCOMPARE(RemoteProfiles::url(loaded.at(1)), RemoteProfiles::url(webdav));

        const QByteArray contents = [&root] {
            QFile file(root.filePath(QStringLiteral("remotesrc")));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }();
        QVERIFY(!contents.isEmpty());
        QVERIFY(!contents.toLower().contains("password="));

        loaded.swapItemsAt(0, 1);
        loaded.removeLast();
        QVERIFY(RemoteProfiles::save(group, loaded, &error));
        config->reparseConfiguration();
        loaded = RemoteProfiles::load(group, &error);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().name, QStringLiteral("Nextcloud"));
        QCOMPARE(group.groupList(), QStringList{loaded.first().id});
    }

    void rejectsDuplicatesAndSkipsInvalidEntries()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("remotesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("RemoteProfiles"));

        Profile duplicate = sftpProfile();
        duplicate.name = QStringLiteral("build SERVER ");
        QList<Profile> profiles{sftpProfile(), duplicate};
        QString error;
        QVERIFY(!RemoteProfiles::save(group, profiles, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(group.groupList().isEmpty());

        profiles = {sftpProfile()};
        QVERIFY(RemoteProfiles::save(group, profiles, &error));
        group.group(profiles.first().id).writeEntry("Host", QStringLiteral("-oProxyCommand=touch"));
        group.group(QStringLiteral("not-a-uuid")).writeEntry("Name", QStringLiteral("Broken"));
        const QList<Profile> loaded = RemoteProfiles::load(group, &error);
        QVERIFY(loaded.isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(RemoteProfilesTest)

#include "remoteprofilestest.moc"
