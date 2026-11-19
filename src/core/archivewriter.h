// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/// Writing new archives from local files and folders with KArchive.
namespace ArchiveWriter
{
/// The formats Compress offers. KArchive has no compression levels, so
/// the format is the only choice of how hard to compress.
enum class Format {
    Zip,
    /// A zip archive whose entries are stored without compression.
    ZipStored,
    TarGz,
    TarXz,
    TarZstd,
};

/// Every format, in the order Compress lists them.
QList<Format> formats();

/// The file name extension of @p format, without the leading dot, such as "tar.gz".
QString suffix(Format format);

/**
 * Writes a new archive. Folders are added with everything below them.
 * Each source keeps its own name at the top of the archive.
 * @param archivePath the archive to create; an existing file is replaced
 * @param format the archive format
 * @param sources local paths of the files and folders to add
 * @returns an empty string on success, otherwise KArchive's error text
 */
QString write(const QString &archivePath, Format format, const QStringList &sources);
}
