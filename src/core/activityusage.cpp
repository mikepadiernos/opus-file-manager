// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityusage.h"

#include "plasmavaults.h"

#include <QDir>

namespace ActivityUsage
{
QStringList encryptedMountPoints(const QList<MountTable::Mounted> &mounts)
{
    static const QStringList encryptedTypes = {
        QStringLiteral("fuse.cryfs"),
        QStringLiteral("fuse.gocryptfs"),
        QStringLiteral("fuse.encfs"),
        QStringLiteral("fuse.securefs"),
        QStringLiteral("ecryptfs"),
    };
    QStringList mountPoints;
    for (const MountTable::Mounted &mount : mounts) {
        if (encryptedTypes.contains(mount.type) && !mountPoints.contains(mount.mountPoint)) {
            mountPoints.append(mount.mountPoint);
        }
    }
    return mountPoints;
}

QUrl reportableUrl(const QUrl &url, const QStringList &excludedFolders)
{
    static const QStringList networkSchemes = {
        QStringLiteral("sftp"),
        QStringLiteral("fish"),
        QStringLiteral("smb"),
        QStringLiteral("ftp"),
        QStringLiteral("ftps"),
        QStringLiteral("webdav"),
        QStringLiteral("webdavs"),
        QStringLiteral("dav"),
        QStringLiteral("davs"),
        QStringLiteral("nfs"),
    };
    if (!url.isValid() || url.isEmpty()) {
        return {};
    }
    if (url.isLocalFile()) {
        const QString path = QDir::cleanPath(url.toLocalFile());
        if (!QDir::isAbsolutePath(path)) {
            return {};
        }
        for (const QString &folder : excludedFolders) {
            if (!folder.isEmpty() && PlasmaVaults::contains(QDir::cleanPath(folder), path)) {
                return {};
            }
        }
        return QUrl::fromLocalFile(path);
    }
    if (!networkSchemes.contains(url.scheme().toLower()) || url.host().isEmpty()) {
        return {};
    }
    return url.adjusted(QUrl::RemovePassword | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}
}
