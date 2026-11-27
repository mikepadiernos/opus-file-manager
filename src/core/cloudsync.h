// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"

#include <KConfigGroup>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

/// Two-way synchronization of local folders with remote locations.
///
/// A pair links a local folder with a KIO location, such as a Nextcloud,
/// WebDAV, SFTP, SMB or S3 folder, or with an rclone remote. KIO pairs keep a
/// snapshot of both sides after each run and compare each side with it, so a
/// run can tell edits, additions, deletions and renames on either side apart
/// and never overwrites a file that changed on both sides. rclone pairs use
/// `rclone bisync`, or `rclone copy` and `rclone sync` for backups.
///
/// Pairs store no credentials: KIO asks for them through KDE's password
/// facilities, and rclone keeps its own configuration.
namespace CloudSync
{
enum class Mode {
    /// Changes on either side are copied to the other.
    Sync,
    /// Local changes are uploaded; the remote is a backup of the local folder.
    CloudBackup,
    /// Remote changes are downloaded; the local folder is a backup of the remote.
    LocalBackup,
};

struct Pair {
    QString id;
    QString localPath;
    /// The KIO location, or empty for an rclone pair.
    QUrl remote;
    /// The rclone location, such as "drive:Documents", or empty for a KIO pair.
    QString rcloneRemote;
    Mode mode = Mode::Sync;
    /// In backup modes, whether deleting an item from the source deletes the backup copy.
    bool mirrorDeletions = false;
    /// The include and exclude patterns and whether subfolders are synchronized.
    FolderSync::Options options;
    /// How often to synchronize in the background, or 0 to synchronize only on request.
    int intervalMinutes = 0;
    bool paused = false;

    bool usesRclone() const
    {
        return !rcloneRemote.isEmpty();
    }
    bool operator==(const Pair &other) const = default;
};

/// A new, unique pair identifier.
QString newId();
/// The remote as shown to the user, without any password.
QString remoteDisplayName(const Pair &pair);
/// The account a pair belongs to, such as "webdavs://user@cloud.example" or
/// "rclone:drive". Pausing an account pauses all its pairs.
QString accountKey(const Pair &pair);
/// The account as shown to the user, such as "user@cloud.example".
QString accountDisplayName(const Pair &pair);
QString modeName(Mode mode);

/// Reads the pairs stored in @p group, skipping invalid ones.
QList<Pair> load(const KConfigGroup &group);
/// Replaces the pairs stored in @p group. Passwords in remote locations are
/// never stored.
bool save(KConfigGroup group, const QList<Pair> &pairs, QString *error = nullptr);

/// A localized reason @p pair cannot be used, or an empty string. Pairs that
/// overlap @p others are allowed; see nesting().
QString pairError(const Pair &pair, const QList<Pair> &others);

/// How a pair overlaps another pair.
struct Nesting {
    QString otherId;
    /// Whether the pair is inside the other pair, rather than containing it.
    bool inside = false;
    /// Whether the folders overlap on the remote side rather than locally.
    bool remote = false;
    /// The inner folder's path relative to the outer folder.
    QString relativePath;
};
/// The first pair in @p others whose folders contain, or are inside, those of
/// @p pair, on either side.
std::optional<Nesting> nesting(const Pair &pair, const QList<Pair> &others);
/// A filter pattern that matches exactly @p relativePath, for excluding a
/// nested pair's folder from the outer pair.
QString exactPattern(const QString &relativePath);

/// Where other synchronization clients keep their configuration.
struct EngineLocations {
    /// $XDG_CONFIG_HOME, usually ~/.config.
    QString configHome;
    /// $XDG_STATE_HOME, usually ~/.local/state.
    QString stateHome;
};
/// The standard locations of the current user.
EngineLocations defaultEngineLocations();
/// The name of another synchronization client, such as "Nextcloud" or
/// "Syncthing", that manages @p localPath, a folder inside it or a folder
/// containing it, or an empty string. Clients are found through their
/// configuration and through the marker files they keep in synchronized
/// folders.
QString managingEngine(const QString &localPath, const EngineLocations &locations);

/// An item on one side of a pair.
struct State {
    bool folder = false;
    qint64 size = 0;
    /// The modification time in seconds since the epoch, or 0 when unknown.
    qint64 modified = 0;

    bool operator==(const State &other) const = default;
};
/// The items on one side, by path relative to the pair's folder.
using Listing = QMap<QString, State>;

/// What both sides held after the last run, and rclone's state.
struct Snapshot {
    /// The pair's folders when the snapshot was taken. A snapshot of other
    /// folders is ignored.
    QString localPath;
    QString remoteLocation;
    Listing local;
    Listing remote;
    bool rcloneInitialized = false;
    /// The rclone filter rules of the last bisync run.
    QStringList rcloneFilters;

    bool isEmpty() const
    {
        return local.isEmpty() && remote.isEmpty() && !rcloneInitialized;
    }
    bool operator==(const Snapshot &other) const = default;
};
/// Where the snapshot of the pair @p id is kept.
QString snapshotPath(const QString &id);
/// Reads a snapshot. A missing file yields an empty snapshot; an unreadable
/// one returns nothing and sets @p error.
std::optional<Snapshot> loadSnapshot(const QString &path, QString *error = nullptr);
bool saveSnapshot(const QString &path, const Snapshot &snapshot, QString *error = nullptr);
/// The snapshot to compare @p pair with: @p snapshot, or an empty one when it
/// was taken for other folders.
Snapshot baseFor(const Pair &pair, const Snapshot &snapshot);
QString snapshotLocation(const Pair &pair);

/// Decides which items a pair synchronizes. Excluded folders exclude
/// everything inside them, folders are kept whatever the include patterns,
/// and only top-level items are kept when subfolders are off, as in folder
/// synchronization.
class Filter
{
public:
    /// @p options must be valid; see FolderSync::optionsError().
    explicit Filter(const FolderSync::Options &options);
    bool includes(const QString &relativePath, bool folder) const;

private:
    QList<QRegularExpression> m_included;
    QList<QRegularExpression> m_excluded;
    bool m_recursive = true;
};

enum class Operation {
    Upload,
    Download,
    CreateRemoteFolder,
    CreateLocalFolder,
    DeleteRemote,
    DeleteLocal,
    /// Renames the remote item at source to path, after a local rename.
    RenameRemote,
    /// Renames the local item at source to path, after a remote rename.
    RenameLocal,
};

struct Change {
    Operation operation = Operation::Upload;
    QString path;
    /// For renames, the previous path. For a download that keeps both
    /// versions of a conflict, the remote file saved as path.
    QString source;
    bool folder = false;
    qint64 size = 0;
    /// Whether the destination exists and is replaced.
    bool overwrite = false;

    bool operator==(const Change &other) const = default;
};

/// A file that changed on both sides, or an item that is a file on one side
/// and a folder on the other.
struct Conflict {
    QString path;
    std::optional<State> local;
    std::optional<State> remote;

    /// Whether the conflict can be resolved by keeping one or both versions.
    /// Conflicts between a file and a folder are left for the user to rename.
    bool isResolvable() const
    {
        return local && remote && !local->folder && !remote->folder;
    }
    bool operator==(const Conflict &other) const = default;
};

enum class Resolution {
    Skip,
    KeepLocal,
    KeepRemote,
    /// Keeps the local file and saves the remote one beside it under a
    /// conflict name. The next run uploads the conflict copy.
    KeepBoth,
};

struct Plan {
    /// The changes in the order they are applied: folder creation, renames,
    /// file transfers, file deletions and folder deletions.
    QList<Change> changes;
    QList<Conflict> conflicts;
    /// Changes the mode does not carry out, such as remote edits in a cloud
    /// backup, described for the report.
    QStringList skipped;
    /// Paths that are the same on both sides, or are missing from both.
    QStringList inSync;
    /// Whether there was no snapshot to compare with.
    bool firstSync = false;

    int deletions() const;
    qint64 uploadBytes() const;
    bool operator==(const Plan &other) const = default;
};

/// The suffix of conflict copies, such as "conflict 2026-11-26 1530 laptop".
QString conflictLabel(const QDateTime &time, const QString &host);
/// @p path with @p label added before its extension, such as
/// "notes (conflict 2026-11-26 1530 laptop).txt".
QString conflictPath(const QString &path, const QString &label);

/// Plans a run from both listings and the snapshot. Changes on one side are
/// copied to the other. A file changed on both sides is a conflict unless
/// both versions have the same size and times within two seconds; a
/// deletion on one side and an edit on the other keep the edited item. A
/// file deleted on one side and added elsewhere on the same side with the
/// same size and time is renamed on the other. Folders are deleted only when
/// everything in them is deleted. @p resolutions resolve conflicts by path,
/// and @p conflictName labels conflict copies.
Plan plan(const Listing &local,
          const Listing &remote,
          const Snapshot &base,
          Mode mode,
          bool mirrorDeletions,
          const QHash<QString, Resolution> &resolutions = {},
          const QString &conflictName = {});

/// Whether a plan deletes enough to need confirmation: at least 50 files,
/// or at least 5 and half of what the snapshot holds.
bool isLargeDeletion(const Plan &plan, const Snapshot &base);
/// Whether a plan needs the user's confirmation before it runs: the first
/// run of a pair, and large deletions.
bool needsReview(const Plan &plan, const Snapshot &base);

/// The snapshot after a run. Items in sync take their current states; items
/// changed by @p done take their state after the run, from @p localAfter and
/// @p remoteAfter for the side that was written; everything else keeps its
/// previous state, so failed and skipped changes are found again next time.
Snapshot nextSnapshot(const Snapshot &base,
                      const Listing &local,
                      const Listing &remote,
                      const Plan &plan,
                      const QList<Change> &done,
                      const Listing &localAfter,
                      const Listing &remoteAfter);

/// A change as shown in a preview.
struct PreviewItem {
    QString path;
    QString description;
    bool deletion = false;

    bool operator==(const PreviewItem &other) const = default;
};
QList<PreviewItem> previewItems(const Plan &plan);
QString describe(const Change &change);

/// How a KIO error affects a run.
enum class ErrorKind {
    Other,
    /// The remote cannot be reached; the run is retried later.
    Offline,
    Permission,
    Quota,
};
ErrorKind classifyError(int kioError);

/// The rclone filter rules for the synchronization filters. "*" becomes "**"
/// so it also matches "/", as in folder synchronization. Sets @p error for
/// patterns rclone cannot express the same way, such as those with "?".
QStringList rcloneFilterRules(const FolderSync::Options &options, QString *error = nullptr);
/// The rclone arguments for a run of @p pair. Sync pairs use bisync, with
/// --resync when @p initialize is set; backups use copy, or sync when
/// deletions are mirrored, and never replace newer files at the destination.
QStringList rcloneArguments(const Pair &pair, bool initialize, bool dryRun, QString *error = nullptr);
/// The changes a dry run reported, from rclone's log output.
QList<PreviewItem> parseRcloneDryRun(const QByteArray &output);
/// Whether bisync's output asks for another --resync run.
bool rcloneNeedsResync(const QByteArray &output);

/// Where the autostart entry that synchronizes pairs from login is kept.
QString autostartPath();
/// The autostart entry that starts @p program in the background.
QByteArray autostartEntry(const QString &program, const QString &iconName);
bool startsAtLogin();
bool setStartAtLogin(bool enabled, const QString &program, const QString &iconName, QString *error = nullptr);
}
