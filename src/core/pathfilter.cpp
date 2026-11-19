// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathfilter.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

bool PathFilter::isActive() const
{
    return !pathText.trimmed().isEmpty() || !parentText.trimmed().isEmpty() || !excludedFolders.isEmpty() || maxDepth >= 0;
}

QString PathFilter::error() const
{
    if (maxDepth < -1) {
        return i18nc("@info", "The recursion depth must be zero or greater, or unlimited.");
    }
    for (const QString &folder : excludedFolders) {
        if (folder.isEmpty() || !QDir::isAbsolutePath(folder)) {
            return i18nc("@info", "Excluded folders must be absolute local paths: %1", folder);
        }
    }
    return {};
}

bool PathFilter::excludes(const QString &path) const
{
    for (const QString &folder : excludedFolders) {
        const QString relative = QDir(QDir::cleanPath(folder)).relativeFilePath(QDir::cleanPath(path));
        if (!QDir::isAbsolutePath(relative) && relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../"))) {
            return true;
        }
    }
    return false;
}

bool PathFilter::matches(const QString &root, const QString &path) const
{
    const QString relative = QDir(root).relativeFilePath(path);
    if (QDir::isAbsolutePath(relative) || relative == QLatin1String("..") || relative.startsWith(QLatin1String("../")) || excludes(path)) {
        return false;
    }
    const int depth = relative.count(QLatin1Char('/'));
    if (maxDepth >= 0 && depth > maxDepth) {
        return false;
    }
    return path.contains(pathText.trimmed(), Qt::CaseInsensitive) && QFileInfo(path).absolutePath().contains(parentText.trimmed(), Qt::CaseInsensitive);
}
