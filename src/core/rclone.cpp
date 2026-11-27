// SPDX-License-Identifier: GPL-3.0-or-later

#include "rclone.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace
{
constexpr int pollInterval = 200;
constexpr int stopTimeout = 5000;
constexpr qsizetype maxErrorBytes = 64 * 1024;

QString unescapeMountPath(const QByteArray &field)
{
    QByteArray path;
    path.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        if (field.at(i) == '\\' && i + 3 < field.size()) {
            bool ok = false;
            const int value = field.mid(i + 1, 3).toInt(&ok, 8);
            if (ok) {
                path.append(char(value));
                i += 3;
                continue;
            }
        }
        path.append(field.at(i));
    }
    return QFile::decodeName(path);
}
}

QString Rclone::findExecutable()
{
    return QStandardPaths::findExecutable(QStringLiteral("rclone"));
}

QStringList Rclone::listArguments()
{
    return {QStringLiteral("listremotes"), QStringLiteral("--long")};
}

QList<Rclone::Remote> Rclone::parseRemotes(const QByteArray &output)
{
    QList<Remote> remotes;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &line : lines) {
        const QString text = QString::fromUtf8(line).trimmed();
        const qsizetype colon = text.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        Remote remote{text.left(colon), text.mid(colon + 1).trimmed()};
        if (isValidName(remote.name)) {
            remotes.append(remote);
        }
    }
    return remotes;
}

bool Rclone::isValidName(const QString &name)
{
    // rclone allows letters, digits, "_", ".", "+", "@", spaces and "-", but not a leading "-" or space.
    static const QRegularExpression pattern(QStringLiteral("^[\\w.+@][\\w.+@ -]*$"), QRegularExpression::UseUnicodePropertiesOption);
    return pattern.match(name).hasMatch() && !name.endsWith(QLatin1Char(' ')) && name != QLatin1String(".") && name != QLatin1String("..");
}

QString Rclone::mountRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath(QStringLiteral("opus-file-manager/rclone"));
}

QString Rclone::mountPoint(const QString &remote)
{
    return QDir(mountRoot()).filePath(remote);
}

QStringList Rclone::mountArguments(const QString &remote, const QString &mountPoint, const QStringList &options)
{
    QStringList arguments{QStringLiteral("mount")};
    if (options.isEmpty()) {
        // Caching writes lets applications save files in place, as on a local disk.
        arguments << QStringLiteral("--vfs-cache-mode") << QStringLiteral("writes");
    } else {
        arguments << options;
    }
    arguments << QStringLiteral("--") << remote + QLatin1Char(':') << mountPoint;
    return arguments;
}

QProcessEnvironment Rclone::environment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("RCLONE_ASK_PASSWORD"), QStringLiteral("false"));
    return environment;
}

QString Rclone::lastError(const QByteArray &standardError)
{
    const QList<QByteArray> lines = standardError.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        QString line = QString::fromUtf8(*it).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        static const QRegularExpression timestamp(QStringLiteral("^\\d{4}/\\d{2}/\\d{2} \\d{2}:\\d{2}:\\d{2}\\s+"));
        line.remove(timestamp);
        static const QRegularExpression level(QStringLiteral("^(CRITICAL|ERROR|NOTICE|INFO|DEBUG)\\s*:\\s*"));
        line.remove(level);
        return line;
    }
    return {};
}

bool Rclone::isMountPoint(const QString &path)
{
    QFile file(QStringLiteral("/proc/self/mountinfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QString target = QDir::cleanPath(path);
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() > 4 && unescapeMountPath(fields.at(4)) == target) {
            return true;
        }
    }
    return false;
}

RcloneMounts::RcloneMounts(QObject *parent)
    : QObject(parent)
    , m_check(Rclone::isMountPoint)
{
    // The main window lives until the process exits, so its children aren't destroyed at quit.
    if (QCoreApplication *application = QCoreApplication::instance()) {
        connect(application, &QCoreApplication::aboutToQuit, this, &RcloneMounts::stopAllNow);
    }
}

RcloneMounts::~RcloneMounts()
{
    stopAllNow();
}

void RcloneMounts::stopAllNow()
{
    if (m_lister) {
        m_lister->disconnect(this);
        m_lister->kill();
        m_lister->waitForFinished(1000);
        delete std::exchange(m_lister, nullptr);
    }
    const QStringList remotes = m_mounts.keys();
    for (const QString &remote : remotes) {
        Mount &mount = m_mounts[remote];
        mount.process->disconnect(this);
        mount.process->terminate();
        if (!mount.process->waitForFinished(stopTimeout)) {
            mount.process->kill();
            mount.process->waitForFinished(1000);
        }
        delete mount.poll;
        delete mount.process;
        QDir().rmdir(mount.path);
    }
    m_mounts.clear();
}

void RcloneMounts::setMountCheck(MountCheck check)
{
    m_check = std::move(check);
}

void RcloneMounts::setTimeout(int milliseconds)
{
    m_timeout = milliseconds;
}

void RcloneMounts::setMountOptions(const QStringList &options)
{
    m_options = options;
}

void RcloneMounts::listRemotes()
{
    if (m_lister) {
        return;
    }
    const QString executable = Rclone::findExecutable();
    if (executable.isEmpty()) {
        Q_EMIT remotesListed({}, i18nc("@info", "rclone is not installed. Install it from your distribution to open its cloud remotes."));
        return;
    }
    m_lister = new QProcess(this);
    m_lister->setProcessEnvironment(Rclone::environment());
    m_lister->setStandardInputFile(QProcess::nullDevice());
    connect(m_lister, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        QProcess *process = std::exchange(m_lister, nullptr);
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString detail = Rclone::lastError(process->readAllStandardError());
            Q_EMIT remotesListed({},
                                 detail.isEmpty() ? i18nc("@info", "rclone could not list its remotes.")
                                                  : i18nc("@info %1 is rclone's error", "rclone could not list its remotes: %1", detail));
            return;
        }
        Q_EMIT remotesListed(Rclone::parseRemotes(process->readAllStandardOutput()), {});
    });
    connect(m_lister, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_lister) {
            return;
        }
        QProcess *process = std::exchange(m_lister, nullptr);
        process->deleteLater();
        Q_EMIT remotesListed({}, i18nc("@info %1 is the error", "Could not start rclone: %1", process->errorString()));
    });
    m_lister->start(executable, Rclone::listArguments());
}

void RcloneMounts::mount(const QString &remote)
{
    if (!Rclone::isValidName(remote)) {
        Q_EMIT failed(remote, i18nc("@info", "The rclone remote name is not valid."));
        return;
    }
    if (m_mounts.contains(remote)) {
        const Mount &mount = m_mounts.value(remote);
        if (mount.ready && !mount.stopping) {
            Q_EMIT mounted(remote, mount.path);
        }
        return;
    }
    const QString path = Rclone::mountPoint(remote);
    if (m_check(path)) {
        // Mounted by another window or an earlier session; open it without taking ownership.
        Q_EMIT mounted(remote, path);
        return;
    }
    const QString executable = Rclone::findExecutable();
    if (executable.isEmpty()) {
        Q_EMIT failed(remote, i18nc("@info", "rclone is not installed. Install it from your distribution to open its cloud remotes."));
        return;
    }
    QDir folder(path);
    if (!QDir().mkpath(path) || !folder.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        Q_EMIT failed(remote, i18nc("@info %1 is a folder", "Could not prepare an empty folder to mount on: %1", path));
        return;
    }
    QFile::setPermissions(Rclone::mountRoot(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    Mount mount;
    mount.path = path;
    mount.process = new QProcess(this);
    mount.process->setProcessEnvironment(Rclone::environment());
    mount.process->setStandardInputFile(QProcess::nullDevice());
    mount.process->setStandardOutputFile(QProcess::nullDevice());
    mount.poll = new QTimer(this);
    mount.poll->setInterval(pollInterval);
    connect(mount.poll, &QTimer::timeout, this, [this, remote] {
        poll(remote);
    });
    connect(mount.process, &QProcess::readyReadStandardError, this, [this, remote] {
        auto it = m_mounts.find(remote);
        if (it == m_mounts.end()) {
            return;
        }
        it->errors.append(it->process->readAllStandardError());
        if (it->errors.size() > maxErrorBytes) {
            it->errors = it->errors.right(maxErrorBytes);
        }
    });
    connect(mount.process, &QProcess::finished, this, [this, remote](int exitCode) {
        auto it = m_mounts.find(remote);
        if (it == m_mounts.end()) {
            return;
        }
        it->errors.append(it->process->readAllStandardError());
        const QString detail = Rclone::lastError(it->errors);
        finish(remote,
               it->ready || it->stopping ? QString()
                   : detail.isEmpty()    ? i18nc("@info %1 is an exit code", "rclone stopped before the remote was mounted (exit code %1).", exitCode)
                                         : detail);
    });
    connect(mount.process, &QProcess::errorOccurred, this, [this, remote](QProcess::ProcessError error) {
        auto it = m_mounts.find(remote);
        if (error == QProcess::FailedToStart && it != m_mounts.end()) {
            finish(remote, i18nc("@info %1 is the error", "Could not start rclone: %1", it->process->errorString()));
        }
    });
    m_mounts.insert(remote, mount);
    mount.process->start(executable, Rclone::mountArguments(remote, path, m_options));
    if (m_mounts.contains(remote)) {
        mount.poll->start();
    }
}

void RcloneMounts::poll(const QString &remote)
{
    auto it = m_mounts.find(remote);
    if (it == m_mounts.end() || it->ready) {
        return;
    }
    if (m_check(it->path)) {
        it->ready = true;
        it->poll->stop();
        Q_EMIT mounted(remote, it->path);
        return;
    }
    it->elapsed += pollInterval;
    if (it->elapsed >= m_timeout) {
        it->poll->stop();
        it->stopping = true;
        const QString detail = Rclone::lastError(it->errors);
        const QString message = detail.isEmpty() ? i18nc("@info", "The remote was not mounted in time.")
                                                 : i18nc("@info %1 is rclone's last message", "The remote was not mounted in time: %1", detail);
        QProcess *process = it->process;
        process->disconnect(this);
        process->terminate();
        if (!process->waitForFinished(stopTimeout)) {
            process->kill();
            process->waitForFinished(1000);
        }
        finish(remote, message);
    }
}

void RcloneMounts::finish(const QString &remote, const QString &error)
{
    const Mount mount = m_mounts.take(remote);
    if (!mount.process) {
        return;
    }
    mount.poll->stop();
    mount.poll->deleteLater();
    mount.process->disconnect(this);
    mount.process->deleteLater();
    QDir().rmdir(mount.path);
    if (!error.isEmpty()) {
        Q_EMIT failed(remote, error);
    } else if (mount.ready) {
        Q_EMIT unmounted(remote);
    }
}

void RcloneMounts::unmount(const QString &remote)
{
    stop(remote);
}

void RcloneMounts::unmountAll()
{
    const QStringList remotes = m_mounts.keys();
    for (const QString &remote : remotes) {
        stop(remote);
    }
}

void RcloneMounts::stop(const QString &remote)
{
    auto it = m_mounts.find(remote);
    if (it == m_mounts.end() || it->stopping) {
        return;
    }
    it->stopping = true;
    it->poll->stop();
    QProcess *process = it->process;
    // rclone unmounts cleanly on SIGTERM; kill it if it doesn't exit.
    process->terminate();
    QTimer::singleShot(stopTimeout, process, [process] {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
}

bool RcloneMounts::isMounted(const QString &remote) const
{
    const auto it = m_mounts.constFind(remote);
    return it != m_mounts.cend() && it->ready && !it->stopping;
}

bool RcloneMounts::isStarting(const QString &remote) const
{
    const auto it = m_mounts.constFind(remote);
    return it != m_mounts.cend() && !it->ready && !it->stopping;
}

QStringList RcloneMounts::mountedRemotes() const
{
    QStringList remotes;
    for (auto it = m_mounts.cbegin(); it != m_mounts.cend(); ++it) {
        if (it->ready && !it->stopping) {
            remotes.append(it.key());
        }
    }
    remotes.sort();
    return remotes;
}
