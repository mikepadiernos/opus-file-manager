// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QStringList>

/**
 * The tag libraries the user opted in to, stored in KConfig. Registering a
 * library never moves or changes the files below its root.
 */
namespace LibraryRegistry
{
/// Registered library roots, sorted.
QStringList roots();

/**
 * Where the document of a library at @p root is kept when it's registered:
 * an existing private document comes first, then an existing document in the
 * folder, and only when neither exists @p preferred.
 */
TagLibrary::Storage storageFor(const QString &root, TagLibrary::Storage preferred);

/**
 * Registers @p root, adopting a valid existing library document where it is
 * (see storageFor()) or creating an empty one in @p storage. With private
 * storage, a readable folder that can't be written can be a library.
 * @returns an error message, or an empty string on success
 */
QString create(const QString &root, TagLibrary::Storage storage = TagLibrary::Storage::LibraryFolder);

/// Unregisters @p root; the library document is kept.
void remove(const QString &root);

/// The registered root that contains @p path, or an empty string.
QString rootFor(const QString &path);

/// Whether @p root is a registered library root.
bool contains(const QString &root);
}
