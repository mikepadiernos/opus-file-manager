// SPDX-License-Identifier: GPL-3.0-or-later

#include "librarymovetracker.h"

#include "libraryregistry.h"

#include <KIO/CopyJob>
#include <KIO/RenameFileDialog>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHash>

#include <memory>

#include <sys/stat.h>

namespace
{
bool identify(const QString &path, quint64 *device, quint64 *inode)
{
    struct stat data;
    if (::lstat(QFile::encodeName(path).constData(), &data) != 0) {
        return false;
    }
    *device = quint64(data.st_dev);
    *inode = quint64(data.st_ino);
    return true;
}

QString identityKey(quint64 device, quint64 inode)
{
    return QStringLiteral("%1:%2").arg(device).arg(inode);
}
}

LibraryMoveTracker::LibraryMoveTracker(QObject *parent)
    : QObject(parent)
{
    m_pendingTimer.setInterval(250);
    connect(&m_pendingTimer, &QTimer::timeout, this, &LibraryMoveTracker::checkPending);
}

LibraryMoveTracker *LibraryMoveTracker::instance()
{
    static auto *tracker = new LibraryMoveTracker(qApp);
    return tracker;
}

void LibraryMoveTracker::trackCopyJob(KIO::CopyJob *job)
{
    if (!job || job->operationMode() != KIO::CopyJob::Move || LibraryRegistry::roots().isEmpty()) {
        return;
    }
    auto moves = std::make_shared<QList<LibraryMoves::Move>>();
    connect(job, &KIO::CopyJob::copyingDone, this, [moves](KIO::Job *, const QUrl &from, const QUrl &to) {
        if (from.isLocalFile() && to.isLocalFile()) {
            moves->append({from.toLocalFile(), to.toLocalFile()});
        }
    });
    // finished is emitted even when the job is killed, so partial moves are kept.
    connect(job, &KJob::finished, this, [this, moves] {
        apply(*moves);
    });
}

void LibraryMoveTracker::trackRenameDialog(KIO::RenameFileDialog *dialog, const KFileItemList &items)
{
    if (!dialog || LibraryRegistry::roots().isEmpty()) {
        return;
    }
    QHash<QString, QString> sources;
    for (const KFileItem &item : items) {
        quint64 device = 0;
        quint64 inode = 0;
        if (item.url().isLocalFile() && identify(item.url().toLocalFile(), &device, &inode)) {
            sources.insert(identityKey(device, inode), item.url().toLocalFile());
        }
    }
    if (sources.isEmpty()) {
        return;
    }
    connect(dialog, &KIO::RenameFileDialog::renamingFinished, this, [this, sources](const QList<QUrl> &urls) {
        QList<LibraryMoves::Move> moves;
        for (const QUrl &url : urls) {
            quint64 device = 0;
            quint64 inode = 0;
            if (!url.isLocalFile() || !identify(url.toLocalFile(), &device, &inode)) {
                continue;
            }
            const QString from = sources.value(identityKey(device, inode));
            if (!from.isEmpty()) {
                moves.append({from, url.toLocalFile(), device, inode});
            }
        }
        apply(moves);
    });
}

void LibraryMoveTracker::expectRename(const QString &from, const QString &to)
{
    quint64 device = 0;
    quint64 inode = 0;
    if (from == to || (LibraryRegistry::rootFor(from).isEmpty() && LibraryRegistry::rootFor(to).isEmpty()) || !identify(from, &device, &inode)) {
        return;
    }
    m_pending.append({{from, to, device, inode}, QDeadlineTimer(30000)});
    m_pendingTimer.start();
}

void LibraryMoveTracker::checkPending()
{
    QList<LibraryMoves::Move> confirmed;
    for (qsizetype i = 0; i < m_pending.size();) {
        const Pending &pending = m_pending.at(i);
        quint64 device = 0;
        quint64 inode = 0;
        if (identify(pending.move.to, &device, &inode) && device == pending.move.device && inode == pending.move.inode) {
            confirmed.append(pending.move);
            m_pending.removeAt(i);
        } else if (pending.deadline.hasExpired()) {
            m_pending.removeAt(i);
        } else {
            ++i;
        }
    }
    if (m_pending.isEmpty()) {
        m_pendingTimer.stop();
    }
    apply(confirmed);
}

void LibraryMoveTracker::apply(const QList<LibraryMoves::Move> &moves)
{
    if (moves.isEmpty()) {
        return;
    }
    const QStringList roots = LibraryRegistry::roots();
    if (roots.isEmpty()) {
        return;
    }
    const LibraryMoves::Outcome outcome = LibraryMoves::apply(moves, roots);
    if (!outcome.changed.isEmpty()) {
        Q_EMIT librariesChanged(outcome.changed);
    }
    for (const QString &error : outcome.errors) {
        Q_EMIT failed(error);
    }
}
