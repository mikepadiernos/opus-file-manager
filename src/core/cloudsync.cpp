// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsync.h"

#include "appidentity.h"
#include "rclone.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QXmlStreamReader>

#include <algorithm>
#include <cstdlib>

using namespace CloudSync;

namespace
{
constexpr int LargeDeletionCount = 50;
constexpr int LargeDeletionMinimum = 5;
constexpr qint64 TimeTolerance = 2;

QString cleanLocal(const QString &path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(path);
}

/// Whether @p inner is @p outer or inside it.
bool within(const QString &inner, const QString &outer)
{
    if (inner == outer) {
        return true;
    }
    const QString prefix = outer.endsWith(QLatin1Char('/')) ? outer : outer + QLatin1Char('/');
    return inner.startsWith(prefix);
}

bool overlaps(const QString &first, const QString &second)
{
    return within(first, second) || within(second, first);
}

QString relativeTo(const QString &inner, const QString &outer)
{
    const QString prefix = outer.endsWith(QLatin1Char('/')) ? outer : outer + QLatin1Char('/');
    return inner.mid(prefix.size());
}

QString modeKey(Mode mode)
{
    switch (mode) {
    case Mode::CloudBackup:
        return QStringLiteral("cloud-backup");
    case Mode::LocalBackup:
        return QStringLiteral("local-backup");
    case Mode::Sync:
        break;
    }
    return QStringLiteral("sync");
}

std::optional<Mode> modeFromKey(const QString &key)
{
    for (const Mode mode : {Mode::Sync, Mode::CloudBackup, Mode::LocalBackup}) {
        if (modeKey(mode) == key) {
            return mode;
        }
    }
    return std::nullopt;
}

QUrl storedUrl(const QUrl &url)
{
    return url.adjusted(QUrl::RemovePassword | QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

/// The rclone remote name and path of "name:path".
std::optional<std::pair<QString, QString>> splitRclone(const QString &location)
{
    const qsizetype colon = location.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return std::nullopt;
    }
    const QString name = location.left(colon);
    if (!Rclone::isValidName(name)) {
        return std::nullopt;
    }
    QString path = location.mid(colon + 1);
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return std::make_pair(name, path);
}

/// The remote path of a pair, for comparing remote folders of pairs on the
/// same account.
QString remotePath(const Pair &pair)
{
    if (pair.usesRclone()) {
        const auto parts = splitRclone(pair.rcloneRemote);
        return parts ? QLatin1Char('/') + parts->second : QString();
    }
    const QString path = storedUrl(pair.remote).path();
    return path.isEmpty() ? QStringLiteral("/") : path;
}

QString remoteLocationKey(const Pair &pair)
{
    if (pair.usesRclone()) {
        const auto parts = splitRclone(pair.rcloneRemote);
        return parts ? QStringLiteral("rclone:") + parts->first + QLatin1Char(':') + parts->second : QString();
    }
    return storedUrl(pair.remote).toString();
}

bool sameSide(const std::optional<State> &current, const std::optional<State> &base)
{
    if (!current || !base) {
        return !current && !base;
    }
    if (current->folder != base->folder) {
        return false;
    }
    // A folder's time changes with its contents, which are compared item by item.
    return current->folder || (current->size == base->size && current->modified == base->modified);
}

/// Whether both sides hold the same item, judging files by size and time.
bool equivalent(const State &local, const State &remote)
{
    if (local.folder != remote.folder) {
        return false;
    }
    return local.folder
        || (local.size == remote.size && local.modified != 0 && remote.modified != 0 && std::abs(local.modified - remote.modified) <= TimeTolerance);
}

std::optional<State> stateAt(const Listing &listing, const QString &path)
{
    const auto it = listing.constFind(path);
    return it == listing.constEnd() ? std::nullopt : std::optional<State>(*it);
}

Change copyTo(bool remote, const QString &path, const State &source, bool overwrite)
{
    Change change;
    change.path = path;
    change.folder = source.folder;
    change.size = source.folder ? 0 : source.size;
    change.overwrite = overwrite;
    if (remote) {
        change.operation = source.folder ? Operation::CreateRemoteFolder : Operation::Upload;
    } else {
        change.operation = source.folder ? Operation::CreateLocalFolder : Operation::Download;
    }
    return change;
}

Change deletion(bool remote, const QString &path, const State &state)
{
    Change change;
    change.operation = remote ? Operation::DeleteRemote : Operation::DeleteLocal;
    change.path = path;
    change.folder = state.folder;
    return change;
}

bool isDeletion(Operation operation)
{
    return operation == Operation::DeleteRemote || operation == Operation::DeleteLocal;
}

int phase(const Change &change)
{
    switch (change.operation) {
    case Operation::CreateRemoteFolder:
    case Operation::CreateLocalFolder:
        return 0;
    case Operation::RenameRemote:
    case Operation::RenameLocal:
        return 1;
    case Operation::Upload:
    case Operation::Download:
        return 2;
    case Operation::DeleteRemote:
    case Operation::DeleteLocal:
        return change.folder ? 4 : 3;
    }
    return 2;
}

/// Pairs of a file deleted on one side and a file added on that side with
/// the same size and time, while the other side still holds the deleted file
/// unchanged. Ambiguous matches are left as a deletion and an addition.
QList<std::pair<QString, QString>> renames(const Listing &side, const Listing &other, const Listing &baseSide, const Listing &baseOther)
{
    using Key = std::pair<qint64, qint64>;
    QMap<Key, QStringList> deleted;
    QMap<Key, QStringList> added;
    for (auto it = baseSide.constBegin(); it != baseSide.constEnd(); ++it) {
        const State &base = it.value();
        if (base.folder || base.modified == 0 || side.contains(it.key())) {
            continue;
        }
        const auto otherNow = stateAt(other, it.key());
        const auto otherBase = stateAt(baseOther, it.key());
        if (otherNow && otherBase && sameSide(otherNow, otherBase)) {
            deleted[{base.size, base.modified}].append(it.key());
        }
    }
    if (deleted.isEmpty()) {
        return {};
    }
    for (auto it = side.constBegin(); it != side.constEnd(); ++it) {
        const State &state = it.value();
        if (state.folder || state.modified == 0 || baseSide.contains(it.key()) || baseOther.contains(it.key()) || other.contains(it.key())) {
            continue;
        }
        added[{state.size, state.modified}].append(it.key());
    }
    QList<std::pair<QString, QString>> result;
    for (auto it = deleted.constBegin(); it != deleted.constEnd(); ++it) {
        const QStringList targets = added.value(it.key());
        if (it.value().size() == 1 && targets.size() == 1) {
            result.append({it.value().constFirst(), targets.constFirst()});
        }
    }
    return result;
}

/// Turns a change into what @p mode carries out. Returns nothing, and
/// describes the change in @p skipped, when the mode leaves it out.
std::optional<Change> forMode(const Change &change, Mode mode, bool mirrorDeletions, const Listing &local, const Listing &remote, QString *skipped)
{
    if (mode == Mode::Sync) {
        return change;
    }
    const bool upward = mode == Mode::CloudBackup;
    const Listing &source = upward ? local : remote;
    Change result = change;
    switch (change.operation) {
    case Operation::Upload:
    case Operation::CreateRemoteFolder:
        if (upward) {
            return change;
        }
        break;
    case Operation::Download:
    case Operation::CreateLocalFolder:
        if (!upward) {
            return change;
        }
        break;
    case Operation::RenameRemote:
    case Operation::RenameLocal: {
        const bool sourceRename = (change.operation == Operation::RenameRemote) == upward;
        if (sourceRename) {
            if (mirrorDeletions) {
                return change;
            }
            // Keep the backup of the old name and back up the new one.
            if (const auto state = stateAt(source, change.path)) {
                return copyTo(upward, change.path, *state, false);
            }
        } else if (const auto state = stateAt(source, change.source)) {
            // The backup was renamed: back up the source again under its name.
            return copyTo(upward, change.source, *state, false);
        }
        break;
    }
    case Operation::DeleteRemote:
    case Operation::DeleteLocal: {
        const bool sourceDeletion = (change.operation == Operation::DeleteRemote) == upward;
        if (sourceDeletion) {
            if (mirrorDeletions) {
                return change;
            }
            *skipped = upward ? i18nc("@item", "Kept on the remote: %1", change.path) : i18nc("@item", "Kept in the local folder: %1", change.path);
            return std::nullopt;
        }
        // The backup copy was deleted: restore it from the source.
        if (const auto state = stateAt(source, change.path)) {
            return copyTo(upward, change.path, *state, false);
        }
        return std::nullopt;
    }
    }
    *skipped = upward ? i18nc("@item", "Remote change not downloaded: %1", change.path) : i18nc("@item", "Local change not uploaded: %1", change.path);
    return std::nullopt;
}

/// Drops folder deletions that would remove items being kept or written.
QList<Change> pruneFolderDeletions(const QList<Change> &changes, const Listing &local, const Listing &remote)
{
    QSet<QString> removedRemote;
    QSet<QString> removedLocal;
    QStringList writtenRemote;
    QStringList writtenLocal;
    for (const Change &change : changes) {
        switch (change.operation) {
        case Operation::DeleteRemote:
            removedRemote.insert(change.path);
            break;
        case Operation::DeleteLocal:
            removedLocal.insert(change.path);
            break;
        case Operation::RenameRemote:
            removedRemote.insert(change.source);
            writtenRemote.append(change.path);
            break;
        case Operation::RenameLocal:
            removedLocal.insert(change.source);
            writtenLocal.append(change.path);
            break;
        case Operation::Upload:
        case Operation::CreateRemoteFolder:
            writtenRemote.append(change.path);
            break;
        case Operation::Download:
        case Operation::CreateLocalFolder:
            writtenLocal.append(change.path);
            break;
        }
    }
    const auto keepsSomething = [](const QString &folder, const Listing &listing, const QSet<QString> &removed, const QStringList &written) {
        const QString prefix = folder + QLatin1Char('/');
        for (auto it = listing.lowerBound(prefix); it != listing.constEnd() && it.key().startsWith(prefix); ++it) {
            if (!removed.contains(it.key())) {
                return true;
            }
        }
        return std::any_of(written.cbegin(), written.cend(), [&prefix](const QString &path) {
            return path.startsWith(prefix);
        });
    };
    QList<Change> result;
    for (const Change &change : changes) {
        if (change.folder && change.operation == Operation::DeleteRemote && keepsSomething(change.path, remote, removedRemote, writtenRemote)) {
            continue;
        }
        if (change.folder && change.operation == Operation::DeleteLocal && keepsSomething(change.path, local, removedLocal, writtenLocal)) {
            continue;
        }
        result.append(change);
    }
    return result;
}

QString escapeExec(const QString &argument)
{
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    if (std::none_of(argument.cbegin(), argument.cend(), [](QChar c) {
            return reserved.contains(c);
        })) {
        return argument;
    }
    QString quoted = argument;
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    quoted.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    quoted.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    // Desktop entry values escape backslashes once more.
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

QJsonObject listingToJson(const Listing &listing)
{
    QJsonObject object;
    for (auto it = listing.constBegin(); it != listing.constEnd(); ++it) {
        object.insert(it.key(), QJsonArray{it->folder ? 1 : 0, it->size, it->modified});
    }
    return object;
}

bool listingFromJson(const QJsonValue &value, Listing &listing)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QJsonArray item = it.value().toArray();
        if (item.size() != 3 || it.key().isEmpty()) {
            return false;
        }
        listing.insert(it.key(), State{item.at(0).toInt() != 0, item.at(1).toInteger(), item.at(2).toInteger()});
    }
    return true;
}

QString rclonePattern(const QString &pattern, QString *error)
{
    for (const QChar c : {QLatin1Char('?'), QLatin1Char('\\'), QLatin1Char('{'), QLatin1Char('}')}) {
        if (pattern.contains(c)) {
            if (error) {
                *error = i18nc("@info", "rclone cannot use the filter pattern %1. Remove \"?\", \"\\\", \"{\" and \"}\" from it.", pattern);
            }
            return {};
        }
    }
    static const QRegularExpression stars(QStringLiteral("\\*+"));
    QString converted = pattern;
    converted.replace(stars, QStringLiteral("**"));
    return QLatin1Char('/') + converted;
}

QStringList configuredFolders(const EngineLocations &locations, const QString &engine)
{
    QStringList folders;
    if (engine == QLatin1String("Nextcloud") || engine == QLatin1String("ownCloud")) {
        const QString file = engine == QLatin1String("Nextcloud") ? QStringLiteral("Nextcloud/nextcloud.cfg") : QStringLiteral("ownCloud/owncloud.cfg");
        QFile config(QDir(locations.configHome).filePath(file));
        if (locations.configHome.isEmpty() || !config.open(QIODevice::ReadOnly)) {
            return {};
        }
        while (!config.atEnd()) {
            const QString line = QString::fromUtf8(config.readLine()).trimmed();
            const qsizetype key = line.indexOf(QLatin1String("\\localPath="));
            if (key > 0) {
                folders.append(line.mid(key + int(qstrlen("\\localPath="))));
            }
        }
        return folders;
    }
    for (const QString &home : {locations.configHome, locations.stateHome}) {
        QFile config(QDir(home).filePath(QStringLiteral("syncthing/config.xml")));
        if (home.isEmpty() || !config.open(QIODevice::ReadOnly)) {
            continue;
        }
        QXmlStreamReader xml(&config);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("folder")) {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                if (path.startsWith(QLatin1String("~/"))) {
                    path = QDir::homePath() + path.mid(1);
                }
                folders.append(path);
            }
        }
    }
    return folders;
}
}

QString CloudSync::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString CloudSync::remoteDisplayName(const Pair &pair)
{
    if (pair.usesRclone()) {
        return pair.rcloneRemote;
    }
    return pair.remote.toDisplayString(QUrl::RemovePassword | QUrl::PreferLocalFile);
}

QString CloudSync::accountKey(const Pair &pair)
{
    if (pair.usesRclone()) {
        const auto parts = splitRclone(pair.rcloneRemote);
        return QStringLiteral("rclone:") + (parts ? parts->first : pair.rcloneRemote);
    }
    QUrl account;
    account.setScheme(pair.remote.scheme());
    if (!pair.remote.userName().isEmpty()) {
        account.setUserName(pair.remote.userName());
    }
    account.setHost(pair.remote.host().toLower());
    account.setPort(pair.remote.port());
    return account.toString();
}

QString CloudSync::accountDisplayName(const Pair &pair)
{
    if (pair.usesRclone()) {
        const auto parts = splitRclone(pair.rcloneRemote);
        return i18nc("@item rclone remote name", "rclone remote %1", parts ? parts->first : pair.rcloneRemote);
    }
    if (pair.remote.host().isEmpty()) {
        return pair.remote.scheme() == QLatin1String("file") ? i18nc("@item", "This computer") : pair.remote.scheme();
    }
    return pair.remote.userName().isEmpty() ? pair.remote.host() : pair.remote.userName() + QLatin1Char('@') + pair.remote.host();
}

QString CloudSync::modeName(Mode mode)
{
    switch (mode) {
    case Mode::CloudBackup:
        return i18nc("@item cloud sync mode", "Cloud backup");
    case Mode::LocalBackup:
        return i18nc("@item cloud sync mode", "Local backup");
    case Mode::Sync:
        break;
    }
    return i18nc("@item cloud sync mode", "Two-way sync");
}

QList<Pair> CloudSync::load(const KConfigGroup &group)
{
    QList<Pair> pairs;
    QSet<QString> ids;
    const QStringList names = group.groupList();
    for (const QString &id : names) {
        const KConfigGroup entry = group.group(id);
        Pair pair;
        pair.id = id;
        pair.localPath = cleanLocal(entry.readEntry("LocalPath", QString()));
        pair.remote = QUrl(entry.readEntry("Remote", QString()));
        pair.rcloneRemote = entry.readEntry("RcloneRemote", QString());
        const auto mode = modeFromKey(entry.readEntry("Mode", QStringLiteral("sync")));
        pair.mirrorDeletions = entry.readEntry("MirrorDeletions", false);
        pair.options.includePatterns = entry.readEntry("Include", QStringList());
        pair.options.excludePatterns = entry.readEntry("Exclude", QStringList());
        pair.options.recursive = entry.readEntry("Recursive", true);
        pair.intervalMinutes = entry.readEntry("IntervalMinutes", 0);
        pair.paused = entry.readEntry("Paused", false);
        if (!mode || QUuid::fromString(id).isNull() || ids.contains(id) || !pairError(pair, {}).isEmpty()) {
            continue;
        }
        pair.mode = *mode;
        ids.insert(id);
        pairs.append(pair);
    }
    return pairs;
}

bool CloudSync::save(KConfigGroup group, const QList<Pair> &pairs, QString *error)
{
    QSet<QString> ids;
    for (const Pair &pair : pairs) {
        const QString problem =
            QUuid::fromString(pair.id).isNull() || ids.contains(pair.id) ? i18nc("@info", "Each pair needs a unique identifier.") : pairError(pair, {});
        if (!problem.isEmpty()) {
            if (error) {
                *error = problem;
            }
            return false;
        }
        ids.insert(pair.id);
    }
    for (const QString &name : group.groupList()) {
        group.deleteGroup(name);
    }
    for (const Pair &pair : pairs) {
        KConfigGroup entry = group.group(pair.id);
        entry.writeEntry("LocalPath", cleanLocal(pair.localPath));
        entry.writeEntry("Remote", pair.usesRclone() ? QString() : storedUrl(pair.remote).toString());
        entry.writeEntry("RcloneRemote", pair.rcloneRemote);
        entry.writeEntry("Mode", modeKey(pair.mode));
        entry.writeEntry("MirrorDeletions", pair.mirrorDeletions);
        entry.writeEntry("Include", pair.options.includePatterns);
        entry.writeEntry("Exclude", pair.options.excludePatterns);
        entry.writeEntry("Recursive", pair.options.recursive);
        entry.writeEntry("IntervalMinutes", pair.intervalMinutes);
        entry.writeEntry("Paused", pair.paused);
    }
    if (!group.sync()) {
        if (error) {
            *error = i18nc("@info", "The cloud sync pairs could not be saved.");
        }
        return false;
    }
    return true;
}

QString CloudSync::pairError(const Pair &pair, const QList<Pair> &others)
{
    if (pair.localPath.isEmpty() || !QDir::isAbsolutePath(pair.localPath)) {
        return i18nc("@info", "Choose a local folder.");
    }
    const QString local = cleanLocal(pair.localPath);
    if (pair.usesRclone()) {
        if (!splitRclone(pair.rcloneRemote)) {
            return i18nc("@info", "Enter an rclone location as remote:path, using a remote configured in rclone.");
        }
        QString filterError;
        rcloneFilterRules(pair.options, &filterError);
        if (!filterError.isEmpty()) {
            return filterError;
        }
    } else {
        if (!pair.remote.isValid() || pair.remote.scheme().isEmpty() || pair.remote.isRelative()) {
            return i18nc("@info", "Enter a remote location, such as webdavs://cloud.example.com/remote.php/dav/files/user/Documents.");
        }
        if (pair.remote.isLocalFile() && overlaps(cleanLocal(pair.remote.toLocalFile()), local)) {
            return i18nc("@info", "The local folder and the remote location cannot contain each other.");
        }
    }
    const QString optionsError = FolderSync::optionsError(pair.options);
    if (!optionsError.isEmpty()) {
        return optionsError;
    }
    if (pair.intervalMinutes < 0) {
        return i18nc("@info", "The synchronization interval cannot be negative.");
    }
    for (const Pair &other : others) {
        if (other.id == pair.id) {
            continue;
        }
        if (cleanLocal(other.localPath) == local) {
            return i18nc("@info", "%1 is already paired with %2.", local, remoteDisplayName(other));
        }
        if (remoteLocationKey(other) == remoteLocationKey(pair)) {
            return i18nc("@info", "%1 is already paired with %2.", remoteDisplayName(pair), cleanLocal(other.localPath));
        }
    }
    return {};
}

std::optional<Nesting> CloudSync::nesting(const Pair &pair, const QList<Pair> &others)
{
    const QString local = cleanLocal(pair.localPath);
    const QString account = accountKey(pair);
    const QString path = remotePath(pair);
    for (const Pair &other : others) {
        if (other.id == pair.id) {
            continue;
        }
        const QString otherLocal = cleanLocal(other.localPath);
        if (local != otherLocal && overlaps(local, otherLocal)) {
            const bool inside = within(local, otherLocal);
            return Nesting{other.id, inside, false, inside ? relativeTo(local, otherLocal) : relativeTo(otherLocal, local)};
        }
        const QString otherPath = remotePath(other);
        if (accountKey(other) == account && path != otherPath && overlaps(path, otherPath)) {
            const bool inside = within(path, otherPath);
            return Nesting{other.id, inside, true, inside ? relativeTo(path, otherPath) : relativeTo(otherPath, path)};
        }
    }
    return std::nullopt;
}

QString CloudSync::exactPattern(const QString &relativePath)
{
    QString pattern;
    for (const QChar c : relativePath) {
        if (c == QLatin1Char('*') || c == QLatin1Char('?') || c == QLatin1Char('[')) {
            pattern += QLatin1Char('[') + c + QLatin1Char(']');
        } else {
            pattern += c;
        }
    }
    return pattern;
}

EngineLocations CloudSync::defaultEngineLocations()
{
    EngineLocations locations;
    locations.configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QByteArray state = qgetenv("XDG_STATE_HOME");
    locations.stateHome = state.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/state") : QFile::decodeName(state);
    return locations;
}

QString CloudSync::managingEngine(const QString &localPath, const EngineLocations &locations)
{
    const QString local = cleanLocal(localPath);
    if (local.isEmpty()) {
        return {};
    }
    for (const QString &engine : {QStringLiteral("Nextcloud"), QStringLiteral("ownCloud"), QStringLiteral("Syncthing")}) {
        const QStringList folders = configuredFolders(locations, engine);
        for (const QString &folder : folders) {
            const QString managed = cleanLocal(folder);
            if (!managed.isEmpty() && QDir::isAbsolutePath(managed) && overlaps(local, managed)) {
                return engine;
            }
        }
    }
    // The folder may not exist yet, so walk up its path rather than with QDir::cdUp().
    QString current = local;
    while (true) {
        const QDir directory(current);
        if (directory.exists(QStringLiteral(".stfolder"))) {
            return QStringLiteral("Syncthing");
        }
        if (directory.exists(QStringLiteral(".dropbox")) || directory.exists(QStringLiteral(".dropbox.cache"))) {
            return QStringLiteral("Dropbox");
        }
        if (!directory.entryList({QStringLiteral(".sync_*.db"), QStringLiteral("._sync_*.db")}, QDir::Files | QDir::Hidden).isEmpty()) {
            return QStringLiteral("Nextcloud");
        }
        const QString parent = QFileInfo(current).path();
        if (parent == current || parent.isEmpty()) {
            break;
        }
        current = parent;
    }
    return {};
}

QString CloudSync::snapshotPath(const QString &id)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("cloud-sync/%1.json").arg(id));
}

std::optional<Snapshot> CloudSync::loadSnapshot(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) {
        return Snapshot{};
    }
    const auto fail = [error, &path]() -> std::optional<Snapshot> {
        if (error) {
            *error = i18nc("@info", "The synchronization state in %1 cannot be read.", path);
        }
        return std::nullopt;
    };
    if (!file.open(QIODevice::ReadOnly)) {
        return fail();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || object.value(QStringLiteral("version")).toInt() != 1) {
        return fail();
    }
    Snapshot snapshot;
    snapshot.localPath = object.value(QStringLiteral("localPath")).toString();
    snapshot.remoteLocation = object.value(QStringLiteral("remoteLocation")).toString();
    snapshot.rcloneInitialized = object.value(QStringLiteral("rcloneInitialized")).toBool();
    for (const auto &rule : object.value(QStringLiteral("rcloneFilters")).toArray()) {
        snapshot.rcloneFilters.append(rule.toString());
    }
    if (!listingFromJson(object.value(QStringLiteral("local")), snapshot.local) || !listingFromJson(object.value(QStringLiteral("remote")), snapshot.remote)) {
        return fail();
    }
    return snapshot;
}

bool CloudSync::saveSnapshot(const QString &path, const Snapshot &snapshot, QString *error)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("localPath"), snapshot.localPath);
    object.insert(QStringLiteral("remoteLocation"), snapshot.remoteLocation);
    object.insert(QStringLiteral("rcloneInitialized"), snapshot.rcloneInitialized);
    object.insert(QStringLiteral("rcloneFilters"), QJsonArray::fromStringList(snapshot.rcloneFilters));
    object.insert(QStringLiteral("local"), listingToJson(snapshot.local));
    object.insert(QStringLiteral("remote"), listingToJson(snapshot.remote));
    QSaveFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
        if (error) {
            *error = i18nc("@info", "The synchronization state could not be saved to %1.", path);
        }
        return false;
    }
    return true;
}

QString CloudSync::snapshotLocation(const Pair &pair)
{
    return remoteLocationKey(pair);
}

Snapshot CloudSync::baseFor(const Pair &pair, const Snapshot &snapshot)
{
    const QString local = cleanLocal(pair.localPath);
    const QString remote = snapshotLocation(pair);
    if (snapshot.localPath == local && snapshot.remoteLocation == remote) {
        return snapshot;
    }
    Snapshot empty;
    empty.localPath = local;
    empty.remoteLocation = remote;
    return empty;
}

CloudSync::Filter::Filter(const FolderSync::Options &options)
    : m_recursive(options.recursive)
{
    const auto compile = [](const QStringList &patterns, QList<QRegularExpression> &result) {
        for (const QString &pattern : patterns) {
            result.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::NonPathWildcardConversion)));
        }
    };
    compile(options.includePatterns, m_included);
    compile(options.excludePatterns, m_excluded);
}

bool CloudSync::Filter::includes(const QString &relativePath, bool folder) const
{
    if (relativePath.isEmpty() || (!m_recursive && relativePath.contains(QLatin1Char('/')))) {
        return false;
    }
    const auto matches = [](const QList<QRegularExpression> &patterns, const QString &path) {
        return std::any_of(patterns.cbegin(), patterns.cend(), [&path](const QRegularExpression &pattern) {
            return pattern.match(path).hasMatch();
        });
    };
    qsizetype end = relativePath.size();
    while (end > 0) {
        if (matches(m_excluded, relativePath.left(end))) {
            return false;
        }
        end = relativePath.lastIndexOf(QLatin1Char('/'), end - 1);
    }
    return folder || m_included.isEmpty() || matches(m_included, relativePath);
}

int Plan::deletions() const
{
    return int(std::count_if(changes.cbegin(), changes.cend(), [](const Change &change) {
        return isDeletion(change.operation) && !change.folder;
    }));
}

qint64 Plan::uploadBytes() const
{
    qint64 total = 0;
    for (const Change &change : changes) {
        if (change.operation == Operation::Upload) {
            total += change.size;
        }
    }
    return total;
}

QString CloudSync::conflictLabel(const QDateTime &time, const QString &host)
{
    QString label = QStringLiteral("conflict ") + time.toString(QStringLiteral("yyyy-MM-dd HHmm"));
    QString cleanHost = host;
    cleanHost.remove(QLatin1Char('/'));
    if (!cleanHost.trimmed().isEmpty()) {
        label += QLatin1Char(' ') + cleanHost.trimmed();
    }
    return label;
}

QString CloudSync::conflictPath(const QString &path, const QString &label)
{
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    const QString folder = path.left(slash + 1);
    const QString name = path.mid(slash + 1);
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const QString text = QStringLiteral(" (") + (label.isEmpty() ? QStringLiteral("conflict") : label) + QLatin1Char(')');
    if (dot <= 0) {
        return folder + name + text;
    }
    return folder + name.left(dot) + text + name.mid(dot);
}

Plan CloudSync::plan(const Listing &local,
                     const Listing &remote,
                     const Snapshot &base,
                     Mode mode,
                     bool mirrorDeletions,
                     const QHash<QString, Resolution> &resolutions,
                     const QString &conflictName)
{
    Plan result;
    result.firstSync = base.local.isEmpty() && base.remote.isEmpty();

    QStringList paths = local.keys() + remote.keys() + base.local.keys() + base.remote.keys();
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    QList<Change> changes;
    QList<Conflict> conflicts;
    QSet<QString> handled;
    for (const auto &[from, to] : renames(local, remote, base.local, base.remote)) {
        const State state = local.value(to);
        changes.append({Operation::RenameRemote, to, from, false, state.size, false});
        handled.insert(from);
        handled.insert(to);
    }
    for (const auto &[from, to] : renames(remote, local, base.remote, base.local)) {
        if (handled.contains(from) || handled.contains(to)) {
            continue;
        }
        const State state = remote.value(to);
        changes.append({Operation::RenameLocal, to, from, false, state.size, false});
        handled.insert(from);
        handled.insert(to);
    }

    for (const QString &path : std::as_const(paths)) {
        if (handled.contains(path)) {
            continue;
        }
        const auto l = stateAt(local, path);
        const auto r = stateAt(remote, path);
        const auto bl = stateAt(base.local, path);
        const auto br = stateAt(base.remote, path);
        const bool localChanged = !sameSide(l, bl);
        const bool remoteChanged = !sameSide(r, br);
        const auto conflict = [&] {
            conflicts.append({path, l, r});
        };
        const auto inSync = [&] {
            result.inSync.append(path);
        };
        // Copies the item on the side that has it to the side that does not,
        // or compares both when both have it.
        const auto reconcile = [&] {
            if (l && r) {
                if (equivalent(*l, *r)) {
                    inSync();
                } else {
                    conflict();
                }
            } else if (l) {
                changes.append(copyTo(true, path, *l, false));
            } else if (r) {
                changes.append(copyTo(false, path, *r, false));
            } else {
                inSync();
            }
        };
        if (!bl && !br) {
            reconcile();
        } else if (!localChanged && !remoteChanged) {
            if (bool(l) == bool(r)) {
                inSync();
            } else {
                reconcile();
            }
        } else if (localChanged && !remoteChanged) {
            if (!l) {
                if (r) {
                    changes.append(deletion(true, path, *r));
                } else {
                    inSync();
                }
            } else if (r && r->folder != l->folder) {
                conflict();
            } else if (r && r->folder) {
                inSync();
            } else {
                changes.append(copyTo(true, path, *l, bool(r)));
            }
        } else if (remoteChanged && !localChanged) {
            if (!r) {
                if (l) {
                    changes.append(deletion(false, path, *l));
                } else {
                    inSync();
                }
            } else if (l && l->folder != r->folder) {
                conflict();
            } else if (l && l->folder) {
                inSync();
            } else {
                changes.append(copyTo(false, path, *r, bool(l)));
            }
        } else {
            // Changed on both sides. A deleted item gives way to an edited one.
            reconcile();
        }
    }

    QList<Change> kept;
    for (const Change &change : std::as_const(changes)) {
        QString skipped;
        if (const auto adjusted = forMode(change, mode, mirrorDeletions, local, remote, &skipped)) {
            kept.append(*adjusted);
        } else if (!skipped.isEmpty()) {
            result.skipped.append(skipped);
        }
    }

    // Resolutions are the user's explicit choices, so they apply in every mode.
    QSet<QString> taken(local.keyBegin(), local.keyEnd());
    for (auto it = remote.keyBegin(); it != remote.keyEnd(); ++it) {
        taken.insert(*it);
    }
    for (const Conflict &item : std::as_const(conflicts)) {
        const Resolution resolution = resolutions.value(item.path, Resolution::Skip);
        if (resolution == Resolution::Skip || !item.isResolvable()) {
            result.conflicts.append(item);
            continue;
        }
        switch (resolution) {
        case Resolution::KeepLocal:
            kept.append(copyTo(true, item.path, *item.local, true));
            break;
        case Resolution::KeepRemote:
            kept.append(copyTo(false, item.path, *item.remote, true));
            break;
        case Resolution::KeepBoth: {
            QString copy = conflictPath(item.path, conflictName);
            for (int number = 2; taken.contains(copy); ++number) {
                copy =
                    conflictPath(item.path, (conflictName.isEmpty() ? QStringLiteral("conflict") : conflictName) + QLatin1Char(' ') + QString::number(number));
            }
            taken.insert(copy);
            kept.append({Operation::Download, copy, item.path, false, item.remote->size, false});
            kept.append(copyTo(true, item.path, *item.local, true));
            break;
        }
        case Resolution::Skip:
            break;
        }
    }

    kept = pruneFolderDeletions(kept, local, remote);
    std::stable_sort(kept.begin(), kept.end(), [](const Change &first, const Change &second) {
        const int a = phase(first);
        const int b = phase(second);
        if (a != b) {
            return a < b;
        }
        if (a == 0) {
            return first.path < second.path;
        }
        if (a == 4) {
            return first.path > second.path;
        }
        return false;
    });
    result.changes = kept;
    return result;
}

bool CloudSync::isLargeDeletion(const Plan &plan, const Snapshot &base)
{
    const int deletions = plan.deletions();
    if (deletions >= LargeDeletionCount) {
        return true;
    }
    const auto files = std::count_if(base.local.cbegin(), base.local.cend(), [](const State &state) {
        return !state.folder;
    });
    return deletions >= LargeDeletionMinimum && deletions * 2 >= files;
}

bool CloudSync::needsReview(const Plan &plan, const Snapshot &base)
{
    return (plan.firstSync && !plan.changes.isEmpty()) || isLargeDeletion(plan, base);
}

Snapshot CloudSync::nextSnapshot(const Snapshot &base,
                                 const Listing &local,
                                 const Listing &remote,
                                 const Plan &plan,
                                 const QList<Change> &done,
                                 const Listing &localAfter,
                                 const Listing &remoteAfter)
{
    Snapshot next = base;
    const auto forget = [&next](const QString &path) {
        next.local.remove(path);
        next.remote.remove(path);
    };
    const auto record = [&next](const QString &path, const std::optional<State> &l, const std::optional<State> &r) {
        // Without both states the previous ones are kept, so the item is
        // compared again next time.
        if (l && r) {
            next.local.insert(path, *l);
            next.remote.insert(path, *r);
        }
    };
    const auto before = [](const Listing &listing, const Listing &after, const QString &path) {
        const auto state = stateAt(listing, path);
        return state ? state : stateAt(after, path);
    };
    for (const QString &path : plan.inSync) {
        const auto l = stateAt(local, path);
        const auto r = stateAt(remote, path);
        if (l && r) {
            record(path, l, r);
        } else if (!l && !r) {
            forget(path);
        }
    }
    for (const Change &change : done) {
        switch (change.operation) {
        case Operation::Upload:
        case Operation::CreateRemoteFolder:
            record(change.path, before(local, localAfter, change.path), stateAt(remoteAfter, change.path));
            break;
        case Operation::Download:
        case Operation::CreateLocalFolder:
            // A conflict copy exists only locally and is uploaded next time.
            if (change.source.isEmpty()) {
                record(change.path, stateAt(localAfter, change.path), before(remote, remoteAfter, change.path));
            }
            break;
        case Operation::DeleteRemote:
        case Operation::DeleteLocal:
            forget(change.path);
            break;
        case Operation::RenameRemote:
            forget(change.source);
            record(change.path, before(local, localAfter, change.path), stateAt(remoteAfter, change.path));
            break;
        case Operation::RenameLocal:
            forget(change.source);
            record(change.path, stateAt(localAfter, change.path), before(remote, remoteAfter, change.path));
            break;
        }
    }
    return next;
}

QString CloudSync::describe(const Change &change)
{
    switch (change.operation) {
    case Operation::Upload:
        return change.overwrite ? i18nc("@item", "Upload, replacing the remote file") : i18nc("@item", "Upload");
    case Operation::Download:
        if (!change.source.isEmpty()) {
            return i18nc("@item", "Save the remote version of %1", change.source);
        }
        return change.overwrite ? i18nc("@item", "Download, replacing the local file") : i18nc("@item", "Download");
    case Operation::CreateRemoteFolder:
        return i18nc("@item", "Create the remote folder");
    case Operation::CreateLocalFolder:
        return i18nc("@item", "Create the local folder");
    case Operation::DeleteRemote:
        return change.folder ? i18nc("@item", "Delete the remote folder") : i18nc("@item", "Delete the remote file");
    case Operation::DeleteLocal:
        return change.folder ? i18nc("@item", "Move the local folder to the trash") : i18nc("@item", "Move the local file to the trash");
    case Operation::RenameRemote:
        return i18nc("@item", "Rename the remote file from %1", change.source);
    case Operation::RenameLocal:
        return i18nc("@item", "Rename the local file from %1", change.source);
    }
    return {};
}

QList<PreviewItem> CloudSync::previewItems(const Plan &plan)
{
    QList<PreviewItem> items;
    for (const Change &change : plan.changes) {
        items.append({change.path, describe(change), isDeletion(change.operation)});
    }
    return items;
}

ErrorKind CloudSync::classifyError(int kioError)
{
    switch (kioError) {
    case KIO::ERR_CANNOT_CONNECT:
    case KIO::ERR_UNKNOWN_HOST:
    case KIO::ERR_CONNECTION_BROKEN:
    case KIO::ERR_SERVER_TIMEOUT:
    case KIO::ERR_UNKNOWN_PROXY_HOST:
        return ErrorKind::Offline;
    case KIO::ERR_ACCESS_DENIED:
    case KIO::ERR_WRITE_ACCESS_DENIED:
    case KIO::ERR_CANNOT_LOGIN:
    case KIO::ERR_CANNOT_AUTHENTICATE:
        return ErrorKind::Permission;
    case KIO::ERR_DISK_FULL:
        return ErrorKind::Quota;
    default:
        return ErrorKind::Other;
    }
}

QStringList CloudSync::rcloneFilterRules(const FolderSync::Options &options, QString *error)
{
    if (error) {
        error->clear();
    }
    QStringList rules;
    for (const QString &pattern : options.excludePatterns) {
        const QString converted = rclonePattern(pattern, error);
        if (converted.isEmpty()) {
            return {};
        }
        rules << QStringLiteral("- ") + converted << QStringLiteral("- ") + converted + QStringLiteral("/**");
    }
    for (const QString &pattern : options.includePatterns) {
        const QString converted = rclonePattern(pattern, error);
        if (converted.isEmpty()) {
            return {};
        }
        rules << QStringLiteral("+ ") + converted;
    }
    if (!options.includePatterns.isEmpty()) {
        rules << QStringLiteral("- **");
    }
    return rules;
}

QStringList CloudSync::rcloneArguments(const Pair &pair, bool initialize, bool dryRun, QString *error)
{
    const QString problem = pairError(pair, {});
    if (!pair.usesRclone() || !problem.isEmpty()) {
        if (error) {
            *error = problem.isEmpty() ? i18nc("@info", "This pair does not use rclone.") : problem;
        }
        return {};
    }
    const QString local = cleanLocal(pair.localPath);
    QStringList arguments;
    switch (pair.mode) {
    case Mode::Sync:
        arguments << QStringLiteral("bisync") << local << pair.rcloneRemote;
        if (initialize) {
            arguments << QStringLiteral("--resync");
        }
        break;
    case Mode::CloudBackup:
        arguments << (pair.mirrorDeletions ? QStringLiteral("sync") : QStringLiteral("copy")) << local << pair.rcloneRemote << QStringLiteral("--update");
        break;
    case Mode::LocalBackup:
        arguments << (pair.mirrorDeletions ? QStringLiteral("sync") : QStringLiteral("copy")) << pair.rcloneRemote << local << QStringLiteral("--update");
        break;
    }
    arguments << QStringLiteral("--create-empty-src-dirs");
    if (!pair.options.recursive) {
        arguments << QStringLiteral("--max-depth") << QStringLiteral("1");
    }
    for (const QString &rule : rcloneFilterRules(pair.options)) {
        arguments << QStringLiteral("--filter") << rule;
    }
    if (dryRun) {
        arguments << QStringLiteral("--dry-run");
    }
    return arguments;
}

QList<PreviewItem> CloudSync::parseRcloneDryRun(const QByteArray &output)
{
    static const QRegularExpression line(QStringLiteral("NOTICE: (.+?): Skipped (.+?) as --dry-run is set"));
    QList<PreviewItem> items;
    for (const QByteArray &raw : output.split('\n')) {
        const auto match = line.match(QString::fromUtf8(raw));
        if (!match.hasMatch()) {
            continue;
        }
        const QString action = match.captured(2);
        PreviewItem item;
        item.path = match.captured(1);
        item.deletion = action.contains(QLatin1String("delete")) || action.contains(QLatin1String("remove"));
        if (action == QLatin1String("copy")) {
            item.description = i18nc("@item", "Copy");
        } else if (action == QLatin1String("delete")) {
            item.description = i18nc("@item", "Delete");
        } else if (action == QLatin1String("make directory")) {
            item.description = i18nc("@item", "Create the folder");
        } else if (action == QLatin1String("remove directory")) {
            item.description = i18nc("@item", "Delete the folder");
        } else {
            item.description = action;
        }
        items.append(item);
    }
    return items;
}

bool CloudSync::rcloneNeedsResync(const QByteArray &output)
{
    return output.contains("Must run --resync") || output.contains("cannot find prior Path1 or Path2 listings");
}

QString CloudSync::autostartPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("autostart/%1-cloud-sync.desktop").arg(AppIdentity::desktopFileName()));
}

QByteArray CloudSync::autostartEntry(const QString &program, const QString &iconName)
{
    QString entry;
    entry += QStringLiteral("[Desktop Entry]\n");
    entry += QStringLiteral("Type=Application\n");
    entry += QStringLiteral("Name=") + i18nc("@title autostart entry", "%1 Cloud Sync", AppIdentity::displayName()) + QLatin1Char('\n');
    entry += QStringLiteral("Comment=") + i18nc("@info autostart entry", "Synchronizes cloud sync pairs in the background") + QLatin1Char('\n');
    entry += QStringLiteral("Exec=") + escapeExec(program) + QStringLiteral(" --background-sync\n");
    entry += QStringLiteral("Icon=") + iconName + QLatin1Char('\n');
    entry += QStringLiteral("NoDisplay=true\n");
    entry += QStringLiteral("X-KDE-autostart-phase=2\n");
    return entry.toUtf8();
}

bool CloudSync::startsAtLogin()
{
    return QFile::exists(autostartPath());
}

QString CloudSync::execArgument(const QString &argument)
{
    return escapeExec(argument);
}

bool CloudSync::setStartAtLogin(bool enabled, const QString &program, const QString &iconName, QString *error)
{
    const QString path = autostartPath();
    if (!enabled) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            if (error) {
                *error = i18nc("@info", "%1 could not be removed.", path);
            }
            return false;
        }
        return true;
    }
    QSaveFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly) || file.write(autostartEntry(program, iconName)) < 0
        || !file.commit()) {
        if (error) {
            *error = i18nc("@info", "%1 could not be written.", path);
        }
        return false;
    }
    return true;
}
