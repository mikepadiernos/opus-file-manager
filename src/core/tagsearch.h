// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "searchquery.h"
#include "taglibrary.h"

#include <QHash>
#include <QSet>
#include <QStringList>

/**
 * Evaluates tag: and tagged: search terms against tag libraries. Only items
 * inside a library that a scan would index can match: an item that isn't
 * indexed yet, or whose entry is missing, has no tags.
 */
namespace TagSearch
{
/**
 * The tags in @p library that the tag @p term selects: those whose name, an
 * alias or shorthand matches, ignoring case, and, for tag:, their child tags.
 */
QSet<QString> selectedTags(const TagLibrary::Library &library, const SearchQuery::Term &term);

/**
 * Matches the items of one search. Each library is read at most once, so
 * every item is compared with the same version of its library. Not thread
 * safe; use one context per search.
 */
class Context
{
public:
    /// Uses @p roots, the registered library roots, such as LibraryRegistry::roots().
    explicit Context(const QStringList &roots);

    /// A matcher for the item at @p absolutePath, valid while the context exists.
    SearchQuery::TagMatcher matcher(const QString &absolutePath);
    /// Whether the item at @p absolutePath satisfies the tag or tagged @p term.
    bool matches(const QString &absolutePath, const SearchQuery::Term &term);
    /// Why libraries couldn't be read, once per library.
    QStringList errors() const;

private:
    struct State {
        TagLibrary::Library library;
        /// Selected tags by term, so they're computed once per search.
        QHash<QString, QSet<QString>> selections;
    };

    QStringList m_roots;
    QHash<QString, State> m_libraries;
    QStringList m_errors;
};
}
