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
 * tab restoration.
 */
struct Options {
    /// Single or dual panes, stacking, splitter sizes, second-pane placement
    /// and detached-pane geometry.
    bool paneArrangement = true;
    /// Each pane's view mode and Yazi mode, with its preview column.
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
    bool operator==(const Options &other) const = default;
};

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
