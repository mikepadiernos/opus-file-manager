// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchlocations.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace
{
bool isInside(const QString &path, const QString &folder)
{
    return path == folder || folder == QLatin1String("/") || path.startsWith(folder + QLatin1Char('/'));
}
}

QStringList SearchLocations::normalized(const QStringList &folders)
{
    QStringList result;
    for (const QString &folder : folders) {
        const QString trimmed = folder.trimmed();
        if (trimmed.isEmpty() || !QDir::isAbsolutePath(trimmed)) {
            continue;
        }
        const QString clean = QDir::cleanPath(trimmed);
        if (!result.contains(clean)) {
            result.append(clean);
        }
    }
    return result;
}

bool SearchLocations::isExcluded(const QString &path, const QStringList &exclusions)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) {
        return false;
    }
    const QString clean = QDir::cleanPath(path);
    const QStringList folders = normalized(exclusions);
    return std::any_of(folders.cbegin(), folders.cend(), [&clean](const QString &folder) {
        return isInside(clean, folder);
    });
}

QStringList SearchLocations::searchRoots(const QStringList &folders, const QStringList &exclusions, QStringList *errors)
{
    QStringList candidates;
    for (const QString &folder : folders) {
        if (!folder.trimmed().isEmpty() && !QDir::isAbsolutePath(folder.trimmed()) && errors) {
            errors->append(i18nc("@info", "Search folders must be absolute local paths: %1", folder));
        }
    }
    for (const QString &folder : normalized(folders)) {
        const QFileInfo info(folder);
        if (!info.isDir() || info.isSymLink()) {
            if (errors) {
                errors->append(i18nc("@info", "Choose an existing local folder that is not a symbolic link: %1", folder));
            }
        } else if (isExcluded(folder, exclusions)) {
            if (errors) {
                errors->append(i18nc("@info", "This search folder is excluded from searches: %1", folder));
            }
        } else {
            candidates.append(folder);
        }
    }
    QStringList roots;
    for (const QString &folder : std::as_const(candidates)) {
        const bool nested = std::any_of(candidates.cbegin(), candidates.cend(), [&folder](const QString &other) {
            return other != folder && isInside(folder, other);
        });
        if (!nested) {
            roots.append(folder);
        }
    }
    return roots;
}

PathFilter SearchLocations::withExclusions(PathFilter filter, const QStringList &exclusions)
{
    for (const QString &folder : normalized(exclusions)) {
        if (!filter.excludedFolders.contains(folder)) {
            filter.excludedFolders.append(folder);
        }
    }
    return filter;
}
