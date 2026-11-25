// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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
 * Registers @p root, creating an empty library document there when none
 * exists or adopting a valid existing one.
 * @returns an error message, or an empty string on success
 */
QString create(const QString &root);

/// Unregisters @p root; the library document is kept.
void remove(const QString &root);

/// The registered root that contains @p path, or an empty string.
QString rootFor(const QString &path);

/// Whether @p root is a registered library root.
bool contains(const QString &root);
}
