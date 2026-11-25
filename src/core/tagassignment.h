// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

/**
 * Assigns library tags to files and folders chosen in the file interface.
 * Items are identified by absolute local paths; tags only apply within the
 * registered library that contains every item.
 */
namespace TagAssignment
{
struct Selection {
    /// The library containing every item, or empty.
    QString root;
    /// The items' paths relative to root, in selection order.
    QStringList relativePaths;
    /// Why the items can't be tagged together, or empty.
    QString problem;
};

/// Finds the registered library that contains all of @p absolutePaths.
Selection resolve(const QStringList &absolutePaths);

/**
 * Assigns @p add and removes @p remove on @p absolutePaths in the library at
 * @p root, indexing items that aren't indexed yet. Symbolic links and items
 * outside the library are refused before anything changes.
 * @param changed receives the number of items whose tags changed
 */
bool apply(const QString &root,
           const QStringList &absolutePaths,
           const QStringList &add,
           const QStringList &remove,
           int *changed = nullptr,
           QString *error = nullptr);

/**
 * Sets the library field values in @p values on @p absolutePaths in the
 * library at @p root, like apply(). An empty value clears the field; fields
 * not in @p values are kept. Files and their labels, ratings and notes are
 * never changed.
 * @param changed receives the number of items whose values changed
 */
bool applyFields(const QString &root, const QStringList &absolutePaths, const QMap<QString, QString> &values, int *changed = nullptr, QString *error = nullptr);

/**
 * The library at @p root, read from a per-thread cache that is refreshed when
 * the document changes on disk. Returns a null library and sets @p error when
 * it can't be read.
 */
TagLibrary::Library cachedLibrary(const QString &root, QString *error = nullptr);

/// Drops the cached library at @p root, or every cached library when @p root is empty.
void invalidate(const QString &root = QString());
}
