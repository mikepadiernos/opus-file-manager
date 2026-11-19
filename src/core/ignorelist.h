// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QRegularExpression>
#include <QStringList>

/**
 * Names that listings never show, such as .directory. Each entry is an
 * exact name or a glob pattern, matched ignoring case. Entries containing a
 * slash, longer than MaxEntryLength, or beyond the first MaxEntries are
 * dropped.
 */
class IgnoreList
{
public:
    static constexpr int MaxEntries = 100;
    static constexpr int MaxEntryLength = 80;

    /// The entries used when the config has none: .directory and desktop.ini.
    static QStringList defaultEntries();

    /**
     * Reads the "IgnoreList" entry of @p group.
     * @param group the config group holding the list
     * @returns the stored list, or the defaults when the entry is missing
     */
    static IgnoreList fromConfig(const KConfigGroup &group);

    IgnoreList() = default;

    /// @param entries names or glob patterns; invalid entries are dropped
    explicit IgnoreList(const QStringList &entries);

    QStringList entries() const;

    /**
     * Whether a listing hides an item.
     * @param name the item's file name
     * @returns true when any entry matches the whole name
     */
    bool ignores(const QString &name) const;

private:
    QStringList m_entries;
    QList<QRegularExpression> m_patterns;
};
