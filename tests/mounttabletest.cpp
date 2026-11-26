// SPDX-License-Identifier: GPL-3.0-or-later

#include "mounttable.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace MountTable;

namespace
{
const QByteArray MountInfo = R"(22 1 259:2 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p2 rw
23 22 0:21 / /proc rw,nosuid shared:5 - proc proc rw
24 22 0:5 / /dev rw,nosuid shared:2 - devtmpfs devtmpfs rw,size=8000k
25 22 0:25 / /tmp rw shared:3 - tmpfs tmpfs rw
26 22 259:1 / /boot/efi rw,relatime shared:4 - vfat /dev/nvme0n1p1 rw
27 22 8:17 / /run/media/user/My\040Stick rw,nosuid,nodev shared:6 - exfat /dev/sdb1 rw
28 22 0:40 / /mnt/nas rw shared:7 - nfs4 nas:/export rw,vers=4.2
29 22 0:41 / /home/user/cloud rw,nosuid,nodev shared:8 - fuse.rclone remote: rw,user_id=1000
30 22 0:42 / /home/user/vault rw,nosuid,nodev shared:9 - fuse.gocryptfs /home/user/.vault rw
31 22 0:43 / /run/user/1000/doc rw,nosuid,nodev shared:10 - fuse.portal portal rw
32 22 7:3 / /snap/core/1 ro,nodev shared:11 - squashfs /dev/loop3 ro
33 26 0:44 / /boot/efi rw shared:12 - autofs systemd-1 rw
34 22 0:45 / /mnt/broken rw - shared:13
)";

const QByteArray Fstab = R"(# /etc/fstab
UUID=1111-2222  /               ext4    defaults        0 1
UUID=3333       /boot/efi       vfat    umask=0077      0 2
/dev/sdc1       /mnt/backup     ext4    noauto,user     0 0
nas:/export     /mnt/nas        nfs4    noauto,users    0 0
LABEL=Data      /mnt/data/      ext4    noauto          0 0
/swapfile       none            swap    defaults        0 0
tmpfs           /tmp            tmpfs   defaults        0 0
/dev/sdd1       /mnt/with\040space ext4 noauto,owner    0 0
incomplete-line
)";
}

class MountTableTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unescapesOctal()
    {
        QCOMPARE(unescape(QStringLiteral("My\\040Stick")), QStringLiteral("My Stick"));
        QCOMPARE(unescape(QStringLiteral("tab\\011and\\134slash")), QStringLiteral("tab\tand\\slash"));
        QCOMPARE(unescape(QStringLiteral("trailing\\04")), QStringLiteral("trailing\\04"));
        QCOMPARE(unescape(QStringLiteral("not\\abc")), QStringLiteral("not\\abc"));
    }

    void parsesMountInfo()
    {
        const QList<Mounted> mounts = parseMountInfo(MountInfo);
        QCOMPARE(mounts.size(), 11);
        QCOMPARE(mounts.first(),
                 (Mounted{QStringLiteral("/dev/nvme0n1p2"), QStringLiteral("/"), QStringLiteral("ext4"), {QStringLiteral("rw"), QStringLiteral("relatime")}}));
        const auto stick = std::find_if(mounts.cbegin(), mounts.cend(), [](const Mounted &mount) {
            return mount.source == QLatin1String("/dev/sdb1");
        });
        QVERIFY(stick != mounts.cend());
        QCOMPARE(stick->mountPoint, QStringLiteral("/run/media/user/My Stick"));
        QCOMPARE(stick->type, QStringLiteral("exfat"));
        // The automount stacked over /boot/efi replaces the entry below it.
        const auto efi = std::find_if(mounts.cbegin(), mounts.cend(), [](const Mounted &mount) {
            return mount.mountPoint == QLatin1String("/boot/efi");
        });
        QCOMPARE(efi->type, QStringLiteral("autofs"));
        QVERIFY(std::none_of(mounts.cbegin(), mounts.cend(), [](const Mounted &mount) {
            return mount.mountPoint == QLatin1String("/mnt/broken");
        }));
    }

    void parsesFstab()
    {
        const QList<Defined> defined = parseFstab(Fstab);
        QCOMPARE(defined.size(), 7);
        QCOMPARE(defined.at(0).spec, QStringLiteral("UUID=1111-2222"));
        QCOMPARE(
            defined.at(2),
            (Defined{QStringLiteral("/dev/sdc1"), QStringLiteral("/mnt/backup"), QStringLiteral("ext4"), {QStringLiteral("noauto"), QStringLiteral("user")}}));
        QCOMPARE(defined.last().mountPoint, QStringLiteral("/mnt/with space"));
    }

    void classifiesKinds()
    {
        QCOMPARE(kindOf(QStringLiteral("ext4")), Kind::Local);
        QCOMPARE(kindOf(QStringLiteral("fuseblk")), Kind::Local);
        QCOMPARE(kindOf(QStringLiteral("nfs4")), Kind::Network);
        QCOMPARE(kindOf(QStringLiteral("cifs")), Kind::Network);
        QCOMPARE(kindOf(QStringLiteral("fuse.sshfs")), Kind::Network);
        QCOMPARE(kindOf(QStringLiteral("fuse.rclone")), Kind::Network);
        QCOMPARE(kindOf(QStringLiteral("fuse.gocryptfs")), Kind::Fuse);
        QCOMPARE(kindOf(QStringLiteral("fuse")), Kind::Fuse);
    }

    void hidesSystemMounts()
    {
        QVERIFY(isSystemMount(QStringLiteral("proc"), QStringLiteral("/proc")));
        QVERIFY(isSystemMount(QStringLiteral("tmpfs"), QStringLiteral("/tmp")));
        QVERIFY(isSystemMount(QStringLiteral("squashfs"), QStringLiteral("/snap/core/1")));
        QVERIFY(isSystemMount(QStringLiteral("fuse.portal"), QStringLiteral("/run/user/1000/doc")));
        QVERIFY(isSystemMount(QStringLiteral("ext4"), QStringLiteral("/run/user/1000/doc/abc")));
        QVERIFY(isSystemMount(QStringLiteral("overlay"), QStringLiteral("/var/lib/docker/overlay2/x/merged")));
        QVERIFY(!isSystemMount(QStringLiteral("ext4"), QStringLiteral("/")));
        QVERIFY(!isSystemMount(QStringLiteral("ext4"), QStringLiteral("/devices")));
        QVERIFY(!isSystemMount(QStringLiteral("exfat"), QStringLiteral("/run/media/user/stick")));
        QVERIFY(!isSystemMount(QStringLiteral("fuse.rclone"), QStringLiteral("/run/user/1000/cloud")));
    }

    void detectsUserMountableOptions()
    {
        QVERIFY(isUserMountable({QStringLiteral("noauto"), QStringLiteral("user")}));
        QVERIFY(isUserMountable({QStringLiteral("users")}));
        QVERIFY(isUserMountable({QStringLiteral("owner")}));
        QVERIFY(isUserMountable({QStringLiteral("group")}));
        QVERIFY(!isUserMountable({QStringLiteral("defaults"), QStringLiteral("user_xattr")}));
        QVERIFY(!isUserMountable({}));
    }

    void mergesMountedAndDefined()
    {
        const QList<Entry> entries = merge(parseMountInfo(MountInfo), parseFstab(Fstab), [](const QString &spec) {
            return spec == QLatin1String("LABEL=Data") ? QStringLiteral("/dev/sde1") : spec;
        });
        QStringList mountPoints;
        for (const Entry &entry : entries) {
            mountPoints.append(entry.mountPoint);
        }
        QCOMPARE(mountPoints,
                 (QStringList{QStringLiteral("/"),
                              QStringLiteral("/boot/efi"),
                              QStringLiteral("/home/user/cloud"),
                              QStringLiteral("/home/user/vault"),
                              QStringLiteral("/mnt/backup"),
                              QStringLiteral("/mnt/data"),
                              QStringLiteral("/mnt/nas"),
                              QStringLiteral("/mnt/with space"),
                              QStringLiteral("/run/media/user/My Stick")}));

        const Entry &root = entries.at(0);
        QVERIFY(root.mounted);
        QVERIFY(root.inFstab);
        QVERIFY(!root.userMountable);
        QCOMPARE(root.device, QStringLiteral("/dev/nvme0n1p2"));

        // Only the automount is mounted, so the fstab definition is listed as not mounted.
        const Entry &efi = entries.at(1);
        QVERIFY(!efi.mounted);
        QVERIFY(efi.inFstab);
        QCOMPARE(efi.type, QStringLiteral("vfat"));

        const Entry &cloud = entries.at(2);
        QCOMPARE(cloud.kind, Kind::Network);
        QVERIFY(cloud.mounted);
        QVERIFY(!cloud.inFstab);

        const Entry &backup = entries.at(4);
        QVERIFY(!backup.mounted);
        QVERIFY(backup.inFstab);
        QVERIFY(backup.userMountable);

        const Entry &data = entries.at(5);
        QCOMPARE(data.device, QStringLiteral("/dev/sde1"));
        QVERIFY(!data.userMountable);

        const Entry &nas = entries.at(6);
        QVERIFY(nas.mounted);
        QVERIFY(nas.inFstab);
        QVERIFY(nas.userMountable);
        QCOMPARE(nas.kind, Kind::Network);
        QCOMPARE(nas.device, QStringLiteral("nas:/export"));
    }

    void resolvesDeviceSpecs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("by-uuid")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("by-label")));
        const QString node = dir.filePath(QStringLiteral("sdz1"));
        QFile file(node);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(QFile::link(node, dir.filePath(QStringLiteral("by-uuid/abcd-1234"))));
        QVERIFY(QFile::link(node, dir.filePath(QStringLiteral("by-label/My Disk"))));

        const QString canonical = QFileInfo(node).canonicalFilePath();
        QCOMPARE(resolveDeviceSpec(QStringLiteral("UUID=abcd-1234"), dir.path()), canonical);
        QCOMPARE(resolveDeviceSpec(QStringLiteral("LABEL=\"My Disk\""), dir.path()), canonical);
        QCOMPARE(resolveDeviceSpec(QStringLiteral("UUID=missing"), dir.path()), QStringLiteral("UUID=missing"));
        QCOMPARE(resolveDeviceSpec(QStringLiteral("/dev/sda1"), dir.path()), QStringLiteral("/dev/sda1"));
    }

    void computesUsage()
    {
        QCOMPARE(usedPercent(100, 25), 75);
        QCOMPARE(usedPercent(100, 100), 0);
        QCOMPARE(usedPercent(100, 0), 100);
        QCOMPARE(usedPercent(100, 200), 0);
        QCOMPARE(usedPercent(0, 0), -1);
        QCOMPARE(usedPercent(100, -1), -1);
    }

    void recognizesBusyErrors()
    {
        QVERIFY(isBusyError(QStringLiteral("umount: /mnt/backup: target is busy.")));
        QVERIFY(isBusyError(QStringLiteral("fusermount3: failed to unmount /x: Device or resource busy")));
        QVERIFY(!isBusyError(QStringLiteral("umount: /mnt/backup: must be superuser to unmount.")));
    }
};

QTEST_GUILESS_MAIN(MountTableTest)

#include "mounttabletest.moc"
