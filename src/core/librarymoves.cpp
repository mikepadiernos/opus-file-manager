// SPDX-License-Identifier: GPL-3.0-or-later

#include "librarymoves.h"

#include "libraryscan.h"
#include "taglibrary.h"

#include <QDir>
#include <QFile>
#include <QHash>

#include <sys/stat.h>

bool LibraryMoves::isGone(const QString &path)
{
    struct stat data;
    return ::lstat(QFile::encodeName(path).constData(), &data) != 0;
}

namespace
{
QString rootOf(const QString &path, const QStringList &roots)
{
    for (const QString &root : roots) {
        if (path.startsWith(root + QLatin1Char('/'))) {
            return root;
        }
    }
    return {};
}

/// Whether the move to @p to happened, by identity or by the source being gone.
bool confirmed(const QString &path, const LibraryMoves::Move &move)
{
    struct stat data;
    if (move.inode) {
        return ::lstat(QFile::encodeName(path).constData(), &data) == 0 && quint64(data.st_dev) == move.device && quint64(data.st_ino) == move.inode;
    }
    return LibraryMoves::isGone(path) && !LibraryMoves::isGone(QDir::cleanPath(move.to));
}

struct Changes {
    QList<QPair<QString, QString>> moves;
    QStringList missing;
    QList<TagLibrary::Observation> observed;

    bool isEmpty() const
    {
        return moves.isEmpty() && missing.isEmpty() && observed.isEmpty();
    }
};
}

LibraryMoves::Outcome LibraryMoves::apply(const QList<Move> &moves, const QStringList &roots)
{
    QStringList cleanRoots;
    for (const QString &root : roots) {
        cleanRoots.append(QDir::cleanPath(root));
    }
    QHash<QString, Changes> byRoot;
    QStringList order;
    const auto changesFor = [&](const QString &root) -> Changes & {
        if (!byRoot.contains(root)) {
            order.append(root);
        }
        return byRoot[root];
    };
    for (const Move &move : moves) {
        const QString from = QDir::cleanPath(move.from);
        const QString to = QDir::cleanPath(move.to);
        if (from == to || cleanRoots.contains(from) || !confirmed(move.inode ? to : from, move)) {
            continue;
        }
        const QString fromRoot = rootOf(from, cleanRoots);
        const QString toRoot = rootOf(to, cleanRoots);
        TagLibrary::Observation observation;
        const bool observed = !toRoot.isEmpty() && LibraryScan::observe(toRoot, to, &observation);
        if (!fromRoot.isEmpty() && fromRoot == toRoot) {
            Changes &changes = changesFor(fromRoot);
            changes.moves.append({from.mid(fromRoot.size() + 1), to.mid(toRoot.size() + 1)});
            if (observed) {
                changes.observed.append(observation);
            }
            continue;
        }
        if (!fromRoot.isEmpty()) {
            changesFor(fromRoot).missing.append(from.mid(fromRoot.size() + 1));
        }
        if (observed) {
            changesFor(toRoot).observed.append(observation);
        }
    }
    Outcome outcome;
    for (const QString &root : std::as_const(order)) {
        const Changes &changes = byRoot.value(root);
        if (changes.isEmpty()) {
            continue;
        }
        QString error;
        bool changed = false;
        const bool saved = TagLibrary::update(
            root,
            [&](TagLibrary::Library &library, QString *) {
                changed = false;
                if (!changes.moves.isEmpty() && library.movePaths(changes.moves) > 0) {
                    changed = true;
                }
                for (const QString &path : changes.missing) {
                    if (library.markMissing(path) > 0) {
                        changed = true;
                    }
                }
                const TagLibrary::ScanChanges scan = library.applyScan(changes.observed, false);
                changed = changed || scan.added || scan.updated || scan.found;
                return changed;
            },
            &error);
        if (saved) {
            outcome.changed.append(root);
        } else if (changed) {
            outcome.errors.append(error);
        }
    }
    return outcome;
}
