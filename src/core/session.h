// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

class KConfigGroup;

/**
 * Serialisable snapshot of the window: the tabs of each pane, which tab is
 * current in each, which pane has focus, whether both panes are shown, and
 * whether they are stacked top/bottom (left is the top pane) instead of side
 * by side. Stored in the KConfig state
 * file so it survives restarts without ending up in the user's settings.
 */
struct PaneSession {
    QList<QUrl> tabs;
    int currentTab = 0;
    /// The first pinnedCount tabs are pinned.
    int pinnedCount = 0;
    /// Indexes of locked tabs.
    QList<int> lockedTabs;
    /// One custom name per tab, empty for none; may be shorter than tabs.
    QStringList names;
    QString filterText;
    int filterScope = 0;

    bool operator==(const PaneSession &other) const = default;
};

struct WindowSession {
    PaneSession left;
    PaneSession right;
    int activePane = 0;
    bool dual = true;
    bool stacked = false;

    bool isEmpty() const;
    /// Drops invalid URLs, with their pin, lock and name, and clamps the
    /// indexes so the result is usable.
    WindowSession sanitized(const QUrl &fallback) const;

    bool operator==(const WindowSession &other) const = default;
};

namespace Session
{
void save(const WindowSession &session, KConfigGroup &group);
WindowSession load(const KConfigGroup &group);
}
