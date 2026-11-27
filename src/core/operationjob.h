// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KJob>

#include <QUrl>

#include <functional>

class KJobTrackerInterface;
class QFutureWatcherBase;

/// A KJob that reports a Plasma Opus-owned operation, such as a checksum run
/// or a folder synchronization, to KDE's job tracker so that Plasma shows its
/// progress and result. The operation itself runs elsewhere; the owner
/// updates the job and finishes it.
///
/// Descriptions show only the file or folder name of a location, never its
/// full path. The destination is also offered to Plasma so that its job view
/// can open the result.
class OperationJob : public KJob
{
    Q_OBJECT

public:
    /// The operations that can report to Plasma, each with its own setting
    /// under KDE Plasma › Notifications.
    enum class Kind {
        Checksums,
        FolderSync,
        DuplicateFinder,
        FileSplit,
        BatchRename,
        ImageConversion,
        LibraryRefresh,
    };

    OperationJob(Kind kind, const QString &title, QObject *parent = nullptr);

    /// Whether the settings let operations of this kind report to Plasma.
    static bool reports(Kind kind);

    /// Registers the job with the tracker, or the default tracker, starts it
    /// and, when a watcher is given, follows it. Without any tracker, such as
    /// in tests, or when the settings turn off reporting for the job's kind,
    /// the job still runs but nothing reports it.
    static OperationJob *track(OperationJob *job, QFutureWatcherBase *watcher = nullptr, KJobTrackerInterface *tracker = nullptr);
    /// The tracker used by track(); the application sets Plasma's job tracker.
    static void setDefaultTracker(KJobTrackerInterface *tracker);
    static KJobTrackerInterface *defaultTracker();

    /// Follows the watcher's progress. When the watcher finishes, the job
    /// finishes too, after the owner's directly connected handlers, unless
    /// one of them has already finished it with a failure. A canceled or
    /// destroyed watcher cancels the job.
    void follow(QFutureWatcherBase *watcher);

    void start() override;

    Kind kind() const;
    QString title() const;
    void setSource(const QUrl &url);
    void setDestination(const QUrl &url);
    QString sourceName() const;
    QString destinationName() const;

    /// Called when the job is canceled from outside the owner, such as from
    /// Plasma's job view. Without a handler the job cannot be canceled.
    void setCancelHandler(std::function<void()> handler);
    void setProgress(qulonglong processed, qulonglong total);

    /// Finishes successfully, or with a failure when the error text is not
    /// empty. Does nothing once the job has finished.
    void finish(const QString &errorText = QString());
    /// Finishes as canceled without calling the cancel handler.
    void cancel();

    /// The name shown for a location: its file name, or its host or scheme
    /// for the root of a remote location.
    static QString displayName(const QUrl &url);

protected:
    bool doKill() override;

private:
    void updateDescription();

    Kind m_kind;
    QString m_title;
    QUrl m_source;
    QUrl m_destination;
    std::function<void()> m_cancelHandler;
};
