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
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <optional>

QString IndexedSearch::balooSearchString(const SearchQuery::Term &term, const QStringList &metadataKeys)
{
    QString text = term.text;
    text.remove(QLatin1Char('"'));
    const QString quoted = QLatin1Char('"') + text + QLatin1Char('"');
    switch (term.field) {
    case SearchQuery::Field::Content:
        return QStringLiteral("content:") + quoted;
    case SearchQuery::Field::Metadata: {
        QStringList alternatives{quoted};
        for (const QString &key : metadataKeys) {
            if (EmbeddedMetadata::isText(key)) {
                alternatives.append(key + QLatin1Char(':') + quoted);
            }
        }
        return QLatin1Char('(') + alternatives.join(QStringLiteral(" OR ")) + QLatin1Char(')');
    }
    default:
        return QStringLiteral("filename:") + quoted;
    }
}

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
    // The term whose Baloo matches are the candidates checked against the query.
    SearchQuery::Term seed;
    seed.text = text;
    seed.field = field == LiveSearch::Field::Contents ? SearchQuery::Field::Content : SearchQuery::Field::Name;
    if (field == LiveSearch::Field::Query) {
        query = SearchQuery::Query::parse(text);
        if (!query->isValid()) {
            result.errors.append(query->error());
            return result;
        }
        const QList<SearchQuery::Term> required = query->requiredTerms();
        if (required.isEmpty()) {
            result.errors.append(i18nc("@info",
                                       "Indexed queries need a name, content or meta term that every result must match, such as report, content:invoice "
                                       "or meta:Canon. Search the current folder or selected locations for other queries."));
            return result;
        }
        // Content terms select fewest candidates, then names; metadata terms are the broadest.
        for (const SearchQuery::Field preferred : {SearchQuery::Field::Content, SearchQuery::Field::Name, SearchQuery::Field::Metadata}) {
            const auto found = std::find_if(required.cbegin(), required.cend(), [preferred](const SearchQuery::Term &term) {
                return term.field == preferred;
            });
            if (found != required.cend()) {
                seed = *found;
                break;
            }
        }
        if (seed.field == SearchQuery::Field::Metadata) {
            static const QRegularExpression numeric(QStringLiteral("^[0-9.:/ -]+$"));
            if (numeric.match(seed.text).hasMatch()) {
                result.errors.append(i18nc("@info",
                                           "Indexed search can't find numbers or dates in metadata. Add a name or content term, "
                                           "or search the current folder or selected locations."));
                return result;
            }
            if (!std::any_of(metadataKeys.cbegin(), metadataKeys.cend(), EmbeddedMetadata::isText)) {
                result.errors.append(i18nc("@info",
                                           "No text metadata properties are enabled for search. Enable them in the search settings, "
                                           "or add a name or content term."));
                return result;
            }
        }
    }
    const bool seedIsContent = seed.field == SearchQuery::Field::Content;
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
    if (seed.field == SearchQuery::Field::Metadata && config.onlyBasicIndexing()) {
        result.errors.append(i18nc("@info",
                                   "Baloo indexes file names only, so it can't find metadata. Enable content indexing in System Settings, "
                                   "add a name term or use current-folder search."));
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
    const auto balooQuery = [limit](const QString &searchString) {
        Baloo::Query baloo;
        baloo.setSearchString(searchString);
        baloo.setSortingOption(Baloo::Query::SortNone);
        baloo.setLimit(limit);
        return baloo;
    };
    // Content terms other than the seed are resolved to the set of indexed files containing them.
    QHash<QString, QSet<QString>> contentMatches;
    if (query) {
        for (const QString &term : query->contentTerms()) {
            if (seedIsContent && term.compare(seed.text, Qt::CaseInsensitive) == 0) {
                continue;
            }
            QSet<QString> &paths = contentMatches[term.toCaseFolded()];
            SearchQuery::Term contentTerm;
            contentTerm.field = SearchQuery::Field::Content;
            contentTerm.text = term;
            auto iterator = balooQuery(balooSearchString(contentTerm, metadataKeys)).exec();
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
    auto iterator = balooQuery(balooSearchString(seed, metadataKeys)).exec();
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
                        if (seedIsContent && needle.compare(seed.text, Qt::CaseInsensitive) == 0) {
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
