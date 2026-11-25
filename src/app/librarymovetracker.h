// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "librarymoves.h"

#include <KFileItem>

#include <QDeadlineTimer>
#include <QObject>
#include <QTimer>

namespace KIO
{
class CopyJob;
class RenameFileDialog;
}

/**
 * Follows moves and renames that Plasma Opus starts and keeps tag libraries
 * in step with them. Each move is confirmed on disk before it's applied;
 * moves made by other programs are left for review after a refresh.
 */
class LibraryMoveTracker : public QObject
{
    Q_OBJECT

public:
    static LibraryMoveTracker *instance();

    /// Follows a KIO move or a cut and paste; copies are ignored.
    void trackCopyJob(KIO::CopyJob *job);
    /// Follows renames made in KIO's rename dialog for @p items.
    void trackRenameDialog(KIO::RenameFileDialog *dialog, const KFileItemList &items);
    /// Follows a rename whose job isn't available, such as an inline rename
    /// in a view; it's applied once the renamed file is confirmed at @p to.
    void expectRename(const QString &from, const QString &to);
    /// Applies moves that Plasma Opus completed.
    void apply(const QList<LibraryMoves::Move> &moves);

Q_SIGNALS:
    /// The documents of the libraries at @p roots changed.
    void librariesChanged(const QStringList &roots);
    void failed(const QString &message);

private:
    explicit LibraryMoveTracker(QObject *parent = nullptr);
    void checkPending();

    struct Pending {
        LibraryMoves::Move move;
        QDeadlineTimer deadline;
    };
    QList<Pending> m_pending;
    QTimer m_pendingTimer;
};
