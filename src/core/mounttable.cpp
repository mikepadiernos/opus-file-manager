// SPDX-License-Identifier: GPL-3.0-or-later

#include "mounttable.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace
{
QStringList fieldsOf(const QString &line)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return line.trimmed().split(whitespace, Qt::SkipEmptyParts);
}

bool isUnder(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}
}

namespace MountTable
{
QString unescape(const QString &field)
{
    QString result;
    result.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        if (field.at(i) == QLatin1Char('\\') && i + 3 < field.size()) {
            bool ok = false;
            const int code = field.mid(i + 1, 3).toInt(&ok, 8);
            if (ok && code < 256) {
                result += QChar(code);
                i += 3;
                continue;
            }
        }
        result += field.at(i);
    }
    return result;
}

QList<Mounted> parseMountInfo(const QByteArray &content)
{
    QList<Mounted> mounts;
    QHash<QString, qsizetype> byMountPoint;
    const QStringList lines = QString::fromUtf8(content).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList fields = fieldsOf(line);
        const qsizetype separator = fields.indexOf(QStringLiteral("-"), 6);
        if (fields.size() < 7 || separator < 0 || separator + 2 >= fields.size()) {
            continue;
        }
        Mounted mount;
        mount.mountPoint = unescape(fields.at(4));
        mount.options = fields.at(5).split(QLatin1Char(','), Qt::SkipEmptyParts);
        mount.type = fields.at(separator + 1);
        mount.source = unescape(fields.at(separator + 2));
        const auto existing = byMountPoint.constFind(mount.mountPoint);
        if (existing != byMountPoint.cend()) {
            mounts[*existing] = mount;
        } else {
            byMountPoint.insert(mount.mountPoint, mounts.size());
            mounts.append(mount);
        }
    }
    return mounts;
}

QList<Defined> parseFstab(const QByteArray &content)
{
    QList<Defined> defined;
    const QStringList lines = QString::fromUtf8(content).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList fields = fieldsOf(trimmed);
        if (fields.size() < 3) {
            continue;
        }
        Defined entry;
        entry.spec = unescape(fields.at(0));
        entry.mountPoint = unescape(fields.at(1));
        entry.type = fields.at(2);
        if (fields.size() > 3) {
            entry.options = fields.at(3).split(QLatin1Char(','), Qt::SkipEmptyParts);
        }
        if (entry.type == QLatin1String("swap") || !entry.mountPoint.startsWith(QLatin1Char('/'))) {
            continue;
        }
        defined.append(entry);
    }
    return defined;
}

Kind kindOf(const QString &type)
{
    static const QSet<QString> network{
        QStringLiteral("nfs"),
        QStringLiteral("nfs4"),
        QStringLiteral("cifs"),
        QStringLiteral("smb3"),
        QStringLiteral("smbfs"),
        QStringLiteral("ncpfs"),
        QStringLiteral("afs"),
        QStringLiteral("ceph"),
        QStringLiteral("glusterfs"),
        QStringLiteral("9p"),
        QStringLiteral("davfs"),
        QStringLiteral("fuse.sshfs"),
        QStringLiteral("fuse.rclone"),
        QStringLiteral("fuse.s3fs"),
        QStringLiteral("fuse.gcsfuse"),
        QStringLiteral("fuse.curlftpfs"),
        QStringLiteral("fuse.smbnetfs"),
        QStringLiteral("fuse.kio-fuse"),
        QStringLiteral("fuse.onedriver"),
    };
    if (network.contains(type)) {
        return Kind::Network;
    }
    if (type == QLatin1String("fuse") || type.startsWith(QLatin1String("fuse."))) {
        return Kind::Fuse;
    }
    return Kind::Local;
}

bool isSystemMount(const QString &type, const QString &mountPoint)
{
    static const QSet<QString> systemTypes{
        QStringLiteral("proc"),        QStringLiteral("sysfs"),           QStringLiteral("devtmpfs"),
        QStringLiteral("devpts"),      QStringLiteral("tmpfs"),           QStringLiteral("ramfs"),
        QStringLiteral("cgroup"),      QStringLiteral("cgroup2"),         QStringLiteral("securityfs"),
        QStringLiteral("debugfs"),     QStringLiteral("tracefs"),         QStringLiteral("pstore"),
        QStringLiteral("bpf"),         QStringLiteral("mqueue"),          QStringLiteral("hugetlbfs"),
        QStringLiteral("configfs"),    QStringLiteral("fusectl"),         QStringLiteral("autofs"),
        QStringLiteral("binfmt_misc"), QStringLiteral("efivarfs"),        QStringLiteral("rpc_pipefs"),
        QStringLiteral("nsfs"),        QStringLiteral("selinuxfs"),       QStringLiteral("nfsd"),
        QStringLiteral("fuse.portal"), QStringLiteral("fuse.gvfsd-fuse"), QStringLiteral("none"),
    };
    if (systemTypes.contains(type)) {
        return true;
    }
    static const QStringList systemRoots{
        QStringLiteral("/proc"),
        QStringLiteral("/sys"),
        QStringLiteral("/dev"),
        QStringLiteral("/snap"),
        QStringLiteral("/var/lib/snapd"),
        QStringLiteral("/var/lib/docker"),
        QStringLiteral("/var/lib/containers"),
        QStringLiteral("/run/credentials"),
        QStringLiteral("/run/netns"),
    };
    if (std::any_of(systemRoots.cbegin(), systemRoots.cend(), [&mountPoint](const QString &root) {
            return isUnder(mountPoint, root);
        })) {
        return true;
    }
    static const QRegularExpression documentPortal(QStringLiteral("^/run/user/\\d+/doc(/|$)"));
    return documentPortal.match(mountPoint).hasMatch();
}

bool isUserMountable(const QStringList &options)
{
    return std::any_of(options.cbegin(), options.cend(), [](const QString &option) {
        return option == QLatin1String("user") || option == QLatin1String("users") || option == QLatin1String("owner") || option == QLatin1String("group")
            || option.startsWith(QLatin1String("user="));
    });
}

QList<Entry> merge(const QList<Mounted> &mounted, const QList<Defined> &defined, const std::function<QString(const QString &)> &resolve)
{
    QList<Entry> entries;
    QHash<QString, qsizetype> byMountPoint;
    for (const Mounted &mount : mounted) {
        if (isSystemMount(mount.type, mount.mountPoint)) {
            continue;
        }
        Entry entry;
        entry.device = mount.source;
        entry.mountPoint = mount.mountPoint;
        entry.type = mount.type;
        entry.options = mount.options;
        entry.kind = kindOf(mount.type);
        entry.mounted = true;
        byMountPoint.insert(entry.mountPoint, entries.size());
        entries.append(entry);
    }
    for (const Defined &definition : defined) {
        const QString mountPoint = QDir::cleanPath(definition.mountPoint);
        if (isSystemMount(definition.type, mountPoint)) {
            continue;
        }
        const auto existing = byMountPoint.constFind(mountPoint);
        if (existing != byMountPoint.cend()) {
            Entry &entry = entries[*existing];
            entry.inFstab = true;
            entry.userMountable = isUserMountable(definition.options);
            continue;
        }
        Entry entry;
        entry.device = resolve ? resolve(definition.spec) : definition.spec;
        entry.mountPoint = mountPoint;
        entry.type = definition.type;
        entry.options = definition.options;
        entry.kind = kindOf(definition.type);
        entry.inFstab = true;
        entry.userMountable = isUserMountable(definition.options);
        byMountPoint.insert(mountPoint, entries.size());
        entries.append(entry);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.mountPoint < b.mountPoint;
    });
    return entries;
}

QString resolveDeviceSpec(const QString &spec, const QString &byRoot)
{
    static const QList<std::pair<QString, QString>> prefixes{
        {QStringLiteral("UUID="), QStringLiteral("by-uuid")},
        {QStringLiteral("LABEL="), QStringLiteral("by-label")},
        {QStringLiteral("PARTUUID="), QStringLiteral("by-partuuid")},
        {QStringLiteral("PARTLABEL="), QStringLiteral("by-partlabel")},
    };
    for (const auto &[prefix, directory] : prefixes) {
        if (!spec.startsWith(prefix)) {
            continue;
        }
        QString value = spec.mid(prefix.size());
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        const QString resolved = QFileInfo(byRoot + QLatin1Char('/') + directory + QLatin1Char('/') + value).canonicalFilePath();
        return resolved.isEmpty() ? spec : resolved;
    }
    return spec;
}

int usedPercent(qint64 size, qint64 available)
{
    if (size <= 0 || available < 0) {
        return -1;
    }
    const qint64 used = std::max<qint64>(0, size - std::min(size, available));
    return int(used * 100 / size);
}

bool isBusyError(const QString &message)
{
    return message.contains(QLatin1String("target is busy"), Qt::CaseInsensitive) || message.contains(QLatin1String("device is busy"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("resource busy"), Qt::CaseInsensitive);
}
}
