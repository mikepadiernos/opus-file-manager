// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagsearch.h"

#include "libraryscan.h"
#include "tagassignment.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace
{
bool within(const QString &path, const QString &root)
{
    return path.startsWith(root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/'));
}

bool matchesTag(const TagLibrary::Tag &tag, const SearchQuery::Term &term)
{
    const auto matches = [&term](const QString &text) {
        return !text.isEmpty() && term.pattern.match(text).hasMatch();
    };
    return matches(tag.name) || matches(tag.shorthand) || std::any_of(tag.aliases.cbegin(), tag.aliases.cend(), matches);
}
}

QSet<QString> TagSearch::selectedTags(const TagLibrary::Library &library, const SearchQuery::Term &term)
{
    QSet<QString> selected;
    if (term.field != SearchQuery::Field::Tag) {
        return selected;
    }
    QStringList found;
    if (term.pattern.pattern().isEmpty()) {
        found = library.findTags(term.text);
    } else {
        for (const TagLibrary::Tag &tag : library.tags()) {
            if (matchesTag(tag, term)) {
                found.append(tag.id);
            }
        }
    }
    for (const QString &id : std::as_const(found)) {
        selected.insert(id);
        if (term.comparison != SearchQuery::Comparison::Equal) {
            for (const QString &child : library.descendants(id)) {
                selected.insert(child);
            }
        }
    }
    return selected;
}

TagSearch::Context::Context(const QStringList &roots)
{
    for (const QString &root : roots) {
        if (QDir::isAbsolutePath(root)) {
            m_roots.append(QDir::cleanPath(root));
        }
    }
}

SearchQuery::TagMatcher TagSearch::Context::matcher(const QString &absolutePath)
{
    return [this, absolutePath](const SearchQuery::Term &term) {
        return matches(absolutePath, term);
    };
}

bool TagSearch::Context::matches(const QString &absolutePath, const SearchQuery::Term &term)
{
    const QString path = QDir::cleanPath(absolutePath);
    if (!LibraryScan::indexable(QFileInfo(path).fileName())) {
        return false;
    }
    const auto root = std::find_if(m_roots.cbegin(), m_roots.cend(), [&path](const QString &root) {
        return within(path, root);
    });
    if (root == m_roots.cend()) {
        return false;
    }
    auto state = m_libraries.find(*root);
    if (state == m_libraries.end()) {
        QString error;
        State loaded;
        loaded.library = TagAssignment::cachedLibrary(*root, &error);
        if (loaded.library.isNull()) {
            m_errors.append(i18nc("@info", "Could not read the tag library in %1: %2", *root, error));
        }
        state = m_libraries.insert(*root, loaded);
    }
    const TagLibrary::Library &library = state->library;
    if (library.isNull()) {
        return false;
    }
    // A missing entry's metadata may describe a different file now at its path.
    const TagLibrary::Entry *entry = library.entry(library.relativePath(path));
    const QStringList tags = entry && !entry->missing ? entry->tags : QStringList();
    if (term.field == SearchQuery::Field::Tagged) {
        return tags.isEmpty() != term.tagged;
    }
    if (term.field != SearchQuery::Field::Tag || tags.isEmpty()) {
        return false;
    }
    const QString key = QString::number(int(term.comparison)) + term.pattern.pattern() + QLatin1Char('\n') + term.text;
    auto selection = state->selections.find(key);
    if (selection == state->selections.end()) {
        selection = state->selections.insert(key, selectedTags(library, term));
    }
    return std::any_of(tags.cbegin(), tags.cend(), [&selection](const QString &id) {
        return selection->contains(id);
    });
}

QStringList TagSearch::Context::errors() const
{
    return m_errors;
}
