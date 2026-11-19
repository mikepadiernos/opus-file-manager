// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlinks.h"

#include <KIO/SimpleJob>
#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

QUrl Symlinks::resolve(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return url;
    }

    const QString canonical = QFileInfo(url.toLocalFile()).canonicalFilePath();
    return canonical.isEmpty() ? url : QUrl::fromLocalFile(canonical);
}

KIO::SimpleJob *Symlinks::create(const QUrl &target, const QString &name, QString &error)
{
    error.clear();
    const QString path = target.toLocalFile();
    const QFileInfo targetInfo(path);
    if (!target.isLocalFile() || !QDir::isAbsolutePath(path) || !targetInfo.exists()) {
        error = i18nc("@info", "The link target must be an existing local file or folder.");
        return nullptr;
    }
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..") || name.contains(QLatin1Char('/')) || name.contains(QChar::Null)) {
        error = i18nc("@info", "Enter a valid link name without a slash.");
        return nullptr;
    }
    const QString link = targetInfo.dir().filePath(name);
    return KIO::symlink(path, QUrl::fromLocalFile(link), KIO::HideProgressInfo);
}
