// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataeditor.h"

#include <KFileMetaData/WriteData>
#include <KFileMetaData/WriterCollection>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>

#include <sys/stat.h>

namespace
{
QStringList tagsFor(const QString &mime)
{
    if (mime == QLatin1String("image/jpeg") || mime == QLatin1String("image/tiff")) {
        return {QStringLiteral("ImageDescription"), QStringLiteral("Artist"), QStringLiteral("Copyright")};
    }
    if (mime == QLatin1String("audio/mpeg")) {
        return {QStringLiteral("Title"), QStringLiteral("Artist"), QStringLiteral("Album")};
    }
    return {};
}

bool identity(MetadataEditor::Snapshot &result)
{
    struct stat info;
    if (!QDir::isAbsolutePath(result.path) || lstat(QFile::encodeName(result.path).constData(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1) {
        result.error = i18nc("@info", "Choose an existing local regular file without symbolic or hard links.");
        return false;
    }
    result.device = info.st_dev;
    result.inode = info.st_ino;
    result.size = info.st_size;
    result.modifiedSeconds = info.st_mtim.tv_sec;
    result.modifiedNanoseconds = info.st_mtim.tv_nsec;
    result.changedSeconds = info.st_ctim.tv_sec;
    result.changedNanoseconds = info.st_ctim.tv_nsec;
    return true;
}

bool sameIdentity(const MetadataEditor::Snapshot &left, const MetadataEditor::Snapshot &right)
{
    return left.device == right.device && left.inode == right.inode && left.size == right.size && left.modifiedSeconds == right.modifiedSeconds
        && left.modifiedNanoseconds == right.modifiedNanoseconds && left.changedSeconds == right.changedSeconds
        && left.changedNanoseconds == right.changedNanoseconds;
}

QString execute(const QStringList &arguments, QByteArray &output)
{
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("exiftool"));
    if (tool.isEmpty()) {
        return i18nc("@info", "ExifTool is required to edit metadata.");
    }
    QProcess process;
    process.start(tool, arguments);
    if (!process.waitForStarted()) {
        return i18nc("@info", "Could not start ExifTool: %1", process.errorString());
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished();
        return i18nc("@info", "ExifTool did not finish. The file may have been modified.");
    }
    output = process.readAllStandardOutput();
    const QString diagnostics = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || !diagnostics.isEmpty()) {
        return i18nc("@info", "ExifTool reported a problem: %1", diagnostics.isEmpty() ? QString::fromUtf8(output).trimmed() : diagnostics);
    }
    return {};
}
}

MetadataEditor::Snapshot MetadataEditor::read(const QString &path)
{
    Snapshot result;
    result.path = path;
    if (!identity(result)) {
        return result;
    }
    result.mimeType = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    const QStringList tags = tagsFor(result.mimeType);
    if (tags.isEmpty()) {
        result.error = i18nc("@info", "Metadata editing supports JPEG, TIFF and MP3 files only.");
        return result;
    }
    if (result.mimeType == QLatin1String("audio/mpeg")) {
        KFileMetaData::WriterCollection writers;
        if (writers.fetchWriters(result.mimeType).isEmpty()) {
            result.error = i18nc("@info", "No KDE metadata writer is available for MP3 files.");
            return result;
        }
    }
    QStringList arguments{QStringLiteral("-j")};
    for (const QString &tag : tags) {
        arguments.append(QLatin1Char('-') + tag);
    }
    arguments.append(path);
    QByteArray output;
    result.error = execute(arguments, output);
    if (!result.error.isEmpty()) {
        return result;
    }
    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isArray() || json.array().size() != 1 || !json.array().first().isObject()) {
        result.error = i18nc("@info", "ExifTool returned invalid metadata.");
        return result;
    }
    const auto object = json.array().first().toObject();
    for (const QString &tag : tags) {
        const auto value = object.value(tag);
        if (!value.isUndefined() && !value.isString()) {
            result.error = i18nc("@info", "Metadata field %1 cannot be represented in the editor.", tag);
            return result;
        }
        const QString text = value.toString();
        if (text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r')) || text.contains(QChar::Null)) {
            result.error = i18nc("@info", "Metadata field %1 contains unsupported characters.", tag);
            return result;
        }
        result.values.insert(tag, text);
    }
    Snapshot after;
    after.path = path;
    if (!identity(after) || !sameIdentity(result, after)) {
        result.error = i18nc("@info", "The file changed while metadata was read. Try again.");
    }
    return result;
}

QString MetadataEditor::write(const Snapshot &snapshot, const QMap<QString, QString> &values)
{
    QStringList tags = tagsFor(snapshot.mimeType);
    tags.sort();
    if (!snapshot.error.isEmpty() || tags.isEmpty() || values.keys() != tags || snapshot.values.keys() != tags) {
        return i18nc("@info", "The metadata preview is invalid.");
    }
    Snapshot current;
    current.path = snapshot.path;
    if (!identity(current) || !sameIdentity(snapshot, current)) {
        return i18nc("@info", "The file changed since the preview. Reload it before applying changes.");
    }
    QStringList arguments;
    KFileMetaData::WriteData audioData(snapshot.path, snapshot.mimeType);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value().contains(QChar::Null) || it.value().contains(QLatin1Char('\n')) || it.value().contains(QLatin1Char('\r'))) {
            return i18nc("@info", "The metadata preview contains an invalid field or value.");
        }
        if (it.value() != snapshot.values.value(it.key())) {
            arguments.append(QLatin1Char('-') + it.key() + QLatin1Char('=') + it.value());
            if (snapshot.mimeType == QLatin1String("audio/mpeg")) {
                const auto property = it.key() == QLatin1String("Title") ? KFileMetaData::Property::Title
                    : it.key() == QLatin1String("Artist")                ? KFileMetaData::Property::Artist
                                                                         : KFileMetaData::Property::Album;
                audioData.add(property, it.value());
            }
        }
    }
    if (arguments.isEmpty()) {
        return i18nc("@info", "There are no metadata changes to apply.");
    }
    if (snapshot.mimeType == QLatin1String("audio/mpeg")) {
        KFileMetaData::WriterCollection writers;
        const auto available = writers.fetchWriters(snapshot.mimeType);
        if (available.isEmpty()) {
            return i18nc("@info", "No KDE metadata writer is available for MP3 files.");
        }
        available.first()->write(audioData);
    } else {
        arguments.prepend(QStringLiteral("-overwrite_original_in_place"));
        arguments.append(snapshot.path);
        QByteArray output;
        const QString error = execute(arguments, output);
        if (!error.isEmpty()) {
            return i18nc("@info", "%1 The file may have been modified; inspect it before retrying.", error);
        }
    }
    const Snapshot after = read(snapshot.path);
    if (!after.error.isEmpty() || after.values != values) {
        return i18nc("@info", "The file was written, but the requested metadata could not be verified. Inspect it before retrying.");
    }
    return {};
}
