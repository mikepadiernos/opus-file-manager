// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexedsearch.h"

#include "embeddedmetadata.h"
#include "libraryregistry.h"
#include "searchquery.h"
#include "tagsearch.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <optional>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#include <Baloo/Query>
#include <Baloo/ResultIterator>

namespace
{
bool folderConfigured(const QString &folder, const Baloo::IndexerConfig &config)
{
    if (folder.isEmpty() || !QDir::isAbsolutePath(folder)) {
        return false;
    }
    const QString path = QDir::cleanPath(folder);
    int closest = -1;
    bool included = false;
    const auto consider = [&](const QStringList &folders, bool value) {
        for (const QString &entry : folders) {
            const QString base = QDir::cleanPath(entry);
            const QString relative = QDir(base).relativeFilePath(path);
            if (!QDir::isAbsolutePath(relative) && relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../")) && base.size() > closest) {
                closest = base.size();
                included = value;
            }
        }
    };
    consider(config.includeFolders(), true);
    consider(config.excludeFolders(), false);
    return included;
}
}
#endif

LiveSearch::Result IndexedSearch::find(const QString &currentFolder,
                                       const QString &text,
                                       LiveSearch::Field field,
                                       const LiveSearch::Cancel &cancel,
                                       const LiveSearch::Progress &progress,
                                       const PathFilter &filter,
                                       const QStringList *workspaceRoots,
                                       const QStringList *tagLibraries,
                                       const TagSearch::Options &tagOptions,
                                       const QStringList &metadataKeys)
{
    LiveSearch::Result result;
    if (text.trimmed().isEmpty()) {
        result.errors.append(i18nc("@info", "Enter text to search for."));
        return result;
    }
    if (text.contains(QLatin1Char('"')) && field != LiveSearch::Field::Query) {
        result.errors.append(i18nc("@info", "Indexed search does not support double quotes in the search text."));
        return result;
    }
    std::optional<SearchQuery::Query> query;
    // The Baloo term that selects candidates and whether it searches content.
    QString seed = text;
    bool seedIsContent = field == LiveSearch::Field::Contents;
    if (field == LiveSearch::Field::Query) {
        query = SearchQuery::Query::parse(text);
        if (!query->isValid()) {
            result.errors.append(query->error());
            return result;
        }
        const QList<SearchQuery::Term> required = query->requiredTerms();
        if (required.isEmpty()) {
            result.errors.append(i18nc("@info",
                                       "Indexed queries need a name or content term that every result must match, such as report or content:invoice. "
                                       "Search the current folder or selected locations for other queries."));
            return result;
        }
        const auto content = std::find_if(required.cbegin(), required.cend(), [](const SearchQuery::Term &term) {
            return term.field == SearchQuery::Field::Content;
        });
        const SearchQuery::Term &term = content != required.cend() ? *content : required.first();
        seed = term.text;
        seedIsContent = term.field == SearchQuery::Field::Content;
    }
    const QString filterError = filter.error();
    if (!filterError.isEmpty()) {
        result.errors.append(filterError);
        return result;
    }
    if (!workspaceRoots && filter.maxDepth >= 0 && !QDir::isAbsolutePath(currentFolder)) {
        result.errors.append(i18nc("@info", "A local current folder is required to limit indexed-search depth."));
        return result;
    }
    if (cancel && cancel()) {
        result.canceled = true;
        return result;
    }
    if (workspaceRoots && workspaceRoots->isEmpty()) {
        result.errors.append(i18nc("@info", "The active workspace has no searchable saved locations."));
        return result;
    }
#ifdef OPUSFM_HAVE_BALOO
    const Baloo::IndexerConfig config;
    if (!config.fileIndexingEnabled()) {
        result.errors.append(i18nc("@info", "Baloo file indexing is disabled. Enable it in System Settings or use current-folder search."));
        return result;
    }
    if ((field == LiveSearch::Field::Contents || (query && query->usesContent())) && config.onlyBasicIndexing()) {
        result.errors.append(i18nc("@info", "Baloo content indexing is disabled. Enable it in System Settings or use current-folder search."));
        return result;
    }
    if (workspaceRoots) {
        result.notices.append(i18nc("@info", "Workspace locations may only be partly indexed. Baloo configuration does not guarantee that files are indexed."));
    } else if (!folderConfigured(currentFolder, config)) {
        result.notices.append(
            i18nc("@info", "The current location is not configured for Baloo indexing; some descendants or other locations may still be indexed."));
    } else {
        result.notices.append(
            i18nc("@info", "The current location may only be partly indexed. Baloo configuration does not guarantee that files are indexed."));
    }
    QString dbFolder = qEnvironmentVariable("BALOO_DB_PATH");
    if (dbFolder.isEmpty()) {
        dbFolder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/baloo");
    }
    const QFileInfo database(dbFolder + QStringLiteral("/index"));
    if (!database.isFile() || !database.isReadable()) {
        result.errors.append(i18nc("@info", "The Baloo index is missing or unreadable. Check file indexing in System Settings."));
        return result;
    }

    const int limit = filter.isActive() || workspaceRoots || query ? 200001 : 5001;
    const auto balooQuery = [limit](const QString &term, bool content) {
        Baloo::Query baloo;
        baloo.setSearchString((content ? QStringLiteral("content:\"") : QStringLiteral("filename:\"")) + term + QLatin1Char('"'));
        baloo.setSortingOption(Baloo::Query::SortNone);
        baloo.setLimit(limit);
        return baloo;
    };
    // Content terms other than the seed are resolved to the set of indexed files containing them.
    QHash<QString, QSet<QString>> contentMatches;
    if (query) {
        for (const QString &term : query->contentTerms()) {
            if (seedIsContent && term.compare(seed, Qt::CaseInsensitive) == 0) {
                continue;
            }
            QSet<QString> &paths = contentMatches[term.toCaseFolded()];
            auto iterator = balooQuery(term, true).exec();
            while (iterator.next()) {
                if (cancel && cancel()) {
                    result.canceled = true;
                    return result;
                }
                paths.insert(iterator.filePath());
            }
        }
    }
    const QStringList roots = workspaceRoots ? *workspaceRoots : QStringList{filter.maxDepth >= 0 ? QDir::cleanPath(currentFolder) : QStringLiteral("/")};
    std::optional<TagSearch::Context> tags;
    if (query && query->usesTags()) {
        tags.emplace(tagLibraries ? *tagLibraries : LibraryRegistry::roots(), tagOptions);
    }
    auto iterator = balooQuery(seed, seedIsContent).exec();
    while (iterator.next()) {
        if (cancel && cancel()) {
            result.canceled = true;
            return result;
        }
        if (result.scannedEntries >= 200000) {
            result.limited = true;
            break;
        }
        ++result.scannedEntries;
        if (progress) {
            progress(result.scannedEntries, iterator.filePath());
        }
        const QString path = iterator.filePath();
        if (!QDir::isAbsolutePath(path)) {
            continue;
        }
        const bool matches = std::any_of(roots.cbegin(), roots.cend(), [&](const QString &root) {
            return ((!workspaceRoots && filter.maxDepth < 0) || QDir::cleanPath(path) != root) && filter.matches(root, path);
        });
        if (!matches) {
            continue;
        }
        if (query) {
            const QFileInfo entry(path);
            const qsizetype tagErrors = tags ? tags->errors().size() : 0;
            const bool queryMatches = entry.exists()
                && query->matches(
                    entry,
                    [&](const QString &needle) {
                        if (seedIsContent && needle.compare(seed, Qt::CaseInsensitive) == 0) {
                            return true;
                        }
                        return contentMatches.value(needle.toCaseFolded()).contains(path);
                    },
                    tags ? tags->matcher(path) : SearchQuery::TagMatcher(),
                    EmbeddedMetadata::lazySearchTexts(path, metadataKeys));
            if (tags && tags->errors().size() > tagErrors) {
                result.errors.append(tags->errors().mid(tagErrors));
            }
            if (!queryMatches) {
                continue;
            }
        }
        if (result.paths.size() == 5000) {
            result.limited = true;
            break;
        }
        result.paths.append(path);
    }
    if (cancel && cancel()) {
        result.canceled = true;
    }
    result.notices.append(i18nc("@info", "Baloo results may be incomplete or stale; Baloo does not report index read errors here."));
#else
    Q_UNUSED(currentFolder)
    Q_UNUSED(field)
    Q_UNUSED(progress)
    Q_UNUSED(seed)
    Q_UNUSED(seedIsContent)
    Q_UNUSED(tagLibraries)
    Q_UNUSED(tagOptions)
    Q_UNUSED(metadataKeys)
    result.errors.append(i18nc("@info", "Indexed search is unavailable because Plasma Opus was built without Baloo."));
#endif
    return result;
}
