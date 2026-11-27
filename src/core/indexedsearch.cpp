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
#include <vector>

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

namespace
{
using SearchQuery::Field;
using SearchQuery::Node;

bool contentOnly(const Node &node)
{
    switch (node.kind) {
    case Node::Kind::Term:
        return node.term.field == Field::Content;
    case Node::Kind::And:
    case Node::Kind::Or:
        return std::all_of(node.children.cbegin(), node.children.cend(), contentOnly);
    case Node::Kind::Not:
        break;
    }
    return false;
}

QString contentString(const Node &node)
{
    if (node.kind == Node::Kind::Term) {
        return IndexedSearch::balooSearchString(node.term, {});
    }
    QStringList parts;
    for (const Node &child : node.children) {
        parts.append(contentString(child));
    }
    return QLatin1Char('(') + parts.join(node.kind == Node::Kind::And ? QStringLiteral(" AND ") : QStringLiteral(" OR ")) + QLatin1Char(')');
}

void countContent(const Node &node, QHash<QString, int> &counts)
{
    if (node.kind == Node::Kind::Term && node.term.field == Field::Content) {
        ++counts[node.term.text.toCaseFolded()];
    }
    for (const Node &child : node.children) {
        countContent(child, counts);
    }
}

// Baloo records Folder for folders and these types for every MIME type with the prefix.
QString balooType(const SearchQuery::Term &term)
{
    if (term.type == SearchQuery::TypeMatch::Folder) {
        return QStringLiteral("Folder");
    }
    if (term.type != SearchQuery::TypeMatch::MimeType && term.type != SearchQuery::TypeMatch::MimePrefix) {
        return {};
    }
    static const std::pair<QLatin1StringView, QLatin1StringView> types[] = {
        {QLatin1StringView("image/"), QLatin1StringView("Image")},
        {QLatin1StringView("audio/"), QLatin1StringView("Audio")},
        {QLatin1StringView("video/"), QLatin1StringView("Video")},
    };
    for (const auto &[prefix, type] : types) {
        if (term.text.startsWith(prefix)) {
            return type;
        }
    }
    return {};
}

// Periods start at local midnight, as Baloo reads dates without a time.
QString dateString(const SearchQuery::Term &term)
{
    const QString from = term.from.date().toString(Qt::ISODate);
    const QString to = term.to.date().toString(Qt::ISODate);
    switch (term.comparison) {
    case SearchQuery::Comparison::Less:
        return QStringLiteral("modified<") + from;
    case SearchQuery::Comparison::LessOrEqual:
        return QStringLiteral("modified<") + to;
    case SearchQuery::Comparison::Greater:
        return QStringLiteral("modified>=") + to;
    case SearchQuery::Comparison::GreaterOrEqual:
        return QStringLiteral("modified>=") + from;
    case SearchQuery::Comparison::Contains:
    case SearchQuery::Comparison::Equal:
        break;
    }
    return QStringLiteral("modified>=") + from + QStringLiteral(" AND modified<") + to;
}
}

IndexedSearch::Plan
IndexedSearch::plan(const SearchQuery::Query *query, const QString &text, LiveSearch::Field field, const QStringList &metadataKeys, bool searchScopes)
{
    Plan result;
    if (!query) {
        SearchQuery::Term seed;
        seed.text = text;
        seed.field = field == LiveSearch::Field::Contents ? Field::Content : Field::Name;
        result.searchString = balooSearchString(seed, metadataKeys);
        if (seed.field == Field::Content) {
            result.decidedContent.append(text);
        }
        return result;
    }
    const Node &root = query->root();
    std::vector<const Node *> required;
    if (root.kind == Node::Kind::And) {
        for (const Node &child : root.children) {
            required.push_back(&child);
        }
    } else {
        required.push_back(&root);
    }
    QStringList parts;
    QStringList dates;
    QHash<QString, int> passed;
    QSet<QString> direct;
    for (const Node *node : required) {
        if (contentOnly(*node)) {
            parts.append(contentString(*node));
            countContent(*node, passed);
            if (node->kind == Node::Kind::Term) {
                direct.insert(node->term.text.toCaseFolded());
            }
        } else if (node->kind == Node::Kind::Term && node->term.field == Field::Type) {
            if (const QString type = balooType(node->term); !type.isEmpty() && !result.types.contains(type)) {
                result.types.append(type);
            }
        } else if (node->kind == Node::Kind::Term && node->term.field == Field::Modified) {
            dates.append(dateString(node->term));
        }
    }

    // As before, the first required content term, or else name or meta term, selects candidates.
    const QList<SearchQuery::Term> terms = query->requiredTerms();
    std::optional<SearchQuery::Term> seed;
    for (const Field preferred : {Field::Content, Field::Name, Field::Metadata}) {
        const auto found = std::find_if(terms.cbegin(), terms.cend(), [preferred](const SearchQuery::Term &term) {
            return term.field == preferred;
        });
        if (found != terms.cend()) {
            seed = *found;
            break;
        }
    }
    if (!seed && parts.isEmpty()) {
        result.error = i18nc("@info",
                             "Indexed queries need a name, content or meta term that every result must match, such as report, content:invoice "
                             "or meta:Canon. Search the current folder or selected locations for other queries.");
        return result;
    }
    if (seed && seed->field == Field::Metadata) {
        static const QRegularExpression numeric(QStringLiteral("^[0-9.:/ -]+$"));
        if (numeric.match(seed->text).hasMatch()) {
            result.error = i18nc("@info",
                                 "Indexed search can't find numbers or dates in metadata. Add a name or content term, "
                                 "or search the current folder or selected locations.");
            return result;
        }
        if (!std::any_of(metadataKeys.cbegin(), metadataKeys.cend(), EmbeddedMetadata::isText)) {
            result.error = i18nc("@info",
                                 "No text metadata properties are enabled for search. Enable them in the search settings, "
                                 "or add a name or content term.");
            return result;
        }
        result.metadataSeed = true;
        result.metadataScope = searchScopes;
    }
    if (seed && seed->field != Field::Content) {
        parts.prepend(balooSearchString(*seed, metadataKeys));
    }
    parts.append(dates);
    result.searchString = parts.join(QStringLiteral(" AND "));

    // A content term Baloo has already decided for every candidate needs no lookup.
    QHash<QString, int> all;
    countContent(root, all);
    for (const QString &term : query->contentTerms()) {
        const QString key = term.toCaseFolded();
        (direct.contains(key) || passed.value(key) == all.value(key) ? result.decidedContent : result.contentLookups).append(term);
    }
    return result;
}

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <baloo_version.h>

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
    if (field == LiveSearch::Field::Query) {
        query = SearchQuery::Query::parse(text);
        if (!query->isValid()) {
            result.errors.append(query->error());
            return result;
        }
    }
#if defined(OPUSFM_HAVE_BALOO) && BALOO_VERSION >= QT_VERSION_CHECK(6, 32, 0)
    const bool searchScopes = true;
#else
    const bool searchScopes = false;
#endif
    const Plan plan = IndexedSearch::plan(query ? &*query : nullptr, text, field, metadataKeys, searchScopes);
    if (!plan.error.isEmpty()) {
        result.errors.append(plan.error);
        return result;
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
    if (plan.metadataSeed && config.onlyBasicIndexing()) {
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
    // Content terms Baloo can't decide within the plan are resolved to the indexed files containing them.
    QHash<QString, QSet<QString>> contentMatches;
    for (const QString &term : plan.contentLookups) {
        QSet<QString> &paths = contentMatches[term.toCaseFolded()];
        SearchQuery::Term contentTerm;
        contentTerm.field = Field::Content;
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
    QSet<QString> decided;
    for (const QString &term : plan.decidedContent) {
        decided.insert(term.toCaseFolded());
    }
    const bool scoped = workspaceRoots || filter.maxDepth >= 0;
    const QStringList roots = workspaceRoots ? *workspaceRoots : QStringList{scoped ? QDir::cleanPath(currentFolder) : QStringLiteral("/")};
    std::optional<TagSearch::Context> tags;
    if (query && query->usesTags()) {
        tags.emplace(tagLibraries ? *tagLibraries : LibraryRegistry::roots(), tagOptions);
    }
    QSet<QString> seen;
    bool stopped = false;
    // Runs the plan in includeFolder, or the whole index when empty, and returns how many files Baloo found.
    const auto run = [&](const QString &includeFolder) {
        Baloo::Query baloo = balooQuery(plan.searchString);
        baloo.setTypes(plan.types);
        baloo.setIncludeFolder(includeFolder);
#if BALOO_VERSION >= QT_VERSION_CHECK(6, 32, 0)
        if (plan.metadataScope) {
            baloo.setSearchScopes({QStringLiteral("Metadata")});
        }
#endif
        qsizetype found = 0;
        auto iterator = baloo.exec();
        while (iterator.next()) {
            ++found;
            if (cancel && cancel()) {
                result.canceled = true;
                stopped = true;
                break;
            }
            const QString path = iterator.filePath();
            if (seen.contains(path)) {
                continue;
            }
            if (result.scannedEntries >= 200000) {
                result.limited = true;
                result.notices.append(i18nc("@info",
                                            "Baloo found more than 200,000 indexed files for this search, so only the first 200,000 were checked against "
                                            "the criteria it can't apply. Add a name or content term, or limit the depth."));
                stopped = true;
                break;
            }
            seen.insert(path);
            ++result.scannedEntries;
            if (progress) {
                progress(result.scannedEntries, path);
            }
            if (!QDir::isAbsolutePath(path)) {
                continue;
            }
            const bool matches = std::any_of(roots.cbegin(), roots.cend(), [&](const QString &root) {
                return (!scoped || QDir::cleanPath(path) != root) && filter.matches(root, path);
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
                            const QString key = needle.toCaseFolded();
                            return decided.contains(key) || contentMatches.value(key).contains(path);
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
                stopped = true;
                break;
            }
            result.paths.append(path);
        }
        return found;
    };
    if (scoped) {
        // Baloo finds nothing in a folder it hasn't indexed, so search the whole index for those locations.
        bool unindexed = false;
        for (const QString &root : roots) {
            unindexed = run(root) == 0 || unindexed;
            if (stopped) {
                break;
            }
        }
        if (unindexed && !stopped) {
            run({});
        }
    } else {
        run({});
    }
    if (cancel && cancel()) {
        result.canceled = true;
    }
    result.notices.append(i18nc("@info", "Baloo results may be incomplete or stale; Baloo does not report index read errors here."));
#else
    Q_UNUSED(currentFolder)
    Q_UNUSED(progress)
    Q_UNUSED(tagLibraries)
    Q_UNUSED(tagOptions)
    Q_UNUSED(plan)
    result.errors.append(i18nc("@info", "Indexed search is unavailable because Plasma Opus was built without Baloo."));
#endif
    return result;
}
