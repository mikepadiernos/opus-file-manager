// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

/**
 * Describes KDE's shared Baloo file index: its settings, the indexer's state
 * and whether a path is in the configured scope. Reading never changes the
 * shared settings.
 */
namespace IndexStatus
{
/// The indexing settings shared by all applications, from KDE File Search.
struct Config {
    /// Whether Plasma Opus was built with Baloo.
    bool available = false;
    bool enabled = false;
    /// Whether only names and basic details are indexed, without contents or metadata.
    bool onlyBasic = false;
    bool indexHidden = false;
    QStringList includeFolders;
    QStringList excludeFolders;
    /// Wildcard patterns matched against file and folder names.
    QStringList excludeFilters;
    QStringList excludeMimeTypes;

    /// Reads the current settings; available is false without Baloo.
    static Config read();
};

/// The Baloo indexer's states, numbered as its scheduler reports them.
enum class State {
    Idle = 0,
    Suspended,
    FirstRun,
    NewFiles,
    ModifiedFiles,
    XAttrFiles,
    ContentIndexing,
    UnindexedFileCheck,
    StaleIndexEntriesClean,
    LowPowerIdle,
    Startup,
};

/// What the indexer service reports, when it runs.
struct Service {
    bool running = false;
    /// A State value, or -1 when the state could not be read.
    int state = -1;
    /// Baloo's estimate of the remaining indexing time, in milliseconds; 0 when unknown.
    quint32 remainingMs = 0;
    /// The file being indexed, if any.
    QString currentFile;
};

/// Whether the settings include a path. This is the configured scope, not
/// confirmation that the path has been indexed.
enum class Coverage {
    Unavailable,
    Disabled,
    Configured,
    OutsideFolders,
    ExcludedFolder,
    ExcludedFilter,
    Hidden,
    ExcludedType,
};

struct PathCoverage {
    Coverage coverage = Coverage::Unavailable;
    /// The excluding folder, pattern, hidden name or type, when there is one.
    QString detail;

    bool configured() const
    {
        return coverage == Coverage::Configured;
    }
};

/**
 * Classifies @p path against @p config as Baloo's indexer does: the closest
 * indexed or excluded folder decides, then hidden names and exclusion
 * patterns apply to each name below it, then @p mimeType when given.
 */
PathCoverage classify(const QString &path, const Config &config, const QString &mimeType = {});
/// Explains @p coverage in a sentence.
QString coverageText(const PathCoverage &coverage);

/// Describes a State value, or an unknown state.
QString stateText(int state);
/// Whether the indexer is adding or updating entries in @p state, so results may be incomplete.
bool isBusy(int state);
/// Describes the service's state and progress in a sentence.
QString serviceText(const Config &config, const Service &service);

/**
 * Warnings that indexed results for @p folder may be missing or stale.
 * @param folder a local folder whose coverage is reported, or empty to skip it
 */
QStringList warnings(const Config &config, const Service &service, const QString &folder = {});

/// Whether files of @p mimeType usually carry contents or metadata that an extractor indexes.
bool expectsMetadata(const QString &mimeType);
/// The distinct types in @p mimeTypes that expect metadata but have no extractor, sorted.
QStringList missingExtractors(const QStringList &mimeTypes, const std::function<bool(const QString &)> &hasExtractor);
}
