// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "remoteprofiles.h"

#include <KConfigGroup>

#include <QByteArray>
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
 * Mounts saved remote connections as local folders, so applications without
 * KIO support can open and save files in place. Credentials are never passed
 * on the command line or written to configuration: KIO FUSE asks KDE's
 * password server, sshfs uses the SSH agent, keys or KDE's askpass helper, and
 * davfs2 and CIFS mounts come from the user's own fstab entries.
 */
namespace RemoteMounts
{
enum class Backend {
    /// KDE's KIO FUSE service, which mounts any KIO location.
    KioFuse,
    /// sshfs, for SFTP.
    Sshfs,
    /// davfs2 through a user-mountable fstab entry, for WebDAV.
    Davfs2,
    /// The kernel CIFS client through a user-mountable fstab entry, for SMB.
    Cifs,
};

struct Options {
    Backend backend = Backend::KioFuse;
    bool readOnly = false;
    /// An empty folder to mount on; empty uses a folder under the runtime directory.
    QString location;
    /// Mount when Plasma Opus starts.
    bool atStartup = false;
};

struct FstabEntry {
    QString source;
    QString mountPoint;
    QString type;
    QStringList options;
};

/// The backends that can mount @p protocol, preferred first.
QList<Backend> backends(RemoteProfiles::Protocol protocol);
/// The translated backend name shown in menus.
QString name(Backend backend);
/// The configuration key of @p backend.
QString key(Backend backend);
/// The command that unmounts FUSE file systems, or an empty string.
QString findFusermount();

/// Finds an executable by name, returning its path or an empty string.
using ExecutableLookup = std::function<QString(const QString &name)>;
/// Looks in PATH and the system administration folders.
QString findExecutable(const QString &name);
/// Why @p backend can't be used, naming the package to install, or an empty string.
QString unavailableReason(Backend backend, const ExecutableLookup &find = findExecutable);
/// Why @p options can't mount @p profile, or an empty string.
QString optionsError(const RemoteProfiles::Profile &profile, const Options &options);

/// The folder holding mounts in the private runtime directory.
QString mountRoot();
/// The folder @p profile is mounted on with @p options.
QString mountPoint(const RemoteProfiles::Profile &profile, const Options &options);

/// The sshfs location, such as user@host:/folder.
QString sshfsTarget(const RemoteProfiles::Profile &profile);
/**
 * The arguments that mount @p profile with sshfs in the foreground.
 * @param batchMode fail instead of prompting, for when no askpass helper is available
 */
QStringList sshfsArguments(const RemoteProfiles::Profile &profile, const QString &mountPoint, bool readOnly, bool batchMode);
/// KDE's askpass helper, or an empty string.
QString findAskPass();
/// The environment for sshfs, prompting through @p askPass when it isn't empty.
QProcessEnvironment sshfsEnvironment(const QString &askPass);

/// Parses fstab, skipping comments and malformed lines.
QList<FstabEntry> parseFstab(const QByteArray &text);
/// The fstab file-system type for @p backend, or an empty string.
QString fstabType(Backend backend);
/// The fstab line that would let the user mount @p profile on @p mountPoint without administrator rights.
QString fstabExample(const RemoteProfiles::Profile &profile, Backend backend, const QString &mountPoint, bool readOnly);
/// Why the fstab entries don't let the user mount @p mountPoint with @p backend, or an empty string.
QString fstabError(const QList<FstabEntry> &entries, Backend backend, const QString &mountPoint, bool readOnly);

/// The saved options for @p profile, or the defaults for its protocol.
Options loadOptions(const KConfigGroup &group, const RemoteProfiles::Profile &profile);
/// Saves @p options for the profile with @p id.
void saveOptions(KConfigGroup group, const QString &id, const Options &options);
/// Forgets the options of profiles not in @p ids.
void pruneOptions(KConfigGroup group, const QStringList &ids);

/// Whether @p path is a FUSE mount whose process has gone.
bool isStale(const QString &path);
}

/**
 * Mounts and unmounts saved remote connections. Only mounts started here are
 * unmounted, and they are unmounted when the application quits.
 */
class RemoteMountManager : public QObject
{
    Q_OBJECT

public:
    using MountCheck = std::function<bool(const QString &path)>;
    using KioFuseReply = std::function<void(const QString &path, const QString &error)>;
    using KioFuseMount = std::function<void(const QUrl &url, QObject *context, const KioFuseReply &reply)>;

    explicit RemoteMountManager(QObject *parent = nullptr);
    ~RemoteMountManager() override;

    /// Replaces how mounts are detected, for tests.
    void setMountCheck(MountCheck check);
    /// Replaces the KIO FUSE request, for tests.
    void setKioFuseMount(KioFuseMount mount);
    /// Reads fstab from @p path instead of /etc/fstab, for tests.
    void setFstabPath(const QString &path);
    /// Sets how long to wait for sshfs to mount, in milliseconds.
    void setTimeout(int milliseconds);

    /// Mounts @p profile, emitting mounted() or failed().
    void mount(const RemoteProfiles::Profile &profile, const RemoteMounts::Options &options);
    /// Unmounts what Plasma Opus mounted for the profile with @p id.
    void unmount(const QString &id);
    /// Unmounts everything Plasma Opus mounted and waits, without emitting signals.
    void stopAllNow();

    bool isMounted(const QString &id) const;
    bool isStarting(const QString &id) const;
    /// The mounted folder, or an empty string.
    QString path(const QString &id) const;
    /// The backend of a mount or mount in progress.
    RemoteMounts::Backend backend(const QString &id) const;
    QStringList mountedIds() const;

Q_SIGNALS:
    void mounted(const QString &id, const QString &path);
    void failed(const QString &id, const QString &error);
    void unmounted(const QString &id);

private:
    struct Mount {
        RemoteMounts::Backend backend = RemoteMounts::Backend::KioFuse;
        QProcess *process = nullptr;
        QTimer *poll = nullptr;
        QString path;
        QByteArray errors;
        bool ready = false;
        bool stopping = false;
        bool ownsFolder = false;
        int elapsed = 0;
    };

    QString prepareFolder(const QString &path, bool ownFolder);
    void startSshfs(const QString &id, const RemoteProfiles::Profile &profile, const RemoteMounts::Options &options, Mount mount);
    void startFstabMount(const QString &id, Mount mount);
    void poll(const QString &id);
    void finish(const QString &id, const QString &error);
    void releaseFolder(const Mount &mount);
    void unmountFstab(const QString &id);

    QHash<QString, Mount> m_mounts;
    MountCheck m_check;
    KioFuseMount m_kioFuse;
    QString m_fstabPath = QStringLiteral("/etc/fstab");
    int m_timeout = 60000;
};
