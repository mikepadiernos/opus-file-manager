// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QUrl>

/**
 * Showing every file below a folder in one list, through KIO's
 * filenamesearch worker. Listed items carry their real URLs.
 */
namespace FlatView
{
/**
 * The URL that lists everything below @p folder.
 * @param folder the folder shown flat
 */
QUrl listingUrl(const QUrl &folder);

/**
 * The folder a flat listing shows.
 * @param url a URL from listingUrl()
 * @returns the folder, or an empty URL for any other URL
 */
QUrl folderOf(const QUrl &url);

/**
 * Where an item lies below the flat folder.
 * @param base the folder shown flat
 * @param item an item below @p base
 * @returns the folders between them, such as "x/y"; empty for an item directly in @p base
 */
QString relativeFolder(const QUrl &base, const QUrl &item);
}
