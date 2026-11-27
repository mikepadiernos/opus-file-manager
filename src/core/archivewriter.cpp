// SPDX-License-Identifier: GPL-3.0-or-later

#include "archivewriter.h"
#include "filesplitter.h"

#include <KLocalizedString>
#include <KTar>
#include <KZip>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <archive.h>
#include <archive_entry.h>

#include <functional>
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
    case Format::SevenZip:
        return nullptr;
    }
    return nullptr;
}

// Writes a new archive with libarchive; configure chooses the format and options.
QString writeWithLibarchive(const QString &archivePath, const QStringList &sources, const std::function<bool(archive *)> &configure)
{
    if (sources.isEmpty()) {
        return i18nc("@info:status archive creation", "No files or folders were selected for compression.");
    }
    const QString destinationFolder = QFileInfo(archivePath).dir().canonicalPath();
    for (const QString &source : sources) {
        const QFileInfo info(source);
        if (info.isDir() && !info.isSymLink()) {
            const QString sourceFolder = info.canonicalFilePath();
            const QString prefix = sourceFolder.endsWith(u'/') ? sourceFolder : sourceFolder + u'/';
            if (!sourceFolder.isEmpty() && (destinationFolder == sourceFolder || destinationFolder.startsWith(prefix))) {
                return i18nc("@info:status archive creation", "The archive cannot be created inside a folder being compressed: %1", source);
            }
        }
    }
    QFile output(archivePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        return output.errorString();
    }
    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
    using EntryPtr = std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;
    ArchivePtr writer(archive_write_new(), archive_write_free);
    if (!writer) {
        return i18nc("@info:status archive creation", "Could not initialize archive writer.");
    }
    const auto errorText = [&writer] {
        const char *text = archive_error_string(writer.get());
        return text ? QString::fromLocal8Bit(text) : i18nc("@info:status archive creation", "Archive writing failed.");
    };
    if (!configure(writer.get()) || archive_write_open_fd(writer.get(), output.handle()) != ARCHIVE_OK) {
        return errorText();
    }
    std::function<QString(const QString &, const QString &)> add = [&](const QString &path, const QString &name) -> QString {
        const QFileInfo info(path);
        if (info.isSymLink() || (!info.isDir() && !info.isFile())) {
            return i18nc("@info:status archive creation", "Unsupported file type: %1", path);
        }
        EntryPtr entry(archive_entry_new(), archive_entry_free);
        if (!entry) {
            return i18nc("@info:status archive creation", "Could not initialize archive entry.");
        }
        const QByteArray entryName = name.toUtf8();
        archive_entry_set_pathname_utf8(entry.get(), entryName.constData());
        archive_entry_set_filetype(entry.get(), info.isDir() ? AE_IFDIR : AE_IFREG);
        archive_entry_set_perm(entry.get(), info.isDir() ? 0755 : 0644);
        archive_entry_set_size(entry.get(), info.isDir() ? 0 : info.size());
        if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK) {
            return errorText();
        }
        if (info.isDir()) {
            const QDir directory(path);
            if (!directory.isReadable()) {
                return i18nc("@info:status archive creation", "Could not read folder: %1", path);
            }
            const QFileInfoList children = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo &child : children) {
                const QString error = add(child.absoluteFilePath(), name + u'/' + child.fileName());
                if (!error.isEmpty()) {
                    return error;
                }
            }
        } else {
            QFile input(path);
            if (!input.open(QIODevice::ReadOnly)) {
                return input.errorString();
            }
            char buffer[64 * 1024];
            qint64 total = 0;
            while (true) {
                const qint64 count = input.read(buffer, sizeof(buffer));
                if (count < 0) {
                    return input.errorString();
                }
                if (count == 0) {
                    break;
                }
                total += count;
                if (total > info.size()) {
                    return i18nc("@info:status archive creation", "The file changed while being compressed: %1", path);
                }
                if (archive_write_data(writer.get(), buffer, static_cast<size_t>(count)) != count) {
                    return errorText();
                }
            }
            if (total != info.size()) {
                return i18nc("@info:status archive creation", "The file changed while being compressed: %1", path);
            }
        }
        return {};
    };
    for (const QString &source : sources) {
        const QString error = add(source, QFileInfo(source).fileName());
        if (!error.isEmpty()) {
            archive_write_close(writer.get());
            return error;
        }
    }
    if (archive_write_close(writer.get()) != ARCHIVE_OK) {
        return errorText();
    }
    return {};
}
}

QString ArchiveWriter::writeEncryptedZip(const QString &archivePath, Format format, const QStringList &sources, const QString &password)
{
    if ((format != Format::Zip && format != Format::ZipStored) || password.isEmpty()) {
        return i18nc("@info:status archive creation", "An encrypted ZIP requires a password.");
    }
    if (password.contains(QChar::Null)) {
        return i18nc("@info:status archive creation", "The password cannot contain a NUL character.");
    }
    const QByteArray passphrase = password.toUtf8();
    return writeWithLibarchive(archivePath, sources, [format, &passphrase](archive *writer) {
        return archive_write_set_format_zip(writer) == ARCHIVE_OK
            && archive_write_set_options(writer, format == Format::ZipStored ? "zip:encryption=aes256,zip:compression=store" : "zip:encryption=aes256")
            == ARCHIVE_OK
            && archive_write_set_passphrase(writer, passphrase.constData()) == ARCHIVE_OK;
    });
}

QList<ArchiveWriter::Format> ArchiveWriter::formats()
{
    return {Format::Zip, Format::ZipStored, Format::TarGz, Format::TarXz, Format::TarZstd, Format::SevenZip};
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
    case Format::SevenZip:
        return QStringLiteral("7z");
    }
    return {};
}

QString ArchiveWriter::write(const QString &archivePath, Format format, const QStringList &sources)
{
    if (format == Format::SevenZip) {
        return writeWithLibarchive(archivePath, sources, [](archive *writer) {
            return archive_write_set_format_7zip(writer) == ARCHIVE_OK;
        });
    }
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

QString ArchiveWriter::firstVolumeName(const QString &archiveFileName)
{
    return archiveFileName + QStringLiteral(".001");
}

ArchiveWriter::Volumes ArchiveWriter::splitIntoVolumes(const QString &archivePath, qint64 volumeSize)
{
    const FileSplitter::Result split = FileSplitter::split(archivePath, archivePath, volumeSize);
    if (!split.error.isEmpty()) {
        for (const QString &volume : split.outputs) {
            QFile::remove(volume);
        }
        return {{}, split.error};
    }
    if (!QFile::remove(archivePath)) {
        return {split.outputs, i18nc("@info:status archive creation", "The volumes were written, but the whole archive could not be removed: %1", archivePath)};
    }
    return {split.outputs, {}};
}
