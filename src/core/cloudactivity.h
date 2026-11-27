// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cloudsync.h"
#include "remoteprofiles.h"

#include <KConfigGroup>

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

class QTimer;
class QueuedTransfer;
class TransferQueue;

namespace CloudSync
{
class Scheduler;
}

/**
 * Shared state behind the Cloud Activity window: transfer limits for rclone,
 * uploads still waiting in rclone's cache, recent transfer speeds and errors,
 * and the export and import of connection and sync-pair definitions.
 */
namespace CloudActivity
{
/// Limits for rclone mounts and rclone sync pairs.
struct Limits {
    /// The bandwidth limit in KiB/s, or 0 for no limit.
    int bandwidthKiB = 0;
    /// The number of files rclone transfers at once.
    int parallelTransfers = 4;
    /// The size limit of the cache that keeps opened files of mounted remotes,
    /// in MiB, or 0 to cache only files being written.
    int cacheMiB = 0;

    bool operator==(const Limits &other) const = default;
};

Limits loadLimits(const KConfigGroup &group);
void saveLimits(KConfigGroup group, const Limits &limits);

/// The folder holding Plasma Opus's rclone cache, so its pending uploads can be found.
QString rcloneCacheDir();
/// The options that apply @p limits to `rclone mount`, using @p cacheDir.
QStringList rcloneMountOptions(const Limits &limits, const QString &cacheDir);
/// The options that apply @p limits to rclone sync runs.
QStringList rcloneTransferOptions(const Limits &limits);

/// Files written to a mounted remote that rclone has not uploaded yet.
struct PendingUploads {
    int files = 0;
    qint64 bytes = 0;

    bool isEmpty() const
    {
        return files == 0;
    }
};
/// Reads rclone's cache in @p cacheDir for files of @p remote not yet uploaded.
PendingUploads pendingUploads(const QString &cacheDir, const QString &remote);
/// The remotes with files in @p cacheDir that rclone has not uploaded yet, in name order.
QStringList remotesWithPendingUploads(const QString &cacheDir);

/// What must finish before unmounting or quitting.
struct UploadStatus {
    PendingUploads uploads;
    /// Cloud sync runs running or queued.
    int syncRuns = 0;

    bool isIdle() const
    {
        return uploads.isEmpty() && syncRuns == 0;
    }
};

/// The seconds left at @p bytesPerSecond, or -1 when unknown.
qint64 remainingSeconds(qint64 totalBytes, qint64 processedBytes, quint64 bytesPerSecond);
/// The free and total space a mounted remote reports, or nullopt when the
/// provider doesn't report its quota.
struct Space {
    qint64 available = 0;
    qint64 total = 0;
};
std::optional<Space> space(qint64 available, qint64 total);
std::optional<Space> mountSpace(const QString &path);

/// Recent transfer speeds, oldest first.
class SpeedHistory
{
public:
    explicit SpeedHistory(int capacity = 60);
    void add(quint64 bytesPerSecond);
    QList<quint64> samples() const;
    quint64 peak() const;
    quint64 latest() const;
    int capacity() const;
    void clear();

private:
    QList<quint64> m_samples;
    int m_capacity;
};

struct Event {
    QDateTime time;
    /// The remote, connection, sync pair or transfer the error belongs to.
    QString source;
    QString message;
};

/// The connection and sync-pair definitions that can be exported.
struct Definitions {
    QList<RemoteProfiles::Profile> profiles;
    QList<CloudSync::Pair> pairs;
};
/// Writes @p definitions to @p path. Passwords are never part of them.
bool exportDefinitions(const QString &path, const Definitions &definitions, QString *error = nullptr);
/// Reads definitions written by exportDefinitions().
std::optional<Definitions> importDefinitions(const QString &path, QString *error = nullptr);

struct MergeResult {
    QList<RemoteProfiles::Profile> profiles;
    QList<CloudSync::Pair> pairs;
    int addedProfiles = 0;
    int addedPairs = 0;
    /// Localized reasons for skipped definitions.
    QStringList skipped;
};
/**
 * Adds @p imported to the existing definitions. Connections whose name is
 * taken and pairs that match or overlap an existing pair are skipped. Added
 * pairs are paused, so they don't run before the user signs in again.
 */
MergeResult merge(const Definitions &existing, const Definitions &imported);

/**
 * Samples the speed of the transfer queue while transfers run and keeps the
 * latest errors from transfers, sync runs and mounts.
 */
class Monitor : public QObject
{
    Q_OBJECT

public:
    explicit Monitor(QObject *parent = nullptr);

    void setTransferQueue(TransferQueue *queue);
    void setScheduler(CloudSync::Scheduler *scheduler);
    /// Sets the sampling interval, for tests.
    void setSampleInterval(int milliseconds);

    const SpeedHistory &speedHistory() const;
    /// The combined speed of running transfers.
    quint64 currentSpeed() const;
    QList<Event> events() const;
    void addEvent(const QString &source, const QString &message);
    void clearEvents();

    /// Pauses the transfer queue and background sync runs.
    void setPaused(bool paused);
    bool isPaused() const;

Q_SIGNALS:
    void changed();

private:
    void sample();
    void queueChanged();
    void syncStatusChanged(const QString &id);

    QPointer<TransferQueue> m_queue;
    QPointer<CloudSync::Scheduler> m_scheduler;
    QTimer *m_timer;
    SpeedHistory m_history;
    QList<Event> m_events;
    QList<QPointer<QueuedTransfer>> m_reported;
    QHash<QString, QDateTime> m_reportedRuns;
};
}
