// SPDX-License-Identifier: GPL-3.0-or-later

#include "ignorelist.h"

#include <algorithm>

namespace
{
constexpr auto ConfigKey = "IgnoreList";
}

QStringList IgnoreList::defaultEntries()
{
    return {QStringLiteral(".directory"), QStringLiteral("desktop.ini")};
}

IgnoreList IgnoreList::fromConfig(const KConfigGroup &group)
{
    return IgnoreList(group.hasKey(ConfigKey) ? group.readEntry(ConfigKey, QStringList()) : defaultEntries());
}

IgnoreList::IgnoreList(const QStringList &entries)
{
    for (const QString &raw : entries) {
        const QString entry = raw.trimmed();
        if (entry.isEmpty() || entry.size() > MaxEntryLength || entry.contains(u'/')) {
            continue;
        }
        m_entries.append(entry);
        m_patterns.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(entry), QRegularExpression::CaseInsensitiveOption));
        if (m_entries.size() == MaxEntries) {
            break;
        }
    }
}

QStringList IgnoreList::entries() const
{
    return m_entries;
}

bool IgnoreList::ignores(const QString &name) const
{
    return std::any_of(m_patterns.cbegin(), m_patterns.cend(), [&name](const QRegularExpression &pattern) {
        return pattern.match(name).hasMatch();
    });
}
