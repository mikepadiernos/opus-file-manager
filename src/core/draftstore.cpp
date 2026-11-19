// SPDX-License-Identifier: GPL-3.0-or-later

#include "draftstore.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
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

DraftStore::~DraftStore() = default;

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
    if (!m_locks.contains(draft.id)) {
        auto lock = std::make_unique<QLockFile>(QDir(m_directory).filePath(draft.id + QStringLiteral(".lock")));
        lock->setStaleLockTime(0);
        if (!lock->tryLock()) {
            return i18nc("@info", "Could not lock the draft for editing.");
        }
        m_locks.emplace(draft.id, std::move(lock));
    }
    QSaveFile file(QDir(m_directory).filePath(draft.id + QStringLiteral(".json")));
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    const QJsonObject object{{QStringLiteral("source"), draft.source.toString(QUrl::FullyEncoded)},
                             {QStringLiteral("text"), draft.text},
                             {QStringLiteral("timestamp"), static_cast<double>(draft.timestamp)},
                             {QStringLiteral("lockVersion"), 1}};
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (file.write(data) != data.size() || !file.commit()) {
        return file.errorString();
    }
    return {};
}

QString DraftStore::remove(const QString &id, const Draft *expected)
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
    std::unique_ptr<QLockFile> temporaryLock;
    if (!m_locks.contains(id)) {
        temporaryLock = std::make_unique<QLockFile>(QDir(m_directory).filePath(id + QStringLiteral(".lock")));
        temporaryLock->setStaleLockTime(0);
        if (!temporaryLock->tryLock()) {
            return i18nc("@info", "The draft is in use or could not be locked.");
        }
    }
    QFile file(path);
    if (expected) {
        if (!file.open(QIODevice::ReadOnly) || file.size() > MaxRecordBytes) {
            return i18nc("@info", "Could not read the draft before discarding it.");
        }
        QJsonParseError parseError;
        const QJsonObject object = QJsonDocument::fromJson(file.readAll(), &parseError).object();
        file.close();
        if (parseError.error != QJsonParseError::NoError || QUrl(object.value(QStringLiteral("source")).toString()) != expected->source
            || object.value(QStringLiteral("text")).toString() != expected->text
            || object.value(QStringLiteral("timestamp")).toDouble(-1) != expected->timestamp) {
            return i18nc("@info", "The draft changed since it was listed; review it again before discarding.");
        }
    }
    if (!file.remove()) {
        return file.errorString();
    }
    release(id);
    return {};
}

void DraftStore::release(const QString &id)
{
    m_locks.erase(id);
}

QString DraftStore::prune(int retentionDays, qint64 now)
{
    if (retentionDays <= 0 || !QFileInfo::exists(m_directory)) {
        return {};
    }
    const QString error = prepareDirectory();
    if (!error.isEmpty()) {
        return error;
    }
    const qint64 cutoff = now - static_cast<qint64>(retentionDays) * 24 * 60 * 60 * 1000;
    const auto files = QDir(m_directory).entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &info : files) {
        const QString id = info.completeBaseName();
        if (!IdPattern.match(id).hasMatch() || m_locks.contains(id)) {
            continue;
        }
        QLockFile lock(QDir(m_directory).filePath(id + QStringLiteral(".lock")));
        lock.setStaleLockTime(0);
        if (!lock.tryLock()) {
            if (lock.error() != QLockFile::LockFailedError) {
                return i18nc("@info", "Could not lock draft %1 for retention.", info.fileName());
            }
            continue;
        }
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > MaxRecordBytes) {
            return i18nc("@info", "Could not read draft %1 for retention.", info.fileName());
        }
        QJsonParseError parseError;
        const QJsonObject object = QJsonDocument::fromJson(file.readAll(), &parseError).object();
        const QJsonValue timestamp = object.value(QStringLiteral("timestamp"));
        if (parseError.error != QJsonParseError::NoError || !object.value(QStringLiteral("source")).isString()
            || !object.value(QStringLiteral("text")).isString() || !timestamp.isDouble() || timestamp.toDouble() < 0 || timestamp.toDouble() > 4102444800000.0
            || object.value(QStringLiteral("text")).toString().toUtf8().size() > MaxTextBytes) {
            return i18nc("@info", "Draft %1 is invalid; it was not removed.", info.fileName());
        }
        if (!object.contains(QStringLiteral("lockVersion"))) {
            continue;
        }
        if (object.value(QStringLiteral("lockVersion")).toInt(-1) != 1) {
            return i18nc("@info", "Draft %1 has an unsupported lock version; it was not removed.", info.fileName());
        }
        file.close();
        if (timestamp.toDouble() < cutoff && !file.remove()) {
            return i18nc("@info", "Could not remove expired draft %1: %2", info.fileName(), file.errorString());
        }
    }
    return {};
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
