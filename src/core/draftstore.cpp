// SPDX-License-Identifier: GPL-3.0-or-later

#include "draftstore.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <cerrno>
#include <sys/stat.h>

namespace
{
constexpr qsizetype MaxTextBytes = 8 * 1024 * 1024;
constexpr qint64 MaxRecordBytes = 6 * MaxTextBytes + 4096;
const QRegularExpression IdPattern(QStringLiteral("^[a-f0-9-]{36}$"));
}

DraftStore::DraftStore(const QString &directory)
    : m_directory(directory)
{
}

QString DraftStore::prepareDirectory() const
{
    if (m_directory.isEmpty()) {
        return i18nc("@info", "No draft storage location is available.");
    }
    const QFileInfo info(m_directory);
    if (info.isSymLink()) {
        return i18nc("@info", "The draft directory must not be a symbolic link.");
    }
    if (!info.exists()) {
        const QFileInfo parent(info.absolutePath());
        if (!QDir().mkpath(parent.absoluteFilePath())) {
            return i18nc("@info", "Could not create the draft directory.");
        }
        if (::mkdir(QFile::encodeName(m_directory).constData(), S_IRWXU) != 0 && errno != EEXIST) {
            return i18nc("@info", "Could not create the draft directory.");
        }
    }
    const QFileInfo actual(m_directory);
    if (actual.isSymLink() || !actual.isDir()
        || !QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        return i18nc("@info", "Could not secure the draft directory.");
    }
    return {};
}

QString DraftStore::write(const Draft &draft)
{
    if (!IdPattern.match(draft.id).hasMatch()) {
        return i18nc("@info", "Invalid draft identifier.");
    }
    if (draft.text.toUtf8().size() > MaxTextBytes) {
        return i18nc("@info", "The draft exceeds the 8 MiB limit.");
    }
    const QString error = prepareDirectory();
    if (!error.isEmpty()) {
        return error;
    }
    QSaveFile file(QDir(m_directory).filePath(draft.id + QStringLiteral(".json")));
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    const QJsonObject object{{QStringLiteral("source"), draft.source.toString(QUrl::FullyEncoded)},
                             {QStringLiteral("text"), draft.text},
                             {QStringLiteral("timestamp"), static_cast<double>(draft.timestamp)}};
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (file.write(data) != data.size() || !file.commit()) {
        return file.errorString();
    }
    return {};
}

QString DraftStore::remove(const QString &id)
{
    if (!IdPattern.match(id).hasMatch()) {
        return i18nc("@info", "Invalid draft identifier.");
    }
    if (m_directory.isEmpty()) {
        return i18nc("@info", "No draft storage location is available.");
    }
    const QFileInfo directory(m_directory);
    if (directory.isSymLink() || (directory.exists() && !directory.isDir())) {
        return i18nc("@info", "The draft directory is not a private directory.");
    }
    const QString path = QDir(m_directory).filePath(id + QStringLiteral(".json"));
    if (!QFileInfo::exists(path)) {
        return {};
    }
    QFile file(path);
    return file.remove() ? QString() : file.errorString();
}

QList<DraftStore::Draft> DraftStore::drafts(const QUrl &source, QString *error) const
{
    QList<Draft> result;
    if (m_directory.isEmpty()) {
        *error = i18nc("@info", "No draft storage location is available.");
        return result;
    }
    const QFileInfo directory(m_directory);
    if (!directory.exists()) {
        return result;
    }
    if (directory.isSymLink() || !directory.isDir()) {
        *error = i18nc("@info", "The draft directory is not a private directory.");
        return result;
    }
    const auto files = QDir(m_directory).entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &info : files) {
        const QString id = info.completeBaseName();
        if (!IdPattern.match(id).hasMatch()) {
            continue;
        }
        QFile file(info.filePath());
        if (info.size() > MaxRecordBytes || !file.open(QIODevice::ReadOnly)) {
            *error = i18nc("@info", "Could not read draft %1.", info.fileName());
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        const QJsonObject object = document.object();
        const QJsonValue timestamp = object.value(QStringLiteral("timestamp"));
        if (parseError.error != QJsonParseError::NoError || !object.value(QStringLiteral("source")).isString()
            || !object.value(QStringLiteral("text")).isString() || !timestamp.isDouble() || timestamp.toDouble() < 0 || timestamp.toDouble() > 4102444800000.0
            || object.value(QStringLiteral("text")).toString().toUtf8().size() > MaxTextBytes) {
            *error = i18nc("@info", "Draft %1 is invalid.", info.fileName());
            continue;
        }
        const QUrl draftSource(object.value(QStringLiteral("source")).toString());
        if (draftSource == source) {
            result.append({id, object.value(QStringLiteral("text")).toString(), draftSource, static_cast<qint64>(timestamp.toDouble())});
        }
    }
    std::sort(result.begin(), result.end(), [](const Draft &a, const Draft &b) {
        return a.timestamp > b.timestamp;
    });
    return result;
}
