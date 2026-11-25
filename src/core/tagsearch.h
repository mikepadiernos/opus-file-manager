// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "searchquery.h"
#include "taglibrary.h"

#include <QHash>
#include <QSet>
#include <QStringList>

/**
 * Evaluates tag: and tagged: search terms against tag libraries. Only items
 * inside a library that a scan would index can match, unless options let
 * tagged:no match items outside every library: an item that isn't indexed
 * yet, or whose entry is missing, has no tags.
 */
namespace TagSearch
{
/// Where a search of one folder with tag criteria starts.
enum class Scope {
    /// In the folder searched.
    CurrentFolder,
    /// In the deepest library folder that contains the folder searched.
    LibraryRoot,
};

/// How tag terms select tags and items.
struct Options {
    /// Whether tag: also selects the child tags of the tags it names; tag= never does.
    bool childTags = true;
    /// Whether tag: and tag= match aliases and shorthands as well as names.
    bool aliases = true;
    /// Whether tagged:no also matches indexable items outside every library.
    bool untaggedOutsideLibraries = false;
    /// Where searches of one folder start; see searchRoot().
    Scope scope = Scope::CurrentFolder;
};

/**
 * The tags in @p library that the tag @p term selects: those whose name, or
 * when @p options allow an alias or shorthand, matches, ignoring case, and,
 * for tag: when @p options allow, their child tags.
 */
QSet<QString> selectedTags(const TagLibrary::Library &library, const SearchQuery::Term &term, const Options &options = {});

/**
 * The folder a search of @p folder for @p query starts in. With
 * Scope::LibraryRoot and a valid @p query with tag or tagged criteria, it is
 * the deepest of @p libraryRoots that contains or is @p folder, if that is an
 * existing folder and not a symbolic link; otherwise it is @p folder.
 */
QString searchRoot(const QString &folder, const SearchQuery::Query &query, const QStringList &libraryRoots, Scope scope);

/**
 * Matches the items of one search. Each library is read at most once, so
 * every item is compared with the same version of its library. Not thread
 * safe; use one context per search.
 */
class Context
{
public:
    /// Uses @p roots, the registered library roots, such as LibraryRegistry::roots().
    explicit Context(const QStringList &roots, const Options &options = {});

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
    Options m_options;
    QHash<QString, State> m_libraries;
    QStringList m_errors;
};
}
