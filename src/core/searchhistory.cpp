// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchhistory.h"

namespace
{
const char QueriesKey[] = "Queries";

QStringList normalized(const QStringList &queries)
{
    QStringList result;
    for (const QString &query : queries) {
        const QString trimmed = query.trimmed();
        if (!trimmed.isEmpty() && !result.contains(trimmed)) {
            result.append(trimmed);
        }
        if (result.size() >= SearchHistory::MaximumEntries) {
            break;
        }
    }
    return result;
}
}

namespace SearchHistory
{
bool retained(const KConfigGroup &search)
{
    return search.readEntry("RetainSearchHistory", true);
}

QStringList load(const KConfigGroup &history)
{
    return normalized(history.readEntry(QueriesKey, QStringList()));
}

QStringList added(const QStringList &history, const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return normalized(history);
    }
    QStringList result = history;
    result.prepend(trimmed);
    return normalized(result);
}

void save(KConfigGroup &history, const QStringList &queries)
{
    const QStringList entries = normalized(queries);
    if (entries.isEmpty()) {
        history.deleteEntry(QueriesKey);
    } else {
        history.writeEntry(QueriesKey, entries);
    }
}
}
