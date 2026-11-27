// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "savedsearches.h"

#include <QStringList>
#include <QUrl>

/**
 * Saved searches offered to KDE's shared places as baloosearch: locations, so
 * Dolphin and file dialogs can list what Baloo finds for them. A shared entry
 * is a copy: Plasma Opus never updates or removes it without being asked.
 */
namespace SharedSearch
{
struct Translation {
    /// What Baloo matches: content, file-name, metadata, MIME type and date terms.
    QString searchString;
    /// Baloo types every match has, such as Image or Folder.
    QStringList types;
    /// The local folder Baloo limits results to, or empty for every indexed location.
    QString folder;
    /// Criteria Baloo can't express, which the shared place ignores.
    QStringList omitted;
    /// Why the search can't be shared; when set, the other fields are empty.
    QString error;
};

/**
 * Translates @p search into a Baloo query. Top-level criteria Baloo can't
 * express are listed in Translation::omitted rather than approximated.
 * @param searchFolders the configured search folders, for searches of them
 * @param exclusions the folders excluded from every Plasma Opus search
 * @param metadataKeys the embedded metadata properties meta: terms match
 */
Translation translate(const SavedSearches::Search &search, const QStringList &searchFolders, const QStringList &exclusions, const QStringList &metadataKeys);

/// A baloosearch: URL titled @p title, or an empty URL without Baloo or when the translation failed.
QUrl url(const Translation &translation, const QString &title);

/// Whether two baloosearch: URLs describe the same search and title, however their queries are encoded.
bool sameSearch(const QUrl &first, const QUrl &second);
}
