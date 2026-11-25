// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QStringList>

/**
 * Keeps tag libraries in step with moves and renames Plasma Opus performed.
 * Only moves that are confirmed on disk are applied: the source must be gone
 * and the destination present. Nothing is guessed from names or sizes.
 */
namespace LibraryMoves
{
/// A move between absolute paths.
struct Move {
    QString from;
    QString to;
    /// When set, the move is confirmed by the destination having this
    /// identity, so renames that swap names can be confirmed. Otherwise it
    /// is confirmed by the source being gone and the destination present.
    quint64 device = 0;
    quint64 inode = 0;
};

struct Outcome {
    /// Roots of libraries whose documents changed.
    QStringList changed;
    QStringList errors;
};

/**
 * Applies @p moves to the libraries in @p roots. A move within one library
 * moves its entries. A move out of a library marks the entries missing,
 * because metadata isn't copied between libraries, and a move into a
 * library indexes the destination. Moving a library root itself is ignored.
 */
Outcome apply(const QList<Move> &moves, const QStringList &roots);

/// Whether @p path no longer exists, not even as a dangling symbolic link.
bool isGone(const QString &path);
}
