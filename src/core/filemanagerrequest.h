// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Interpreting requests to the `org.freedesktop.FileManager1` D-Bus service,
 * which other applications use for actions such as "Show in folder".
 */
namespace FileManagerRequest
{
/// The well-known name and object path of the service.
QString serviceName();
QString objectPath();

/**
 * The URLs named by a request, in order and without duplicates. Callers
 * should send URIs, but absolute local paths are accepted too.
 * @param uris the request's URI list
 * @param rejected receives entries that are not valid absolute URLs
 */
QList<QUrl> parseUris(const QStringList &uris, QStringList *rejected = nullptr);

/// A folder to open and the items in it to select.
struct Reveal {
    QUrl folder;
    QList<QUrl> items;
    bool operator==(const Reveal &other) const = default;
};

/**
 * Groups items by their parent folder, in order of first appearance, so each
 * folder opens once with all its requested items selected. An item without a
 * parent, such as the root folder, opens itself with nothing selected.
 */
QList<Reveal> groupByFolder(const QList<QUrl> &items);

/**
 * Whether Plasma Opus should claim the service name: only while it is the
 * user's default folder handler, so it never takes these requests from
 * another file manager the user prefers.
 * @param folderHandlerDesktopId the desktop entry preferred for
 *        `inode/directory`, with or without the `.desktop` suffix
 */
bool shouldClaim(const QString &folderHandlerDesktopId);
}
