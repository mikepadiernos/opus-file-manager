# Yazi mode

M25 adds Yazi mode, an optional way to browse a pane in columns inspired by
[Yazi](https://yazi-rs.github.io/docs/quick-start/), a terminal file manager.
It is a native Qt and KDE layout built on Plasma Opus's own folder view, file
models, actions and KIO jobs.

## Overview

Yazi mode is per pane and off by default. A pane in Yazi mode shows:

1. a **parent column**: a read-only listing of the parent folder, with the
   current folder highlighted;
2. the **current column**: the pane's usual folder view, with the same
   selection, filter, sorting, actions, context menu and keyboard behaviour;
3. an optional **preview column** after it. It stays hidden for now; a later
   M25 feature fills it.

The other pane, the tab bar, the location bar, the filter bar and the status
bar are unchanged.

Yazi mode is not an embedded Yazi process. Plasma Opus doesn't need Yazi
installed, and it doesn't read Yazi's configuration, key maps, themes or
plugins.

The first M25 feature provides the toggle, the parent column and persistence.
Narrow-window adaptation, the preview, keyboard-first navigation and Vim-style
keys follow in later M25 features; see the
[roadmap](ROADMAP.md#m25-yazi-mode).

## Entering and leaving

- Choose **View › Yazi Mode** to turn the mode on or off in the active pane.
  The menu item is checked when the active pane is in Yazi mode and follows
  the active pane as focus moves between panes.
- The action has no default shortcut, so existing bindings don't change.
  Assign one in **Settings › Configure Keyboard Shortcuts** (action
  `toggle_yazi_mode`) or add it to a toolbar.
- While the mode is on, a **Yazi** label below the columns marks the pane. Its
  accessible name is "Yazi mode", so screen readers announce it too.

In the parent column:

- clicking a folder, or pressing Return on it, opens that folder in the
  current tab, like any other navigation;
- clicking a file opens the folder the parent column lists, with that file
  selected;
- hidden items follow the pane's **Show Hidden Files** setting;
- items are sorted by name, folders first. Renaming, dragging and context
  menus are not available there; use the current column for file actions.

At a root, such as `/` or `sftp://host/`, there is no parent, so the parent
column is empty.

## What is preserved

Entering or leaving Yazi mode moves the pane's existing folder view into or out
of the columns; it doesn't recreate the view or reload the folder. Leaving the
mode restores exactly what was there:

- both panes, and all of each pane's tabs, with their pins, locks and names;
- each tab's location and history;
- the selection, current item and scroll position;
- the name filter and its scope;
- sorting;
- the view mode (details, icons, compact or cards) and view settings such as
  icon size, previews and hidden files.

Yazi mode does not change the view mode. If a later feature applies a
list-like presentation in the mode, it will be temporary and never saved as
the pane's view mode.

## Persistence

- **Window sessions.** Each pane's mode is saved with the session and restored
  at the next start. It is the `YaziMode` entry in the pane's `LeftPane` or
  `RightPane` group of the session state; a missing entry means off. When
  **On startup** opens the home folder or fixed folders instead of restoring
  the last session, both panes start with the mode off.
- **Workspaces.** Saving a workspace layout records each pane's mode as the
  `yaziMode` boolean in that pane's session in the workspace catalogue.
  Restoring the layout restores the mode when **Preferences › Workspaces ›
  Layouts › Each pane's view mode and Yazi mode** is checked. Turning the
  mode on or off marks the active workspace's layout as modified.
- Layouts saved before Yazi mode existed have no `yaziMode` value and restore
  with the mode off. A `yaziMode` value that isn't a boolean makes the stored
  catalogue invalid, as with other malformed layout values.

Choosing whether a workspace includes Yazi mode, and Yazi-specific KConfig
options, come with later M25 features.
