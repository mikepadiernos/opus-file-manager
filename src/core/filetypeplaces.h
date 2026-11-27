// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * File-type categories shown in Places beside KDE's Search For entries. Each
 * entry has a placeholder URL that opens the category as a Baloo search when
 * indexing can answer it, or explains why not and offers a live search.
 */
namespace FileTypePlaces
{
enum class Category {
    Documents,
    Images,
    Audio,
    Video,
    Archives,
    Ebooks,
    Packages,
    LargeFiles,
};

/// All categories, in the order they appear in Places.
QList<Category> categories();
/// A stable identifier, such as "documents".
QString id(Category category);
/// The translated place name.
QString title(Category category);
QString iconName(Category category);

/**
 * The URL stored for the category's place. Its scheme contains "search", so
 * KDE's places model groups it under Search For; no KIO worker handles it.
 */
QUrl placeUrl(Category category);
bool isPlaceUrl(const QUrl &url);
/// The category of a place URL; false when @p url is not one.
bool fromPlaceUrl(const QUrl &url, Category *category);

/// Whether the category can be answered by the file index. Baloo doesn't index sizes, so large files can't.
bool usesIndex(Category category);
/// The Baloo file type the category selects, or an empty string when it uses balooSearchString().
QString balooType(Category category);
/// The Baloo search string for categories without a Baloo type, matching their MIME types.
QString balooSearchString(Category category);
/// The MIME types a Baloo search string matches.
QStringList mimeTypes(Category category);
/// A baloosearch: URL titled with the category, or an empty URL without Baloo or for large files.
QUrl balooUrl(Category category);

/// The size above which a file is large, in bytes.
qint64 largeFileSize();
/// An advanced search query that finds the category's files during a live search.
QString liveQuery(Category category);
}
