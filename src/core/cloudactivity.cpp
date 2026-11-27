// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudactivity.h"

#include "cloudsyncrunner.h"
#include "rclone.h"
#include "transferqueue.h"

#include <KConfig>
#include <KLocalizedString>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTimer>

#include <algorithm>

namespace
{
const QString definitionsGroup = QStringLiteral("PlasmaOpusDefinitions");
const QString profilesGroup = QStringLiteral("RemoteProfiles");
const QString pairsGroup = QStringLiteral("CloudSyncPairs");
constexpr int definitionsVersion = 1;
constexpr int maximumEvents = 100;
constexpr int maximumRunErrors = 5;
/// rclone's cache metadata files are small; larger files aren't its metadata.
constexpr qint64 maximumMetadataSize = 1024 * 1024;
/// rclone reports 1 PiB when a provider has no quota information.
constexpr qint64 unknownQuota = qint64(1) << 50;

QString nameKey(const QString &name)
{
    return name.trimmed().toCaseFolded();
}
}

CloudActivity::Limits CloudActivity::loadLimits(const KConfigGroup &group)
{
    Limits limits;
    limits.bandwidthKiB = std::clamp(group.readEntry("BandwidthLimitKiB", 0), 0, 1048576);
    limits.parallelTransfers = std::clamp(group.readEntry("ParallelTransfers", 4), 1, 32);
    limits.cacheMiB = std::clamp(group.readEntry("CacheSizeMiB", 0), 0, 1048576);
    return limits;
}

void CloudActivity::saveLimits(KConfigGroup group, const Limits &limits)
{
    group.writeEntry("BandwidthLimitKiB", limits.bandwidthKiB);
    group.writeEntry("ParallelTransfers", limits.parallelTransfers);
    group.writeEntry("CacheSizeMiB", limits.cacheMiB);
    group.sync();
}

QString CloudActivity::rcloneCacheDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("rclone"));
}

QStringList CloudActivity::rcloneMountOptions(const Limits &limits, const QString &cacheDir)
{
    QStringList options{QStringLiteral("--cache-dir"), cacheDir, QStringLiteral("--vfs-cache-mode")};
    if (limits.cacheMiB > 0) {
        // Full caching keeps recently opened files, up to the size limit.
        options << QStringLiteral("full") << QStringLiteral("--vfs-cache-max-size") << QStringLiteral("%1M").arg(limits.cacheMiB);
    } else {
        options << QStringLiteral("writes");
    }
    return options << rcloneTransferOptions(limits);
}

QStringList CloudActivity::rcloneTransferOptions(const Limits &limits)
{
    QStringList options{QStringLiteral("--transfers"), QString::number(limits.parallelTransfers)};
    if (limits.bandwidthKiB > 0) {
        options << QStringLiteral("--bwlimit") << QStringLiteral("%1K").arg(limits.bandwidthKiB);
    }
    return options;
}

CloudActivity::PendingUploads CloudActivity::pendingUploads(const QString &cacheDir, const QString &remote)
{
    PendingUploads pending;
    if (!Rclone::isValidName(remote)) {
        return pending;
    }
    // rclone keeps a JSON metadata file for each cached file and marks files
    // not yet uploaded as dirty. They are uploaded the next time the remote
    // is mounted with the same cache.
    QDirIterator it(QDir(cacheDir).filePath(QStringLiteral("vfsMeta/") + remote), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isSymLink() || it.fileInfo().size() > maximumMetadataSize) {
            continue;
        }
        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        if (metadata.value(QStringLiteral("Dirty")).toBool()) {
            ++pending.files;
            pending.bytes += std::max<qint64>(0, metadata.value(QStringLiteral("Size")).toInteger());
        }
    }
    return pending;
}

QStringList CloudActivity::remotesWithPendingUploads(const QString &cacheDir)
{
    QStringList remotes;
    const QStringList names = QDir(QDir(cacheDir).filePath(QStringLiteral("vfsMeta"))).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : names) {
        if (!pendingUploads(cacheDir, name).isEmpty()) {
            remotes.append(name);
        }
    }
    return remotes;
}

qint64 CloudActivity::remainingSeconds(qint64 totalBytes, qint64 processedBytes, quint64 bytesPerSecond)
{
    if (totalBytes <= 0 || processedBytes < 0) {
        return -1;
    }
    if (processedBytes >= totalBytes) {
        return 0;
    }
    if (bytesPerSecond == 0) {
        return -1;
    }
    const auto left = quint64(totalBytes - processedBytes);
    return qint64((left + bytesPerSecond - 1) / bytesPerSecond);
}

std::optional<CloudActivity::Space> CloudActivity::space(qint64 available, qint64 total)
{
    if (total <= 0 || available < 0 || available > total || total >= unknownQuota) {
        return std::nullopt;
    }
    return Space{available, total};
}

std::optional<CloudActivity::Space> CloudActivity::mountSpace(const QString &path)
{
    const QStorageInfo info(path);
    if (!info.isValid() || !info.isReady() || QDir::cleanPath(info.rootPath()) != QDir::cleanPath(path)) {
        return std::nullopt;
    }
    return space(info.bytesAvailable(), info.bytesTotal());
}

CloudActivity::SpeedHistory::SpeedHistory(int capacity)
    : m_capacity(std::max(1, capacity))
{
}

void CloudActivity::SpeedHistory::add(quint64 bytesPerSecond)
{
    m_samples.append(bytesPerSecond);
    if (m_samples.size() > m_capacity) {
        m_samples.remove(0, m_samples.size() - m_capacity);
    }
}

QList<quint64> CloudActivity::SpeedHistory::samples() const
{
    return m_samples;
}

quint64 CloudActivity::SpeedHistory::peak() const
{
    return m_samples.isEmpty() ? 0 : *std::max_element(m_samples.cbegin(), m_samples.cend());
}

quint64 CloudActivity::SpeedHistory::latest() const
{
    return m_samples.isEmpty() ? 0 : m_samples.constLast();
}

int CloudActivity::SpeedHistory::capacity() const
{
    return m_capacity;
}

void CloudActivity::SpeedHistory::clear()
{
    m_samples.clear();
}

bool CloudActivity::exportDefinitions(const QString &path, const Definitions &definitions, QString *error)
{
    KConfig config(path, KConfig::SimpleConfig);
    const QStringList groups = config.groupList();
    for (const QString &group : groups) {
        config.deleteGroup(group);
    }
    config.group(definitionsGroup).writeEntry("Version", definitionsVersion);
    QList<RemoteProfiles::Profile> profiles = definitions.profiles;
    if (!RemoteProfiles::save(config.group(profilesGroup), profiles, error) || !CloudSync::save(config.group(pairsGroup), definitions.pairs, error)) {
        return false;
    }
    if (!config.sync()) {
        if (error) {
            *error = i18nc("@info %1 is a file", "Could not write %1.", path);
        }
        return false;
    }
    return true;
}

std::optional<CloudActivity::Definitions> CloudActivity::importDefinitions(const QString &path, QString *error)
{
    if (!QFileInfo(path).isFile()) {
        if (error) {
            *error = i18nc("@info %1 is a file", "Could not read %1.", path);
        }
        return std::nullopt;
    }
    const KConfig config(path, KConfig::SimpleConfig);
    const int version = config.group(definitionsGroup).readEntry("Version", 0);
    if (version < 1 || version > definitionsVersion) {
        if (error) {
            *error = i18nc("@info", "The file does not contain Plasma Opus connections and sync pairs, or was written by a newer version.");
        }
        return std::nullopt;
    }
    Definitions definitions;
    definitions.profiles = RemoteProfiles::load(config.group(profilesGroup));
    definitions.pairs = CloudSync::load(config.group(pairsGroup));
    if (definitions.profiles.isEmpty() && definitions.pairs.isEmpty()) {
        if (error) {
            *error = i18nc("@info", "The file contains no valid connections or sync pairs.");
        }
        return std::nullopt;
    }
    return definitions;
}

CloudActivity::MergeResult CloudActivity::merge(const Definitions &existing, const Definitions &imported)
{
    MergeResult result;
    result.profiles = existing.profiles;
    result.pairs = existing.pairs;
    QSet<QString> names;
    for (const RemoteProfiles::Profile &profile : std::as_const(existing.profiles)) {
        names.insert(nameKey(profile.name));
    }
    for (RemoteProfiles::Profile profile : imported.profiles) {
        if (names.contains(nameKey(profile.name))) {
            result.skipped.append(i18nc("@info %1 is a connection name", "A connection named “%1” already exists.", profile.name.trimmed()));
            continue;
        }
        const QString problem = RemoteProfiles::validationError(profile);
        if (!problem.isEmpty()) {
            result.skipped.append(i18nc("@info %1 is a connection name, %2 the problem", "%1: %2", profile.name.trimmed(), problem));
            continue;
        }
        // Saving assigns a new identifier.
        profile.id.clear();
        names.insert(nameKey(profile.name));
        result.profiles.append(profile);
        ++result.addedProfiles;
    }
    for (CloudSync::Pair pair : imported.pairs) {
        pair.id = CloudSync::newId();
        pair.paused = true;
        const QString problem = CloudSync::pairError(pair, result.pairs);
        if (!problem.isEmpty()) {
            result.skipped.append(i18nc("@info %1 is a sync pair, %2 the problem", "%1: %2", CloudSync::remoteDisplayName(pair), problem));
            continue;
        }
        result.pairs.append(pair);
        ++result.addedPairs;
    }
    return result;
}

CloudActivity::Monitor::Monitor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Monitor::sample);
}

void CloudActivity::Monitor::setTransferQueue(TransferQueue *queue)
{
    if (m_queue) {
        m_queue->disconnect(this);
    }
    m_queue = queue;
    if (m_queue) {
        connect(m_queue, &TransferQueue::changed, this, &Monitor::queueChanged);
        queueChanged();
    }
}

void CloudActivity::Monitor::setScheduler(CloudSync::Scheduler *scheduler)
{
    if (m_scheduler) {
        m_scheduler->disconnect(this);
    }
    m_scheduler = scheduler;
    if (m_scheduler) {
        connect(m_scheduler, &CloudSync::Scheduler::statusChanged, this, &Monitor::syncStatusChanged);
    }
}

void CloudActivity::Monitor::setSampleInterval(int milliseconds)
{
    m_timer->setInterval(milliseconds);
}

const CloudActivity::SpeedHistory &CloudActivity::Monitor::speedHistory() const
{
    return m_history;
}

quint64 CloudActivity::Monitor::currentSpeed() const
{
    quint64 speed = 0;
    if (m_queue) {
        const auto entries = m_queue->entries();
        for (const QueuedTransfer *entry : entries) {
            if (entry->state() == QueuedTransfer::State::Running) {
                speed += entry->bytesPerSecond();
            }
        }
    }
    return speed;
}

QList<CloudActivity::Event> CloudActivity::Monitor::events() const
{
    return m_events;
}

void CloudActivity::Monitor::addEvent(const QString &source, const QString &message)
{
    m_events.append({QDateTime::currentDateTime(), source, message});
    if (m_events.size() > maximumEvents) {
        m_events.remove(0, m_events.size() - maximumEvents);
    }
    Q_EMIT changed();
}

void CloudActivity::Monitor::clearEvents()
{
    m_events.clear();
    Q_EMIT changed();
}

void CloudActivity::Monitor::setPaused(bool paused)
{
    if (m_queue) {
        m_queue->setPaused(paused);
    }
    if (m_scheduler) {
        m_scheduler->setBackgroundPaused(paused);
    }
    Q_EMIT changed();
}

bool CloudActivity::Monitor::isPaused() const
{
    return (m_queue && m_queue->isPaused()) || (m_scheduler && m_scheduler->isBackgroundPaused());
}

void CloudActivity::Monitor::sample()
{
    const quint64 speed = currentSpeed();
    m_history.add(speed);
    const auto entries = m_queue ? m_queue->entries() : QList<QueuedTransfer *>();
    const bool running = std::any_of(entries.cbegin(), entries.cend(), [](const QueuedTransfer *entry) {
        return entry->state() == QueuedTransfer::State::Running;
    });
    if (!running) {
        m_timer->stop();
    }
    Q_EMIT changed();
}

void CloudActivity::Monitor::queueChanged()
{
    if (!m_queue) {
        return;
    }
    m_reported.removeAll(nullptr);
    const auto entries = m_queue->entries();
    bool running = false;
    for (QueuedTransfer *entry : entries) {
        if (entry->state() == QueuedTransfer::State::Running) {
            running = true;
        } else if (entry->state() == QueuedTransfer::State::Failed && !m_reported.contains(entry)) {
            m_reported.append(entry);
            addEvent(entry->description(), entry->failure());
        }
    }
    if (running && !m_timer->isActive()) {
        m_timer->start();
    }
    Q_EMIT changed();
}

void CloudActivity::Monitor::syncStatusChanged(const QString &id)
{
    if (!m_scheduler || m_scheduler->state(id) != CloudSync::Scheduler::State::Idle) {
        return;
    }
    const CloudSync::Report report = m_scheduler->lastReport(id);
    if (!report.finishedAt.isValid() || m_reportedRuns.value(id) == report.finishedAt) {
        return;
    }
    m_reportedRuns.insert(id, report.finishedAt);
    using Outcome = CloudSync::Report::Outcome;
    if (report.outcome != Outcome::Failed && report.outcome != Outcome::Partial && report.outcome != Outcome::Offline) {
        return;
    }
    const auto pair = m_scheduler->pair(id);
    const QString source = pair ? CloudSync::remoteDisplayName(*pair) : id;
    addEvent(source, CloudSync::summary(report));
    // The Cloud Sync window lists every error; a few explain the summary here.
    for (const CloudSync::ItemError &error : report.errors.mid(0, maximumRunErrors)) {
        addEvent(source, i18nc("@info %1 is a path, %2 the error", "%1: %2", error.path, error.message));
    }
}
