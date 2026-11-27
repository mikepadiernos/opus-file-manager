// SPDX-License-Identifier: GPL-3.0-or-later

#include "remotemounts.h"

#include "appidentity.h"
#include "cloudsync.h"
#include "rclone.h"
#include "settings.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <cerrno>
#include <sys/stat.h>

namespace
{
constexpr int pollInterval = 200;
constexpr int stopTimeout = 5000;
constexpr int kioFuseTimeout = 120000;
constexpr qsizetype maxErrorBytes = 64 * 1024;

const QString kioFuseService = QStringLiteral("org.kde.KIOFuse");

QString unescapeFstabField(const QString &field)
{
    QString text;
    text.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        if (field.at(i) == QLatin1Char('\\') && i + 3 < field.size()) {
            bool ok = false;
            const int value = field.mid(i + 1, 3).toInt(&ok, 8);
            if (ok) {
                text.append(QChar(value));
                i += 3;
                continue;
            }
        }
        text.append(field.at(i));
    }
    return text;
}

QString escapeFstabField(const QString &field)
{
    QString text = field;
    text.replace(QLatin1Char('\\'), QStringLiteral("\\134"));
    text.replace(QLatin1Char(' '), QStringLiteral("\\040"));
    text.replace(QLatin1Char('\t'), QStringLiteral("\\011"));
    return text;
}

QString escapeFuseOption(const QString &value)
{
    QString text = value;
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char(','), QStringLiteral("\\,"));
    return text;
}

bool isIpv6(const QString &host)
{
    // Host names and IPv4 addresses never contain a colon.
    return host.contains(QLatin1Char(':'));
}

bool backendFromKey(const QString &key, RemoteMounts::Backend *backend)
{
    for (RemoteMounts::Backend candidate :
         {RemoteMounts::Backend::KioFuse, RemoteMounts::Backend::Sshfs, RemoteMounts::Backend::Davfs2, RemoteMounts::Backend::Cifs}) {
        if (RemoteMounts::key(candidate) == key) {
            *backend = candidate;
            return true;
        }
    }
    return false;
}

QString folderName(const QString &profileName)
{
    QString name = profileName.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    if (name.startsWith(QLatin1Char('.'))) {
        name.prepend(QLatin1Char('_'));
    }
    return name;
}

bool isFstabBackend(RemoteMounts::Backend backend)
{
    return backend == RemoteMounts::Backend::Davfs2 || backend == RemoteMounts::Backend::Cifs;
}

void stopProcess(QProcess *process)
{
    process->terminate();
    if (!process->waitForFinished(stopTimeout)) {
        process->kill();
        process->waitForFinished(1000);
    }
}

void defaultKioFuseMount(const QUrl &url, QObject *context, const RemoteMountManager::KioFuseReply &reply)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *interface = bus.isConnected() ? bus.interface() : nullptr;
    if (!interface) {
        reply({}, i18nc("@info", "The session bus is not available, so KIO FUSE can't be reached."));
        return;
    }
    if (!interface->isServiceRegistered(kioFuseService).value() && !interface->activatableServiceNames().value().contains(kioFuseService)) {
        reply({}, i18nc("@info", "KIO FUSE is not installed. Install kio-fuse, or choose another way to mount in the mount options."));
        return;
    }
    QDBusMessage message =
        QDBusMessage::createMethodCall(kioFuseService, QStringLiteral("/org/kde/KIOFuse"), QStringLiteral("org.kde.KIOFuse.VFS"), QStringLiteral("mountUrl"));
    message << url.toString();
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, kioFuseTimeout), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [reply](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QString> result = *call;
        if (result.isError()) {
            reply({}, result.error().message());
        } else {
            reply(result.value(), {});
        }
    });
}
}

QList<RemoteMounts::Backend> RemoteMounts::backends(RemoteProfiles::Protocol protocol)
{
    switch (protocol) {
    case RemoteProfiles::Protocol::Sftp:
        return {Backend::KioFuse, Backend::Sshfs};
    case RemoteProfiles::Protocol::WebDav:
    case RemoteProfiles::Protocol::WebDavs:
        return {Backend::KioFuse, Backend::Davfs2};
    case RemoteProfiles::Protocol::Smb:
        return {Backend::KioFuse, Backend::Cifs};
    case RemoteProfiles::Protocol::Ftp:
    case RemoteProfiles::Protocol::Ftps:
        return {Backend::KioFuse};
    }
    return {Backend::KioFuse};
}

QString RemoteMounts::name(Backend backend)
{
    switch (backend) {
    case Backend::KioFuse:
        return i18nc("@item:inlistbox mount backend", "KIO FUSE");
    case Backend::Sshfs:
        return i18nc("@item:inlistbox mount backend", "sshfs");
    case Backend::Davfs2:
        return i18nc("@item:inlistbox mount backend", "davfs2 (fstab entry)");
    case Backend::Cifs:
        return i18nc("@item:inlistbox mount backend", "Kernel CIFS client (fstab entry)");
    }
    return {};
}

QString RemoteMounts::key(Backend backend)
{
    switch (backend) {
    case Backend::KioFuse:
        return QStringLiteral("kio-fuse");
    case Backend::Sshfs:
        return QStringLiteral("sshfs");
    case Backend::Davfs2:
        return QStringLiteral("davfs2");
    case Backend::Cifs:
        return QStringLiteral("cifs");
    }
    return {};
}

QString RemoteMounts::findExecutable(const QString &name)
{
    const QString path = QStandardPaths::findExecutable(name);
    if (!path.isEmpty()) {
        return path;
    }
    // Mount helpers live in the system administration folders, which aren't always in PATH.
    return QStandardPaths::findExecutable(name, {QStringLiteral("/usr/sbin"), QStringLiteral("/sbin"), QStringLiteral("/usr/local/sbin")});
}

QString RemoteMounts::findFusermount()
{
    const QString fusermount = QStandardPaths::findExecutable(QStringLiteral("fusermount3"));
    return fusermount.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("fusermount")) : fusermount;
}

QString RemoteMounts::unavailableReason(Backend backend, const ExecutableLookup &find)
{
    switch (backend) {
    case Backend::KioFuse:
        // Reported when the service is asked, since D-Bus may start it on demand.
        return {};
    case Backend::Sshfs:
        if (find(QStringLiteral("sshfs")).isEmpty()) {
            return i18nc("@info", "sshfs is not installed. Install the sshfs package from your distribution.");
        }
        if (find(QStringLiteral("fusermount3")).isEmpty() && find(QStringLiteral("fusermount")).isEmpty()) {
            return i18nc("@info", "FUSE is not available. Install the fuse3 package from your distribution.");
        }
        return {};
    case Backend::Davfs2:
        if (find(QStringLiteral("mount.davfs")).isEmpty()) {
            return i18nc("@info", "davfs2 is not installed. Install the davfs2 package from your distribution.");
        }
        return {};
    case Backend::Cifs:
        if (find(QStringLiteral("mount.cifs")).isEmpty()) {
            return i18nc("@info", "The CIFS mount helper is not installed. Install the cifs-utils package from your distribution.");
        }
        return {};
    }
    return {};
}

QString RemoteMounts::optionsError(const RemoteProfiles::Profile &profile, const Options &options)
{
    if (!backends(profile.protocol).contains(options.backend)) {
        return i18nc("@info %1 is a mount backend, %2 a protocol",
                     "%1 can't mount %2 connections.",
                     name(options.backend),
                     RemoteProfiles::name(profile.protocol));
    }
    if (options.backend == Backend::KioFuse && options.readOnly) {
        return i18nc("@info", "KIO FUSE can't mount read-only. Choose another way to mount, or turn off read-only.");
    }
    if (options.backend == Backend::KioFuse && !options.location.isEmpty()) {
        return i18nc("@info", "KIO FUSE chooses its own folder. Leave the location empty, or choose another way to mount.");
    }
    if (!options.location.isEmpty() && (!QDir::isAbsolutePath(options.location) || options.location.contains(QLatin1Char('\n')))) {
        return i18nc("@info", "Enter the location as an absolute path, or leave it empty.");
    }
    return {};
}

QString RemoteMounts::mountRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath(QStringLiteral("opus-file-manager/mounts"));
}

QString RemoteMounts::mountPoint(const RemoteProfiles::Profile &profile, const Options &options, const QString &folder)
{
    if (!options.location.isEmpty()) {
        return QDir::cleanPath(options.location);
    }
    return QDir(folder.isEmpty() ? mountRoot() : QDir::cleanPath(folder)).filePath(folderName(profile.name));
}

bool RemoteMounts::covers(const RemoteProfiles::Profile &profile, const QUrl &location, const QString &mountPoint)
{
    const auto inside = [](const QString &path, const QString &folder) {
        return folder == QLatin1String("/") || path == folder || path.startsWith(folder + QLatin1Char('/'));
    };
    if (location.isLocalFile()) {
        return !mountPoint.isEmpty() && inside(QDir::cleanPath(location.toLocalFile()), QDir::cleanPath(mountPoint));
    }
    const QUrl remote = RemoteProfiles::url(profile);
    if (location.scheme().compare(remote.scheme(), Qt::CaseInsensitive) != 0 || location.host().compare(remote.host(), Qt::CaseInsensitive) != 0
        || location.port() != remote.port()) {
        return false;
    }
    if (!location.userName().isEmpty() && location.userName() != remote.userName()) {
        return false;
    }
    // An empty path opens the login folder of SFTP and FTP accounts; such a connection is taken to cover the whole server.
    return remote.path().isEmpty() || inside(location.path().isEmpty() ? QStringLiteral("/") : QDir::cleanPath(location.path()), remote.path());
}

QString RemoteMounts::sshfsTarget(const RemoteProfiles::Profile &profile)
{
    QString target;
    if (!profile.user.isEmpty()) {
        target = profile.user + QLatin1Char('@');
    }
    const QString host = RemoteProfiles::url(profile).host();
    target += isIpv6(host) ? QLatin1Char('[') + host + QLatin1Char(']') : host;
    target += QLatin1Char(':');
    if (!profile.path.isEmpty()) {
        target += QDir::cleanPath(profile.path);
    }
    return target;
}

QStringList RemoteMounts::sshfsArguments(const RemoteProfiles::Profile &profile, const QString &mountPoint, bool readOnly, bool batchMode, bool foreground)
{
    // In the foreground Plasma Opus can stop the mount it started by stopping sshfs.
    QStringList arguments;
    if (foreground) {
        arguments << QStringLiteral("-f");
    }
    arguments << QStringLiteral("-o") << QStringLiteral("reconnect,ServerAliveInterval=15,ServerAliveCountMax=3");
    if (readOnly) {
        arguments << QStringLiteral("-o") << QStringLiteral("ro");
    }
    if (profile.port > 0) {
        arguments << QStringLiteral("-p") << QString::number(profile.port);
    }
    if (profile.authentication == RemoteProfiles::Authentication::KeyOrAgent && !profile.identityFile.isEmpty()) {
        arguments << QStringLiteral("-o") << QStringLiteral("IdentityFile=") + escapeFuseOption(profile.identityFile);
    }
    if (batchMode) {
        arguments << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }
    arguments << QStringLiteral("--") << sshfsTarget(profile) << mountPoint;
    return arguments;
}

QString RemoteMounts::findAskPass()
{
    return QStandardPaths::findExecutable(QStringLiteral("ksshaskpass"));
}

QProcessEnvironment RemoteMounts::sshfsEnvironment(const QString &askPass)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!askPass.isEmpty()) {
        // KDE's helper asks for passwords and new host keys, and can keep passwords in KWallet.
        environment.insert(QStringLiteral("SSH_ASKPASS"), askPass);
        environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    }
    return environment;
}

QList<RemoteMounts::FstabEntry> RemoteMounts::parseFstab(const QByteArray &text)
{
    QList<FstabEntry> entries;
    const QList<QByteArray> lines = text.split('\n');
    for (const QByteArray &line : lines) {
        QString content = QString::fromLocal8Bit(line);
        const qsizetype comment = content.indexOf(QLatin1Char('#'));
        if (comment >= 0) {
            content.truncate(comment);
        }
        static const QRegularExpression whitespace(QStringLiteral("\\s+"));
        const QStringList fields = content.split(whitespace, Qt::SkipEmptyParts);
        if (fields.size() < 3) {
            continue;
        }
        FstabEntry entry;
        entry.source = unescapeFstabField(fields.at(0));
        entry.mountPoint = QDir::cleanPath(unescapeFstabField(fields.at(1)));
        entry.type = fields.at(2);
        if (fields.size() > 3) {
            entry.options = fields.at(3).split(QLatin1Char(','), Qt::SkipEmptyParts);
        }
        entries.append(entry);
    }
    return entries;
}

QString RemoteMounts::fstabType(Backend backend)
{
    switch (backend) {
    case Backend::Davfs2:
        return QStringLiteral("davfs");
    case Backend::Cifs:
        return QStringLiteral("cifs");
    case Backend::KioFuse:
    case Backend::Sshfs:
        return {};
    }
    return {};
}

QString RemoteMounts::fstabExample(const RemoteProfiles::Profile &profile, Backend backend, const QString &mountPoint, bool readOnly)
{
    QString source;
    QStringList options{QStringLiteral("user"), QStringLiteral("noauto")};
    if (backend == Backend::Davfs2) {
        QUrl url = RemoteProfiles::url(profile);
        url.setScheme(profile.protocol == RemoteProfiles::Protocol::WebDav ? QStringLiteral("http") : QStringLiteral("https"));
        url.setUserName({});
        source = url.toString(QUrl::FullyEncoded);
    } else if (backend == Backend::Cifs) {
        source =
            QStringLiteral("//") + RemoteProfiles::url(profile).host() + (profile.path.isEmpty() ? QStringLiteral("/share") : QDir::cleanPath(profile.path));
        if (!profile.user.isEmpty()) {
            options << QStringLiteral("username=") + profile.user;
        }
        if (profile.port > 0) {
            options << QStringLiteral("port=") + QString::number(profile.port);
        }
    } else {
        return {};
    }
    if (readOnly) {
        options << QStringLiteral("ro");
    }
    return QStringLiteral("%1 %2 %3 %4 0 0")
        .arg(escapeFstabField(source), escapeFstabField(QDir::cleanPath(mountPoint)), fstabType(backend), options.join(QLatin1Char(',')));
}

QString RemoteMounts::fstabError(const QList<FstabEntry> &entries, Backend backend, const QString &mountPoint, bool readOnly)
{
    const QString target = QDir::cleanPath(mountPoint);
    for (const FstabEntry &entry : entries) {
        if (entry.mountPoint != target) {
            continue;
        }
        if (entry.type != fstabType(backend)) {
            return i18nc("@info %1 is a folder, %2 and %3 file-system types",
                         "The fstab entry for %1 mounts %2, not %3.",
                         target,
                         entry.type,
                         fstabType(backend));
        }
        const bool userMountable = std::any_of(entry.options.cbegin(), entry.options.cend(), [](const QString &option) {
            return option == QLatin1String("user") || option == QLatin1String("users") || option == QLatin1String("owner") || option == QLatin1String("group");
        });
        if (!userMountable) {
            return i18nc("@info %1 is a folder", "The fstab entry for %1 needs administrator authorization to mount. Add the user option to it.", target);
        }
        if (readOnly && !entry.options.contains(QLatin1String("ro"))) {
            return i18nc("@info %1 is a folder", "The fstab entry for %1 mounts read-write. Add the ro option to it to mount read-only.", target);
        }
        return {};
    }
    return i18nc("@info %1 is a mount backend, %2 a folder",
                 "Mounting with %1 needs a user-mountable fstab entry for %2, which an administrator adds.",
                 name(backend),
                 target);
}

RemoteMounts::Options RemoteMounts::defaultOptions(const RemoteProfiles::Profile &profile, const Defaults &defaults)
{
    Options options;
    const QList<Backend> available = backends(profile.protocol);
    const Backend preferred = defaults.backends.value(profile.protocol, available.first());
    options.backend = available.contains(preferred) ? preferred : available.first();
    // KIO FUSE can't mount read-only, so it ignores the default.
    options.readOnly = defaults.readOnly && options.backend != Backend::KioFuse;
    return options;
}

RemoteMounts::Defaults RemoteMounts::defaultsFromSettings()
{
    Defaults defaults;
    if (Settings::remoteDriveSftpBackend() == Settings::EnumRemoteDriveSftpBackend::Sshfs) {
        defaults.backends.insert(RemoteProfiles::Protocol::Sftp, Backend::Sshfs);
    }
    if (Settings::remoteDriveWebDavBackend() == Settings::EnumRemoteDriveWebDavBackend::Davfs2) {
        defaults.backends.insert(RemoteProfiles::Protocol::WebDav, Backend::Davfs2);
        defaults.backends.insert(RemoteProfiles::Protocol::WebDavs, Backend::Davfs2);
    }
    if (Settings::remoteDriveSmbBackend() == Settings::EnumRemoteDriveSmbBackend::Cifs) {
        defaults.backends.insert(RemoteProfiles::Protocol::Smb, Backend::Cifs);
    }
    defaults.readOnly = Settings::remoteDriveReadOnly();
    const QUrl folder = Settings::remoteDriveMountFolder();
    defaults.folder = folder.isLocalFile() ? folder.toLocalFile() : QString();
    return defaults;
}

RemoteMounts::Options RemoteMounts::loadOptions(const KConfigGroup &group, const RemoteProfiles::Profile &profile, const Defaults &defaults)
{
    Options options = defaultOptions(profile, defaults);
    const QList<Backend> available = backends(profile.protocol);
    if (profile.id.isEmpty() || !group.hasGroup(profile.id)) {
        return options;
    }
    options.backend = available.first();
    const KConfigGroup entry = group.group(profile.id);
    Backend backend = options.backend;
    if (backendFromKey(entry.readEntry("Backend", QString()), &backend) && available.contains(backend)) {
        options.backend = backend;
    }
    options.readOnly = entry.readEntry("ReadOnly", false);
    options.location = entry.readEntry("Location", QString());
    options.atStartup = entry.readEntry("AtStartup", false);
    if (!optionsError(profile, options).isEmpty()) {
        options.readOnly = options.readOnly && options.backend != Backend::KioFuse;
        options.location = options.backend == Backend::KioFuse || !QDir::isAbsolutePath(options.location) ? QString() : options.location;
    }
    return options;
}

void RemoteMounts::saveOptions(KConfigGroup group, const QString &id, const Options &options)
{
    KConfigGroup entry = group.group(id);
    entry.writeEntry("Backend", key(options.backend));
    entry.writeEntry("ReadOnly", options.readOnly);
    entry.writeEntry("Location", options.location);
    entry.writeEntry("AtStartup", options.atStartup);
    group.sync();
}

void RemoteMounts::pruneOptions(KConfigGroup group, const QStringList &ids)
{
    const QStringList saved = group.groupList();
    for (const QString &id : saved) {
        if (!ids.contains(id)) {
            group.deleteGroup(id);
        }
    }
    group.sync();
}

bool RemoteMounts::isStale(const QString &path)
{
    struct stat info;
    return ::stat(QFile::encodeName(path).constData(), &info) != 0 && errno == ENOTCONN;
}

QString RemoteMounts::autostartPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("autostart/%1-remote-drives.desktop").arg(AppIdentity::desktopFileName()));
}

QByteArray RemoteMounts::autostartEntry(const QString &program, const QString &iconName)
{
    QString entry;
    entry += QStringLiteral("[Desktop Entry]\n");
    entry += QStringLiteral("Type=Application\n");
    entry += QStringLiteral("Name=") + i18nc("@title autostart entry", "%1 Remote Drives", AppIdentity::displayName()) + QLatin1Char('\n');
    entry += QStringLiteral("Comment=") + i18nc("@info autostart entry", "Mounts saved remote connections in the background") + QLatin1Char('\n');
    entry += QStringLiteral("Exec=") + CloudSync::execArgument(program) + QStringLiteral(" --background-mount\n");
    entry += QStringLiteral("Icon=") + iconName + QLatin1Char('\n');
    entry += QStringLiteral("NoDisplay=true\n");
    entry += QStringLiteral("X-KDE-autostart-phase=2\n");
    return entry.toUtf8();
}

bool RemoteMounts::mountsAtLogin()
{
    return QFile::exists(autostartPath());
}

bool RemoteMounts::setMountAtLogin(bool enabled, const QString &program, const QString &iconName, QString *error)
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
    const QByteArray entry = autostartEntry(program, iconName);
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == entry) {
        return true;
    }
    QSaveFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly) || file.write(entry) < 0 || !file.commit()) {
        if (error) {
            *error = i18nc("@info", "%1 could not be written.", path);
        }
        return false;
    }
    return true;
}

RemoteMountManager::RemoteMountManager(QObject *parent)
    : QObject(parent)
    , m_check(Rclone::isMountPoint)
    , m_kioFuse(defaultKioFuseMount)
{
    // The main window lives until the process exits, so its children aren't destroyed at quit.
    if (QCoreApplication *application = QCoreApplication::instance()) {
        connect(application, &QCoreApplication::aboutToQuit, this, &RemoteMountManager::stopAllNow);
    }
}

RemoteMountManager::~RemoteMountManager()
{
    stopAllNow();
}

void RemoteMountManager::setMountCheck(MountCheck check)
{
    m_check = std::move(check);
}

void RemoteMountManager::setKioFuseMount(KioFuseMount mount)
{
    m_kioFuse = std::move(mount);
}

void RemoteMountManager::setFstabPath(const QString &path)
{
    m_fstabPath = path;
}

void RemoteMountManager::setTimeout(int milliseconds)
{
    m_timeout = milliseconds;
}

void RemoteMountManager::setMountFolder(const QString &folder)
{
    m_folder = folder;
}

QString RemoteMountManager::mountFolder() const
{
    return m_folder;
}

void RemoteMountManager::setUnmountOnExit(bool unmount)
{
    m_unmountOnExit = unmount;
}

bool RemoteMountManager::unmountsOnExit() const
{
    return m_unmountOnExit;
}

void RemoteMountManager::mount(const RemoteProfiles::Profile &profile, const RemoteMounts::Options &options)
{
    const QString id = profile.id;
    if (id.isEmpty() || !RemoteProfiles::validationError(profile).isEmpty()) {
        Q_EMIT failed(id, i18nc("@info", "Save the connection before mounting it."));
        return;
    }
    if (m_mounts.contains(id)) {
        const Mount &mount = m_mounts.value(id);
        if (mount.ready && !mount.stopping) {
            Q_EMIT mounted(id, mount.path);
        }
        return;
    }
    const QString invalid = RemoteMounts::optionsError(profile, options);
    if (!invalid.isEmpty()) {
        Q_EMIT failed(id, invalid);
        return;
    }
    const QString unavailable = RemoteMounts::unavailableReason(options.backend);
    if (!unavailable.isEmpty()) {
        Q_EMIT failed(id, unavailable);
        return;
    }

    Mount mount;
    mount.backend = options.backend;
    if (options.backend == RemoteMounts::Backend::KioFuse) {
        m_mounts.insert(id, mount);
        m_kioFuse(RemoteProfiles::url(profile), this, [this, id](const QString &path, const QString &error) {
            auto it = m_mounts.find(id);
            if (it == m_mounts.end() || it->stopping) {
                return;
            }
            if (!error.isEmpty() || path.isEmpty()) {
                m_mounts.erase(it);
                Q_EMIT failed(id, i18nc("@info %1 is the error", "KIO FUSE could not mount the connection: %1", error));
                return;
            }
            it->path = path;
            it->ready = true;
            Q_EMIT mounted(id, path);
        });
        return;
    }

    const QString path = RemoteMounts::mountPoint(profile, options, m_folder);
    if (m_check(path)) {
        if (RemoteMounts::isStale(path)) {
            Q_EMIT failed(
                id,
                i18nc("@info %1 is a folder", "%1 is a stale mount whose connection has gone. Unmount it with fusermount3 -u, then try again.", path));
            return;
        }
        // Mounted by another window or an earlier session; it can be unmounted here but is left in place at quit.
        mount.path = path;
        mount.ready = true;
        mount.adopted = true;
        m_mounts.insert(id, mount);
        Q_EMIT mounted(id, path);
        return;
    }
    if (isFstabBackend(options.backend)) {
        QFile fstab(m_fstabPath);
        const QList<RemoteMounts::FstabEntry> entries =
            fstab.open(QIODevice::ReadOnly) ? RemoteMounts::parseFstab(fstab.readAll()) : QList<RemoteMounts::FstabEntry>();
        const QString error = RemoteMounts::fstabError(entries, options.backend, path, options.readOnly);
        if (!error.isEmpty()) {
            Q_EMIT failed(id,
                          i18nc("@info %1 is the problem, %2 an fstab line",
                                "%1\nAsk an administrator to add this line to /etc/fstab, and to add you to the group the mount helper needs:\n%2",
                                error,
                                RemoteMounts::fstabExample(profile, options.backend, path, options.readOnly)));
            return;
        }
    }
    const bool ownFolder = options.location.isEmpty();
    const QString folderError = prepareFolder(path, ownFolder);
    if (!folderError.isEmpty()) {
        Q_EMIT failed(id, folderError);
        return;
    }
    mount.path = path;
    mount.ownsFolder = ownFolder;
    if (options.backend == RemoteMounts::Backend::Sshfs) {
        // sshfs keeps mounts after quitting only when it runs on its own.
        mount.detached = !m_unmountOnExit;
        startSshfs(id, profile, options, mount);
    } else {
        startFstabMount(id, mount);
    }
}

QString RemoteMountManager::prepareFolder(const QString &path, bool ownFolder)
{
    if (ownFolder) {
        if (!QDir().mkpath(path)) {
            return i18nc("@info %1 is a folder", "Could not create a folder to mount on: %1", path);
        }
        if (m_folder.isEmpty()) {
            QFile::setPermissions(RemoteMounts::mountRoot(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        }
    } else if (!QFileInfo(path).isDir()) {
        return i18nc("@info %1 is a folder", "The folder to mount on doesn't exist: %1", path);
    }
    if (!QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        return i18nc("@info %1 is a folder", "Choose an empty folder to mount on; %1 isn't empty.", path);
    }
    return {};
}

void RemoteMountManager::startSshfs(const QString &id, const RemoteProfiles::Profile &profile, const RemoteMounts::Options &options, Mount mount)
{
    const QString askPass = RemoteMounts::findAskPass();
    mount.process = new QProcess(this);
    mount.process->setProcessEnvironment(RemoteMounts::sshfsEnvironment(askPass));
    mount.process->setStandardInputFile(QProcess::nullDevice());
    mount.process->setStandardOutputFile(QProcess::nullDevice());
    mount.poll = new QTimer(this);
    mount.poll->setInterval(pollInterval);
    connect(mount.poll, &QTimer::timeout, this, [this, id] {
        poll(id);
    });
    connect(mount.process, &QProcess::readyReadStandardError, this, [this, id] {
        auto it = m_mounts.find(id);
        if (it == m_mounts.end()) {
            return;
        }
        it->errors.append(it->process->readAllStandardError());
        if (it->errors.size() > maxErrorBytes) {
            it->errors = it->errors.right(maxErrorBytes);
        }
    });
    connect(mount.process, &QProcess::finished, this, [this, id](int exitCode, QProcess::ExitStatus status) {
        auto it = m_mounts.find(id);
        if (it == m_mounts.end()) {
            return;
        }
        it->errors.append(it->process->readAllStandardError());
        if (it->detached && !it->ready && !it->stopping && status == QProcess::NormalExit && exitCode == 0 && m_check(it->path)) {
            // sshfs left a daemon running the mount, which fusermount unmounts.
            it->poll->stop();
            std::exchange(it->process, nullptr)->deleteLater();
            it->ready = true;
            Q_EMIT mounted(id, it->path);
            return;
        }
        const QString detail = Rclone::lastError(it->errors);
        finish(id,
               it->ready || it->stopping ? QString()
                   : detail.isEmpty()    ? i18nc("@info %1 is an exit code", "sshfs stopped before the connection was mounted (exit code %1).", exitCode)
                                         : detail);
    });
    connect(mount.process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        auto it = m_mounts.find(id);
        if (error == QProcess::FailedToStart && it != m_mounts.end()) {
            finish(id, i18nc("@info %1 is the error", "Could not start sshfs: %1", it->process->errorString()));
        }
    });
    QProcess *process = mount.process;
    QTimer *timer = mount.poll;
    m_mounts.insert(id, mount);
    process->start(RemoteMounts::findExecutable(QStringLiteral("sshfs")),
                   RemoteMounts::sshfsArguments(profile, mount.path, options.readOnly, askPass.isEmpty(), !mount.detached));
    if (m_mounts.contains(id)) {
        timer->start();
    }
}

void RemoteMountManager::startFstabMount(const QString &id, Mount mount)
{
    mount.process = new QProcess(this);
    mount.process->setStandardInputFile(QProcess::nullDevice());
    connect(mount.process, &QProcess::finished, this, [this, id](int exitCode, QProcess::ExitStatus status) {
        auto it = m_mounts.find(id);
        if (it == m_mounts.end()) {
            return;
        }
        if (it->stopping) {
            finish(id, {});
            return;
        }
        if (status == QProcess::NormalExit && exitCode == 0 && m_check(it->path)) {
            std::exchange(it->process, nullptr)->deleteLater();
            it->ready = true;
            Q_EMIT mounted(id, it->path);
            return;
        }
        const QString detail = Rclone::lastError(it->process->readAllStandardError());
        finish(id, detail.isEmpty() ? i18nc("@info %1 is an exit code", "mount could not mount the connection (exit code %1).", exitCode) : detail);
    });
    connect(mount.process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        auto it = m_mounts.find(id);
        if (error == QProcess::FailedToStart && it != m_mounts.end() && it->process) {
            finish(id, i18nc("@info %1 is the error", "Could not start mount: %1", it->process->errorString()));
        }
    });
    QProcess *process = mount.process;
    const QString path = mount.path;
    m_mounts.insert(id, mount);
    process->start(RemoteMounts::findExecutable(QStringLiteral("mount")), {path});
}

void RemoteMountManager::poll(const QString &id)
{
    auto it = m_mounts.find(id);
    if (it == m_mounts.end() || it->ready) {
        return;
    }
    // A detached sshfs is mounted once it exits successfully.
    if (!it->detached && m_check(it->path)) {
        it->ready = true;
        it->poll->stop();
        Q_EMIT mounted(id, it->path);
        return;
    }
    it->elapsed += pollInterval;
    if (it->elapsed >= m_timeout) {
        it->poll->stop();
        it->stopping = true;
        const QString detail = Rclone::lastError(it->errors);
        const QString message = detail.isEmpty() ? i18nc("@info", "The connection was not mounted in time.")
                                                 : i18nc("@info %1 is sshfs's last message", "The connection was not mounted in time: %1", detail);
        QProcess *process = it->process;
        process->disconnect(this);
        stopProcess(process);
        finish(id, message);
    }
}

void RemoteMountManager::finish(const QString &id, const QString &error)
{
    const Mount mount = m_mounts.take(id);
    if (mount.poll) {
        mount.poll->stop();
        mount.poll->deleteLater();
    }
    if (mount.process) {
        mount.process->disconnect(this);
        mount.process->deleteLater();
    }
    releaseFolder(mount);
    if (!error.isEmpty()) {
        Q_EMIT failed(id, error);
    } else if (mount.ready) {
        Q_EMIT unmounted(id);
    }
}

void RemoteMountManager::releaseFolder(const Mount &mount)
{
    if (mount.backend == RemoteMounts::Backend::Sshfs && !mount.path.isEmpty() && m_check(mount.path)) {
        // sshfs exited without unmounting, leaving a stale mount that only Plasma Opus started.
        const QString fusermount = RemoteMounts::findFusermount();
        if (!fusermount.isEmpty()) {
            QProcess::execute(fusermount, {QStringLiteral("-u"), QStringLiteral("-z"), mount.path});
        }
    }
    if (mount.ownsFolder) {
        QDir().rmdir(mount.path);
    }
}

void RemoteMountManager::unmount(const QString &id)
{
    auto it = m_mounts.find(id);
    if (it == m_mounts.end() || it->stopping) {
        return;
    }
    switch (it->backend) {
    case RemoteMounts::Backend::KioFuse: {
        // KIO FUSE can't unmount a single location; Plasma Opus stops showing it.
        const bool ready = it->ready;
        m_mounts.erase(it);
        if (ready) {
            Q_EMIT unmounted(id);
        }
        return;
    }
    case RemoteMounts::Backend::Sshfs: {
        if (!it->process) {
            // A detached or adopted sshfs mount has no process here to stop.
            const QString fusermount = RemoteMounts::findFusermount();
            if (fusermount.isEmpty()) {
                Q_EMIT failed(id, i18nc("@info", "fusermount, which unmounts sshfs, is not installed. Install the fuse3 package."));
                return;
            }
            runUnmount(id, fusermount, {QStringLiteral("-u"), it->path});
            return;
        }
        it->stopping = true;
        if (it->poll) {
            it->poll->stop();
        }
        QProcess *process = it->process;
        // sshfs unmounts cleanly on SIGTERM; kill it if it doesn't exit.
        process->terminate();
        QTimer::singleShot(stopTimeout, process, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
        return;
    }
    case RemoteMounts::Backend::Davfs2:
    case RemoteMounts::Backend::Cifs:
        if (it->process) {
            it->stopping = true;
            it->process->kill();
            return;
        }
        unmountFstab(id);
        return;
    }
}

void RemoteMountManager::unmountFstab(const QString &id)
{
    runUnmount(id, RemoteMounts::findExecutable(QStringLiteral("umount")), {m_mounts.value(id).path});
}

void RemoteMountManager::runUnmount(const QString &id, const QString &program, const QStringList &arguments)
{
    auto it = m_mounts.find(id);
    it->stopping = true;
    auto *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    process->setProcessChannelMode(QProcess::MergedChannels);
    it->process = process;
    connect(process, &QProcess::finished, this, [this, id, process](int exitCode, QProcess::ExitStatus status) {
        auto it = m_mounts.find(id);
        process->deleteLater();
        if (it == m_mounts.end()) {
            return;
        }
        it->process = nullptr;
        if (status == QProcess::NormalExit && exitCode == 0) {
            finish(id, {});
            return;
        }
        it->stopping = false;
        const QString output = QString::fromLocal8Bit(process->readAll());
        const QString detail = Rclone::lastError(output.toUtf8());
        Q_EMIT failed(id,
                      output.contains(QLatin1String("busy"), Qt::CaseInsensitive)
                          ? i18nc("@info %1 is a folder", "%1 is busy. Close the files and folders open on it, then unmount it again.", it->path)
                          : i18nc("@info %1 is a folder, %2 the error", "Could not unmount %1: %2", it->path, detail));
    });
    connect(process, &QProcess::errorOccurred, this, [this, id, process, program](QProcess::ProcessError error) {
        auto it = m_mounts.find(id);
        if (error != QProcess::FailedToStart || it == m_mounts.end()) {
            return;
        }
        process->deleteLater();
        it->process = nullptr;
        it->stopping = false;
        Q_EMIT failed(id,
                      i18nc("@info %1 is a command, %2 the error",
                            "Could not start %1: %2",
                            program.isEmpty() ? QStringLiteral("umount") : QFileInfo(program).fileName(),
                            process->errorString()));
    });
    process->start(program, arguments);
}

void RemoteMountManager::unmountNow(const Mount &mount)
{
    QString program;
    QStringList arguments{mount.path};
    if (mount.backend == RemoteMounts::Backend::Sshfs) {
        program = RemoteMounts::findFusermount();
        arguments.prepend(QStringLiteral("-u"));
    } else if (isFstabBackend(mount.backend)) {
        program = RemoteMounts::findExecutable(QStringLiteral("umount"));
    }
    if (program.isEmpty()) {
        return;
    }
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForFinished(stopTimeout)) {
        process.kill();
        process.waitForFinished(1000);
    }
}

void RemoteMountManager::stopAllNow()
{
    const QStringList ids = m_mounts.keys();
    for (const QString &id : ids) {
        Mount &mount = m_mounts[id];
        if (mount.process) {
            mount.process->disconnect(this);
            stopProcess(mount.process);
        }
        // Stopping a foreground sshfs unmounts it; other mounts run without a process here.
        const bool foreground = mount.backend == RemoteMounts::Backend::Sshfs && !mount.detached && !mount.adopted;
        // Adopted mounts always stay, and the others stay when unmounting on exit is off.
        const bool left = !foreground && mount.ready && (mount.adopted || !m_unmountOnExit);
        if (!foreground && mount.ready && !left && mount.backend != RemoteMounts::Backend::KioFuse) {
            unmountNow(mount);
        }
        delete mount.poll;
        delete mount.process;
        mount.poll = nullptr;
        mount.process = nullptr;
        if (!left) {
            releaseFolder(mount);
        }
    }
    m_mounts.clear();
}

bool RemoteMountManager::isMounted(const QString &id) const
{
    const auto it = m_mounts.constFind(id);
    return it != m_mounts.cend() && it->ready && !it->stopping;
}

bool RemoteMountManager::isMountedAt(const RemoteProfiles::Profile &profile, const RemoteMounts::Options &options) const
{
    if (options.backend == RemoteMounts::Backend::KioFuse) {
        return false;
    }
    const QString path = RemoteMounts::mountPoint(profile, options, m_folder);
    return m_check(path) && !RemoteMounts::isStale(path);
}

bool RemoteMountManager::isStarting(const QString &id) const
{
    const auto it = m_mounts.constFind(id);
    return it != m_mounts.cend() && !it->ready && !it->stopping;
}

QString RemoteMountManager::path(const QString &id) const
{
    const auto it = m_mounts.constFind(id);
    return it != m_mounts.cend() && it->ready ? it->path : QString();
}

RemoteMounts::Backend RemoteMountManager::backend(const QString &id) const
{
    return m_mounts.value(id).backend;
}

QStringList RemoteMountManager::mountedIds() const
{
    QStringList ids;
    for (auto it = m_mounts.cbegin(); it != m_mounts.cend(); ++it) {
        if (it->ready && !it->stopping) {
            ids.append(it.key());
        }
    }
    return ids;
}
