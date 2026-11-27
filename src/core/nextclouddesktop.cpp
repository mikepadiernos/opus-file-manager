// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextclouddesktop.h"

#include "nextcloudaccount.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace
{
/// @p groups sorted by number, as the client numbers accounts and folders.
QStringList sortedGroups(QStringList groups)
{
    std::sort(groups.begin(), groups.end(), [](const QString &a, const QString &b) {
        bool aNumber = false;
        bool bNumber = false;
        const int aValue = a.toInt(&aNumber);
        const int bValue = b.toInt(&bNumber);
        if (aNumber && bNumber) {
            return aValue < bValue;
        }
        if (aNumber != bNumber) {
            return aNumber;
        }
        return a < b;
    });
    return groups;
}

QString cleanLocalPath(const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) {
        return {};
    }
    return QDir::cleanPath(path);
}

QUrl cleanServer(const QUrl &url)
{
    QUrl clean = url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments | QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
    if (clean.path() == QLatin1String("/")) {
        clean.setPath(QString());
    }
    return clean;
}
}

namespace NextcloudDesktop
{
bool usesVirtualFiles(const Folder &folder)
{
    return !folder.virtualFilesMode.isEmpty() && folder.virtualFilesMode != QLatin1String("off");
}

QString configurationPath(const QString &configHome)
{
    if (configHome.isEmpty()) {
        return {};
    }
    return QDir(configHome).filePath(QStringLiteral("Nextcloud/nextcloud.cfg"));
}

QString defaultConfigurationPath()
{
    return configurationPath(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
}

QString defaultSocketPath()
{
    // The client places its socket in the runtime folder Qt reports, named after the application.
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        return {};
    }
    return QDir(runtime).filePath(QStringLiteral("Nextcloud/socket"));
}

Configuration readConfiguration(const QString &path)
{
    Configuration configuration;
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile()) {
        return configuration;
    }
    configuration.exists = true;
    if (!info.isReadable()) {
        configuration.error = i18nc("@info %1 is a file path", "The Nextcloud desktop client's configuration %1 can't be read.", path);
        return configuration;
    }
    // QSettings only reads here: it writes and locks the file only after a change.
    QSettings settings(path, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        configuration.error = i18nc("@info %1 is a file path", "The Nextcloud desktop client's configuration %1 is damaged.", path);
        return configuration;
    }
    settings.beginGroup(QStringLiteral("Accounts"));
    const QStringList accounts = sortedGroups(settings.childGroups());
    for (const QString &account : accounts) {
        settings.beginGroup(account);
        const QUrl server = cleanServer(QUrl(settings.value(QStringLiteral("url")).toString()));
        QString user;
        for (const QString &key : {QStringLiteral("dav_user"), QStringLiteral("webflow_user"), QStringLiteral("http_user")}) {
            if (user.isEmpty()) {
                user = settings.value(key).toString();
            }
        }
        // Folders with virtual files are kept apart so that older clients leave them alone.
        for (const QString &group : {QStringLiteral("Folders"), QStringLiteral("FoldersWithPlaceholders"), QStringLiteral("Multifolders")}) {
            settings.beginGroup(group);
            const QStringList ids = sortedGroups(settings.childGroups());
            for (const QString &id : ids) {
                settings.beginGroup(id);
                Folder folder;
                folder.accountId = account;
                folder.id = id;
                folder.localPath = cleanLocalPath(settings.value(QStringLiteral("localPath")).toString());
                folder.targetPath = settings.value(QStringLiteral("targetPath")).toString();
                folder.virtualFilesMode = settings.value(QStringLiteral("virtualFilesMode")).toString();
                if (folder.virtualFilesMode.isEmpty() && settings.value(QStringLiteral("usePlaceholders"), false).toBool()) {
                    folder.virtualFilesMode = QStringLiteral("suffix");
                }
                folder.paused = settings.value(QStringLiteral("paused"), false).toBool();
                folder.server = server;
                folder.user = user;
                settings.endGroup();
                if (!folder.localPath.isEmpty()) {
                    configuration.folders.append(folder);
                }
            }
            settings.endGroup();
        }
        settings.endGroup();
    }
    settings.endGroup();
    return configuration;
}

std::optional<NextcloudAccounts::Account> accountFor(const Folder &folder, const QList<NextcloudAccounts::Account> &accounts)
{
    if (folder.server.isEmpty() || folder.user.isEmpty()) {
        return std::nullopt;
    }
    const QUrl server = cleanServer(folder.server);
    for (const NextcloudAccounts::Account &account : accounts) {
        if (account.user == folder.user && cleanServer(account.server) == server) {
            return account;
        }
    }
    return std::nullopt;
}

QUrl serverFolder(const NextcloudAccounts::Account &account, const Folder &folder)
{
    QUrl url = NextcloudAccounts::filesUrl(account);
    const QString target = QDir::cleanPath(QLatin1Char('/') + folder.targetPath);
    if (target != QLatin1String("/")) {
        url.setPath(url.path() + target.mid(1) + QLatin1Char('/'));
    }
    return url;
}

Sync parseSync(const QString &status)
{
    // "+SWM" marks items shared with others.
    const QString tag = status.section(QLatin1Char('+'), 0, 0);
    if (tag == QLatin1String("OK")) {
        return Sync::UpToDate;
    }
    if (tag == QLatin1String("SYNC") || tag == QLatin1String("NEW")) {
        return Sync::Syncing;
    }
    // The client reports both warnings and excluded items as IGNORE.
    if (tag == QLatin1String("IGNORE") || tag == QLatin1String("WARNING")) {
        return Sync::Warning;
    }
    if (tag == QLatin1String("ERROR")) {
        return Sync::Error;
    }
    if (tag == QLatin1String("NOP")) {
        return Sync::NotSynchronized;
    }
    return Sync::Unknown;
}

std::optional<StatusMessage> parseStatusMessage(const QByteArray &line)
{
    QString text = QString::fromUtf8(line);
    while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r'))) {
        text.chop(1);
    }
    const QLatin1String prefix("STATUS:");
    if (!text.startsWith(prefix)) {
        return std::nullopt;
    }
    const QString rest = text.mid(prefix.size());
    const qsizetype colon = rest.indexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == rest.size() - 1) {
        return std::nullopt;
    }
    return StatusMessage{rest.left(colon), rest.mid(colon + 1)};
}

QByteArray statusRequest(const QString &path)
{
    return QByteArray("RETRIEVE_FILE_STATUS:") + path.toUtf8() + '\n';
}

FolderState folderState(const Folder &folder, bool exists, bool clientRunning, const std::optional<QString> &status)
{
    FolderState state;
    const bool virtualFiles = usesVirtualFiles(folder);
    if (!exists) {
        state.description = i18nc("@info", "The folder isn't on this computer. The desktop client may not have created it yet, or it was moved or removed.");
        return state;
    }
    if (!clientRunning) {
        state.availability = Availability::Offline;
        state.description = virtualFiles
            ? i18nc("@info", "The desktop client isn't running, so changes aren't synchronized and virtual files can't be downloaded until it starts.")
            : i18nc("@info", "The desktop client isn't running, so changes aren't synchronized until it starts.");
        return state;
    }
    state.sync = status ? parseSync(*status) : Sync::Unknown;
    if (folder.paused) {
        state.availability = Availability::Offline;
        state.description = i18nc("@info", "Synchronization of this folder is paused in the desktop client.");
        return state;
    }
    if (state.sync == Sync::Unknown || state.sync == Sync::NotSynchronized) {
        state.description =
            i18nc("@info", "The desktop client didn't report this folder's status. It may be starting, signed out or unable to reach the server.");
        return state;
    }
    if (virtualFiles) {
        state.availability = Availability::Remote;
        state.description =
            i18nc("@info", "The folder holds virtual files: their contents stay on the server until opened, so opening them needs a connection.");
    } else {
        state.availability = Availability::Local;
        state.description = i18nc("@info", "Full copies of the files are on this computer, and the desktop client keeps them synchronized.");
    }
    return state;
}

QString availabilityText(Availability availability)
{
    switch (availability) {
    case Availability::Local:
        return i18nc("@item where a sync folder's files are", "Local");
    case Availability::Remote:
        return i18nc("@item where a sync folder's files are", "Remote");
    case Availability::Offline:
        return i18nc("@item where a sync folder's files are", "Offline");
    case Availability::Unknown:
        break;
    }
    return i18nc("@item where a sync folder's files are", "Unknown");
}

QString syncText(Sync sync)
{
    switch (sync) {
    case Sync::UpToDate:
        return i18nc("@item sync status", "Up to date");
    case Sync::Syncing:
        return i18nc("@item sync status", "Synchronizing");
    case Sync::Warning:
        return i18nc("@item sync status", "Needs attention");
    case Sync::Error:
        return i18nc("@item sync status", "Error");
    case Sync::NotSynchronized:
        return i18nc("@item sync status", "Not synchronized");
    case Sync::Unknown:
        break;
    }
    return i18nc("@item sync status", "Status unavailable");
}

StatusQuery::StatusQuery(const QString &socketPath, QObject *parent)
    : QObject(parent)
    , m_socketPath(socketPath)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] {
        finish(m_connected);
    });
}

StatusQuery::~StatusQuery()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

bool StatusQuery::isRunning() const
{
    return m_running;
}

void StatusQuery::start(const QStringList &paths, int timeoutMs)
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_pending.clear();
    m_statuses.clear();
    m_connected = false;
    m_running = true;
    const int generation = ++m_generation;
    for (const QString &path : paths) {
        if (!path.isEmpty() && !path.contains(QLatin1Char('\n'))) {
            m_pending.insert(path);
        }
    }
    m_timer->start(timeoutMs);
    if (m_socketPath.isEmpty() || !QFileInfo::exists(m_socketPath)) {
        QTimer::singleShot(0, this, [this, generation] {
            if (generation == m_generation) {
                finish(false);
            }
        });
        return;
    }
    auto *socket = new QLocalSocket(this);
    m_socket = socket;
    connect(socket, &QLocalSocket::connected, this, [this, socket] {
        m_connected = true;
        if (m_pending.isEmpty()) {
            finish(true);
            return;
        }
        for (const QString &path : std::as_const(m_pending)) {
            socket->write(statusRequest(path));
        }
    });
    connect(socket, &QLocalSocket::readyRead, this, &StatusQuery::readMessages);
    connect(socket, &QLocalSocket::errorOccurred, this, [this] {
        finish(m_connected);
    });
    connect(socket, &QLocalSocket::disconnected, this, [this] {
        finish(m_connected);
    });
    // Connecting later keeps finished() from being emitted during start().
    QMetaObject::invokeMethod(
        socket,
        [socket, path = m_socketPath] {
            socket->connectToServer(path);
        },
        Qt::QueuedConnection);
}

void StatusQuery::readMessages()
{
    if (!m_socket) {
        return;
    }
    while (m_socket && m_socket->canReadLine()) {
        const std::optional<StatusMessage> message = parseStatusMessage(m_socket->readLine());
        if (message && m_pending.remove(message->path)) {
            m_statuses.insert(message->path, message->status);
        }
    }
    if (m_pending.isEmpty()) {
        finish(true);
    }
}

void StatusQuery::finish(bool connected)
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_timer->stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    Q_EMIT finished(connected, m_statuses);
}
}
