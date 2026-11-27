// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QProcess;
class QTimer;

/**
 * Opens cloud storage configured in rclone by mounting remotes through
 * `rclone mount`, or by serving them over WebDAV on the loopback interface
 * through `rclone serve webdav`. Plasma Opus only lists remote names and
 * types: it never reads, stores or changes rclone's configuration or
 * credentials.
 */
namespace Rclone
{
struct Remote {
    QString name;
    /// The backend, such as "drive" or "dropbox"; empty when rclone doesn't report it.
    QString type;
};

/// The rclone executable in PATH, or an empty string.
QString findExecutable();
/// The arguments that list configured remotes with their types.
QStringList listArguments();
/// Parses `rclone listremotes --long` output, skipping malformed lines.
QList<Remote> parseRemotes(const QByteArray &output);
/// Whether @p name can be passed to rclone as a remote.
bool isValidName(const QString &name);
/// The folder that holds Plasma Opus's mounts, inside the private runtime directory.
QString mountRoot();
/// The empty folder @p remote is mounted on.
QString mountPoint(const QString &remote);
/**
 * The arguments that mount @p remote on @p mountPoint in the foreground.
 * @param options cache and transfer options; empty caches only files being written
 */
QStringList mountArguments(const QString &remote, const QString &mountPoint, const QStringList &options = {});
/// The user name that served remotes require; their passwords are random.
QString serveUser();
/// The realm that served remotes name when they ask for the password.
QString serveRealm();
/**
 * The arguments that serve @p remote over WebDAV on the loopback interface
 * at @p port in the foreground, requiring serveUser() and the password that
 * serveEnvironment() passes, which stays out of the command line.
 * @param options cache and transfer options; empty caches only files being written
 */
QStringList serveArguments(const QString &remote, quint16 port, const QStringList &options = {});
/// environment() with the password a served remote requires.
QProcessEnvironment serveEnvironment(const QString &password);
/// The location that opens a remote served at @p port, with the user name but never the password.
QUrl serveUrl(quint16 port);
/// A random password for a served remote.
QString randomPassword();
/**
 * The port @p remote is served on when it is free: the same for each remote
 * and home folder, so that its location and the password KDE keeps for it
 * stay the same. It lies below the kernel's ephemeral port range.
 */
quint16 preferredServePort(const QString &remote);
/**
 * The first TCP port on the loopback interface from @p preferred onwards
 * that is free now, trying a few, then any free port, or 0.
 */
quint16 freeLoopbackPort(quint16 preferred = 0);
/// Whether a server accepts connections on the loopback interface at @p port.
bool isListening(quint16 port);
/// The environment for rclone, which fails instead of prompting for a configuration password.
QProcessEnvironment environment();
/// The last message rclone wrote to standard error, without its timestamp.
QString lastError(const QByteArray &standardError);
/// Whether a file system is mounted exactly at @p path.
bool isMountPoint(const QString &path);
}

/**
 * Runs and stops the `rclone mount` and `rclone serve webdav` processes
 * started by Plasma Opus. They stop when the application quits or the object
 * is destroyed. Each remote runs at most once, mounted or served.
 */
class RcloneMounts : public QObject
{
    Q_OBJECT

public:
    using MountCheck = std::function<bool(const QString &path)>;
    using ServeCheck = std::function<bool(quint16 port)>;
    /**
     * Looks up or creates the password that serving @p remote at @p url
     * requires, and calls @p done with it, now or later; with an empty
     * password, the remote isn't served.
     */
    using PasswordProvider = std::function<void(const QString &remote, const QUrl &url, const std::function<void(const QString &password)> &done)>;

    explicit RcloneMounts(QObject *parent = nullptr);
    ~RcloneMounts() override;

    /// Replaces how mounts are detected, for tests.
    void setMountCheck(MountCheck check);
    /// Replaces how a started server is detected, for tests.
    void setServeCheck(ServeCheck check);
    /// Replaces the random password used for each server, which no client knows.
    void setPasswordProvider(PasswordProvider provider);
    /// Sets how long to wait for a mount or server to appear, in milliseconds.
    void setTimeout(int milliseconds);
    /// Sets the cache and transfer options of later mounts and servers.
    void setMountOptions(const QStringList &options);
    QStringList mountOptions() const;

    /**
     * Mounts @p remote, emitting mounted() or failed(). Mounting a mounted
     * remote emits mounted() again, and a served one emits served() again.
     */
    void mount(const QString &remote);
    /**
     * Serves @p remote over WebDAV on the loopback interface, emitting served()
     * or failed(). Serving a served remote emits served() again, and a mounted
     * one emits mounted() again.
     */
    void serve(const QString &remote);
    /// Lists the configured remotes in the background, emitting remotesListed().
    void listRemotes();
    /// Stops the mount or server Plasma Opus started for @p remote.
    void unmount(const QString &remote);
    /// Stops every mount and server Plasma Opus started.
    void unmountAll();
    /// Stops every mount and server and waits for rclone to exit, without emitting signals.
    void stopAllNow();

    bool isMounted(const QString &remote) const;
    bool isServed(const QString &remote) const;
    bool isStarting(const QString &remote) const;
    QStringList mountedRemotes() const;
    QStringList servedRemotes() const;
    /// The location of a served remote, or an empty URL.
    QUrl servedUrl(const QString &remote) const;

Q_SIGNALS:
    void mounted(const QString &remote, const QString &path);
    /// @p url opens the remote with the password from the PasswordProvider.
    void served(const QString &remote, const QUrl &url);
    void failed(const QString &remote, const QString &error);
    void unmounted(const QString &remote);
    void serveStopped(const QString &remote, const QUrl &url);
    /// @p error is empty when listing succeeded.
    void remotesListed(const QList<Rclone::Remote> &remotes, const QString &error);

private:
    struct Mount {
        QProcess *process = nullptr;
        QTimer *poll = nullptr;
        QString path;
        /// Set while the remote is served instead of mounted.
        QUrl url;
        QByteArray errors;
        bool ready = false;
        bool stopping = false;
        int elapsed = 0;
    };

    /// Starts serving @p remote at @p url with @p password, unless it was stopped meanwhile.
    void startServing(const QString &remote, const QUrl &url, const QString &password);
    /// Starts rclone for @p remote with @p mount's path or URL set.
    void start(const QString &remote, Mount mount, const QStringList &arguments, const QProcessEnvironment &environment);
    /// Emits mounted() or served() again when @p remote runs; whether it runs.
    bool reopen(const QString &remote);
    void poll(const QString &remote);
    void finish(const QString &remote, const QString &error);
    void stop(const QString &remote);

    QHash<QString, Mount> m_mounts;
    QProcess *m_lister = nullptr;
    MountCheck m_check;
    ServeCheck m_serveCheck;
    PasswordProvider m_passwords;
    QStringList m_options;
    int m_timeout = 20000;
};
