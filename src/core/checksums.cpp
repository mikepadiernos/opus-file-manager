// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksums.h"

#include <KLocalizedString>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr qint64 MaxLine = 65536;
constexpr int MaxEntries = 10000;

bool canceled(const Checksums::Cancel &cancel)
{
    return cancel && cancel();
}

bool validName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") && !name.contains(QLatin1Char('/')) && !name.contains(QChar::Null);
}

QString validateFile(const QString &path)
{
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isFile()) {
        return i18nc("@info", "Not an existing regular file, or is a symbolic link.");
    }
    return {};
}

QString openRegular(QFile &file, const QString &path)
{
    const QByteArray nativePath = QFile::encodeName(path);
    const int fd = ::open(nativePath.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return QString::fromLocal8Bit(std::strerror(errno));
    }
    struct stat status;
    if (::fstat(fd, &status) != 0) {
        const QString error = QString::fromLocal8Bit(std::strerror(errno));
        ::close(fd);
        return error;
    }
    if (!S_ISREG(status.st_mode)) {
        ::close(fd);
        return i18nc("@info", "Not a regular file.");
    }
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return file.errorString();
    }
    return {};
}

QString hashFile(const QString &path, QByteArray &digest, const Checksums::Cancel &cancel)
{
    QFile file(path);
    const QString openError = openRegular(file, path);
    if (!openError.isEmpty()) {
        return openError;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!canceled(cancel)) {
        const QByteArray chunk = file.read(256 * 1024);
        if (chunk.isEmpty()) {
            if (file.error() != QFileDevice::NoError) {
                return file.errorString();
            }
            digest = hash.result().toHex();
            return {};
        }
        hash.addData(chunk);
    }
    return {};
}

QByteArray escapedName(const QString &name, bool &escaped)
{
    QByteArray bytes = name.toUtf8();
    escaped = bytes.contains('\\') || bytes.contains('\n') || bytes.contains('\r');
    if (escaped) {
        bytes.replace("\\", "\\\\");
        bytes.replace("\n", "\\n");
        bytes.replace("\r", "\\r");
    }
    return bytes;
}

bool parseLine(QByteArray line, QString &name, QByteArray &digest)
{
    if (line.endsWith('\n')) {
        line.chop(1);
    }
    if (line.endsWith('\r')) {
        line.chop(1);
    }
    const bool escaped = line.startsWith('\\');
    if (escaped) {
        line.remove(0, 1);
    }
    if (line.size() < 67 || line.at(64) != ' ' || (line.at(65) != ' ' && line.at(65) != '*')) {
        return false;
    }
    digest = line.first(64).toLower();
    for (char c : digest) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    QByteArray bytes = line.sliced(66);
    if (escaped) {
        QByteArray decoded;
        for (qsizetype i = 0; i < bytes.size(); ++i) {
            if (bytes.at(i) != '\\') {
                decoded.append(bytes.at(i));
                continue;
            }
            if (++i == bytes.size()) {
                return false;
            }
            switch (bytes.at(i)) {
            case '\\':
                decoded.append('\\');
                break;
            case 'n':
                decoded.append('\n');
                break;
            case 'r':
                decoded.append('\r');
                break;
            default:
                return false;
            }
        }
        bytes = decoded;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    name = decoder.decode(bytes);
    return !decoder.hasError() && validName(name);
}
}

Checksums::Result Checksums::create(const QStringList &paths, const QString &manifest, const Cancel &cancel, const Progress &progress)
{
    Result result;
    if (paths.isEmpty() || paths.size() > MaxEntries) {
        result.error = i18nc("@info", "Select between 1 and %1 files.", MaxEntries);
        return result;
    }
    const QFileInfo output(manifest);
    const QString parent = output.absolutePath();
    for (const QString &path : paths) {
        const QFileInfo input(path);
        if (!input.isAbsolute() || input.absolutePath() != parent || !validName(input.fileName())) {
            result.error = i18nc("@info", "The checksum file and all selected files must be in the same folder.");
            return result;
        }
        if (input.absoluteFilePath() == output.absoluteFilePath()
            || (!input.canonicalFilePath().isEmpty() && input.canonicalFilePath() == output.canonicalFilePath())) {
            result.error = i18nc("@info", "The checksum file must not overwrite an input file.");
            return result;
        }
        const QString error = validateFile(path);
        if (!error.isEmpty()) {
            result.error = i18nc("@info", "Cannot read %1: %2", path, error);
            return result;
        }
    }
    QSaveFile outputFile(manifest);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        result.error = outputFile.errorString();
        return result;
    }
    for (int i = 0; i < paths.size(); ++i) {
        const QString name = QFileInfo(paths.at(i)).fileName();
        if (progress) {
            progress(i, paths.size(), name);
        }
        QByteArray digest;
        const QString error = hashFile(paths.at(i), digest, cancel);
        if (canceled(cancel)) {
            result.canceled = true;
            return result;
        }
        if (!error.isEmpty()) {
            result.error = i18nc("@info", "Cannot read %1: %2", paths.at(i), error);
            return result;
        }
        bool escaped = false;
        const QByteArray fileName = escapedName(name, escaped);
        const QByteArray line = (escaped ? QByteArray("\\") : QByteArray()) + digest + "  " + fileName + "\n";
        if (outputFile.write(line) != line.size()) {
            result.error = outputFile.errorString();
            return result;
        }
        result.entries.append({name, {}});
    }
    if (canceled(cancel)) {
        result.canceled = true;
    } else if (!outputFile.commit()) {
        result.error = outputFile.errorString();
    }
    return result;
}

Checksums::Result Checksums::verify(const QString &manifest, const Cancel &cancel, const Progress &progress)
{
    Result result;
    QFile file(manifest);
    result.error = openRegular(file, manifest);
    if (!result.error.isEmpty()) {
        return result;
    }
    QList<QPair<QString, QByteArray>> inputs;
    while (!file.atEnd()) {
        if (canceled(cancel)) {
            result.canceled = true;
            return result;
        }
        const QByteArray line = file.readLine(MaxLine + 1);
        if (file.error() != QFileDevice::NoError || line.size() > MaxLine || inputs.size() >= MaxEntries) {
            result.error = i18nc("@info", "Cannot read the checksum file, or it exceeds the supported limits.");
            return result;
        }
        QString name;
        QByteArray digest;
        if (!parseLine(line, name, digest)) {
            result.error = i18nc("@info", "Invalid or unsafe checksum entry on line %1.", inputs.size() + 1);
            return result;
        }
        inputs.append({name, digest});
    }
    if (inputs.isEmpty()) {
        result.error = i18nc("@info", "The checksum file has no entries.");
        return result;
    }
    const QDir folder(QFileInfo(manifest).absolutePath());
    for (int i = 0; i < inputs.size(); ++i) {
        const auto &[name, expected] = inputs.at(i);
        if (progress) {
            progress(i, inputs.size(), name);
        }
        QByteArray actual;
        QString error = hashFile(folder.filePath(name), actual, cancel);
        if (canceled(cancel)) {
            result.canceled = true;
            return result;
        }
        if (error.isEmpty() && actual != expected) {
            error = i18nc("@info", "Checksum does not match.");
        }
        result.entries.append({name, error});
    }
    return result;
}
