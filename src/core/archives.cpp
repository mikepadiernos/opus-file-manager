// SPDX-License-Identifier: GPL-3.0-or-later

#include "archives.h"

#include <K7Zip>
#include <KArchiveDirectory>
#include <KArchiveEntry>
#include <KArchiveFile>
#include <KLocalizedString>
#include <KProtocolManager>
#include <KTar>
#include <KZip>

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>

#include <archive.h>
#include <archive_entry.h>

#include <functional>
#include <memory>

namespace
{
bool hasEncryptedSevenZipHeader(const QString &fileName)
{
    using ReadPtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ReadPtr reader(archive_read_new(), archive_read_free);
    if (!reader || archive_read_support_format_7zip(reader.get()) != ARCHIVE_OK) {
        return false;
    }
    const QByteArray path = QFile::encodeName(fileName);
    if (archive_read_open_filename(reader.get(), path.constData(), 64 * 1024) != ARCHIVE_OK) {
        return false;
    }
    archive_entry *entry = nullptr;
    const int status = archive_read_next_header(reader.get(), &entry);
    const char *error = archive_error_string(reader.get());
    return status < ARCHIVE_OK && error && QByteArray(error) == QByteArrayLiteral("The archive header is encrypted, but currently not supported");
}

Archives::TestResult testZipIntegrity(const QString &fileName, const QString &password, QString *error)
{
    using ReadPtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ReadPtr reader(archive_read_new(), archive_read_free);
    if (!reader) {
        *error = i18nc("@info:status archive integrity", "Could not initialize archive reader.");
        return Archives::TestResult::Failed;
    }
    const auto fail = [&reader, error] {
        const char *text = archive_error_string(reader.get());
        *error = text ? QString::fromLocal8Bit(text) : i18nc("@info:status archive integrity", "Archive integrity check failed.");
        return Archives::TestResult::Failed;
    };
    const QByteArray passwordBytes = password.toUtf8();
    const QByteArray path = QFile::encodeName(fileName);
    if (archive_read_support_format_zip(reader.get()) != ARCHIVE_OK
        || (!password.isEmpty() && archive_read_add_passphrase(reader.get(), passwordBytes.constData()) != ARCHIVE_OK)
        || archive_read_open_filename(reader.get(), path.constData(), 64 * 1024) != ARCHIVE_OK) {
        return fail();
    }
    archive_entry *entry = nullptr;
    int status;
    while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        if (archive_entry_is_encrypted(entry) == 1 && password.isEmpty()) {
            return Archives::TestResult::PasswordRequired;
        }
        char buffer[64 * 1024];
        la_ssize_t count;
        while ((count = archive_read_data(reader.get(), buffer, sizeof(buffer))) > 0) { }
        if (count < 0) {
            return fail();
        }
    }
    if (status != ARCHIVE_EOF) {
        return fail();
    }
    return Archives::TestResult::Passed;
}

bool readArchiveDirectory(const KArchiveDirectory *directory, QString *error)
{
    for (const QString &name : directory->entries()) {
        const KArchiveEntry *entry = directory->entry(name);
        if (!entry) {
            *error = i18nc("@info:status archive integrity", "Missing archive entry: %1", name);
            return false;
        }
        if (entry->isDirectory()) {
            if (!readArchiveDirectory(static_cast<const KArchiveDirectory *>(entry), error)) {
                return false;
            }
            continue;
        }
        if (!entry->isFile()) {
            continue;
        }

        const auto *file = static_cast<const KArchiveFile *>(entry);
        std::unique_ptr<QIODevice> device(file->createDevice());
        if (!device || !device->isOpen()) {
            *error = i18nc("@info:status archive integrity", "Could not read archive entry: %1", file->name());
            return false;
        }
        char buffer[64 * 1024];
        qint64 bytesRead = 0;
        while (true) {
            const qint64 count = device->read(buffer, sizeof(buffer));
            if (count < 0) {
                *error = i18nc("@info:status archive integrity", "Could not decompress archive entry: %1", file->name());
                return false;
            }
            if (count == 0) {
                break;
            }
            bytesRead += count;
            if (bytesRead > file->size()) {
                *error = i18nc("@info:status archive integrity", "Archive entry is larger than advertised: %1", file->name());
                return false;
            }
        }
        if (bytesRead != file->size()) {
            *error = i18nc("@info:status archive integrity", "Archive entry is truncated: %1", file->name());
            return false;
        }
    }
    return true;
}

QString extractSevenZip(const QString &fileName, const QString &destination, const QString &password)
{
    K7Zip source(fileName);
    source.setPassword(password);
    if (!source.open(QIODevice::ReadOnly)) {
        return source.errorString().isEmpty() ? i18nc("@info:status archive extraction", "Could not open the 7z archive.") : source.errorString();
    }
    using DiskPtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
    using EntryPtr = std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;
    DiskPtr disk(archive_write_disk_new(), archive_write_free);
    if (!disk) {
        return i18nc("@info:status archive extraction", "Could not initialize archive extraction.");
    }
    if (!QDir().mkdir(destination)) {
        return i18nc("@info:status archive extraction", "Could not create destination folder: %1", destination);
    }
    archive_write_disk_set_options(disk.get(), ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_NO_OVERWRITE);
    const auto diskError = [&disk] {
        const char *text = archive_error_string(disk.get());
        return text ? QString::fromLocal8Bit(text) : i18nc("@info:status archive extraction", "Archive extraction failed.");
    };
    std::function<QString(const KArchiveDirectory *, const QString &)> extract = [&](const KArchiveDirectory *directory, const QString &parent) -> QString {
        for (const QString &name : directory->entries()) {
            const KArchiveEntry *item = directory->entry(name);
            if (!item || name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..") || name.contains(u'/') || name.contains(u'\\')
                || !item->symLinkTarget().isEmpty() || (!item->isDirectory() && !item->isFile())) {
                return i18nc("@info:status archive extraction", "Unsafe or unsupported archive entry: %1", name);
            }
            const QString path = QDir(parent).filePath(name);
            EntryPtr entry(archive_entry_new(), archive_entry_free);
            if (!entry) {
                return i18nc("@info:status archive extraction", "Could not initialize archive entry.");
            }
            const QByteArray pathBytes = QFile::encodeName(path);
            archive_entry_set_pathname(entry.get(), pathBytes.constData());
            archive_entry_set_filetype(entry.get(), item->isDirectory() ? AE_IFDIR : AE_IFREG);
            archive_entry_set_perm(entry.get(), item->isDirectory() ? 0700 : 0600);
            const auto *file = item->isFile() ? static_cast<const KArchiveFile *>(item) : nullptr;
            archive_entry_set_size(entry.get(), file ? file->size() : 0);
            if (archive_write_header(disk.get(), entry.get()) != ARCHIVE_OK) {
                return diskError();
            }
            if (file) {
                std::unique_ptr<QIODevice> input(file->createDevice());
                if (!input || !input->isOpen()) {
                    return i18nc("@info:status archive extraction", "Could not read archive entry: %1", name);
                }
                qint64 total = 0;
                char buffer[64 * 1024];
                while (true) {
                    const qint64 count = input->read(buffer, sizeof(buffer));
                    if (count < 0) {
                        return input->errorString();
                    }
                    if (count == 0) {
                        break;
                    }
                    total += count;
                    if (total > file->size()) {
                        return i18nc("@info:status archive extraction", "Archive entry is larger than advertised: %1", name);
                    }
                    if (archive_write_data(disk.get(), buffer, static_cast<size_t>(count)) != count) {
                        return diskError();
                    }
                }
                if (total != file->size()) {
                    return i18nc("@info:status archive extraction", "Archive entry is truncated: %1", name);
                }
            }
            if (archive_write_finish_entry(disk.get()) != ARCHIVE_OK) {
                return diskError();
            }
            if (item->isDirectory()) {
                const QString error = extract(static_cast<const KArchiveDirectory *>(item), path);
                if (!error.isEmpty()) {
                    return error;
                }
            }
        }
        return {};
    };
    const KArchiveDirectory *root = source.directory();
    if (!root) {
        return i18nc("@info:status archive extraction", "Could not read archive contents.");
    }
    const QString error = extract(root, destination);
    if (!error.isEmpty()) {
        return error;
    }
    if (archive_write_close(disk.get()) != ARCHIVE_OK) {
        return diskError();
    }
    if (!source.close()) {
        return source.errorString().isEmpty() ? i18nc("@info:status archive extraction", "Could not close the 7z archive.") : source.errorString();
    }
    return {};
}
}

QUrl Archives::browseUrl(const QUrl &file, const QString &mimeType)
{
    if (!file.isLocalFile()) {
        return {};
    }
    const QString protocol = KProtocolManager::protocolForArchiveMimetype(mimeType);
    if (protocol.isEmpty()) {
        return {};
    }
    QUrl url;
    url.setScheme(protocol);
    url.setPath(file.toLocalFile() + u'/');
    return url;
}

QString Archives::baseName(const QString &fileName)
{
    const QString suffix = QMimeDatabase().suffixForFileName(fileName);
    return suffix.isEmpty() ? fileName : fileName.chopped(suffix.size() + 1);
}

Archives::TestResult Archives::testIntegrity(const QString &fileName, const QString &password, QString *error)
{
    QString localError;
    QString *resultError = error ? error : &localError;
    resultError->clear();
    if (password.contains(QChar::Null)) {
        *resultError = i18nc("@info:status archive integrity", "The password cannot contain a NUL character.");
        return TestResult::Failed;
    }
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("zip")) {
        return testZipIntegrity(fileName, password, resultError);
    }
    if (suffix == QLatin1String("7z") && password.isEmpty() && hasEncryptedSevenZipHeader(fileName)) {
        return TestResult::PasswordRequired;
    }
    if (suffix == QLatin1String("7z")) {
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("7z"));
        if (!executable.isEmpty()) {
            if (password.contains(u'\n') || password.contains(u'\r') || password.contains(QChar::Null)) {
                *resultError = i18nc("@info:status archive integrity", "The password contains a character that cannot be passed to 7z.");
                return TestResult::Failed;
            }
            QProcess process;
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
            process.setProcessEnvironment(environment);
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(executable, {QStringLiteral("t"), QStringLiteral("-bd"), QStringLiteral("--"), fileName});
            if (!process.waitForStarted()) {
                *resultError = process.errorString();
                return TestResult::Failed;
            }
            const QByteArray input = password.toUtf8() + '\n';
            if (process.write(input) != input.size()) {
                *resultError = process.errorString();
                process.kill();
                process.waitForFinished();
                return TestResult::Failed;
            }
            process.closeWriteChannel();
            if (!process.waitForFinished(-1)) {
                *resultError = process.errorString();
                return TestResult::Failed;
            }
            const QByteArray output = process.readAll();
            if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
                return TestResult::Passed;
            }
            if (password.isEmpty() && output.contains("Enter password")) {
                return TestResult::PasswordRequired;
            }
            *resultError = QString::fromLocal8Bit(output).trimmed();
            if (resultError->isEmpty()) {
                *resultError = process.errorString();
            }
            return TestResult::Failed;
        }
    }
    std::unique_ptr<KArchive> archive;
    K7Zip *sevenZip = nullptr;
    if (suffix == QLatin1String("7z")) {
        auto candidate = std::make_unique<K7Zip>(fileName);
        sevenZip = candidate.get();
        if (!password.isEmpty()) {
            sevenZip->setPassword(password);
        }

        archive = std::move(candidate);
    } else if (suffix == QLatin1String("zip")) {
        archive = std::make_unique<KZip>(fileName);
    } else if (suffix == QLatin1String("tar") || suffix == QLatin1String("tgz") || suffix == QLatin1String("gz") || suffix == QLatin1String("bz2")
               || suffix == QLatin1String("xz") || suffix == QLatin1String("zst")) {
        archive = std::make_unique<KTar>(fileName);
    } else {
        *resultError = i18nc("@info:status archive integrity", "Unsupported archive format: %1", suffix);
        return TestResult::Failed;
    }

    if (!archive->open(QIODevice::ReadOnly)) {
        if (sevenZip && sevenZip->passwordNeeded() && password.isEmpty()) {
            return TestResult::PasswordRequired;
        }
        *resultError = archive->errorString();
        return TestResult::Failed;
    }
    const KArchiveDirectory *root = archive->directory();
    const bool read = root && readArchiveDirectory(root, resultError);
    const bool closed = archive->close();
    if (!read || !closed) {
        if (resultError->isEmpty()) {
            *resultError = archive->errorString();
        }
        return TestResult::Failed;
    }
    return TestResult::Passed;
}

QString Archives::extractProtected(const QString &fileName, const QString &destination, const QString &password)
{
    if (password.isEmpty() || !QDir::isAbsolutePath(destination)) {
        return i18nc("@info:status archive extraction", "A password and an absolute destination are required.");
    }
    if (password.contains(QChar::Null)) {
        return i18nc("@info:status archive extraction", "The password cannot contain a NUL character.");
    }
    if (QFileInfo(fileName).suffix().compare(QLatin1String("7z"), Qt::CaseInsensitive) == 0) {
        return extractSevenZip(fileName, destination, password);
    }
    using ReadPtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    using DiskPtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
    ReadPtr reader(archive_read_new(), archive_read_free);
    DiskPtr disk(archive_write_disk_new(), archive_write_free);
    if (!reader || !disk) {
        return i18nc("@info:status archive extraction", "Could not initialize archive extraction.");
    }
    const auto errorText = [](archive *object) {
        const char *text = archive_error_string(object);
        return text ? QString::fromLocal8Bit(text) : i18nc("@info:status archive extraction", "Archive extraction failed.");
    };
    const QByteArray passwordBytes = password.toUtf8();
    const QByteArray archiveName = QFile::encodeName(fileName);
    if (archive_read_support_filter_all(reader.get()) != ARCHIVE_OK || archive_read_support_format_all(reader.get()) != ARCHIVE_OK
        || archive_read_add_passphrase(reader.get(), passwordBytes.constData()) != ARCHIVE_OK
        || archive_read_open_filename(reader.get(), archiveName.constData(), 64 * 1024) != ARCHIVE_OK) {
        return errorText(reader.get());
    }
    if (!QDir().mkdir(destination)) {
        return i18nc("@info:status archive extraction", "Could not create destination folder: %1", destination);
    }
    archive_write_disk_set_options(disk.get(), ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_NO_OVERWRITE);
    archive_entry *entry = nullptr;
    int status;
    while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const char *rawName = archive_entry_pathname_utf8(entry);
        const QString name = rawName ? QString::fromUtf8(rawName) : QString();
        const QStringList segments = name.split(u'/', Qt::SkipEmptyParts);
        if (name.isEmpty() || name.startsWith(u'/') || name.contains(u'\\') || segments.contains(QStringLiteral("..")) || segments.contains(QStringLiteral("."))
            || archive_entry_symlink(entry) || archive_entry_hardlink(entry)
            || (archive_entry_filetype(entry) != AE_IFREG && archive_entry_filetype(entry) != AE_IFDIR)) {
            return i18nc("@info:status archive extraction", "Unsafe or unsupported archive entry: %1", name);
        }
        const QByteArray path = QFile::encodeName(QDir(destination).filePath(name));
        archive_entry_set_pathname(entry, path.constData());
        if (archive_write_header(disk.get(), entry) != ARCHIVE_OK) {
            return errorText(disk.get());
        }
        char buffer[64 * 1024];
        while (true) {
            const la_ssize_t count = archive_read_data(reader.get(), buffer, sizeof(buffer));
            if (count < 0) {
                return errorText(reader.get());
            }
            if (count == 0) {
                break;
            }
            if (archive_write_data(disk.get(), buffer, static_cast<size_t>(count)) != count) {
                return errorText(disk.get());
            }
        }
        if (archive_write_finish_entry(disk.get()) != ARCHIVE_OK) {
            return errorText(disk.get());
        }
    }
    if (status != ARCHIVE_EOF) {
        return errorText(reader.get());
    }
    if (archive_write_close(disk.get()) != ARCHIVE_OK) {
        return errorText(disk.get());
    }
    return {};
}
