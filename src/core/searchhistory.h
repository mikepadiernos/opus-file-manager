// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QStringList>

/// Recent search queries, most recent first, kept in the application state configuration.
namespace SearchHistory
{
constexpr qsizetype MaximumEntries = 50;

/// Returns whether recent queries are kept, from the Search settings group.
bool retained(const KConfigGroup &search);

/// Reads the queries, dropping blank and repeated entries and keeping at most MaximumEntries.
QStringList load(const KConfigGroup &history);

/// Returns @p history with the trimmed @p query moved to the front; a blank query changes nothing.
QStringList added(const QStringList &history, const QString &query);

/// Writes @p queries, or removes the stored list when it is empty. The caller syncs the group.
void save(KConfigGroup &history, const QStringList &queries);
}
