// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;
class QTimer;

/**
 * Opens cloud storage configured in rclone by mounting remotes through
 * `rclone mount`. Plasma Opus only lists remote names and types: it never
 * reads, stores or changes rclone's configuration or credentials.
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
/// The arguments that mount @p remote on @p mountPoint in the foreground.
QStringList mountArguments(const QString &remote, const QString &mountPoint);
/// The environment for rclone, which fails instead of prompting for a configuration password.
QProcessEnvironment environment();
/// The last message rclone wrote to standard error, without its timestamp.
QString lastError(const QByteArray &standardError);
/// Whether a file system is mounted exactly at @p path.
bool isMountPoint(const QString &path);
}

/**
 * Runs and stops `rclone mount` processes started by Plasma Opus. Mounts
 * stop when the application quits or the object is destroyed.
 */
class RcloneMounts : public QObject
{
    Q_OBJECT

public:
    using MountCheck = std::function<bool(const QString &path)>;

    explicit RcloneMounts(QObject *parent = nullptr);
    ~RcloneMounts() override;

    /// Replaces how mounts are detected, for tests.
    void setMountCheck(MountCheck check);
    /// Sets how long to wait for a mount to appear, in milliseconds.
    void setTimeout(int milliseconds);

    /// Mounts @p remote, emitting mounted() or failed(). Mounting a mounted remote emits mounted() again.
    void mount(const QString &remote);
    /// Lists the configured remotes in the background, emitting remotesListed().
    void listRemotes();
    /// Stops the mount Plasma Opus started for @p remote.
    void unmount(const QString &remote);
    /// Stops every mount Plasma Opus started.
    void unmountAll();
    /// Stops every mount and waits for rclone to exit, without emitting signals.
    void stopAllNow();

    bool isMounted(const QString &remote) const;
    bool isStarting(const QString &remote) const;
    QStringList mountedRemotes() const;

Q_SIGNALS:
    void mounted(const QString &remote, const QString &path);
    void failed(const QString &remote, const QString &error);
    void unmounted(const QString &remote);
    /// @p error is empty when listing succeeded.
    void remotesListed(const QList<Rclone::Remote> &remotes, const QString &error);

private:
    struct Mount {
        QProcess *process = nullptr;
        QTimer *poll = nullptr;
        QString path;
        QByteArray errors;
        bool ready = false;
        bool stopping = false;
        int elapsed = 0;
    };

    void poll(const QString &remote);
    void finish(const QString &remote, const QString &error);
    void stop(const QString &remote);

    QHash<QString, Mount> m_mounts;
    QProcess *m_lister = nullptr;
    MountCheck m_check;
    int m_timeout = 20000;
};
