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
 * The Baloo type every match of a type: @p term has, such as Folder or Image,
 * or an empty string. A specific MIME type gives the broader Baloo type.
 */
QString balooType(const SearchQuery::Term &term);

/// The Baloo search string for a modified: @p term, with periods starting at local midnight.
QString balooDateString(const SearchQuery::Term &term);

/**
 * How a search is split between Baloo and Plasma Opus. Baloo applies the
 * search string, types and folder; Plasma Opus checks every candidate against
 * the whole query and filter, so the plan only narrows candidates.
 */
struct Plan {
    /// Content, file-name or metadata terms and modified dates every match satisfies.
    QString searchString;
    /// Baloo types every match has, such as Image or Folder.
    QStringList types;
    /// Whether a meta term selects candidates, which needs Baloo's content indexing.
    bool metadataSeed = false;
    /// Limits plain terms to embedded metadata; set only with Baloo search scopes.
    bool metadataScope = false;
    /// Content terms every candidate satisfies, so they need no lookup of their own.
    QStringList decidedContent;
    /// Content terms Baloo resolves with separate lookups, such as those under NOT.
    QStringList contentLookups;
    QString error;
};

/**
 * Plans an indexed search for @p text in @p field. For queries, @p query is
 * the valid parsed query. Required content subexpressions, types and
 * modification dates are passed to Baloo; with no required content, the
 * first required name or meta term selects candidates as before.
 * @param searchScopes whether Baloo supports search scopes (6.32 or later)
 */
Plan plan(const SearchQuery::Query *query, const QString &text, LiveSearch::Field field, const QStringList &metadataKeys, bool searchScopes);

/**
 * Searches the index, optionally limiting matches and depth to workspaceRoots.
 * Depth-limited and workspace searches ask Baloo only for each location's
 * indexed files, and search the whole index when a location isn't indexed.
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
