// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsbackup.h"

#include <KArchiveDirectory>
#include <KArchiveEntry>
#include <KArchiveFile>
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KTar>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <algorithm>
namespace
{
constexpr auto SettingsEntry = "settings.ini";
constexpr auto LayoutEntry = "layout.ini";
constexpr auto ManifestEntry = "manifest.txt";
constexpr auto ManifestContents = "opus-settings-backup\n1\n";
constexpr auto PendingRestoreFile = "pending-settings-restore.tar.gz";
constexpr qint64 MaxConfigSize = 32 * 1024 * 1024;
constexpr qint64 MaxArchiveSize = 64 * 1024 * 1024;

struct BackupContents {
    QByteArray settings;
    QByteArray layout;
};

bool hasCredential(const QVariant &value)
{
    static const QRegularExpression credentials(
        QStringLiteral(R"((?:[a-z][a-z0-9+.-]*://)[^/\s?#@]+@|(?:password|username|credential|secret|token|api[_-]?key)\s*=)"),
        QRegularExpression::CaseInsensitiveOption);
    if (value.metaType().id() == QMetaType::QString) {
        return credentials.match(value.toString()).hasMatch();
    }
    if (value.metaType().id() == QMetaType::QStringList) {
        const QStringList values = value.toStringList();
        return std::any_of(values.cbegin(), values.cend(), [](const QString &item) {
            return credentials.match(item).hasMatch();
        });
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        const QVariantList values = value.toList();
        return std::any_of(values.cbegin(), values.cend(), [](const QVariant &item) {
            return hasCredential(item);
        });
    }
    return false;
}

bool isCredentialKey(const QString &key)
{
    static const QRegularExpression credentialKey(QStringLiteral(R"((?:password|username|credential|secret|token|api[_-]?key|private[_-]?key))"),
                                                  QRegularExpression::CaseInsensitiveOption);
    return credentialKey.match(key).hasMatch();
}

void copySanitizedEntries(const KConfigGroup &source, KConfigGroup &destination, const QString &path, bool stateConfig)
{
    for (const QString &key : source.keyList()) {
        const QString value = source.readEntry(key, QString());
        const bool panelLocations =
            stateConfig && path == QLatin1String("Panels") && (key == QLatin1String("FoldersExpanded") || key == QLatin1String("CombinedExpanded"));
        if (!panelLocations && !isCredentialKey(key) && !hasCredential(value)) {
            destination.writeEntry(key, value);
        }
    }
}

void copySanitizedGroup(const KConfigGroup &source, KConfigGroup &destination, const QString &path, bool stateConfig)
{
    copySanitizedEntries(source, destination, path, stateConfig);
    for (const QString &name : source.groupList()) {
        KConfigGroup destinationGroup = destination.group(name);
        const QString childPath = path.isEmpty() ? name : path + QLatin1Char('/') + name;
        copySanitizedGroup(source.group(name), destinationGroup, childPath, stateConfig);
    }
}

QString copySanitizedConfig(const QString &sourcePath, const QString &destinationPath, bool stateConfig)
{
    KConfig source(sourcePath, KConfig::SimpleConfig);
    KConfig destination(destinationPath, KConfig::SimpleConfig);
    const KConfigGroup sourceRoot = source.group(QString());
    KConfigGroup destinationRoot = destination.group(QString());
    copySanitizedEntries(sourceRoot, destinationRoot, {}, stateConfig);
    for (const QString &name : source.groupList()) {
        if ((stateConfig && (name == QLatin1String("Session") || name == QLatin1String("Flat")))
            || (!stateConfig && (name == QLatin1String("CustomCommands") || name == QLatin1String("Credentials")))) {
            continue;
        }
        KConfigGroup destinationGroup = destination.group(name);
        destination.deleteGroup(name);
        copySanitizedGroup(source.group(name), destinationGroup, name, stateConfig);
    }
    if (!destination.sync()) {
        return i18nc("@info", "Could not write the sanitized settings file.");
    }
    return {};
}

int groupCount(const QByteArray &contents)
{
    const QList<QByteArray> lines = contents.split('\n');
    return std::count_if(lines.cbegin(), lines.cend(), [](const QByteArray &line) {
        return line.startsWith('[');
    });
}

bool hasExcludedSettings(const QByteArray &contents, bool stateConfig)
{
    const QString text = QString::fromUtf8(contents);
    static const QRegularExpression sensitiveValues(
        QStringLiteral(R"((?:[a-z][a-z0-9+.-]*://)[^/\s?#@]+@|^[^=\r\n]*(?:password|username|credential|secret|token|api[_-]?key|private[_-]?key)[^=\r\n]*=)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    static const QRegularExpression sensitiveGroups(QStringLiteral(R"(^\[(?:CustomCommands|Credentials)(?:\]\[[^\r\n]*)?\]$)"),
                                                    QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    if (sensitiveValues.match(text).hasMatch() || (!stateConfig && sensitiveGroups.match(text).hasMatch())) {
        return true;
    }
    if (stateConfig) {
        static const QRegularExpression transientGroups(QStringLiteral(R"(^\[(?:Session|Flat)(?:\]\[[^\r\n]*)?\]$)"),
                                                        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        static const QRegularExpression expandedLocations(QStringLiteral(R"(^\s*(?:FoldersExpanded|CombinedExpanded)\s*=)"),
                                                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        return transientGroups.match(text).hasMatch() || expandedLocations.match(text).hasMatch();
    }
    return false;
}

bool readArchive(const QString &archivePath, BackupContents *contents, QString *error)
{
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile() || archiveInfo.isSymLink() || archiveInfo.size() <= 0 || archiveInfo.size() > MaxArchiveSize) {
        *error = i18nc("@info", "The selected backup is not a supported regular file.");
        return false;
    }

    KTar archive(archivePath, QStringLiteral("application/gzip"));
    if (!archive.open(QIODevice::ReadOnly)) {
        *error = archive.errorString();
        return false;
    }
    const KArchiveDirectory *root = archive.directory();
    if (!root || root->entries().size() != 3) {
        *error = i18nc("@info", "The backup does not have the expected contents.");
        return false;
    }

    const auto readEntry = [root, error](const QString &name, QByteArray *data, bool manifest = false) {
        const KArchiveEntry *entry = root->entry(name);
        if (!entry || !entry->isFile()) {
            *error = i18nc("@info", "The backup is missing a required file: %1", name);
            return false;
        }
        const auto *file = static_cast<const KArchiveFile *>(entry);
        const qint64 limit = manifest ? 128 : MaxConfigSize;
        if (file->size() < 0 || file->size() > limit) {
            *error = i18nc("@info", "The backup contains an oversized file: %1", name);
            return false;
        }
        *data = file->data();
        if (data->size() != file->size()) {
            *error = i18nc("@info", "The backup file could not be read: %1", name);
            return false;
        }
        return true;
    };

    QByteArray manifest;
    if (!readEntry(QString::fromLatin1(ManifestEntry), &manifest, true) || manifest != QByteArray(ManifestContents)
        || !readEntry(QString::fromLatin1(SettingsEntry), &contents->settings) || !readEntry(QString::fromLatin1(LayoutEntry), &contents->layout)) {
        if (error->isEmpty()) {
            *error = i18nc("@info", "The selected file is not a supported Opus settings backup.");
        }
        return false;
    }
    if (hasExcludedSettings(contents->settings, false) || hasExcludedSettings(contents->layout, true)) {
        *error = i18nc("@info", "The backup contains credentials or configuration that must not be imported.");
        return false;
    }
    if (!archive.close()) {
        *error = archive.errorString();
        return false;
    }
    return true;
}

QString writeAtomically(const QString &path, const QByteArray &contents)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        return i18nc("@info", "Could not create the restore folder.");
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        return output.errorString();
    }
    if (output.write(contents) != contents.size()) {
        const QString error = output.errorString();
        output.cancelWriting();
        return error;
    }
    if (!output.commit()) {
        return output.errorString();
    }
    return {};
}

bool readFile(const QString &path, QByteArray *contents, bool *exists, QString *error)
{
    QFile input(path);
    *exists = input.exists();
    if (!*exists) {
        contents->clear();
        return true;
    }
    if (!input.open(QIODevice::ReadOnly)) {
        *error = input.errorString();
        return false;
    }
    *contents = input.readAll();
    if (input.error() != QFileDevice::NoError) {
        *error = input.errorString();
        return false;
    }
    return true;
}

void preserveCredentialEntries(const KConfigGroup &source, KConfigGroup &destination)
{
    for (const QString &key : source.keyList()) {
        const QString value = source.readEntry(key, QString());
        if (isCredentialKey(key) || hasCredential(value)) {
            destination.writeEntry(key, value);
        }
    }
    for (const QString &name : source.groupList()) {
        KConfigGroup destinationGroup = destination.group(name);
        preserveCredentialEntries(source.group(name), destinationGroup);
    }
}

QByteArray
mergeCurrentExcludedSettings(const QString &currentPath, const QByteArray &backupBytes, const QString &temporaryPath, bool stateConfig, QString *error)
{
    *error = writeAtomically(temporaryPath, backupBytes);
    if (!error->isEmpty()) {
        return {};
    }
    KConfig current(currentPath, KConfig::SimpleConfig);
    KConfig restored(temporaryPath, KConfig::SimpleConfig);
    const QStringList excludedGroups = stateConfig ? QStringList{QStringLiteral("Session"), QStringLiteral("Flat")}
                                                   : QStringList{QStringLiteral("CustomCommands"), QStringLiteral("Credentials")};
    for (const QString &name : excludedGroups) {
        if (current.hasGroup(name)) {
            restored.deleteGroup(name);
            KConfigGroup restoredGroup = restored.group(name);
            current.group(name).copyTo(&restoredGroup);
        }
    }
    KConfigGroup currentRoot = current.group(QString());
    KConfigGroup restoredRoot = restored.group(QString());
    preserveCredentialEntries(currentRoot, restoredRoot);
    for (const QString &name : current.groupList()) {
        KConfigGroup restoredGroup = restored.group(name);
        preserveCredentialEntries(current.group(name), restoredGroup);
    }
    if (stateConfig && current.hasGroup(QStringLiteral("Panels"))) {
        const KConfigGroup currentPanels = current.group(QStringLiteral("Panels"));
        KConfigGroup restoredPanels = restored.group(QStringLiteral("Panels"));
        for (const QString &key : {QStringLiteral("FoldersExpanded"), QStringLiteral("CombinedExpanded")}) {
            if (currentPanels.hasKey(key)) {
                restoredPanels.writeEntry(key, currentPanels.readEntry(key, QString()));
            }
        }
    }
    if (!restored.sync()) {
        *error = i18nc("@info", "Could not prepare restored settings.");
        return {};
    }
    QFile input(temporaryPath);
    if (!input.open(QIODevice::ReadOnly)) {
        *error = input.errorString();
        return {};
    }
    const QByteArray merged = input.readAll();
    if (input.error() != QFileDevice::NoError) {
        *error = input.errorString();
        return {};
    }
    return merged;
}

QString createArchive(const QString &archivePath, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig)
{
    if (!appConfig || !stateConfig || appConfig->name().isEmpty() || stateConfig->name().isEmpty()) {
        return i18nc("@info", "The application settings are unavailable.");
    }
    if (!appConfig->sync() || !stateConfig->sync()) {
        return i18nc("@info", "Could not save the current settings before creating the backup.");
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return i18nc("@info", "Could not create temporary files for the backup.");
    }
    const QString settingsPath = temporary.filePath(QString::fromLatin1(SettingsEntry));
    const QString layoutPath = temporary.filePath(QString::fromLatin1(LayoutEntry));
    QString error = copySanitizedConfig(appConfig->name(), settingsPath, false);
    if (error.isEmpty()) {
        error = copySanitizedConfig(stateConfig->name(), layoutPath, true);
    }
    if (!error.isEmpty()) {
        return i18nc("@info", "Could not prepare the backup: %1", error);
    }

    const QString temporaryArchive = temporary.filePath(QStringLiteral("backup.tar.gz"));
    KTar archive(temporaryArchive, QStringLiteral("application/gzip"));
    if (!archive.open(QIODevice::WriteOnly)) {
        return archive.errorString();
    }
    if (!archive.addLocalFile(settingsPath, QString::fromLatin1(SettingsEntry)) || !archive.addLocalFile(layoutPath, QString::fromLatin1(LayoutEntry))
        || !archive.writeFile(QString::fromLatin1(ManifestEntry), QString::fromLatin1(ManifestContents).toUtf8()) || !archive.close()) {
        return archive.errorString();
    }

    QFile input(temporaryArchive);
    if (!input.open(QIODevice::ReadOnly) || input.size() > MaxArchiveSize) {
        return input.isOpen() ? i18nc("@info", "The settings backup is too large.") : input.errorString();
    }
    QSaveFile output(archivePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return output.errorString();
    }
    const QByteArray bytes = input.readAll();
    if (bytes.size() != input.size() || output.write(bytes) != bytes.size()) {
        const QString writeError = output.errorString();
        output.cancelWriting();
        return writeError;
    }
    if (!output.commit()) {
        return output.errorString();
    }
    return {};
}
}

QString SettingsBackup::create(const QString &archivePath, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig)
{
    return createArchive(archivePath, appConfig, stateConfig);
}

QString SettingsBackup::inspect(const QString &archivePath, Preview *preview)
{
    if (!preview) {
        return i18nc("@info", "A backup preview is required.");
    }
    BackupContents contents;
    QString error;
    if (!readArchive(archivePath, &contents, &error)) {
        return error;
    }
    preview->included = {
        i18nc("@item:inlistbox", "Opus settings: %1 groups, %2 KiB", groupCount(contents.settings), (contents.settings.size() + 1023) / 1024),
        i18nc("@item:inlistbox", "Window and panel layouts: %1 groups, %2 KiB", groupCount(contents.layout), (contents.layout.size() + 1023) / 1024),
    };
    preview->excluded = {i18nc("@item:inlistbox", "Current tabs and folder locations"),
                         i18nc("@item:inlistbox", "Credential values and saved custom commands"),
                         i18nc("@item:inlistbox", "Shared KDE and KIO settings")};
    return {};
}

QString SettingsBackup::pendingRestorePath(const QString &dataDirectory)
{
    return QDir(dataDirectory).filePath(QString::fromLatin1(PendingRestoreFile));
}

QString SettingsBackup::stageRestore(const QString &archivePath, const QString &dataDirectory)
{
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile() || archiveInfo.isSymLink() || archiveInfo.size() <= 0 || archiveInfo.size() > MaxArchiveSize) {
        return i18nc("@info", "The selected backup is not a supported regular file.");
    }
    QFile input(archivePath);
    if (!input.open(QIODevice::ReadOnly)) {
        return input.errorString();
    }
    const QByteArray bytes = input.readAll();
    if (input.error() != QFileDevice::NoError || bytes.size() != input.size()) {
        return input.errorString();
    }
    if (!QDir().mkpath(dataDirectory)) {
        return i18nc("@info", "Could not create the restore folder.");
    }
    QTemporaryFile candidate(QDir(dataDirectory).filePath(QStringLiteral(".restore-XXXXXX.tar.gz")));
    if (!candidate.open()) {
        return candidate.errorString();
    }
    if (candidate.write(bytes) != bytes.size() || !candidate.flush()) {
        return candidate.errorString();
    }
    const QString candidatePath = candidate.fileName();
    candidate.close();
    BackupContents contents;
    QString error;
    if (!readArchive(candidatePath, &contents, &error)) {
        return error;
    }
    return writeAtomically(pendingRestorePath(dataDirectory), bytes);
}

QString SettingsBackup::applyPendingRestore(const QString &dataDirectory, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig)
{
    const QString pendingPath = pendingRestorePath(dataDirectory);
    if (!QFileInfo::exists(pendingPath)) {
        return {};
    }
    if (!appConfig || !stateConfig || appConfig->name().isEmpty() || stateConfig->name().isEmpty() || appConfig->name() == stateConfig->name()) {
        return i18nc("@info", "The application settings could not be opened for restore.");
    }

    BackupContents contents;
    QString error;
    if (!readArchive(pendingPath, &contents, &error)) {
        return error;
    }
    if (!appConfig->sync() || !stateConfig->sync()) {
        return i18nc("@info", "Could not save current settings before applying the restore.");
    }

    QByteArray oldAppConfig;
    QByteArray oldStateConfig;
    bool hadAppConfig = false;
    bool hadStateConfig = false;
    if (!readFile(appConfig->name(), &oldAppConfig, &hadAppConfig, &error) || !readFile(stateConfig->name(), &oldStateConfig, &hadStateConfig, &error)) {
        return error;
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return i18nc("@info", "Could not create temporary files for the restore.");
    }
    const QByteArray restoredSettings =
        mergeCurrentExcludedSettings(appConfig->name(), contents.settings, temporary.filePath(QStringLiteral("settings.ini")), false, &error);
    if (!error.isEmpty()) {
        return error;
    }
    const QByteArray restoredLayout =
        mergeCurrentExcludedSettings(stateConfig->name(), contents.layout, temporary.filePath(QStringLiteral("layout.ini")), true, &error);
    if (!error.isEmpty()) {
        return error;
    }
    error = writeAtomically(appConfig->name(), restoredSettings);
    if (!error.isEmpty()) {
        return error;
    }
    error = writeAtomically(stateConfig->name(), restoredLayout);
    if (!error.isEmpty()) {
        if (hadAppConfig) {
            const QString rollbackError = writeAtomically(appConfig->name(), oldAppConfig);
            if (!rollbackError.isEmpty()) {
                return i18nc("@info", "%1 The application settings could not be rolled back: %2", error, rollbackError);
            }
        } else {
            if (!QFile::remove(appConfig->name())) {
                return i18nc("@info", "%1 The partially restored application settings could not be removed.", error);
            }
        }
        return error;
    }
    appConfig->reparseConfiguration();
    stateConfig->reparseConfiguration();
    if (!QFile::remove(pendingPath)) {
        return i18nc("@info", "Settings were restored, but the pending restore file could not be removed.");
    }
    return {};
}
