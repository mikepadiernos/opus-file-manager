// SPDX-License-Identifier: GPL-3.0-or-later

#include "filemanagerrequest.h"

#include "appidentity.h"

#include <QDir>
#include <QSet>

#include <algorithm>

namespace FileManagerRequest
{
QString serviceName()
{
    return QStringLiteral("org.freedesktop.FileManager1");
}

QString objectPath()
{
    return QStringLiteral("/org/freedesktop/FileManager1");
}

QList<QUrl> parseUris(const QStringList &uris, QStringList *rejected)
{
    QList<QUrl> urls;
    QSet<QUrl> seen;
    for (const QString &uri : uris) {
        const QString trimmed = uri.trimmed();
        QUrl url;
        if (trimmed.startsWith(QLatin1Char('/'))) {
            url = QUrl::fromLocalFile(QDir::cleanPath(trimmed));
        } else {
            url = QUrl(trimmed, QUrl::StrictMode);
            if (url.isLocalFile()) {
                url = QUrl::fromLocalFile(QDir::cleanPath(url.toLocalFile()));
            }
        }
        if (!url.isValid() || url.isRelative() || url.path().isEmpty() || (url.isLocalFile() && !url.toLocalFile().startsWith(QLatin1Char('/')))) {
            if (rejected) {
                rejected->append(uri);
            }
            continue;
        }
        url = url.adjusted(QUrl::StripTrailingSlash);
        if (url.path().isEmpty()) {
            url.setPath(QStringLiteral("/"));
        }
        if (!seen.contains(url)) {
            seen.insert(url);
            urls.append(url);
        }
    }
    return urls;
}

QList<Reveal> groupByFolder(const QList<QUrl> &items)
{
    QList<Reveal> reveals;
    for (const QUrl &item : items) {
        const QUrl normalized = item.adjusted(QUrl::StripTrailingSlash);
        QUrl folder = normalized.adjusted(QUrl::RemoveFilename);
        const bool hasParent = !normalized.fileName().isEmpty() && folder.path() != normalized.path();
        if (hasParent) {
            folder = folder.adjusted(QUrl::StripTrailingSlash);
            if (folder.path().isEmpty()) {
                folder.setPath(QStringLiteral("/"));
            }
        } else {
            folder = normalized;
        }
        auto it = std::find_if(reveals.begin(), reveals.end(), [&folder](const Reveal &reveal) {
            return reveal.folder == folder;
        });
        if (it == reveals.end()) {
            reveals.append({folder, {}});
            it = reveals.end() - 1;
        }
        if (hasParent && !it->items.contains(normalized)) {
            it->items.append(normalized);
        }
    }
    return reveals;
}

bool shouldClaim(const QString &folderHandlerDesktopId)
{
    QString id = folderHandlerDesktopId.trimmed();
    if (id.endsWith(QLatin1String(".desktop"))) {
        id.chop(8);
    }
    return !id.isEmpty() && id == AppIdentity::desktopFileName();
}
}
