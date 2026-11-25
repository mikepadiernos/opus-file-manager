// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QList>
#include <QRegularExpression>

#include <functional>

/**
 * Walks a library root to find the files and folders it indexes. Scanning
 * only reads: nothing is moved, renamed, duplicated or written.
 */
namespace LibraryScan
{
/// What a refresh indexes; built from the Tagging settings on the GUI thread.
struct Options {
    /**
     * Wildcard patterns using * and ?, matched case-sensitively. A pattern
     * without / matches item names at any depth; one with / matches the path
     * relative to the library root, where * and ? don't match /. A trailing
     * / matches folders only. Items below an excluded folder are excluded too.
     */
    QStringList exclusions;
    /// Whether items whose path has a component starting with . are indexed.
    bool hidden = true;
    /// The most items one refresh indexes; 0 for no limit.
    int limit = 0;
    /// How many folder levels below the root are indexed, 1 for the root's children only; 0 for no limit.
    int depth = 0;
};

/// Decides which paths below a library root a refresh indexes, with the exclusion patterns compiled once.
class Scope
{
public:
    Scope() = default;
    explicit Scope(const Options &options);

    /// Whether @p relativePath, a file or a @p folder, and every folder above it are indexed.
    bool contains(const QString &relativePath, bool folder) const;
    /// Whether the item itself is indexed, without checking the folders above it.
    bool containsItem(const QString &relativePath, bool folder) const;
    /// Whether every path is indexed.
    bool isUnrestricted() const;

private:
    struct Pattern {
        QRegularExpression expression;
        bool path = false;
        bool foldersOnly = false;
    };
    QList<Pattern> m_patterns;
    bool m_hidden = true;
    int m_depth = 0;
};

/// Whether @p relativePath is indexed with @p options; see Scope.
bool inScope(const Options &options, const QString &relativePath, bool folder);

struct Result {
    QList<TagLibrary::Observation> items;
    /// Entries that can't be indexed, such as names that aren't valid in the system encoding.
    int skipped = 0;
    QStringList errors;
    bool canceled = false;
    /// Set when the scan stopped at Options::limit.
    bool limited = false;

    /// Whether every indexed item was found, so unseen entries can be marked missing.
    bool isComplete() const;
};

using Cancel = std::function<bool()>;
/// Receives the number of entries scanned so far and the current path.
using Progress = std::function<void(int, const QString &)>;

/**
 * Lists every file and folder below @p root, of any type. Symbolic links,
 * the library document and per-folder .mfm.json files are not indexed, and
 * linked folders are not followed.
 */
Result scan(const QString &root, const Cancel &cancel = {}, const Progress &progress = {});

/**
 * Lists the files and folders below @p root that @p options include. Items
 * out of scope are neither indexed nor descended into, and the scan stops
 * with Result::limited once Options::limit items are indexed and more remain.
 */
Result scan(const QString &root, const Options &options, const Cancel &cancel = {}, const Progress &progress = {});

/// Whether a file or folder named @p name can be indexed; the library document and .mfm.json files can't.
bool indexable(const QString &name);

/// Reads a single path below @p root, or returns false if it can't be indexed. Options don't apply.
bool observe(const QString &root, const QString &absolutePath, TagLibrary::Observation *observation);

/// The outcome of applying a scan to a library document.
struct Applied {
    /// Whether the document was saved, or nothing needed saving.
    bool saved = false;
    QString error;
    TagLibrary::ScanChanges changes;
    /// The number of missing entries in the library afterwards.
    int missing = 0;
};

/**
 * Records @p result in the library at @p root with TagLibrary::update(). Only
 * a complete result marks entries missing, and entries out of the scope of
 * @p options are never marked missing, so they keep their state.
 */
Applied apply(const QString &root, const Result &result, const Options &options);

/// Describes @p applied and the problems of @p result, one sentence per line, for the refresh summary.
QString summary(const Result &result, const Applied &applied);
}
