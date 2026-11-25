// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

/**
 * Reads selected EXIF image metadata and ID3 (and other audio) tags with the
 * KDE metadata extractors, for display, search and mapping to library tags
 * and fields. Files are never written.
 */
namespace EmbeddedMetadata
{
struct Value {
    /// A stable identifier, the KFileMetaData property name such as "photoDateTimeOriginal".
    QString key;
    /// The localized property name.
    QString name;
    /// The value as text; several values are joined with "; ". Dates and times use ISO 8601.
    QString text;
    /// The individual values, such as each genre or keyword.
    QStringList items;
    /// The date of date and time properties, otherwise null.
    QDate date;
};

struct Reading {
    QString path;
    QString mimeType;
    /// Whether a KDE extractor reads the file's format.
    bool supported = false;
    /// The supported properties the file has, in a fixed order.
    QList<Value> values;
    /// Why the file's metadata can't be read, or empty.
    QString problem;
};

/// The keys of the properties that are read, images first, then audio.
QStringList keys();
/// The keys of the properties read from images, in keys() order.
QStringList imageKeys();
/// The keys of the properties read from audio files, in keys() order; some are also read from images.
QStringList audioKeys();
/// The keys() not in @p disabled, in keys() order.
QStringList enabledKeys(const QStringList &disabled);
/// The localized name of property @p key.
QString displayName(const QString &key);
/// Whether property @p key holds a date and time.
bool isDate(const QString &key);

/**
 * Reads the supported properties of the regular file at @p path; symbolic links are not followed.
 * Only the properties whose keys are in @p enabled are read; the file isn't opened when none apply.
 */
Reading read(const QString &path, const QStringList &enabled = keys());

/// The texts search criteria match: each value of the properties in @p enabled and, for dates, the date as YYYY-MM-DD.
QStringList searchTexts(const Reading &reading, const QStringList &enabled = keys());

/**
 * Returns a function that reads the search texts of the properties in @p enabled of @p path once, on first call,
 * and then returns them again. Nothing is read when @p enabled is empty.
 */
std::function<QStringList()> lazySearchTexts(const QString &path, const QStringList &enabled = keys());
}
