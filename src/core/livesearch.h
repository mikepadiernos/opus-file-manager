// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pathfilter.h"

#include <QStringList>

#include <functional>

namespace LiveSearch
{
enum class Field {
    FileName,
    Contents,
    /// An advanced query; see SearchQuery::Query.
    Query,
};

struct Result {
    QStringList paths;
    QStringList errors;
    QStringList notices;
    int skippedFiles = 0;
    int scannedEntries = 0;
    bool canceled = false;
    bool limited = false;
};

using Cancel = std::function<bool()>;
using Progress = std::function<void(int, const QString &)>;

/**
 * Searches a local folder, stopping after at most entryLimit entries.
 * @param tagLibraries the registered tag library roots for tag: and tagged:
 *        criteria, or null to read them from LibraryRegistry on this thread
 */
Result find(const QString &folder,
            const QString &text,
            Field field,
            const Cancel &cancel = {},
            const Progress &progress = {},
            const PathFilter &filter = {},
            int entryLimit = 200000,
            const QStringList *tagLibraries = nullptr);
}
