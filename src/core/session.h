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
    /// -1 means no scope was saved; use the configured default.
    int filterScope = -1;
    /// -1 means no presentation was saved; use the pane's configured default.
    int locationPresentation = -1;
    /// FolderView::ViewMode: details, icons, compact or cards.
    int viewMode = 0;
    static constexpr int MaximumViewMode = 3;
    /// Whether the pane shows its folder view in Yazi mode's columns.
    bool yaziMode = false;
    /// Whether Yazi mode shows the preview column: 1 shown, 0 hidden, -1 not
    /// saved; use the YaziPreviewVisible default.
    int yaziPreview = -1;
    /// What Yazi mode's preview column shows, a YaziMode::PreviewContent
    /// value, or -1 when not saved; use the YaziPreviewContent default.
    int yaziPreviewContent = -1;
    /// Whether Yazi mode shows the parent column: 1 shown, 0 hidden, -1 not
    /// saved; use the YaziParentColumnVisible default.
    int yaziParentColumn = -1;
    /// The relative widths of Yazi mode's parent, current and preview columns
    /// as three positive values, or empty when not saved; use the
    /// YaziColumnRatios default.
    QList<int> yaziColumnRatios;

    bool operator==(const PaneSession &other) const = default;
};

struct WindowSession {
    PaneSession left;
    PaneSession right;
    int activePane = 0;
    bool dual = true;
    bool stacked = true;

    bool isEmpty() const;
    /// Drops invalid URLs, with their pin, lock and name, and clamps the
    /// indexes so the result is usable.
    WindowSession sanitized(const QUrl &fallback) const;

    bool operator==(const WindowSession &other) const = default;
};

namespace Session
{
void save(const WindowSession &session, KConfigGroup &group);
/// Panes without a saved view mode, such as sessions saved before view modes
/// were recorded, use @p defaultViewMode.
WindowSession load(const KConfigGroup &group, int defaultViewMode = 0);
}
