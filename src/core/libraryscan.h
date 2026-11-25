// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <functional>

/**
 * Walks a library root to find the files and folders it indexes. Scanning
 * only reads: nothing is moved, renamed, duplicated or written.
 */
namespace LibraryScan
{
struct Result {
    QList<TagLibrary::Observation> items;
    /// Entries that can't be indexed, such as names that aren't valid in the system encoding.
    int skipped = 0;
    QStringList errors;
    bool canceled = false;
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

/// Whether a file or folder named @p name can be indexed; the library document and .mfm.json files can't.
bool indexable(const QString &name);

/// Reads a single path below @p root, or returns false if it can't be indexed.
bool observe(const QString &root, const QString &absolutePath, TagLibrary::Observation *observation);
}
