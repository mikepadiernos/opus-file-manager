// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

/// Reads the kernel's mount table and /etc/fstab for the mount manager. It
/// only parses and classifies; mounting, unmounting and ejecting go through
/// Solid and UDisks2, or mount(8) for fstab entries a user may mount.
/// Partitioning, formatting and fstab editing are left to KDE Partition
/// Manager.
namespace MountTable
{
/// What kind of file system a mount is, for its icon and how to unmount it.
enum class Kind {
    Local,
    Network,
    Fuse,
};

/// A mounted file system as listed in /proc/self/mountinfo.
struct Mounted {
    QString source;
    QString mountPoint;
    QString type;
    QStringList options;

    bool operator==(const Mounted &other) const = default;
};

/// A file system defined in /etc/fstab.
struct Defined {
    /// The device as written, such as /dev/sdb1, UUID=… or server:/share.
    QString spec;
    QString mountPoint;
    QString type;
    QStringList options;

    bool operator==(const Defined &other) const = default;
};

/// A file system the mount manager lists: mounted, defined in fstab or both.
struct Entry {
    /// The mounted source, or the fstab device when it is not mounted.
    QString device;
    QString mountPoint;
    QString type;
    QStringList options;
    Kind kind = Kind::Local;
    bool mounted = false;
    bool inFstab = false;
    /// Whether fstab lets an ordinary user mount it, with user, users, owner or group.
    bool userMountable = false;
    /// Whether it is a system mount, as isSystemMount() decides.
    bool system = false;

    bool operator==(const Entry &other) const = default;
};

/// Decodes the octal escapes, such as \040 for a space, used by the kernel and fstab.
QString unescape(const QString &field);

/// Parses /proc/self/mountinfo. When several file systems are stacked on one
/// mount point, only the topmost is kept.
QList<Mounted> parseMountInfo(const QByteArray &content);

/// Parses /etc/fstab, skipping comments, swap and entries without a mount point.
QList<Defined> parseFstab(const QByteArray &content);

/// Classifies a file-system type: network file systems and FUSE remote
/// drives such as sshfs and rclone are Network, other FUSE file systems are
/// Fuse and the rest are Local.
Kind kindOf(const QString &type);

/// Whether a mount is part of the system rather than storage a user would
/// open, such as proc, sysfs, cgroups, tmpfs, Snap images and the document
/// portal, or lies under /proc, /sys or /dev.
bool isSystemMount(const QString &type, const QString &mountPoint);

/// Whether fstab options let an ordinary user mount the file system.
bool isUserMountable(const QStringList &options);

/// Merges mounted and fstab-defined file systems by mount point, leaving out
/// system mounts unless @p includeSystem is set, which marks them as system.
/// @p resolve maps an fstab device, such as UUID=…, to the device node it
/// names so it can be shown; it may be empty. Entries are sorted by mount
/// point.
QList<Entry>
merge(const QList<Mounted> &mounted, const QList<Defined> &defined, const std::function<QString(const QString &)> &resolve = {}, bool includeSystem = false);

/// Maps UUID=, LABEL=, PARTUUID= and PARTLABEL= to their /dev/disk/by-* link
/// and resolves it to the device node, or returns @p spec unchanged.
QString resolveDeviceSpec(const QString &spec, const QString &byRoot = QStringLiteral("/dev/disk"));

/// The percentage of @p size that is used, from 0 to 100, or -1 when unknown.
int usedPercent(qint64 size, qint64 available);

/// Whether a failed mount or unmount reported that the device is busy.
bool isBusyError(const QString &message);
}
