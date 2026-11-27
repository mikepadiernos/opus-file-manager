// SPDX-License-Identifier: GPL-3.0-or-later

#include "sharedsearch.h"

#include "indexedsearch.h"
#include "searchquery.h"

#include <KLocalizedString>

#include <QDir>
#include <QRegularExpression>
#include <QUrlQuery>

#include <optional>
#include <vector>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/Query>
#endif

namespace
{
using SearchQuery::Field;
using SearchQuery::Node;
using SearchQuery::TypeMatch;

QString quoted(QString text)
{
    text.remove(QLatin1Char('"'));
    return QLatin1Char('"') + text + QLatin1Char('"');
}

// The Baloo search string for a criterion Baloo matches as Plasma Opus does, or nothing.
std::optional<QString> expression(const Node &node, const QStringList &metadataKeys)
{
    switch (node.kind) {
    case Node::Kind::Term: {
        const SearchQuery::Term &term = node.term;
        switch (term.field) {
        case Field::Content:
            return QStringLiteral("content:") + quoted(term.text);
        case Field::Name:
            // Baloo matches words in names; it has no wildcards or whole-name comparison.
            if (term.comparison != SearchQuery::Comparison::Contains || !term.pattern.pattern().isEmpty()) {
                return std::nullopt;
            }
            return QStringLiteral("filename:") + quoted(term.text);
        case Field::Metadata: {
            static const QRegularExpression numeric(QStringLiteral("^[0-9.:/ -]+$"));
            if (numeric.match(term.text).hasMatch()) {
                return std::nullopt;
            }
            return IndexedSearch::balooSearchString(term, metadataKeys);
        }
        case Field::Modified: {
            // A shared place can't move today and yesterday along with the date.
            const QString text = term.text.toLower();
            if (text == QLatin1String("today") || text == QLatin1String("yesterday")) {
                return std::nullopt;
            }
            return QLatin1Char('(') + IndexedSearch::balooDateString(term) + QLatin1Char(')');
        }
        case Field::Type:
            if (term.type == TypeMatch::MimeType) {
                return QStringLiteral("mimetype:") + quoted(term.text);
            }
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }
    case Node::Kind::And:
    case Node::Kind::Or: {
        QStringList parts;
        for (const Node &child : node.children) {
            const auto part = expression(child, metadataKeys);
            if (!part) {
                return std::nullopt;
            }
            parts.append(*part);
        }
        return QLatin1Char('(') + parts.join(node.kind == Node::Kind::And ? QStringLiteral(" AND ") : QStringLiteral(" OR ")) + QLatin1Char(')');
    }
    case Node::Kind::Not:
        break;
    }
    return std::nullopt;
}

SharedSearch::Translation failure(const QString &error)
{
    SharedSearch::Translation result;
    result.error = error;
    return result;
}
}

SharedSearch::Translation
SharedSearch::translate(const SavedSearches::Search &search, const QStringList &searchFolders, const QStringList &exclusions, const QStringList &metadataKeys)
{
    Translation result;
    const QString text = search.text.trimmed();
    if (text.isEmpty()) {
        return failure(i18nc("@info", "This saved search has no search text."));
    }
    QStringList parts;
    switch (search.field) {
    case LiveSearch::Field::FileName:
        if (text.contains(QLatin1Char('*')) || text.contains(QLatin1Char('?'))) {
            return failure(i18nc("@info", "Baloo can't match wildcards such as * and ? in names, so this search can't be shared."));
        }
        parts.append(QStringLiteral("filename:") + quoted(text));
        break;
    case LiveSearch::Field::Contents:
        parts.append(QStringLiteral("content:") + quoted(text));
        break;
    case LiveSearch::Field::Query: {
        const SearchQuery::Query query = SearchQuery::Query::parse(text);
        if (!query.isValid()) {
            return failure(query.error());
        }
        const Node &root = query.root();
        std::vector<const Node *> required;
        if (root.kind == Node::Kind::And) {
            for (const Node &child : root.children) {
                required.push_back(&child);
            }
        } else {
            required.push_back(&root);
        }
        for (const Node *node : required) {
            if (node->kind == Node::Kind::Term && node->term.field == Field::Type
                && (node->term.type == TypeMatch::Folder || node->term.type == TypeMatch::MimePrefix)) {
                if (const QString type = IndexedSearch::balooType(node->term); !type.isEmpty()) {
                    if (!result.types.contains(type)) {
                        result.types.append(type);
                    }
                    continue;
                }
            }
            if (const auto part = expression(*node, metadataKeys)) {
                parts.append(*part);
            } else {
                result.omitted.append(SearchQuery::toString(*node));
            }
        }
        if (parts.isEmpty() && result.types.isEmpty()) {
            return failure(i18nc("@info",
                                 "Baloo can't express any criterion of this query, so a shared place would list every indexed file. "
                                 "Add a name or content term that every result must match."));
        }
        break;
    }
    }
    result.searchString = parts.join(QStringLiteral(" AND "));

    if (search.searchFolders) {
        QStringList roots;
        for (const QString &folder : searchFolders) {
            if (QDir::isAbsolutePath(folder) && !roots.contains(QDir::cleanPath(folder))) {
                roots.append(QDir::cleanPath(folder));
            }
        }
        if (roots.isEmpty()) {
            return failure(i18nc("@info", "No search folders are set. Add them in Preferences, on the Search page of Tools."));
        }
        if (roots.size() == 1) {
            result.folder = roots.first();
        } else {
            result.omitted.append(i18nc("@item %1 is a list of folders",
                                        "Search folders %1: Baloo limits a search to one folder, so the place searches every indexed location",
                                        roots.join(QStringLiteral(", "))));
        }
    } else if (!search.indexed || search.filter.maxDepth >= 0) {
        // Unlimited indexed searches cover the whole index, so only a depth limit or a live search makes the folder matter.
        const QString folder = search.folder.isLocalFile() ? search.folder.toLocalFile() : QString();
        if (!QDir::isAbsolutePath(folder)) {
            return failure(i18nc("@info %1 is a location",
                                 "Baloo indexes local folders only, so a search of %1 can't be shared.",
                                 search.folder.toDisplayString(QUrl::RemoveUserInfo)));
        }
        result.folder = QDir::cleanPath(folder);
    }

    if (search.filter.maxDepth >= 0) {
        result.omitted.append(i18ncp("@item", "Depth limit of %1 level", "Depth limit of %1 levels", search.filter.maxDepth));
    }
    if (!search.filter.pathText.trimmed().isEmpty()) {
        result.omitted.append(i18nc("@item", "Path contains: %1", search.filter.pathText.trimmed()));
    }
    if (!search.filter.parentText.trimmed().isEmpty()) {
        result.omitted.append(i18nc("@item", "Parent path contains: %1", search.filter.parentText.trimmed()));
    }
    QStringList excluded;
    for (const QString &folder : search.filter.excludedFolders + exclusions) {
        if (!folder.trimmed().isEmpty() && !excluded.contains(QDir::cleanPath(folder))) {
            excluded.append(QDir::cleanPath(folder));
        }
    }
    if (!excluded.isEmpty()) {
        result.omitted.append(i18nc("@item %1 is a list of folders", "Excluded folders: %1", excluded.join(QStringLiteral(", "))));
    }
    return result;
}

QUrl SharedSearch::url(const Translation &translation, const QString &title)
{
#ifdef OPUSFM_HAVE_BALOO
    if (!translation.error.isEmpty()) {
        return {};
    }
    Baloo::Query query;
    query.setSearchString(translation.searchString);
    query.setTypes(translation.types);
    query.setIncludeFolder(translation.folder);
    return query.toSearchUrl(title);
#else
    Q_UNUSED(translation)
    Q_UNUSED(title)
    return {};
#endif
}

bool SharedSearch::sameSearch(const QUrl &first, const QUrl &second)
{
    if (first.scheme() != QLatin1String("baloosearch") || second.scheme() != QLatin1String("baloosearch")) {
        return false;
    }
    const QUrlQuery a(first);
    const QUrlQuery b(second);
    for (const QString &key : {QStringLiteral("json"), QStringLiteral("title")}) {
        if (a.queryItemValue(key, QUrl::FullyDecoded) != b.queryItemValue(key, QUrl::FullyDecoded)) {
            return false;
        }
    }
    return true;
}
