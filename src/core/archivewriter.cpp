// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivewriter.h"

#include <KTar>
#include <KZip>

#include <QFileInfo>

#include <memory>

namespace
{
std::unique_ptr<KArchive> createArchive(const QString &archivePath, ArchiveWriter::Format format)
{
    using ArchiveWriter::Format;
    switch (format) {
    case Format::Zip:
    case Format::ZipStored: {
        auto zip = std::make_unique<KZip>(archivePath);
        zip->setCompression(format == Format::Zip ? KZip::DeflateCompression : KZip::NoCompression);
        return zip;
    }
    case Format::TarGz:
        return std::make_unique<KTar>(archivePath, QStringLiteral("application/x-compressed-tar"));
    case Format::TarXz:
        return std::make_unique<KTar>(archivePath, QStringLiteral("application/x-xz-compressed-tar"));
    case Format::TarZstd:
        return std::make_unique<KTar>(archivePath, QStringLiteral("application/x-zstd-compressed-tar"));
    }
    return nullptr;
}
}

QList<ArchiveWriter::Format> ArchiveWriter::formats()
{
    return {Format::Zip, Format::ZipStored, Format::TarGz, Format::TarXz, Format::TarZstd};
}

QString ArchiveWriter::suffix(Format format)
{
    switch (format) {
    case Format::Zip:
    case Format::ZipStored:
        return QStringLiteral("zip");
    case Format::TarGz:
        return QStringLiteral("tar.gz");
    case Format::TarXz:
        return QStringLiteral("tar.xz");
    case Format::TarZstd:
        return QStringLiteral("tar.zst");
    }
    return {};
}

QString ArchiveWriter::write(const QString &archivePath, Format format, const QStringList &sources)
{
    const std::unique_ptr<KArchive> archive = createArchive(archivePath, format);
    if (!archive->open(QIODevice::WriteOnly)) {
        return archive->errorString();
    }
    for (const QString &source : sources) {
        const QFileInfo info(source);
        const bool added = info.isDir() ? archive->addLocalDirectory(source, info.fileName()) : archive->addLocalFile(source, info.fileName());
        if (!added) {
            const QString error = archive->errorString();
            archive->close();
            return error;
        }
    }
    if (!archive->close()) {
        return archive->errorString();
    }
    return {};
}
