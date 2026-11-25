// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"

#include <KConfigGroup>

/// Initial search criteria shared by the toolbar search field and the search panel.
namespace SearchDefaults
{
enum class Scope {
    CurrentFolder,
    Indexed,
    SearchFolders,
};

/// How search results and collection references are listed.
enum class Presentation {
    FullPaths,
    NamesAndFolders,
};

struct Defaults {
    Scope scope = Scope::CurrentFolder;
    LiveSearch::Field field = LiveSearch::Field::FileName;
    bool workspaceOnly = true;
    /// Name of the shared saved path filter applied initially; empty for none.
    QString pathFilter;
    Presentation presentation = Presentation::FullPaths;
    bool operator==(const Defaults &other) const = default;
};

/// Reads the defaults from the Search group, falling back to the Boolean keys used before M22.
Defaults read(const KConfigGroup &search);

/**
 * Replaces the Boolean SearchIndexedByDefault and SearchContentsByDefault keys
 * with DefaultSearchScope and DefaultSearchField, keeping an existing newer
 * value. Returns whether the group changed; the caller syncs it.
 */
bool migrate(KConfigGroup &search);

/**
 * Returns the saved path filter named @p name from @p savedFilters. An empty
 * name gives an empty filter. If the filter cannot be read or no longer
 * exists, returns an empty filter and sets @p notice.
 */
PathFilter pathFilter(const QString &name, const KConfigGroup &savedFilters, QString *notice);
}
