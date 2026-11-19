// SPDX-License-Identifier: GPL-3.0-or-later

#include "linkednavigation.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

namespace
{
bool localFolder(const QUrl &url)
{
    return url.isLocalFile() && (url.host().isEmpty() || url.host() == QLatin1String("localhost")) && !url.hasQuery() && !url.hasFragment()
        && QFileInfo(url.toLocalFile()).isDir();
}

bool within(const QString &root, const QString &path)
{
    const QString relative = QDir(root).relativeFilePath(path);
    return !QDir::isAbsolutePath(relative) && relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../"));
}
}

LinkedNavigation::Result LinkedNavigation::target(const QUrl &sourceRoot, const QUrl &otherRoot, const QUrl &location)
{
    Result result;
    if (!localFolder(sourceRoot) || !localFolder(otherRoot) || !localFolder(location)) {
        result.error = i18nc("@info:status", "Linked browsing requires existing local folders.");
        return result;
    }
    const QString sourcePath = QDir::cleanPath(sourceRoot.toLocalFile());
    const QString locationPath = QDir::cleanPath(location.toLocalFile());
    const QString sourceCanonical = QFileInfo(sourcePath).canonicalFilePath();
    const QString locationCanonical = QFileInfo(locationPath).canonicalFilePath();
    if (!within(sourcePath, locationPath) || sourceCanonical.isEmpty() || locationCanonical.isEmpty() || !within(sourceCanonical, locationCanonical)) {
        result.error = i18nc("@info:status", "The folder is outside the linked starting folder.");
        return result;
    }
    const QString relative = QDir(sourcePath).relativeFilePath(locationPath);
    const QString otherPath = QDir::cleanPath(QDir(otherRoot.toLocalFile()).filePath(relative));
    const QString otherCanonical = QFileInfo(otherRoot.toLocalFile()).canonicalFilePath();
    const QString targetCanonical = QFileInfo(otherPath).canonicalFilePath();
    if (!QFileInfo(otherPath).isDir() || otherCanonical.isEmpty() || targetCanonical.isEmpty() || !within(otherCanonical, targetCanonical)) {
        result.error = i18nc("@info:status", "The corresponding folder is missing or outside the other linked starting folder: %1", otherPath);
        return result;
    }
    result.target = QUrl::fromLocalFile(otherPath);
    return result;
}
