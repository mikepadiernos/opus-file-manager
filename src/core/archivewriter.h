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
    /// A 7z archive, written with libarchive, without a password.
    SevenZip,
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

/// Creates an AES-encrypted ZIP without exposing the password in process arguments.
QString writeEncryptedZip(const QString &archivePath, Format format, const QStringList &sources, const QString &password);

/// The result of splitting an archive into volumes.
struct Volumes {
    QStringList files;
    QString error;
};

/**
 * Splits a finished archive into numbered volumes of at most @p volumeSize
 * bytes, named like photos.7z.001, as 7-Zip writes them. On success the
 * whole archive is removed; on failure the volumes written so far are removed
 * and the archive is kept. Existing volumes are never overwritten.
 */
Volumes splitIntoVolumes(const QString &archivePath, qint64 volumeSize);

/// The first volume's name for an archive split into volumes, such as photos.7z.001.
QString firstVolumeName(const QString &archiveFileName);
}
