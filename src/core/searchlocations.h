// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pathfilter.h"

#include <QStringList>

/**
 * Plasma Opus search folders and exclusions.
 *
 * Search folders are local folders searched together by the "Search folders"
 * scope. Exclusions are local folders that every Plasma Opus search skips,
 * live or indexed. Neither changes the Baloo index, which KDE File Search
 * configures for all applications.
 */
namespace SearchLocations
{
/// Cleans absolute folder paths, dropping empty, relative and duplicate entries.
QStringList normalized(const QStringList &folders);

/// Whether @p path is one of @p exclusions or inside one.
bool isExcluded(const QString &path, const QStringList &exclusions);

/**
 * Returns the searchable roots among @p folders: existing local folders that
 * are not symbolic links, excluded or inside another root.
 * @param errors receives an explanation for each folder that was skipped
 */
QStringList searchRoots(const QStringList &folders, const QStringList &exclusions, QStringList *errors = nullptr);

/// Adds @p exclusions to the folders that @p filter excludes.
PathFilter withExclusions(PathFilter filter, const QStringList &exclusions);
}
