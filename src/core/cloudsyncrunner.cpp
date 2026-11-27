// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudsyncrunner.h"

#include "rclone.h"

#include <KIO/CopyJob>
#include <KIO/FileCopyJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/ListJob>
#include <KIO/MkdirJob>
#include <KIO/MkpathJob>
#include <KIO/SimpleJob>
#include <KIO/StatJob>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QNetworkInformation>
#include <QTimer>

using namespace CloudSync;

namespace
{
constexpr int LargeRcloneDeletion = 50;
constexpr qsizetype MaximumOutput = 4 * 1024 * 1024;

std::optional<State> stateAt(const Listing &listing, const QString &path)
{
    const auto it = listing.constFind(path);
    return it == listing.constEnd() ? std::nullopt : std::optional<State>(*it);
}

std::optional<State> localState(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return std::nullopt;
    }
    if (info.isSymLink()) {
        // A link that replaced an item never matches it.
        return State{false, -1, -1};
    }
    return State{info.isDir(), info.isDir() ? 0 : info.size(), info.lastModified().toSecsSinceEpoch()};
}

State remoteState(const KIO::UDSEntry &entry)
{
    const bool folder = entry.isDir();
    return State{folder, folder ? 0 : entry.numberValue(KIO::UDSEntry::UDS_SIZE, 0), entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0)};
}

/// Whether an item still matches what the listing found.
bool unchanged(const std::optional<State> &actual, const std::optional<State> &expected)
{
    if (!actual || !expected) {
        return !actual && !expected;
    }
    if (actual->folder != expected->folder) {
        return false;
    }
    return actual->folder || (actual->size == expected->size && actual->modified == expected->modified);
}

void dropUnder(Listing &listing, const QString &folder)
{
    const QString prefix = folder + QLatin1Char('/');
    listing.remove(folder);
    auto it = listing.lowerBound(prefix);
    while (it != listing.end() && it.key().startsWith(prefix)) {
        it = listing.erase(it);
    }
}

struct LocalListing {
    Listing items;
    QStringList links;
    QStringList unreadable;
};

LocalListing listLocal(const QString &root, const Filter &filter, bool recursive)
{
    LocalListing result;
    QStringList folders{QString()};
    while (!folders.isEmpty()) {
        const QString relative = folders.takeLast();
        const QString path = relative.isEmpty() ? root : root + QLatin1Char('/') + relative;
        const QFileInfo folderInfo(path);
        if (!folderInfo.isReadable() || !folderInfo.isExecutable()) {
            result.unreadable.append(relative);
            continue;
        }
        const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &info : entries) {
            const QString item = relative.isEmpty() ? info.fileName() : relative + QLatin1Char('/') + info.fileName();
            if (info.isSymLink()) {
                if (filter.includes(item, false)) {
                    result.links.append(item);
                }
                continue;
            }
            if (!info.isDir() && !info.isFile()) {
                continue;
            }
            const bool folder = info.isDir();
            if (!filter.includes(item, folder)) {
                continue;
            }
            result.items.insert(item, State{folder, folder ? 0 : info.size(), info.lastModified().toSecsSinceEpoch()});
            if (folder && recursive) {
                folders.append(item);
            }
        }
    }
    return result;
}

QString changedLocally()
{
    return i18nc("@info", "Changed in the local folder during synchronization; it will be synchronized next time.");
}

QString changedRemotely()
{
    return i18nc("@info", "Changed on the remote during synchronization; it will be synchronized next time.");
}
}

bool Report::hasErrors(ErrorKind kind) const
{
    return std::any_of(errors.cbegin(), errors.cend(), [kind](const ItemError &error) {
        return error.kind == kind;
    });
}

QString CloudSync::summary(const Report &report)
{
    QStringList parts;
    if (report.uploaded) {
        parts.append(i18ncp("@info", "%1 uploaded", "%1 uploaded", report.uploaded));
    }
    if (report.downloaded) {
        parts.append(i18ncp("@info", "%1 downloaded", "%1 downloaded", report.downloaded));
    }
    if (report.renamed) {
        parts.append(i18ncp("@info", "%1 renamed", "%1 renamed", report.renamed));
    }
    if (report.deleted) {
        parts.append(i18ncp("@info", "%1 deleted", "%1 deleted", report.deleted));
    }
    if (report.folders) {
        parts.append(i18ncp("@info", "%1 folder created", "%1 folders created", report.folders));
    }
    if (!report.conflicts.isEmpty()) {
        parts.append(i18ncp("@info", "%1 conflict", "%1 conflicts", report.conflicts.size()));
    }
    if (!report.errors.isEmpty()) {
        parts.append(i18ncp("@info", "%1 error", "%1 errors", report.errors.size()));
    }
    const QString details = parts.join(i18nc("@info list separator", ", "));
    switch (report.outcome) {
    case Report::Outcome::None:
        return i18nc("@info:status", "Not synchronized yet");
    case Report::Outcome::Succeeded:
        return details.isEmpty() ? i18nc("@info:status", "Up to date") : i18nc("@info:status", "Synchronized: %1", details);
    case Report::Outcome::Partial:
        return i18nc("@info:status", "Partly synchronized: %1", details);
    case Report::Outcome::NeedsReview:
        if (!report.message.isEmpty()) {
            return report.message;
        }
        return i18ncp("@info:status", "Waiting for review: %1 change", "Waiting for review: %1 changes", report.preview.size());
    case Report::Outcome::Offline:
        return i18nc("@info:status", "Offline: %1", report.message);
    case Report::Outcome::Failed:
        return i18nc("@info:status", "Failed: %1", report.message);
    case Report::Outcome::Canceled:
        return i18nc("@info:status", "Canceled");
    }
    return {};
}

Runner::Runner(const Pair &pair, QObject *parent)
    : QObject(parent)
    , m_pair(pair)
    , m_snapshotPath(snapshotPath(pair.id))
    , m_rcloneProgram(Rclone::findExecutable())
    , m_conflictName(conflictLabel(QDateTime::currentDateTime(), QHostInfo::localHostName()))
{
}

Runner::~Runner()
{
    if (m_job) {
        m_job->kill(KJob::Quietly);
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void Runner::setSnapshotPath(const QString &path)
{
    m_snapshotPath = path;
}

void Runner::setResolutions(const QHash<QString, Resolution> &resolutions)
{
    m_resolutions = resolutions;
}

void Runner::setConfirmedPreview(const QList<PreviewItem> &preview)
{
    m_confirmed = preview;
}

void Runner::setConfirmedConflicts(const QList<Conflict> &conflicts)
{
    m_confirmedConflicts = conflicts;
}

void Runner::setUseTrash(bool useTrash)
{
    m_useTrash = useTrash;
}

void Runner::setRcloneProgram(const QString &program)
{
    m_rcloneProgram = program;
}

void Runner::setConflictName(const QString &name)
{
    m_conflictName = name;
}

bool Runner::isRunning() const
{
    return m_running;
}

const Pair &Runner::pair() const
{
    return m_pair;
}

const Report &Runner::report() const
{
    return m_report;
}

QUrl Runner::remoteUrl(const QString &path) const
{
    QUrl url = m_pair.remote.adjusted(QUrl::StripTrailingSlash);
    url.setPath(url.path() + QLatin1Char('/') + path);
    return url;
}

QString Runner::localPath(const QString &path) const
{
    return QDir::cleanPath(m_pair.localPath) + QLatin1Char('/') + path;
}

void Runner::start(Run run)
{
    if (m_running) {
        return;
    }
    m_run = run;
    m_running = true;
    m_canceled = false;
    m_report = {};
    m_local.clear();
    m_remote.clear();
    m_localAfter.clear();
    m_remoteAfter.clear();
    m_done.clear();
    m_next = 0;
    m_remoteMissing = false;

    QString error = pairError(m_pair, {});
    std::optional<Snapshot> snapshot;
    if (error.isEmpty()) {
        snapshot = loadSnapshot(m_snapshotPath, &error);
    }
    if (!snapshot) {
        finish(Report::Outcome::Failed, error);
        return;
    }
    m_base = baseFor(m_pair, *snapshot);
    const QString engine = managingEngine(m_pair.localPath, defaultEngineLocations());
    if (!engine.isEmpty()) {
        finish(Report::Outcome::Failed, i18nc("@info", "%1 is managed by %2. Synchronize it with one program only.", m_pair.localPath, engine));
        return;
    }
    const QFileInfo root(m_pair.localPath);
    const bool hasBase = m_pair.usesRclone() ? m_base.rcloneInitialized : !m_base.local.isEmpty();
    if (root.exists() && !root.isDir()) {
        finish(Report::Outcome::Failed, i18nc("@info", "%1 is not a folder.", m_pair.localPath));
        return;
    }
    if (!root.exists() && hasBase) {
        finish(Report::Outcome::Failed,
               i18nc("@info", "The local folder %1 is missing. Restore it, or remove the pair and add it again to start over.", m_pair.localPath));
        return;
    }
    if (m_pair.usesRclone()) {
        startRclone();
        return;
    }

    Q_EMIT progressChanged(i18nc("@info:status", "Listing the local folder…"));
    if (root.exists()) {
        const Filter filter(m_pair.options);
        const LocalListing listing = listLocal(QDir::cleanPath(m_pair.localPath), filter, m_pair.options.recursive);
        m_local = listing.items;
        for (const QString &link : listing.links) {
            m_report.skipped.append(i18nc("@item", "Symbolic link not synchronized: %1", link));
        }
        for (const QString &folder : listing.unreadable) {
            if (folder.isEmpty()) {
                finish(Report::Outcome::Failed, i18nc("@info", "The local folder %1 cannot be read.", m_pair.localPath));
                return;
            }
            // An unreadable folder looks empty, so it is left out rather than
            // treated as deleted.
            m_report.errors.append({folder, i18nc("@info", "This folder cannot be read, so it was left out."), ErrorKind::Permission});
        }
    }
    listRemote();
}

void Runner::listRemote()
{
    Q_EMIT progressChanged(i18nc("@info:status", "Listing %1…", remoteDisplayName(m_pair)));
    KIO::ListJob *job =
        m_pair.options.recursive ? KIO::listRecursive(m_pair.remote, KIO::HideProgressInfo) : KIO::listDir(m_pair.remote, KIO::HideProgressInfo);
    m_job = job;
    const Filter filter(m_pair.options);
    connect(job, &KIO::ListJob::entries, this, [this, filter](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &entry : entries) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..") || name.endsWith(QLatin1String("/."))
                || name.endsWith(QLatin1String("/.."))) {
                continue;
            }
            const bool link = !entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST).isEmpty();
            if (!filter.includes(name, entry.isDir() && !link)) {
                continue;
            }
            if (link) {
                m_report.skipped.append(i18nc("@item", "Remote symbolic link not synchronized: %1", name));
                continue;
            }
            m_remote.insert(name, remoteState(entry));
        }
    });
    connect(job, &KJob::result, this, [this](KJob *job) {
        m_job = nullptr;
        if (m_canceled) {
            finish(Report::Outcome::Canceled);
            return;
        }
        if (job->error() == KIO::ERR_DOES_NOT_EXIST && m_base.remote.isEmpty()) {
            m_remoteMissing = true;
            m_remote.clear();
        } else if (job->error() == KIO::ERR_DOES_NOT_EXIST) {
            finish(
                Report::Outcome::Failed,
                i18nc("@info", "The remote folder %1 is missing. Restore it, or remove the pair and add it again to start over.", remoteDisplayName(m_pair)));
            return;
        } else if (job->error()) {
            const ErrorKind kind = classifyError(job->error());
            if (kind == ErrorKind::Offline) {
                finish(Report::Outcome::Offline, job->errorString());
                return;
            }
            m_report.errors.append({QString(), job->errorString(), kind});
            finish(Report::Outcome::Failed, job->errorString());
            return;
        }
        planRun();
    });
}

void Runner::planRun()
{
    for (const ItemError &error : std::as_const(m_report.errors)) {
        if (error.kind == ErrorKind::Permission && !error.path.isEmpty()) {
            dropUnder(m_local, error.path);
            dropUnder(m_remote, error.path);
            dropUnder(m_base.local, error.path);
            dropUnder(m_base.remote, error.path);
        }
    }
    m_plan = plan(m_local, m_remote, m_base, m_pair.mode, m_pair.mirrorDeletions, m_resolutions, m_conflictName);
    m_report.conflicts = m_plan.conflicts;
    m_report.skipped += m_plan.skipped;
    m_report.preview = previewItems(m_plan);

    if (m_run == Run::Preview) {
        finish(Report::Outcome::NeedsReview);
        return;
    }
    if (m_run == Run::Confirmed) {
        // The user reviewed the plan before choosing resolutions.
        const Plan reviewed = m_resolutions.isEmpty() ? m_plan : plan(m_local, m_remote, m_base, m_pair.mode, m_pair.mirrorDeletions, {}, m_conflictName);
        if (previewItems(reviewed) != m_confirmed || reviewed.conflicts != m_confirmedConflicts) {
            finish(Report::Outcome::NeedsReview, i18nc("@info:status", "The folders changed since the preview. Review the new preview."));
            return;
        }
    }
    if (m_run == Run::Automatic && CloudSync::needsReview(m_plan, m_base)) {
        finish(
            Report::Outcome::NeedsReview,
            m_plan.firstSync
                ? i18ncp("@info:status",
                         "Waiting for review of the first synchronization: %1 change",
                         "Waiting for review of the first synchronization: %1 changes",
                         m_plan.changes.size())
                : i18ncp("@info:status", "Waiting for review: %1 file would be deleted", "Waiting for review: %1 files would be deleted", m_plan.deletions()));
        return;
    }
    checkSpace();
}

void Runner::checkSpace()
{
    const qint64 bytes = m_plan.uploadBytes();
    if (bytes <= 0) {
        createRoots();
        return;
    }
    Q_EMIT progressChanged(i18nc("@info:status", "Checking free space…"));
    auto *job = KIO::fileSystemFreeSpace(m_remoteMissing ? KIO::upUrl(m_pair.remote) : m_pair.remote);
    m_job = job;
    connect(job, &KJob::result, this, [this, job, bytes] {
        m_job = nullptr;
        if (m_canceled) {
            finish(Report::Outcome::Canceled);
            return;
        }
        if (!job->error() && job->size() > 0) {
            m_report.freeBytes = qint64(job->availableSize());
            if (bytes > m_report.freeBytes) {
                const QString message = i18nc("@info",
                                              "The remote has %1 free, but %2 would be uploaded.",
                                              KIO::convertSize(job->availableSize()),
                                              KIO::convertSize(KIO::filesize_t(bytes)));
                m_report.errors.append({QString(), message, ErrorKind::Quota});
                finish(Report::Outcome::Failed, message);
                return;
            }
        }
        // Remotes that do not report free space are synchronized anyway.
        createRoots();
    });
}

void Runner::createRoots()
{
    if (!QFileInfo::exists(m_pair.localPath) && !QDir().mkpath(m_pair.localPath)) {
        finish(Report::Outcome::Failed, i18nc("@info", "The local folder %1 could not be created.", m_pair.localPath));
        return;
    }
    if (!m_remoteMissing) {
        applyNext();
        return;
    }
    auto *job = KIO::mkpath(m_pair.remote, QUrl(), KIO::HideProgressInfo);
    m_job = job;
    connect(job, &KJob::result, this, [this](KJob *job) {
        m_job = nullptr;
        if (m_canceled) {
            finish(Report::Outcome::Canceled);
        } else if (job->error()) {
            m_report.errors.append({QString(), job->errorString(), classifyError(job->error())});
            finish(classifyError(job->error()) == ErrorKind::Offline ? Report::Outcome::Offline : Report::Outcome::Failed, job->errorString());
        } else {
            applyNext();
        }
    });
}

void Runner::applyNext()
{
    if (m_canceled) {
        complete();
        return;
    }
    // A lost connection fails every remaining change, so stop at the first.
    if (m_report.hasErrors(ErrorKind::Offline) || m_next >= m_plan.changes.size()) {
        complete();
        return;
    }
    const Change change = m_plan.changes.at(m_next++);
    Q_EMIT progressChanged(i18nc("@info:status %1 is an action, %2 a file", "%1: %2", describe(change), change.path));
    const QString local = localPath(change.path);
    const QUrl remote = remoteUrl(change.path);
    switch (change.operation) {
    case Operation::Upload: {
        if (!unchanged(localState(local), stateAt(m_local, change.path))) {
            fail(change, changedLocally(), ErrorKind::Other);
            return;
        }
        const auto copy = [this, change, local, remote] {
            KIO::JobFlags flags = KIO::HideProgressInfo;
            if (change.overwrite) {
                flags |= KIO::Overwrite;
            }
            runJob(KIO::file_copy(QUrl::fromLocalFile(local), remote, -1, flags), change, [this, change] {
                recordRemote(change);
            });
        };
        if (change.overwrite) {
            verifyRemote(change, change.path, stateAt(m_remote, change.path), copy);
        } else {
            copy();
        }
        return;
    }
    case Operation::Download: {
        const QString source = change.source.isEmpty() ? change.path : change.source;
        if (!unchanged(localState(local), change.overwrite ? stateAt(m_local, change.path) : std::nullopt)) {
            fail(change, changedLocally(), ErrorKind::Other);
            return;
        }
        QDir().mkpath(QFileInfo(local).absolutePath());
        KIO::JobFlags flags = KIO::HideProgressInfo;
        if (change.overwrite) {
            flags |= KIO::Overwrite;
        }
        runJob(KIO::file_copy(remoteUrl(source), QUrl::fromLocalFile(local), -1, flags), change, [this, change, local] {
            if (const auto state = localState(local)) {
                m_localAfter.insert(change.path, *state);
            }
            succeed(change);
        });
        return;
    }
    case Operation::CreateRemoteFolder: {
        auto *job = KIO::mkdir(remote);
        job->setUiDelegate(nullptr);
        runJob(job, change, [this, change] {
            m_remoteAfter.insert(change.path, State{true, 0, 0});
            succeed(change);
        });
        return;
    }
    case Operation::CreateLocalFolder:
        if (!QDir().mkpath(local) || !QFileInfo(local).isDir()) {
            fail(change, i18nc("@info", "The folder could not be created."), ErrorKind::Permission);
            return;
        }
        m_localAfter.insert(change.path, State{true, 0, 0});
        succeed(change);
        return;
    case Operation::DeleteRemote:
        if (change.folder) {
            runJob(KIO::rmdir(remote), change, [this, change] {
                succeed(change);
            });
            return;
        }
        verifyRemote(change, change.path, stateAt(m_remote, change.path), [this, change, remote] {
            runJob(KIO::file_delete(remote, KIO::HideProgressInfo), change, [this, change] {
                succeed(change);
            });
        });
        return;
    case Operation::DeleteLocal:
        if (change.folder) {
            if (!QDir().rmdir(local)) {
                fail(change, i18nc("@info", "The folder is not empty, so it was kept."), ErrorKind::Other);
                return;
            }
            succeed(change);
            return;
        }
        if (!unchanged(localState(local), stateAt(m_local, change.path))) {
            fail(change, changedLocally(), ErrorKind::Other);
            return;
        }
        if (m_useTrash) {
            runJob(KIO::trash(QUrl::fromLocalFile(local), KIO::HideProgressInfo), change, [this, change] {
                succeed(change);
            });
        } else if (QFile::remove(local)) {
            succeed(change);
        } else {
            fail(change, i18nc("@info", "The file could not be deleted."), ErrorKind::Permission);
        }
        return;
    case Operation::RenameRemote:
        verifyRemote(change, change.source, stateAt(m_remote, change.source), [this, change, remote] {
            runJob(KIO::rename(remoteUrl(change.source), remote, KIO::HideProgressInfo), change, [this, change] {
                recordRemote(change);
            });
        });
        return;
    case Operation::RenameLocal: {
        const QString source = localPath(change.source);
        if (!unchanged(localState(source), stateAt(m_local, change.source)) || localState(local)) {
            fail(change, changedLocally(), ErrorKind::Other);
            return;
        }
        QDir().mkpath(QFileInfo(local).absolutePath());
        if (!QFile::rename(source, local)) {
            fail(change, i18nc("@info", "The file could not be renamed."), ErrorKind::Permission);
            return;
        }
        if (const auto state = localState(local)) {
            m_localAfter.insert(change.path, *state);
        }
        succeed(change);
        return;
    }
    }
}

void Runner::verifyRemote(const Change &change, const QString &path, const std::optional<State> &expected, std::function<void()> next)
{
    auto *job = KIO::stat(remoteUrl(path), KIO::StatJob::SourceSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
    m_job = job;
    connect(job, &KJob::result, this, [this, job, change, expected, next = std::move(next)] {
        m_job = nullptr;
        if (m_canceled) {
            complete();
            return;
        }
        std::optional<State> actual;
        if (!job->error()) {
            actual = remoteState(job->statResult());
        } else if (job->error() != KIO::ERR_DOES_NOT_EXIST) {
            fail(change, job->errorString(), classifyError(job->error()));
            return;
        }
        if (!unchanged(actual, expected)) {
            fail(change, changedRemotely(), ErrorKind::Other);
            return;
        }
        next();
    });
}

void Runner::recordRemote(const Change &change)
{
    // The snapshot needs the remote state a write produced, which may differ
    // from the local one, such as when the server sets its own times.
    auto *job = KIO::stat(remoteUrl(change.path), KIO::StatJob::SourceSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
    m_job = job;
    connect(job, &KJob::result, this, [this, job, change] {
        m_job = nullptr;
        if (!job->error()) {
            m_remoteAfter.insert(change.path, remoteState(job->statResult()));
        }
        succeed(change);
    });
}

void Runner::runJob(KJob *job, const Change &change, std::function<void()> onSuccess)
{
    m_job = job;
    connect(job, &KJob::result, this, [this, change, onSuccess = std::move(onSuccess)](KJob *job) {
        m_job = nullptr;
        if (m_canceled) {
            complete();
            return;
        }
        if (job->error() == KIO::ERR_DIR_ALREADY_EXIST && change.operation == Operation::CreateRemoteFolder) {
            onSuccess();
            return;
        }
        if (job->error()) {
            fail(change, job->errorString(), classifyError(job->error()));
            return;
        }
        onSuccess();
    });
}

void Runner::succeed(const Change &change)
{
    m_done.append(change);
    switch (change.operation) {
    case Operation::Upload:
        ++m_report.uploaded;
        break;
    case Operation::Download:
        ++m_report.downloaded;
        break;
    case Operation::CreateRemoteFolder:
    case Operation::CreateLocalFolder:
        ++m_report.folders;
        break;
    case Operation::DeleteRemote:
    case Operation::DeleteLocal:
        ++m_report.deleted;
        break;
    case Operation::RenameRemote:
    case Operation::RenameLocal:
        ++m_report.renamed;
        break;
    }
    QMetaObject::invokeMethod(this, &Runner::applyNext, Qt::QueuedConnection);
}

void Runner::fail(const Change &change, const QString &message, ErrorKind kind)
{
    m_report.errors.append({change.path, message, kind});
    QMetaObject::invokeMethod(this, &Runner::applyNext, Qt::QueuedConnection);
}

void Runner::complete()
{
    if (!m_running) {
        return;
    }
    const Snapshot next = nextSnapshot(m_base, m_local, m_remote, m_plan, m_done, m_localAfter, m_remoteAfter);
    QString error;
    if (next != m_base && !saveSnapshot(m_snapshotPath, next, &error)) {
        m_report.errors.append({QString(), error, ErrorKind::Other});
    }
    if (m_canceled) {
        finish(Report::Outcome::Canceled);
    } else if (m_report.hasErrors(ErrorKind::Offline)) {
        finish(Report::Outcome::Partial, i18nc("@info", "The connection was lost; the remaining changes will be synchronized next time."));
    } else {
        finish(m_report.errors.isEmpty() ? Report::Outcome::Succeeded : Report::Outcome::Partial);
    }
}

void Runner::finish(Report::Outcome outcome, const QString &message)
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_report.outcome = outcome;
    if (!message.isEmpty()) {
        m_report.message = message;
    }
    m_report.finishedAt = QDateTime::currentDateTime();
    Q_EMIT finished();
}

void Runner::cancel()
{
    if (!m_running) {
        return;
    }
    m_canceled = true;
    if (m_job) {
        m_job->kill(KJob::EmitResult);
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void Runner::startRclone()
{
    if (m_rcloneProgram.isEmpty()) {
        finish(Report::Outcome::Failed, i18nc("@info", "rclone is not installed. Install rclone to synchronize this pair."));
        return;
    }
    if (!QFileInfo::exists(m_pair.localPath) && !QDir().mkpath(m_pair.localPath)) {
        finish(Report::Outcome::Failed, i18nc("@info", "The local folder %1 could not be created.", m_pair.localPath));
        return;
    }
    m_rcloneFilters = rcloneFilterRules(m_pair.options);
    // bisync must compare both sides again when it starts or its filters change.
    m_rcloneInitialize = m_pair.mode == Mode::Sync && (!m_base.rcloneInitialized || m_base.rcloneFilters != m_rcloneFilters);
    const bool dryRun = m_run == Run::Preview || (m_run == Run::Automatic && (m_rcloneInitialize || (m_pair.mode != Mode::Sync && m_pair.mirrorDeletions)));
    runRclone(dryRun);
}

void Runner::runRclone(bool dryRun)
{
    QString error;
    const QStringList arguments = rcloneArguments(m_pair, m_rcloneInitialize, dryRun, &error);
    if (arguments.isEmpty()) {
        finish(Report::Outcome::Failed, error);
        return;
    }
    m_rcloneDryRun = dryRun;
    m_output.clear();
    delete m_process;
    m_process = new QProcess(this);
    m_process->setProcessEnvironment(Rclone::environment());
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, [this] {
        m_output += m_process->readAll();
        if (m_output.size() > MaximumOutput) {
            m_output = m_output.right(MaximumOutput);
        }
    });
    connect(m_process, &QProcess::finished, this, &Runner::rcloneFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart) {
            finish(Report::Outcome::Failed, i18nc("@info", "rclone could not be started: %1", m_process->errorString()));
        }
    });
    Q_EMIT progressChanged(dryRun ? i18nc("@info:status", "Previewing with rclone…") : i18nc("@info:status", "Synchronizing with rclone…"));
    m_process->start(m_rcloneProgram, arguments);
}

void Runner::rcloneFinished(int exitCode, QProcess::ExitStatus status)
{
    m_output += m_process->readAll();
    if (m_canceled) {
        finish(Report::Outcome::Canceled);
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        if (rcloneNeedsResync(m_output)) {
            Snapshot next = m_base;
            next.rcloneInitialized = false;
            saveSnapshot(m_snapshotPath, next);
            finish(Report::Outcome::Failed,
                   i18nc("@info", "rclone needs to compare both sides again. Preview the pair to review a new first synchronization."));
            return;
        }
        const QString message = Rclone::lastError(m_output);
        finish(Report::Outcome::Failed, message.isEmpty() ? i18nc("@info", "rclone stopped with exit code %1.", exitCode) : message);
        return;
    }
    if (m_rcloneDryRun) {
        m_report.preview = parseRcloneDryRun(m_output);
        const auto deletions = std::count_if(m_report.preview.cbegin(), m_report.preview.cend(), [](const PreviewItem &item) {
            return item.deletion;
        });
        if (m_run == Run::Preview || m_rcloneInitialize || deletions >= LargeRcloneDeletion) {
            finish(Report::Outcome::NeedsReview,
                   m_rcloneInitialize ? i18nc("@info:status", "Waiting for review of the first synchronization with rclone") : QString());
            return;
        }
        runRclone(false);
        return;
    }
    Snapshot next = m_base;
    next.rcloneInitialized = true;
    next.rcloneFilters = m_rcloneFilters;
    QString error;
    if (!saveSnapshot(m_snapshotPath, next, &error)) {
        m_report.errors.append({QString(), error, ErrorKind::Other});
        finish(Report::Outcome::Partial);
        return;
    }
    finish(Report::Outcome::Succeeded);
}

Scheduler::Scheduler(KSharedConfig::Ptr config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_pairs(load(m_config->group(QStringLiteral("CloudSyncPairs"))))
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(std::chrono::minutes(1));
    connect(m_timer, &QTimer::timeout, this, &Scheduler::checkDue);
    m_meteredCheck = [] {
        static const bool loaded = QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Metered);
        const QNetworkInformation *information = QNetworkInformation::instance();
        return loaded && information && information->isMetered();
    };
}

Scheduler::~Scheduler()
{
    m_queue.clear();
    delete m_runner;
}

QList<Pair> Scheduler::pairs() const
{
    return m_pairs;
}

std::optional<Pair> Scheduler::pair(const QString &id) const
{
    for (const Pair &pair : m_pairs) {
        if (pair.id == id) {
            return pair;
        }
    }
    return std::nullopt;
}

bool Scheduler::setPairs(const QList<Pair> &pairs, QString *error)
{
    for (const Pair &pair : pairs) {
        const QString problem = pairError(pair, pairs);
        if (!problem.isEmpty()) {
            if (error) {
                *error = problem;
            }
            return false;
        }
    }
    if (!save(m_config->group(QStringLiteral("CloudSyncPairs")), pairs, error)) {
        return false;
    }
    QSet<QString> kept;
    for (const Pair &pair : pairs) {
        kept.insert(pair.id);
    }
    const QList<Pair> previous = m_pairs;
    m_pairs = pairs;
    for (const Pair &pair : previous) {
        if (!kept.contains(pair.id)) {
            cancel(pair.id);
            QFile::remove(snapshotPath(pair.id));
            m_reports.remove(pair.id);
            m_lastRuns.remove(pair.id);
        }
        Q_EMIT statusChanged(pair.id);
    }
    for (const Pair &pair : pairs) {
        Q_EMIT statusChanged(pair.id);
    }
    return true;
}

QStringList Scheduler::pausedAccounts() const
{
    return m_config->group(QStringLiteral("CloudSync")).readEntry("PausedAccounts", QStringList());
}

void Scheduler::setAccountPaused(const QString &account, bool paused)
{
    QStringList accounts = pausedAccounts();
    accounts.removeAll(account);
    if (paused) {
        accounts.append(account);
    }
    KConfigGroup group = m_config->group(QStringLiteral("CloudSync"));
    group.writeEntry("PausedAccounts", accounts);
    group.sync();
    for (const Pair &pair : std::as_const(m_pairs)) {
        if (accountKey(pair) == account) {
            Q_EMIT statusChanged(pair.id);
        }
    }
}

bool Scheduler::isPaused(const Pair &pair) const
{
    return pair.paused || pausedAccounts().contains(accountKey(pair));
}

bool Scheduler::pausesOnMeteredConnections() const
{
    return m_config->group(QStringLiteral("CloudSync")).readEntry("PauseOnMeteredConnections", true);
}

void Scheduler::setPausesOnMeteredConnections(bool pause)
{
    KConfigGroup group = m_config->group(QStringLiteral("CloudSync"));
    group.writeEntry("PauseOnMeteredConnections", pause);
    group.sync();
}

void Scheduler::setMeteredCheck(std::function<bool()> check)
{
    m_meteredCheck = std::move(check);
}

void Scheduler::setRunnerSetup(std::function<void(Runner *)> setup)
{
    m_runnerSetup = std::move(setup);
}

bool Scheduler::isMetered() const
{
    return m_meteredCheck && m_meteredCheck();
}

Scheduler::State Scheduler::state(const QString &id) const
{
    if (m_runner && m_current.id == id) {
        return State::Running;
    }
    return std::any_of(m_queue.cbegin(),
                       m_queue.cend(),
                       [&id](const Request &request) {
                           return request.id == id;
                       })
        ? State::Queued
        : State::Idle;
}

Report Scheduler::lastReport(const QString &id) const
{
    return m_reports.value(id);
}

QString Scheduler::progress(const QString &id) const
{
    return m_progress.value(id);
}

bool Scheduler::needsReview(const QString &id) const
{
    return m_reports.value(id).outcome == Report::Outcome::NeedsReview;
}

void Scheduler::syncNow(const QString &id)
{
    enqueue({id, Runner::Run::Automatic, {}, {}, {}, false});
}

void Scheduler::preview(const QString &id)
{
    enqueue({id, Runner::Run::Preview, {}, {}, {}, false});
}

void Scheduler::confirm(const QString &id, const QHash<QString, Resolution> &resolutions, const Report &reviewed)
{
    enqueue({id, Runner::Run::Confirmed, resolutions, reviewed.preview, reviewed.conflicts, false});
}

void Scheduler::cancel(const QString &id)
{
    m_queue.erase(std::remove_if(m_queue.begin(),
                                 m_queue.end(),
                                 [&id](const Request &request) {
                                     return request.id == id;
                                 }),
                  m_queue.end());
    if (m_runner && m_current.id == id) {
        m_runner->cancel();
    }
    Q_EMIT statusChanged(id);
}

void Scheduler::start()
{
    m_timer->start();
    QTimer::singleShot(0, this, &Scheduler::checkDue);
}

void Scheduler::stop()
{
    m_timer->stop();
    const QStringList queued = [this] {
        QStringList ids;
        for (const Request &request : std::as_const(m_queue)) {
            ids.append(request.id);
        }
        return ids;
    }();
    m_queue.clear();
    if (m_runner) {
        m_runner->cancel();
    }
    for (const QString &id : queued) {
        Q_EMIT statusChanged(id);
    }
}

void Scheduler::checkDue()
{
    const QDateTime now = QDateTime::currentDateTime();
    QStringList due;
    for (const Pair &pair : std::as_const(m_pairs)) {
        if (pair.intervalMinutes <= 0 || isPaused(pair) || state(pair.id) != State::Idle || needsReview(pair.id)) {
            continue;
        }
        const QDateTime last = m_lastRuns.value(pair.id);
        if (!last.isValid() || last.secsTo(now) >= qint64(pair.intervalMinutes) * 60) {
            due.append(pair.id);
        }
    }
    // Checking the connection loads a network information backend, so only
    // check when a pair is due.
    if (due.isEmpty() || (pausesOnMeteredConnections() && isMetered())) {
        return;
    }
    for (const QString &id : std::as_const(due)) {
        enqueue({id, Runner::Run::Automatic, {}, {}, {}, true});
    }
}

void Scheduler::enqueue(const Request &request)
{
    if (!pair(request.id)) {
        return;
    }
    // A newer request replaces a queued one for the same pair.
    m_queue.erase(std::remove_if(m_queue.begin(),
                                 m_queue.end(),
                                 [&request](const Request &queued) {
                                     return queued.id == request.id;
                                 }),
                  m_queue.end());
    m_queue.append(request);
    Q_EMIT statusChanged(request.id);
    runNext();
}

void Scheduler::runNext()
{
    if (m_runner || m_queue.isEmpty()) {
        return;
    }
    m_current = m_queue.takeFirst();
    const auto current = pair(m_current.id);
    if (!current) {
        QTimer::singleShot(0, this, &Scheduler::runNext);
        return;
    }
    auto *runner = new Runner(*current, this);
    m_runner = runner;
    runner->setResolutions(m_current.resolutions);
    runner->setConfirmedPreview(m_current.preview);
    runner->setConfirmedConflicts(m_current.conflicts);
    if (m_runnerSetup) {
        m_runnerSetup(runner);
    }
    const QString id = m_current.id;
    connect(runner, &Runner::progressChanged, this, [this, id](const QString &message) {
        m_progress.insert(id, message);
        Q_EMIT statusChanged(id);
    });
    connect(runner, &Runner::finished, this, [this, runner, id] {
        const Report report = runner->report();
        const bool background = m_current.background;
        m_reports.insert(id, report);
        m_lastRuns.insert(id, QDateTime::currentDateTime());
        m_progress.remove(id);
        m_runner = nullptr;
        m_current = {};
        runner->deleteLater();
        if (background && (report.outcome == Report::Outcome::NeedsReview || !report.conflicts.isEmpty())) {
            Q_EMIT attentionNeeded(id, summary(report));
        }
        Q_EMIT statusChanged(id);
        QTimer::singleShot(0, this, &Scheduler::runNext);
    });
    Q_EMIT statusChanged(id);
    runner->start(m_current.run);
}
