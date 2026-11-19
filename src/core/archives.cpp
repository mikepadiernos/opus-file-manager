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

#include <QFileInfo>
#include <QMimeDatabase>

#include <memory>

namespace
{
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
    const QString suffix = QFileInfo(fileName).suffix().toLower();
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
