// SPDX-License-Identifier: GPL-3.0-or-later

#include "flatview.h"

#include <QUrlQuery>

namespace
{
constexpr auto Scheme = "filenamesearch";
constexpr auto FolderKey = "url";
}

QUrl FlatView::listingUrl(const QUrl &folder)
{
    QUrlQuery query;
    // filenamesearch takes a regular expression; "." matches every name.
    query.addQueryItem(QStringLiteral("search"), QStringLiteral("."));
    query.addQueryItem(QLatin1String(FolderKey), folder.toString(QUrl::FullyEncoded));
    query.addQueryItem(QStringLiteral("checkContent"), QStringLiteral("no"));
    QUrl url;
    url.setScheme(QLatin1String(Scheme));
    url.setQuery(query);
    return url;
}

QUrl FlatView::folderOf(const QUrl &url)
{
    if (url.scheme() != QLatin1String(Scheme)) {
        return {};
    }
    return QUrl(QUrlQuery(url).queryItemValue(QLatin1String(FolderKey), QUrl::FullyDecoded));
}

QString FlatView::relativeFolder(const QUrl &base, const QUrl &item)
{
    const QString basePath = base.adjusted(QUrl::StripTrailingSlash).path();
    const QString parentPath = item.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
    if (parentPath == basePath || !parentPath.startsWith(basePath + u'/')) {
        return {};
    }
    return parentPath.mid(basePath.size() + 1);
}
