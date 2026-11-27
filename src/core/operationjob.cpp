// SPDX-License-Identifier: GPL-3.0-or-later

#include "operationjob.h"

#include "settings.h"

#include <KJobTrackerInterface>
#include <KLocalizedString>

#include <QFutureWatcherBase>
#include <QPointer>

namespace
{
QPointer<KJobTrackerInterface> &defaultTrackerStorage()
{
    static QPointer<KJobTrackerInterface> tracker;
    return tracker;
}
}

void OperationJob::setDefaultTracker(KJobTrackerInterface *tracker)
{
    defaultTrackerStorage() = tracker;
}

KJobTrackerInterface *OperationJob::defaultTracker()
{
    return defaultTrackerStorage();
}

bool OperationJob::reports(Kind kind)
{
    switch (kind) {
    case Kind::Checksums:
        return Settings::notifyChecksums();
    case Kind::FolderSync:
        return Settings::notifyFolderSync();
    case Kind::DuplicateFinder:
        return Settings::notifyDuplicateFinder();
    case Kind::FileSplit:
        return Settings::notifyFileSplit();
    case Kind::BatchRename:
        return Settings::notifyBatchRename();
    case Kind::ImageConversion:
        return Settings::notifyImageConversion();
    case Kind::LibraryRefresh:
        return Settings::notifyLibraryRefresh();
    }
    return true;
}

OperationJob *OperationJob::track(OperationJob *job, QFutureWatcherBase *watcher, KJobTrackerInterface *tracker)
{
    if (!tracker) {
        tracker = defaultTracker();
    }
    if (tracker && reports(job->kind())) {
        tracker->registerJob(job);
    }
    job->start();
    if (watcher) {
        job->follow(watcher);
    }
    return job;
}

void OperationJob::follow(QFutureWatcherBase *watcher)
{
    QPointer<QFutureWatcherBase> guard(watcher);
    connect(watcher, &QFutureWatcherBase::progressRangeChanged, this, [this, guard](int minimum, int maximum) {
        if (guard && maximum > minimum) {
            setProgress(qulonglong(qMax(0, guard->progressValue() - minimum)), qulonglong(maximum - minimum));
        }
    });
    connect(watcher, &QFutureWatcherBase::progressValueChanged, this, [this, guard](int value) {
        if (guard && guard->progressMaximum() > guard->progressMinimum()) {
            setProgress(qulonglong(qMax(0, value - guard->progressMinimum())), qulonglong(guard->progressMaximum() - guard->progressMinimum()));
        }
    });
    // Queued, so the owner's own finished handler can report a failure first.
    connect(
        watcher,
        &QFutureWatcherBase::finished,
        this,
        [this, guard] {
            if (guard && guard->isCanceled()) {
                cancel();
            } else {
                finish();
            }
        },
        Qt::QueuedConnection);
    connect(watcher, &QObject::destroyed, this, &OperationJob::cancel);
}

OperationJob::OperationJob(Kind kind, const QString &title, QObject *parent)
    : KJob(parent)
    , m_kind(kind)
    , m_title(title)
{
    setProgressUnit(KJob::Items);
    setCapabilities(KJob::NoCapabilities);
}

void OperationJob::start()
{
    updateDescription();
}

OperationJob::Kind OperationJob::kind() const
{
    return m_kind;
}

QString OperationJob::title() const
{
    return m_title;
}

void OperationJob::setSource(const QUrl &url)
{
    m_source = url;
    updateDescription();
}

void OperationJob::setDestination(const QUrl &url)
{
    m_destination = url;
    setProperty("destUrl", url);
    updateDescription();
}

QString OperationJob::sourceName() const
{
    return displayName(m_source);
}

QString OperationJob::destinationName() const
{
    return displayName(m_destination);
}

void OperationJob::setCancelHandler(std::function<void()> handler)
{
    m_cancelHandler = std::move(handler);
    setCapabilities(m_cancelHandler ? KJob::Killable : KJob::NoCapabilities);
}

void OperationJob::setProgress(qulonglong processed, qulonglong total)
{
    if (isFinished()) {
        return;
    }
    setTotalAmount(KJob::Items, total);
    setProcessedAmount(KJob::Items, qMin(processed, total));
}

void OperationJob::finish(const QString &errorText)
{
    if (isFinished()) {
        return;
    }
    m_cancelHandler = nullptr;
    if (!errorText.isEmpty()) {
        setError(KJob::UserDefinedError);
        setErrorText(errorText);
    } else if (totalAmount(KJob::Items) > 0) {
        setProcessedAmount(KJob::Items, totalAmount(KJob::Items));
    }
    emitResult();
}

void OperationJob::cancel()
{
    if (isFinished()) {
        return;
    }
    m_cancelHandler = nullptr;
    kill(KJob::EmitResult);
}

QString OperationJob::displayName(const QUrl &url)
{
    if (url.isEmpty()) {
        return QString();
    }
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    if (!name.isEmpty()) {
        return name;
    }
    if (!url.host().isEmpty()) {
        return url.host();
    }
    return url.isLocalFile() ? QStringLiteral("/") : url.scheme();
}

bool OperationJob::doKill()
{
    if (m_cancelHandler) {
        const auto handler = std::move(m_cancelHandler);
        m_cancelHandler = nullptr;
        handler();
    }
    return true;
}

void OperationJob::updateDescription()
{
    const QString source = sourceName();
    const QString destination = destinationName();
    Q_EMIT description(this,
                       m_title,
                       source.isEmpty() ? std::pair<QString, QString>() : std::pair{i18nc("@label operation source", "Source"), source},
                       destination.isEmpty() ? std::pair<QString, QString>() : std::pair{i18nc("@label operation destination", "Destination"), destination});
}
