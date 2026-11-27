// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityusage.h"

#include <QTest>

using namespace ActivityUsage;

class ActivityUsageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findsEncryptedMounts()
    {
        const QByteArray mountInfo =
            "22 1 259:2 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p2 rw\n"
            "90 22 0:50 / /home/ada/Vaults/Taxes rw,nosuid,nodev shared:60 - fuse.gocryptfs /home/ada/.local/share/plasma-vault/Taxes.enc rw\n"
            "91 22 0:51 / /home/ada/Vaults/Work\\040notes rw,nosuid,nodev shared:61 - fuse.cryfs cryfs@/home/ada/.local/share/plasma-vault/Work.enc rw\n"
            "92 22 0:52 / /home/ada/Remote rw,nosuid,nodev shared:62 - fuse.sshfs ada@host:/srv rw\n"
            "93 22 0:53 / /home/ada/Old rw shared:63 - fuse.encfs encfs rw\n";
        QCOMPARE(encryptedMountPoints(MountTable::parseMountInfo(mountInfo)),
                 (QStringList{QStringLiteral("/home/ada/Vaults/Taxes"), QStringLiteral("/home/ada/Vaults/Work notes"), QStringLiteral("/home/ada/Old")}));
        QVERIFY(encryptedMountPoints({}).isEmpty());
    }

    void reportsLocalAndNetworkLocations()
    {
        QCOMPARE(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Documents/")), {}), QUrl::fromLocalFile(QStringLiteral("/home/ada/Documents")));
        QCOMPARE(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/./Documents/../notes.txt")), {}),
                 QUrl::fromLocalFile(QStringLiteral("/home/ada/notes.txt")));
        QCOMPARE(reportableUrl(QUrl(QStringLiteral("sftp://ada:secret@host:2222/srv/share/?x=1#top")), {}),
                 QUrl(QStringLiteral("sftp://ada@host:2222/srv/share")));
        QCOMPARE(reportableUrl(QUrl(QStringLiteral("SMB://server/Public")), {}), QUrl(QStringLiteral("smb://server/Public")));
        QCOMPARE(reportableUrl(QUrl(QStringLiteral("webdavs://cloud.example.org/remote.php/dav/files/ada/")), {}),
                 QUrl(QStringLiteral("webdavs://cloud.example.org/remote.php/dav/files/ada")));
    }

    void skipsVirtualAndPrivateLocations()
    {
        for (const QString &url : {QStringLiteral("trash:/"),
                                   QStringLiteral("admin:/etc/fstab"),
                                   QStringLiteral("baloosearch:/documents"),
                                   QStringLiteral("filenamesearch:?search=tax&url=file:///home/ada"),
                                   QStringLiteral("tags:/work"),
                                   QStringLiteral("recentlyused:/files"),
                                   QStringLiteral("activities:/current"),
                                   QStringLiteral("remote:/"),
                                   QStringLiteral("mtp:/Phone"),
                                   QStringLiteral("smb:/"),
                                   QStringLiteral("https://example.org/file.pdf")}) {
            QVERIFY2(reportableUrl(QUrl(url), {}).isEmpty(), qPrintable(url));
        }
        QVERIFY(reportableUrl({}, {}).isEmpty());
        QVERIFY(reportableUrl(QUrl(QStringLiteral("file:relative/path")), {}).isEmpty());

        const QStringList excluded = {QStringLiteral("/home/ada/Vaults/Taxes/"), QStringLiteral("/tmp")};
        QVERIFY(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults/Taxes")), excluded).isEmpty());
        QVERIFY(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults/Taxes/2026/return.pdf")), excluded).isEmpty());
        QVERIFY(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults/../Vaults/Taxes/a")), excluded).isEmpty());
        QVERIFY(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/opus-file-manager-mail-1/notes.txt")), excluded).isEmpty());
        QCOMPARE(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults/Taxes old")), excluded),
                 QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults/Taxes old")));
        QCOMPARE(reportableUrl(QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults")), excluded), QUrl::fromLocalFile(QStringLiteral("/home/ada/Vaults")));
    }
};

QTEST_GUILESS_MAIN(ActivityUsageTest)

#include "activityusagetest.moc"
