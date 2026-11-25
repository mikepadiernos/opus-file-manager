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
3. an optional **preview column** after it, showing the focused item.

The other pane, the tab bar, the location bar, the filter bar and the status
bar are unchanged.

Yazi mode is not an embedded Yazi process. Plasma Opus doesn't need Yazi
installed, and it doesn't read Yazi's configuration, key maps, themes or
plugins.

The first M25 features provide the toggle, the parent and preview columns,
narrow-pane adaptation and persistence. Keyboard-first navigation and
Vim-style keys follow in later M25 features; see the
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

The parent column uses the current column's font, so it follows the pane's
list font settings. It keeps the current folder highlighted and scrolled into
view after navigation, when it is shown and after a font change.

## Preview column

The preview column shows the **focused item** of the current column: its
current index, which moves with the keyboard even when the selection doesn't.

- It reuses the preview panel's previews: images, media, text and the other
  KIO previews, with the same size limits. Its own controls (close,
  placement, previous, next, gallery and open) are hidden, since the current
  column already provides navigation.
- A focused folder shows a summary: its name, type and modification date. Its
  contents are not listed.
- Updates wait 120 ms after the focus stops moving, so holding an arrow key
  doesn't start a preview for every item passed. A newer focus change cancels
  the pending update; the preview panel then discards results for the
  previous item and stops its remote downloads.
- With nothing focused, it shows "Nothing to preview.".
- While the column is hidden, it doesn't update. Hiding it clears the preview
  and stops any playing media.

To show or hide it:

- Choose **View › Yazi Preview** (action `toggle_yazi_preview`). It applies to
  the active pane, is checked when that pane shows the preview, and is enabled
  only while the active pane is in Yazi mode. It has no default shortcut.
- **Preferences › Interface › Previews › Show the preview column in Yazi mode
  unless a pane was set otherwise** sets the default. It is the
  `YaziPreviewVisible` entry in the `[YaziMode]` group of the configuration,
  and is on by default. Panes whose preview was never toggled follow it,
  including when it changes; a pane toggled with **View › Yazi Preview** keeps
  its own choice.

## Narrow panes

The columns adapt to the pane's own width, never the window's, so the other
pane is never resized, hidden or replaced.

- The parent, current and preview columns share the width in the ratio 1 : 3
  : 2, with minimums of 100, 220 and 160 pixels.
- Below 480 pixels, plus the splitter handles, the preview is hidden first.
- Below 320 pixels, plus a handle, the parent column is hidden too, and the
  current column takes the whole pane.
- When the pane widens again, the hidden columns come back.

The column widths can still be adjusted by dragging the splitter handles. The
resulting proportions are kept as the pane is resized, and reused when hidden
columns return, but they are not saved between sessions.

If a column holding the keyboard focus is hidden, the focus moves to the
current column.

## Accessibility and keyboard

Each column has an accessible name and description: **Parent folder**,
**Current folder** and **Preview**. The preview's description names the
focused item, or says there is nothing to preview. The current column's
original names are restored when the mode is turned off.

- The columns are in the focus chain in order: parent, current, preview.
- By default, Tab switches panes (**Switch Pane**, with the
  `TabSwitchesPanes` setting), so Shift+Tab from the current column reaches
  the parent column. When that setting is off, Tab also moves from the
  current column to the preview.
- In the preview, Escape returns to the current column.
- Focus moves freely out of every column; none of them traps it.
- Focusing the parent column or the preview makes its pane the active pane.
- When columns are hidden in a narrow pane, the current column keeps all of
  its keyboard navigation.

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
  Layouts › Each pane's view mode, Yazi mode and Yazi preview** is checked.
  Turning the mode on or off marks the active workspace's layout as modified.
- Layouts saved before Yazi mode existed have no `yaziMode` value and restore
  with the mode off. A `yaziMode` value that isn't a boolean makes the stored
  catalogue invalid, as with other malformed layout values.
- **Preview visibility.** A pane's own preview choice is saved as the
  `YaziPreview` entry next to `YaziMode` in the session, and as the
  `yaziPreview` boolean in a workspace layout, restored with the same
  **Layouts** option. Both are left out when the pane follows the default, so
  older sessions and layouts follow **Show the preview column in Yazi mode**.
  Toggling the preview marks the active workspace's layout as modified. A
  `yaziPreview` value that isn't a boolean makes the catalogue invalid.

Choosing whether a workspace includes Yazi mode comes with a later M25
feature.
