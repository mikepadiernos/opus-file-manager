// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QString>

namespace WorkspaceRestore
{
/**
 * Which parts of a saved workspace layout are restored when switching. Each
 * part applies only when the workspace's behavior selection includes it:
 * window parts need the layout behavior, and pane and tab parts need pane and
 * tab restoration. Each pane's Yazi mode and columns follow the selection's
 * own Yazi mode behavior; see yaziParts().
 */
struct Options {
    /// Single or dual panes, stacking, splitter sizes, second-pane placement
    /// and detached-pane geometry.
    bool paneArrangement = true;
    /// Each pane's view mode. Workspaces saved before the Yazi mode behavior
    /// existed also restore each pane's Yazi mode with it.
    bool views = true;
    /// Main and pane toolbar entries.
    bool toolbars = true;
    /// Dock and toolbar positions, and the preview, editor and terminal panels.
    bool panels = true;
    /// Each pane's tabs, with their pins, locks and names.
    bool tabs = true;
    /// The current tab of each pane and the active pane.
    bool activeTabs = true;
    /// Each pane's name filter.
    bool filters = true;
    /// Each pane's Yazi mode, on or off.
    bool yaziMode = true;
    /// Each pane's Yazi columns: preview visibility and content, parent
    /// column and column ratios.
    bool yaziColumns = true;
    bool operator==(const Options &other) const = default;
};

/// The parts of each pane's Yazi state that switching restores.
struct YaziParts {
    /// Whether the pane's Yazi mode is on.
    bool mode = false;
    /// The pane's preview visibility and content, parent column and column ratios.
    bool columns = false;
    bool any() const
    {
        return mode || columns;
    }
    bool operator==(const YaziParts &other) const = default;
};

/**
 * Decides which parts of each pane's Yazi state switching to a workspace
 * restores. Nothing is restored unless the workspace's Yazi mode behavior
 * applies; then the Yazi options choose the parts. Parts that are not
 * restored keep the pane's current state. The workspace's saved layout and
 * behavior selection are never changed.
 * @param selection the workspace's behavior selection
 * @param options the restore options
 * @returns the parts to restore
 */
YaziParts yaziParts(const WorkspaceCatalog::BehaviorSelection &selection, const Options &options);

/**
 * Reads the restore options from the Settings singleton.
 * @returns the current restore options
 */
Options fromSettings();

/**
 * Reads the behavior selection given to new workspaces from the Settings
 * singleton. Duplicated workspaces keep their source's selection instead.
 * @returns the configured default behavior selection
 */
WorkspaceCatalog::BehaviorSelection defaultBehaviorSelection();

/**
 * Builds the layout to apply when switching to a workspace. Parts that are
 * not restored keep their current state.
 * @param saved the workspace's saved layout
 * @param current the window's current layout
 * @param selection the workspace's behavior selection
 * @param options which parts of the layout to restore
 * @param result receives the layout to apply; @p saved unchanged when
 *        everything is restored
 * @param error describes a missing layout or pane snapshot
 * @returns whether a layout could be built
 */
bool merge(const WorkspaceCatalog::Layout &saved,
           const WorkspaceCatalog::Layout &current,
           const WorkspaceCatalog::BehaviorSelection &selection,
           const Options &options,
           WorkspaceCatalog::Layout &result,
           QString &error);
}
