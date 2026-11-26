// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexstatus.h"

#include <KFormat>
#include <KLocalizedString>

#include <QDir>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#endif

namespace
{
bool contains(const QString &base, const QString &path)
{
    return path == base || path.startsWith(base.endsWith(QLatin1Char('/')) ? base : base + QLatin1Char('/'));
}

bool matchesFilter(const QString &name, const QStringList &filters, QString *matched)
{
    for (const QString &filter : filters) {
        const QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(filter, QRegularExpression::NonPathWildcardConversion));
        if (expression.isValid() && expression.match(name).hasMatch()) {
            *matched = filter;
            return true;
        }
    }
    return false;
}
}

IndexStatus::Config IndexStatus::Config::read()
{
    Config config;
#ifdef OPUSFM_HAVE_BALOO
    const Baloo::IndexerConfig baloo;
    config.available = true;
    config.enabled = baloo.fileIndexingEnabled();
    config.onlyBasic = baloo.onlyBasicIndexing();
    config.indexHidden = baloo.indexHidden();
    config.includeFolders = baloo.includeFolders();
    config.excludeFolders = baloo.excludeFolders();
    config.excludeFilters = baloo.excludeFilters();
    config.excludeMimeTypes = baloo.excludeMimetypes();
#endif
    return config;
}

IndexStatus::PathCoverage IndexStatus::classify(const QString &path, const Config &config, const QString &mimeType)
{
    if (!config.available) {
        return {Coverage::Unavailable, {}};
    }
    if (!config.enabled) {
        return {Coverage::Disabled, {}};
    }
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) {
        return {Coverage::OutsideFolders, {}};
    }
    const QString clean = QDir::cleanPath(path);
    QString closest;
    bool included = false;
    const auto consider = [&](const QStringList &folders, bool value) {
        for (const QString &folder : folders) {
            const QString base = QDir::cleanPath(folder);
            // On a tie, exclusion wins, as in Baloo.
            if (QDir::isAbsolutePath(base) && contains(base, clean)
                && (closest.isNull() || base.size() > closest.size() || (base.size() == closest.size() && !value))) {
                closest = base;
                included = value;
            }
        }
    };
    consider(config.includeFolders, true);
    consider(config.excludeFolders, false);
    if (closest.isNull()) {
        return {Coverage::OutsideFolders, {}};
    }
    if (!included) {
        return {Coverage::ExcludedFolder, closest};
    }
    const QStringList names = clean.mid(closest.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &name : names) {
        if (!config.indexHidden && name.startsWith(QLatin1Char('.'))) {
            return {Coverage::Hidden, name};
        }
        QString filter;
        if (matchesFilter(name, config.excludeFilters, &filter)) {
            return {Coverage::ExcludedFilter, filter};
        }
    }
    if (!mimeType.isEmpty() && config.excludeMimeTypes.contains(mimeType)) {
        return {Coverage::ExcludedType, mimeType};
    }
    return {Coverage::Configured, closest};
}

QString IndexStatus::coverageText(const PathCoverage &coverage)
{
    switch (coverage.coverage) {
    case Coverage::Unavailable:
        return i18nc("@info", "Indexed search is unavailable because Plasma Opus was built without Baloo.");
    case Coverage::Disabled:
        return i18nc("@info", "File indexing is turned off, so nothing is indexed.");
    case Coverage::Configured:
        return i18nc("@info", "This location is in the indexed folders, below %1.", coverage.detail);
    case Coverage::OutsideFolders:
        return i18nc("@info", "This location is outside the indexed folders, so it is not indexed.");
    case Coverage::ExcludedFolder:
        return i18nc("@info", "This location is in the excluded folder %1, so it is not indexed.", coverage.detail);
    case Coverage::ExcludedFilter:
        return i18nc("@info", "This location matches the exclusion pattern %1, so it is not indexed.", coverage.detail);
    case Coverage::Hidden:
        return i18nc("@info", "This location is inside the hidden item %1, and hidden files are not indexed.", coverage.detail);
    case Coverage::ExcludedType:
        return i18nc("@info", "Files of type %1 are excluded from indexing.", coverage.detail);
    }
    return {};
}

QString IndexStatus::stateText(int state)
{
    switch (static_cast<State>(state)) {
    case State::Idle:
        return i18nc("@info indexer state", "Idle");
    case State::Suspended:
        return i18nc("@info indexer state", "Suspended");
    case State::FirstRun:
        return i18nc("@info indexer state", "Initial indexing");
    case State::NewFiles:
        return i18nc("@info indexer state", "Indexing new files");
    case State::ModifiedFiles:
        return i18nc("@info indexer state", "Indexing modified files");
    case State::XAttrFiles:
        return i18nc("@info indexer state", "Indexing extended attributes");
    case State::ContentIndexing:
        return i18nc("@info indexer state", "Indexing file contents");
    case State::UnindexedFileCheck:
        return i18nc("@info indexer state", "Checking for unindexed files");
    case State::StaleIndexEntriesClean:
        return i18nc("@info indexer state", "Checking for stale index entries");
    case State::LowPowerIdle:
        return i18nc("@info indexer state", "Idle (on battery power)");
    case State::Startup:
        return i18nc("@info indexer state", "Starting");
    }
    return i18nc("@info indexer state", "Unknown");
}

bool IndexStatus::isBusy(int state)
{
    switch (static_cast<State>(state)) {
    case State::FirstRun:
    case State::NewFiles:
    case State::ModifiedFiles:
    case State::XAttrFiles:
    case State::ContentIndexing:
    case State::UnindexedFileCheck:
    case State::StaleIndexEntriesClean:
    case State::Startup:
        return true;
    case State::Idle:
    case State::Suspended:
    case State::LowPowerIdle:
        return false;
    }
    return false;
}

QString IndexStatus::serviceText(const Config &config, const Service &service)
{
    if (!config.available) {
        return i18nc("@info", "Unavailable: built without Baloo");
    }
    if (!service.running) {
        return config.enabled ? i18nc("@info", "Not running") : i18nc("@info", "Not running: file indexing is turned off");
    }
    if (service.state < 0) {
        return i18nc("@info", "Running; state unknown");
    }
    QString text = stateText(service.state);
    if (isBusy(service.state) && service.remainingMs > 0) {
        text = i18nc("@info indexer state, remaining time", "%1, about %2 remaining", text, KFormat().formatSpelloutDuration(service.remainingMs));
    }
    return text;
}

QStringList IndexStatus::warnings(const Config &config, const Service &service, const QString &folder)
{
    QStringList result;
    if (!config.available) {
        result.append(i18nc("@info", "Indexed search is unavailable because Plasma Opus was built without Baloo."));
        return result;
    }
    if (!config.enabled) {
        result.append(i18nc("@info", "File indexing is turned off in KDE File Search settings, so indexed search finds nothing."));
        return result;
    }
    if (!service.running) {
        result.append(i18nc("@info", "The file indexer is not running, so recent changes are not indexed and results may be stale."));
    } else if (service.state == static_cast<int>(State::Suspended)) {
        result.append(i18nc("@info", "File indexing is suspended, so recent changes are not indexed and results may be stale."));
    } else if (isBusy(service.state)) {
        result.append(i18nc("@info", "The file indexer is busy (%1), so results may be incomplete.", stateText(service.state)));
    }
    if (config.onlyBasic) {
        result.append(i18nc("@info", "Only file names and basic details are indexed, so file contents and metadata are not searched."));
    }
    if (!folder.isEmpty()) {
        const PathCoverage coverage = classify(folder, config);
        if (!coverage.configured()) {
            result.append(coverageText(coverage));
        }
    }
    return result;
}

bool IndexStatus::expectsMetadata(const QString &mimeType)
{
    static const QStringList prefixes = {QStringLiteral("text/"),
                                         QStringLiteral("image/"),
                                         QStringLiteral("audio/"),
                                         QStringLiteral("video/"),
                                         QStringLiteral("application/vnd.oasis.opendocument."),
                                         QStringLiteral("application/vnd.openxmlformats-officedocument."),
                                         QStringLiteral("application/vnd.ms-")};
    static const QStringList types = {QStringLiteral("application/pdf"),
                                      QStringLiteral("application/epub+zip"),
                                      QStringLiteral("application/msword"),
                                      QStringLiteral("application/postscript"),
                                      QStringLiteral("application/rtf"),
                                      QStringLiteral("application/x-mobipocket-ebook"),
                                      QStringLiteral("application/x-fictionbook+xml")};
    return types.contains(mimeType) || std::any_of(prefixes.cbegin(), prefixes.cend(), [&mimeType](const QString &prefix) {
               return mimeType.startsWith(prefix);
           });
}

QStringList IndexStatus::missingExtractors(const QStringList &mimeTypes, const std::function<bool(const QString &)> &hasExtractor)
{
    QSet<QString> checked;
    QSet<QString> missing;
    for (const QString &type : mimeTypes) {
        if (checked.contains(type)) {
            continue;
        }
        checked.insert(type);
        if (expectsMetadata(type) && !hasExtractor(type)) {
            missing.insert(type);
        }
    }
    QStringList result(missing.cbegin(), missing.cend());
    result.sort();
    return result;
}
