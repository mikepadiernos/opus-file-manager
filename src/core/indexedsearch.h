// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"
#include "searchquery.h"

namespace IndexedSearch
{
/**
 * The Baloo search string that selects candidates for @p term: a file-name
 * term, a content term, or a metadata term matched in any of the text
 * properties among @p metadataKeys and in other indexed text. Candidates are
 * then checked against the whole query.
 */
QString balooSearchString(const SearchQuery::Term &term, const QStringList &metadataKeys);
/**
 * Searches the index, optionally limiting matches and depth to workspaceRoots.
 * @param tagLibraries, tagOptions, metadataKeys as for LiveSearch::find()
 */
LiveSearch::Result find(const QString &currentFolder,
                        const QString &text,
                        LiveSearch::Field field,
                        const LiveSearch::Cancel &cancel = {},
                        const LiveSearch::Progress &progress = {},
                        const PathFilter &filter = {},
                        const QStringList *workspaceRoots = nullptr,
                        const QStringList *tagLibraries = nullptr,
                        const TagSearch::Options &tagOptions = {},
                        const QStringList &metadataKeys = EmbeddedMetadata::keys());
}
