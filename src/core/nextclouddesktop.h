// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

class QLocalSocket;
class QTimer;

namespace NextcloudAccounts
{
struct Account;
}

/**
 * The folders the Nextcloud desktop client synchronizes, read from its
 * configuration, and their status, asked of the running client through its
 * local socket. Plasma Opus only reads the configuration and asks for status:
 * it never changes the client's configuration, synchronizes the folders
 * itself or starts the client unless asked to open it.
 */
namespace NextcloudDesktop
{
/// A folder the desktop client synchronizes.
struct Folder {
    /// The client's identifier of the account, such as "0".
    QString accountId;
    /// The client's identifier of the folder within its account, such as "1".
    QString id;
    /// The folder on this computer, without a trailing slash.
    QString localPath;
    /// The folder on the server, relative to the user's files, such as "/" or
    /// "/Documents".
    QString targetPath;
    /// The client's virtual files mode, such as "off", "suffix" or "xattr";
    /// empty when the configuration doesn't say.
    QString virtualFilesMode;
    /// Whether synchronization of the folder is paused in the client.
    bool paused = false;
    /// The account's server address, without a trailing slash.
    QUrl server;
    /// The account's user id, which names the user's WebDAV folder.
    QString user;

    bool operator==(const Folder &) const = default;
};

/// Whether the folder keeps virtual files that are downloaded when opened,
/// rather than full copies.
bool usesVirtualFiles(const Folder &folder);

struct Configuration {
    /// Whether the configuration file exists.
    bool exists = false;
    /// The synchronized folders, in the order of their accounts and identifiers.
    QList<Folder> folders;
    /// Why the configuration couldn't be read, or an empty string.
    QString error;
};

/// The desktop client's configuration file under @p configHome, usually
/// ~/.config/Nextcloud/nextcloud.cfg.
QString configurationPath(const QString &configHome);
/// The desktop client's configuration file of the current user.
QString defaultConfigurationPath();
/// The local socket of the desktop client of the current user, usually
/// $XDG_RUNTIME_DIR/Nextcloud/socket.
QString defaultSocketPath();
/**
 * Reads the synchronized folders from the configuration file at @p path,
 * which can also be an ownCloud client's configuration. The file is only
 * read, never written or locked.
 */
Configuration readConfiguration(const QString &path);

/// The account of @p accounts with the server and user of @p folder.
std::optional<NextcloudAccounts::Account> accountFor(const Folder &folder, const QList<NextcloudAccounts::Account> &accounts);
/// The webdavs:// URL of the folder on the server, in the files of @p account.
QUrl serverFolder(const NextcloudAccounts::Account &account, const Folder &folder);

/// What the client reports about a path.
enum class Sync {
    /// The client didn't answer, or answered with a status Plasma Opus doesn't know.
    Unknown,
    /// The client doesn't synchronize the path, or can't tell, such as while
    /// it can't reach the server.
    NotSynchronized,
    UpToDate,
    Syncing,
    /// The client shows a warning, such as for excluded or ignored items.
    Warning,
    Error,
};
/// The status in a STATUS message, such as "OK" or "SYNC+SWM".
Sync parseSync(const QString &status);

/// A path and its status, from a "STATUS:<status>:<path>" message.
struct StatusMessage {
    QString status;
    QString path;
};
/// The status message in @p line, a line the client sent, or nullopt for other messages.
std::optional<StatusMessage> parseStatusMessage(const QByteArray &line);
/// The request for the status of @p path, ending with a newline.
QByteArray statusRequest(const QString &path);

/// Where a folder's files are, as far as Plasma Opus can tell.
enum class Availability {
    /// Full copies are on this computer and the client keeps them synchronized.
    Local,
    /// The folder holds virtual files whose contents stay on the server until
    /// they are opened, so they need a connection.
    Remote,
    /// The client isn't running or has paused the folder, so changes aren't
    /// synchronized until it resumes.
    Offline,
    /// The status isn't available.
    Unknown,
};

/// What Plasma Opus shows about a folder.
struct FolderState {
    Availability availability = Availability::Unknown;
    Sync sync = Sync::Unknown;
    /// A sentence for the folder's tooltip.
    QString description;
};
/**
 * The state of @p folder. @p clientRunning is whether the client's socket
 * answered, and @p status the status it reported for the folder, if any.
 * @p exists is whether the folder exists on this computer.
 */
FolderState folderState(const Folder &folder, bool exists, bool clientRunning, const std::optional<QString> &status);
/// A short name for @p availability, such as "Local" or "Remote".
QString availabilityText(Availability availability);
/// A short name for @p sync, such as "Up to date".
QString syncText(Sync sync);

/**
 * Asks the running desktop client for the status of paths through its local
 * socket. The query only reads status; it never asks the client to change
 * anything.
 */
class StatusQuery : public QObject
{
    Q_OBJECT
public:
    explicit StatusQuery(const QString &socketPath, QObject *parent = nullptr);
    ~StatusQuery() override;

    /**
     * Asks for the status of @p paths, local paths without trailing slashes.
     * finished() follows once the client has answered for every path, or
     * after @p timeoutMs.
     */
    void start(const QStringList &paths, int timeoutMs = 2000);
    bool isRunning() const;

Q_SIGNALS:
    /**
     * @param connected whether the client's socket accepted the connection
     * @param statuses the status the client reported, by requested path
     */
    void finished(bool connected, const QHash<QString, QString> &statuses);

private:
    void readMessages();
    void finish(bool connected);

    QString m_socketPath;
    QPointer<QLocalSocket> m_socket;
    QTimer *m_timer = nullptr;
    QSet<QString> m_pending;
    QHash<QString, QString> m_statuses;
    bool m_connected = false;
    bool m_running = false;
    int m_generation = 0;
};
}
