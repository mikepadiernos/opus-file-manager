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

The M25 features so far provide the toggle, the parent and preview columns,
narrow-pane adaptation, persistence, keyboard-first navigation with optional
Vim-style keys, and selection and file actions through Plasma Opus's own
actions; see the [roadmap](ROADMAP.md#m25-yazi-mode).

## Entering and leaving

- Choose **View › Yazi Mode** to turn the mode on or off in the active pane.
  The menu item is checked when the active pane is in Yazi mode and follows
  the active pane as focus moves between panes.
- The action has no default shortcut, so existing bindings don't change.
  Assign one in **Settings › Configure Keyboard Shortcuts** (action
  `toggle_yazi_mode`) or add it to a toolbar.
- While the mode is on, a label below the columns marks the pane: **Yazi**, or
  **Yazi · Vim** with Vim-style keys. Its accessible name is "Yazi mode" or
  "Yazi mode with Vim-style keys", so screen readers announce it too. While a
  key sequence is being typed, the label shows the keys so far, such as
  **Yazi · Vim · g…**; see [Keyboard](#keyboard).

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

## Keyboard

Yazi mode can be browsed from the keyboard alone. Its keys come in two
presets:

- **Standard**, the default, uses the arrow keys and the usual navigation
  keys;
- **Vim-style** adds Vim-like letters, multi-key sequences such as `gg` and
  counts such as `5j` to the standard keys.

Choose **View › Vim-Style Keys in Yazi Mode** (action `toggle_yazi_vim_keys`,
no default shortcut) to switch between them. The choice applies to both panes
and is saved as the `YaziKeyPreset` entry (`Standard` or `Vim`) in the
`[YaziMode]` group of the configuration. A preset chooser in Preferences comes
with M26.

### Standard keys

| Key                  | Command                                  |
| -------------------- | ---------------------------------------- |
| Up, Down             | Move the focus up or down                |
| Home, End            | Move the focus to the first or last item |
| Page Up, Page Down   | Move the focus a page up or down         |
| Left                 | Open the parent folder                   |
| Right, Return, Enter | Open the focused folder or file          |
| Ctrl+Space           | Select or deselect the focused item      |

In right-to-left layouts, Left and Right swap, as they do in the folder view.

The standard keys add no letters for file actions. The window's own
shortcuts, such as Ctrl+C, Ctrl+X, Ctrl+V, F2, Delete, F5, F6, Ctrl+Z and
Ctrl+A, are not bound by the preset and keep working in the columns. Space
alone stays **Open Gallery**.

### Vim-style keys

All the standard keys, plus:

| Keys             | Command                                             |
| ---------------- | --------------------------------------------------- |
| `j`, `k`         | Move the focus down or up                           |
| `gg`, `G`        | Move the focus to the first or last item            |
| Ctrl+D, Ctrl+U   | Move the focus half a page down or up               |
| `h`              | Open the parent folder                              |
| `l`              | Open the focused folder or file                     |
| `H`, `L`         | Go back or forward in the tab's history             |
| `gh`             | Open the home folder (**Go › Home**'s folder)       |
| `gp`, `gc`, `gv` | Focus the parent, current or preview column         |
| `zp`             | Show or hide the preview column                     |
| `q`              | Leave Yazi mode                                     |
| Space            | Select or deselect the focused item, then move down |
| `v`              | Start or end range selection                        |
| `V`, Ctrl+A      | Select all                                          |
| Escape           | Clear the selection                                 |
| `yy`             | Copy the items to the clipboard                     |
| `x`              | Cut the items to the clipboard                      |
| `p`              | Paste into the current folder                       |
| `r`              | Rename the items                                    |
| `dd`             | Move the items to the Trash                         |
| `a`              | Create a new folder or file                         |
| `o`              | Open the items with another application             |
| `c`, `m`         | Copy or move the items to the other pane            |
| `u`              | Undo the last file operation                        |
| `.`              | Show or hide hidden files                           |

`G`, `H`, `L` and `V` are Shift with the letter. A number typed first repeats
the movement commands and Space: `5j` moves five items down, `2` Ctrl+D a
whole page and `3` Space selects or deselects three items. Other commands
ignore the number. Counts stop at 999. Ctrl+Space, from the standard keys,
selects or deselects without moving.

"The items" are the selected items, or the focused item when nothing is
selected; see [Selection and file actions](#selection-and-file-actions).

### How the commands behave

- Movement acts on the focused column: the parent column when it has the
  focus, otherwise the current column. It moves the focus and selection
  exactly as the column's own arrow keys would, so in icon and card views
  Up and Down move by a row of items.
- Opening the parent folder focuses and selects the folder just left, and the
  parent column highlights the new current folder.
- Opening an item uses the same path as activating it: folders open in the
  pane and files open as they normally would. In the parent column, opening
  the current folder moves the focus to the current column.
- Focusing a hidden parent or preview column shows a status message instead.

### When the keys apply

The keys apply only while the pane is in Yazi mode and the keyboard focus is
in one of its columns: the parent column, the current column's items or the
preview. Elsewhere, and in panes outside Yazi mode, keys behave as they
always do.

- Typing is never intercepted. Text fields, such as the filter bar, the
  location bar, search fields and an inline rename editor, spin boxes and
  combo boxes keep every key, and so do the terminal and editor panels.
- Keys the preset doesn't bind are let through untouched, so the views' own
  keys, type-ahead filtering and window shortcuts keep working. With
  Vim-style keys, the bound letters are taken in the columns, but other
  letters still start type-ahead.
- Keys the preset binds take precedence over window shortcuts while the focus
  is in the columns. With Vim-style keys, Ctrl+D moves half a page there
  rather than adding a favourite; **Go › Add to Favorites** still works from
  the menu and elsewhere in the window.
- With Vim-style keys, digits 1 to 9 start a count, so type-ahead can't begin
  with them in the columns. 0 is only part of a count after another digit.
- With Vim-style keys, Space selects rather than opening the gallery in the
  columns; **Go › Open Gallery** and its shortcut still work elsewhere.

### Key sequences

- After the first key of a sequence such as `g`, the mode waits for the next
  key and the indicator shows the keys so far, such as **Yazi · Vim · g…**,
  with a matching accessible name.
- A pending sequence or count is cancelled after about one second without a
  key, when the focus moves, or with Escape. Escape cancels only; it doesn't
  leave the mode and is otherwise left to the columns, so Escape in the
  preview still returns to the current column.
- A key that doesn't continue the sequence cancels it and then counts on its
  own, so `g` followed by `j` moves down.
- Modifier keys pressed on their own, such as Shift before `G`, don't break a
  sequence.

### Shortcuts

The multi-key sequences and counts are Yazi mode's own and are not listed in
**Settings › Configure Keyboard Shortcuts**. Commands that make sense on their
own are also actions, with no default shortcuts, so they can be bound there or
added to a toolbar:

| Action                      | Menu                                         |
| --------------------------- | -------------------------------------------- |
| `toggle_yazi_mode`          | **View › Yazi Mode**                         |
| `toggle_yazi_preview`       | **View › Yazi Preview**                      |
| `toggle_yazi_vim_keys`      | **View › Vim-Style Keys in Yazi Mode**       |
| `yazi_focus_parent_column`  | **Go › Yazi Columns › Focus Parent Column**  |
| `yazi_focus_current_column` | **Go › Yazi Columns › Focus Current Column** |
| `yazi_focus_preview`        | **Go › Yazi Columns › Focus Preview Column** |
| `yazi_toggle_selection`     | **Edit › Toggle Selection of Focused Item**  |
| `yazi_visual_selection`     | **Edit › Range Selection**                   |

The column and selection actions are enabled only while the active pane is in
Yazi mode. The file keys use the window's existing actions, which already
have their own shortcuts. The standard and Vim-style keys can't be changed
yet; M26 adds a preset chooser and a review of conflicts with other
shortcuts.

## Selection and file actions

Yazi mode selects and acts on files through Plasma Opus's own selection and
actions, so the results are the same as from the menus, the context menu and
the window's shortcuts.

### Selection

- File actions use the **selected items**. When nothing is selected, they use
  the **focused item**, which is selected first so the action, and what it
  shows afterwards, apply to it.
- Selecting with Space or Ctrl+Space **marks** items. While items are marked,
  moving the focus keeps the marks instead of selecting the focused item, so
  items far apart can be marked one at a time. Marking stops when the
  selection is cleared or only the focused item is left selected.
- `v` starts range selection at the focused item. Moving then selects every
  item between that item and the focus, added to any items already marked.
  `v` again ends it and keeps the range marked. Space, select all, a click or
  another change to the selection also ends it.
- Escape clears the selection, as it does in the folder view. It does so only
  when no key sequence or count is pending; otherwise it cancels that first.
- While items are marked, or more than one is selected, the status row below
  the columns shows how many, such as **3 selected**, or **Visual · 3
  selected** during range selection.
- Selection and the file actions on items work in the current column. From the
  parent column, they show a status message instead of acting on the current
  column's items.
- Leaving the folder, or turning Yazi mode off, ends marking and range
  selection. The selection itself is kept as the folder view keeps it.

### File actions

Each Vim-style file key triggers the window's existing action, so it is
enabled, confirmed, queued and undone exactly as that action is:

| Keys     | Action                                                                  |
| -------- | ----------------------------------------------------------------------- |
| `yy`     | **Edit › Copy**                                                         |
| `x`      | **Edit › Cut**                                                          |
| `p`      | **Edit › Paste**, into the folder the pane shows                        |
| `r`      | **Rename…** (F2), with its inline rename, rename dialog or batch rename |
| `dd`     | **Move to Trash**                                                       |
| `a`      | The **Create New** menu, opened at the focused item                     |
| `o`      | An **Open With** menu for the items, opened at the focus                |
| `c`, `m` | **Copy to Other Pane** (F5) and **Move to Other Pane** (F6)             |
| `u`      | **Edit › Undo**                                                         |
| `.`      | **Show Hidden Files**                                                   |

When the action isn't available, such as pasting an empty clipboard or
moving to the Trash outside a local folder, a status message says so and
nothing happens.

### Safety

- **No permanent deletion.** No preset binds permanent deletion. `dd` always
  moves to the Trash, and `D` is not bound. **Delete Permanently** stays on
  the window's own shortcut, Shift+Delete, and its menu item, with its usual
  confirmation. The key engine has no command for permanent deletion, and a
  test checks that none of the preset keys can reach it.
- **Confirmations are kept.** Moving to the Trash asks first when KIO is set
  to confirm trashing, as it does from the menu.
- **Conflicts are always handled.** Pasting goes through KIO, which asks what
  to do when an item already exists. Copying and moving to the other pane go
  through the transfer queue and its **conflict policy**, as F5 and F6 do; the
  default policy asks.
- **Undo is available.** Copies, moves, renames, new folders and trashing are
  recorded for **Edit › Undo**, from `u`, Ctrl+Z or the menu. Undoing a copy
  deletes the copies, so KIO asks first.

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
