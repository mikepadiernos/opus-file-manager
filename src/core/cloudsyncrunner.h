// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cloudsync.h"

#include <KSharedConfig>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcess>

#include <functional>
#include <memory>
#include <optional>

class KJob;
class QTimer;

namespace CloudSync
{
struct ItemError {
    QString path;
    QString message;
    ErrorKind kind = ErrorKind::Other;
};

/// The result of a run.
struct Report {
    enum class Outcome {
        None,
        /// Every planned change was made. Conflicts may remain.
        Succeeded,
        /// Some changes failed; they are retried next time.
        Partial,
        /// The run stopped before changing anything, so the user can review
        /// the preview: the first run, large deletions or a preview request.
        NeedsReview,
        /// The remote could not be reached; nothing was changed.
        Offline,
        Failed,
        Canceled,
    };

    Outcome outcome = Outcome::None;
    /// A localized summary of why the run failed or needs review.
    QString message;
    int uploaded = 0;
    int downloaded = 0;
    int deleted = 0;
    int renamed = 0;
    int folders = 0;
    QList<Conflict> conflicts;
    QStringList skipped;
    QList<ItemError> errors;
    /// The changes the run would make, when it needs review.
    QList<PreviewItem> preview;
    /// Free space on the remote in bytes, or -1 when the remote does not report it.
    qint64 freeBytes = -1;
    QDateTime finishedAt;

    bool hasErrors(ErrorKind kind) const;
};
/// A one-line, localized summary of @p report.
QString summary(const Report &report);

/// Runs one synchronization of a pair: lists both sides, plans the run and,
/// unless the plan needs review, applies it and saves the new snapshot.
///
/// Before replacing or deleting an item, the runner checks that it still
/// matches the listing, so items changed during a run are left for the next
/// one. Local deletions go to the trash.
class Runner : public QObject
{
    Q_OBJECT

public:
    enum class Run {
        /// Applies the plan unless it needs review.
        Automatic,
        /// Only reports the plan.
        Preview,
        /// Applies the plan if it still matches the confirmed preview.
        Confirmed,
    };

    explicit Runner(const Pair &pair, QObject *parent = nullptr);
    ~Runner() override;

    void setSnapshotPath(const QString &path);
    void setResolutions(const QHash<QString, Resolution> &resolutions);
    /// The preview the user confirmed, for Run::Confirmed. Resolutions are
    /// not part of it, so a confirmed run compares the plan without them.
    void setConfirmedPreview(const QList<PreviewItem> &preview);
    /// The conflicts the user reviewed, for Run::Confirmed. A confirmed run
    /// stops for another review when they have changed since.
    void setConfirmedConflicts(const QList<Conflict> &conflicts);
    /// Deletes local items instead of moving them to the trash, for tests.
    void setUseTrash(bool useTrash);
    void setRcloneProgram(const QString &program);
    /// Options added to every rclone command, such as bandwidth limits.
    void setRcloneOptions(const QStringList &options);
    void setConflictName(const QString &name);

    void start(Run run);
    void cancel();
    bool isRunning() const;
    const Pair &pair() const;
    const Report &report() const;

Q_SIGNALS:
    void progressChanged(const QString &message);
    void finished();

private:
    void listRemote();
    void planRun();
    void checkSpace();
    void createRoots();
    void applyNext();
    void verifyRemote(const Change &change, const QString &path, const std::optional<State> &expected, std::function<void()> next);
    void recordRemote(const Change &change);
    void runJob(KJob *job, const Change &change, std::function<void()> onSuccess);
    void succeed(const Change &change);
    void fail(const Change &change, const QString &message, ErrorKind kind);
    void finish(Report::Outcome outcome, const QString &message = {});
    void complete();
    void startRclone();
    void runRclone(bool dryRun);
    void rcloneFinished(int exitCode, QProcess::ExitStatus status);
    QUrl remoteUrl(const QString &path) const;
    QString localPath(const QString &path) const;

    Pair m_pair;
    Run m_run = Run::Automatic;
    QString m_snapshotPath;
    QHash<QString, Resolution> m_resolutions;
    QList<PreviewItem> m_confirmed;
    QList<Conflict> m_confirmedConflicts;
    bool m_useTrash = true;
    QString m_rcloneProgram;
    QStringList m_rcloneOptions;
    QString m_conflictName;
    Report m_report;
    Snapshot m_base;
    Listing m_local;
    Listing m_remote;
    Listing m_localAfter;
    Listing m_remoteAfter;
    Plan m_plan;
    QList<Change> m_done;
    int m_next = 0;
    bool m_remoteMissing = false;
    bool m_running = false;
    bool m_canceled = false;
    QPointer<KJob> m_job;
    QProcess *m_process = nullptr;
    QByteArray m_output;
    bool m_rcloneDryRun = false;
    bool m_rcloneInitialize = false;
    QStringList m_rcloneFilters;
};

/// Whether the network connection is metered, or nothing when no network
/// information backend reports it.
std::optional<bool> isConnectionMetered();

/// Keeps the pairs, runs them on request and on their intervals, one at a
/// time, and remembers each pair's last report. Background runs skip paused
/// pairs, paused accounts and, optionally, metered connections.
class Scheduler : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Queued,
        Running,
    };

    explicit Scheduler(KSharedConfig::Ptr config, QObject *parent = nullptr);
    ~Scheduler() override;

    QList<Pair> pairs() const;
    std::optional<Pair> pair(const QString &id) const;
    /// Saves @p pairs, forgetting the snapshots of removed pairs.
    bool setPairs(const QList<Pair> &pairs, QString *error = nullptr);

    QStringList pausedAccounts() const;
    void setAccountPaused(const QString &account, bool paused);
    /// Whether background runs skip @p pair: it or its account is paused.
    bool isPaused(const Pair &pair) const;
    bool pausesOnMeteredConnections() const;
    void setPausesOnMeteredConnections(bool pause);
    /// How often pairs that use the default interval are checked, in minutes.
    int defaultIntervalMinutes() const;
    /// How often @p pair is synchronized in the background, in minutes, or 0
    /// when it is synchronized only on request.
    int intervalMinutes(const Pair &pair) const;
    /// Whether background runs of every pair are paused.
    bool isBackgroundPaused() const;
    void setBackgroundPaused(bool paused);
    /// Options added to the rclone commands of later runs.
    void setRcloneOptions(const QStringList &options);
    QStringList rcloneOptions() const;
    /// Replaces how metered connections are detected, for tests.
    void setMeteredCheck(std::function<bool()> check);
    /// Adjusts each runner before it starts, for tests.
    void setRunnerSetup(std::function<void(Runner *)> setup);

    State state(const QString &id) const;
    /// The report of the pair's last run.
    Report lastReport(const QString &id) const;
    /// The running pair's latest progress message.
    QString progress(const QString &id) const;
    /// Whether a pair waits for the user to review its preview.
    bool needsReview(const QString &id) const;
    /// The number of runs running or queued.
    int activeRuns() const;

    /// Queues an automatic run, even if the pair is paused.
    void syncNow(const QString &id);
    void preview(const QString &id);
    /// Queues a run that applies the @p reviewed preview with @p resolutions.
    void confirm(const QString &id, const QHash<QString, Resolution> &resolutions, const Report &reviewed);
    void cancel(const QString &id);

    /// Starts running pairs on their intervals.
    void start();
    /// Stops background runs, drops queued runs and cancels the running one,
    /// which keeps the changes it has made.
    void stop();
    /// Queues the pairs whose interval has passed.
    void checkDue();

Q_SIGNALS:
    void statusChanged(const QString &id);
    /// A background run stopped for the user to review its preview, or
    /// found conflicts.
    void attentionNeeded(const QString &id, const QString &message);

private:
    struct Request {
        QString id;
        Runner::Run run = Runner::Run::Automatic;
        QHash<QString, Resolution> resolutions;
        QList<PreviewItem> preview;
        QList<Conflict> conflicts;
        bool background = false;
    };

    void enqueue(const Request &request);
    void runNext();
    bool isMetered() const;

    KSharedConfig::Ptr m_config;
    QList<Pair> m_pairs;
    QList<Request> m_queue;
    QPointer<Runner> m_runner;
    Request m_current;
    QHash<QString, Report> m_reports;
    QHash<QString, QDateTime> m_lastRuns;
    QHash<QString, QString> m_progress;
    QTimer *m_timer;
    std::function<bool()> m_meteredCheck;
    std::function<void(Runner *)> m_runnerSetup;
    QStringList m_rcloneOptions;
};
}
