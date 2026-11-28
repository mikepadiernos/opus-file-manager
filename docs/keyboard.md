# Keyboard shortcuts

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M31 Keyboard shortcut settings](https://img.shields.io/badge/roadmap-M31%20Keyboard%20shortcut%20settings-8957e5?style=flat-square)](ROADMAP.md#m31-keyboard-shortcut-settings)
[![Caveats: Keyboard shortcuts](https://img.shields.io/badge/caveats-Keyboard%20shortcuts-bf8700?style=flat-square)](CAVEATS.md#keyboard-shortcuts)

![Milestone M31](https://img.shields.io/badge/milestone-M31-8957e5?style=flat-square)
![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square)

Plasma Opus uses KDE's own shortcut editor, **Settings › Configure Keyboard
Shortcuts…**, for every window action. **Preferences › Keyboard** (M31) adds a
summary around it: which shortcuts you have changed, which ones get in each
other's way, a reset to the defaults, and the keys that the editor doesn't
show, such as pressing <kbd>Shift</kbd> twice to search, the Yazi mode key
presets and the global shortcuts.

## Contents

- [Overview](#overview)
- [Changing shortcuts](#changing-shortcuts)
- [The Shortcuts tab](#the-shortcuts-tab)
- [Where shortcuts apply](#where-shortcuts-apply)
- [Search, Yazi mode and global shortcuts](#search-yazi-mode-and-global-shortcuts)
- [Global shortcut status](#global-shortcut-status)
- [Configuration reference](#configuration-reference)
- [Accessibility](#accessibility)
- [Known limitations](#known-limitations)

## Overview

| Where                                        | What it does                                                               |
| -------------------------------------------- | -------------------------------------------------------------------------- |
| **Settings › Configure Keyboard Shortcuts…** | KDE's editor: change, add or remove any action's shortcuts                 |
| **Preferences › Keyboard › Shortcuts**       | Customized shortcuts, conflicts, reset to defaults, search and global keys |
| **Preferences › Yazi Mode › Keyboard**       | The Yazi mode key presets and the window shortcuts each one takes          |
| **Preferences › KDE Plasma › Shortcuts**     | The global shortcut that shows or hides Plasma Opus                        |
| **Preferences › KDE Plasma › KWin**          | The drop-down window's global shortcut                                     |
| **Preferences › Tools › Search**             | Whether pressing <kbd>Shift</kbd> twice opens search                       |

New features never take a key that an existing action already uses. Actions
added later, such as the Yazi mode actions, have no default shortcut unless
their key is unused everywhere it could apply.

## Changing shortcuts

**Configure Keyboard Shortcuts…** is on the **Settings** menu and on the
**Keyboard › Shortcuts** tab. It is KDE's standard dialog, the same as in
Dolphin and Kate:

- **Key combinations.** A shortcut is keys pressed together, such as
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd>.
- **Key sequences.** A shortcut can also be up to four combinations pressed
  one after another, such as <kbd>Ctrl</kbd>+<kbd>K</kbd>,
  <kbd>Ctrl</kbd>+<kbd>C</kbd>. The dialog records a sequence when keys are
  pressed in turn while it is listening.
- **Alternates.** Each action has a shortcut and an alternate one; either
  runs it.
- **Conflicts.** The dialog warns before giving a shortcut that another action
  already has, and offers to take it from that action.
- **Defaults.** It restores one action's default shortcuts, or every action's.

Shortcuts take effect as soon as the dialog saves them, in every open Plasma
Opus window, and are kept in Plasma Opus's configuration file (see
[Window shortcuts](#window-shortcuts)). Preferences can stay open meanwhile:
the **Keyboard** and **Yazi Mode** pages update when the shortcuts are saved.

## The Shortcuts tab

**Preferences › Keyboard › Shortcuts** has these parts:

- **Customized Shortcuts** lists each action whose shortcuts differ from its
  defaults, with its current shortcuts, its defaults and where it applies
  (**Applies in**, see [Where shortcuts apply](#where-shortcuts-apply)).
  When <kbd>Tab</kbd>
  doesn't switch panes (**Preferences › Interface › Folders and Tabs**),
  **Switch Pane** without <kbd>Tab</kbd> isn't counted as a change.
- **Conflicts** lists shortcuts that get in each other's way:
  - two actions with the same shortcut: Qt runs neither of them;
  - a shortcut that begins a longer key sequence, such as
    <kbd>Ctrl</kbd>+<kbd>K</kbd> and <kbd>Ctrl</kbd>+<kbd>K</kbd>,
    <kbd>Ctrl</kbd>+<kbd>C</kbd>: the shorter one runs as soon as it is
    pressed, so the longer one can't be typed, even when both belong to one
    action;
  - a global shortcut that is also a window shortcut: KDE's global shortcut
    service takes the key first;
  - a changed window shortcut that begins with a key that text fields, file
    views or the folder tree keep for themselves, or with a key used to move
    around without a mouse, such as <kbd>Escape</kbd> or <kbd>F10</kbd>.

  Shortcuts that can't be pressed in the same place, such as a window
  shortcut and an image gallery key, don't conflict.

  With the default shortcuts, the list is empty.

- **Configure Keyboard Shortcuts…** opens KDE's editor.
- **Reset All Shortcuts…** asks, then gives every action its default
  shortcuts and saves them right away. It is available only while some
  shortcuts are customized. Global shortcuts are kept; change those where
  they are configured.

Preferences opened without a Plasma Opus window, as tests do, lists nothing
and explains that a window is needed.

## Where shortcuts apply

Each shortcut applies in one context, and only shortcuts whose contexts
overlap can conflict:

| Context              | Shortcuts                                    | Overlaps                        |
| -------------------- | -------------------------------------------- | ------------------------------- |
| Window               | Every action in Configure Keyboard Shortcuts | All but the image gallery       |
| File views and lists | Keys that a pane's view or a list keeps      | Window                          |
| Folder tree          | Keys that the folder tree keeps              | Window                          |
| Text fields          | Keys that a text field keeps                 | Window                          |
| Image gallery        | The gallery's own keys, in its own window    | Image gallery                   |
| Global               | KDE's global shortcuts                       | Everything, in every app        |

Window shortcuts work wherever the focus is in the window, but the focused
widget keeps the keys it needs:

- **Text fields** keep the keys used for typing and editing: letters, digits
  and symbols with or without <kbd>Shift</kbd>, <kbd>Space</kbd>,
  <kbd>Backspace</kbd>, <kbd>Delete</kbd>, <kbd>Insert</kbd>, the arrow keys,
  <kbd>Home</kbd>, <kbd>End</kbd>, <kbd>Page Up</kbd>, <kbd>Page Down</kbd>,
  <kbd>Enter</kbd>, <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd>, and
  <kbd>Ctrl</kbd> with <kbd>A</kbd>, <kbd>C</kbd>, <kbd>V</kbd>,
  <kbd>X</kbd>, <kbd>Z</kbd>, <kbd>Y</kbd>, the arrows, <kbd>Home</kbd>,
  <kbd>End</kbd>, <kbd>Backspace</kbd> or <kbd>Delete</kbd>. This covers the
  location bar, the filter and search fields, inline rename editors, spin
  boxes and editable combo boxes. So <kbd>Tab</kbd> moves the focus out of a
  text field even when it switches panes elsewhere.
- **File views, other lists and the folder tree** keep the arrow keys,
  <kbd>Home</kbd>, <kbd>End</kbd>, <kbd>Page Up</kbd> and
  <kbd>Page Down</kbd>, alone or with <kbd>Shift</kbd>, to move and select.
- **Other keys** still run window shortcuts there, such as
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd> in a text field or
  <kbd>F5</kbd> in a file view.
- **Read-only text, combo boxes without a text field and the embedded
  terminal** are left as they are; the terminal decides which keys it keeps.
- **Dialogs and other windows**, including the image gallery, have their own
  keys and don't run window shortcuts.

The default shortcuts are chosen to fit: some, such as <kbd>Space</kbd> for
**Open Gallery** or <kbd>Backspace</kbd> for **Up**, still run while a file
view has the focus, and the **Conflicts** list doesn't report them. Only
shortcuts you change are checked against the keys widgets keep.

The image gallery's keys are fixed and apply only in its window:

| Key                                                     | Action                   |
| ------------------------------------------------------- | ------------------------ |
| <kbd>Left</kbd>, <kbd>Right</kbd>                       | Previous or next image   |
| <kbd>Home</kbd>, <kbd>End</kbd>                         | First or last image      |
| <kbd>+</kbd>, <kbd>-</kbd>, <kbd>0</kbd>                | Zoom in, out or to fit   |
| <kbd>Space</kbd>                                        | Start or stop slideshow  |
| <kbd>I</kbd>                                            | Show or hide information |
| <kbd>R</kbd>                                            | Rotate right             |
| <kbd>F</kbd>                                            | Full screen              |
| <kbd>Ctrl</kbd>+<kbd>C</kbd>                            | Copy image               |

In right-to-left languages, <kbd>Left</kbd> and <kbd>Right</kbd> swap.

## Search, Yazi mode and global shortcuts

The **Search, Yazi Mode and Global Shortcuts** group lists keys that are
configured elsewhere, each with a button that shows its settings:

| Row              | What it shows                                   | Button                       |
| ---------------- | ----------------------------------------------- | ---------------------------- |
| Search           | Search Current Folder's keys and Shift twice    | **Search Settings**          |
| Yazi mode        | Window shortcuts the chosen key preset takes    | **Yazi Mode Keys**           |
| Global shortcuts | Show or Hide Plasma Opus and the drop-down keys | **Global Shortcut Settings** |

- **Search** shows the **Search Current Folder…** shortcut (`search_folder`,
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd> by default) and whether pressing
  <kbd>Shift</kbd> twice searches, is off or is unavailable.
- **Yazi mode** shows how many window shortcuts the key preset chosen in
  Preferences takes while a Yazi mode column has the focus.
- **Global shortcuts** shows the keys of **Show or Hide Plasma Opus** and of
  the drop-down window, or that KDE's global shortcut service isn't running.
  When **Show or Hide Plasma Opus** is on without a key, it says that KDE
  didn't assign one, and it adds the limitation of a Wayland session that
  isn't Plasma (see [Global shortcut status](#global-shortcut-status)).

- Pressing <kbd>Shift</kbd> twice is a gesture rather than a shortcut, so
  Configure Keyboard Shortcuts doesn't list it. The search panel's own keys
  (<kbd>Down</kbd> to reach the results, <kbd>Escape</kbd> to return) are
  fixed.
- The Yazi mode key presets are described in
  [Yazi mode › Keyboard](yazi-mode.md#keyboard). Their keys apply only while
  a Yazi mode column has the keyboard focus; the **Yazi Mode › Keyboard** tab
  lists each window shortcut a preset takes. The Yazi mode actions themselves
  are ordinary actions without default shortcuts, so they can be given
  shortcuts in Configure Keyboard Shortcuts.
- The global shortcuts are registered with KDE's global shortcut service
  (KGlobalAccel), which System Settings › Shortcuts also edits.

## Global shortcut status

Both global shortcuts are optional. Plasma Opus registers them only through
KDE's global shortcut service, KGlobalAccel, and never grabs keys itself.
**Show or Hide Plasma Opus** is set on **Preferences › KDE Plasma ›
Shortcuts** and the drop-down window's shortcut on **Preferences › KDE Plasma
› KWin**. Each has a status line below its key:

| Status                       | When                                         |
| ---------------------------- | -------------------------------------------- |
| Off                          | The shortcut is turned off                   |
| The service isn't running    | KGlobalAccel isn't running and can't start   |
| KDE didn't accept the key    | Another application has the chosen key       |
| No key combination           | KDE left the shortcut without a key          |
| Press the key                | The shortcut works with the key KDE assigned |
| Wayland session isn't Plasma | Added when the desktop may not pass keys on  |

- KDE assigns a key only when no other application uses it. After **Apply**
  or **OK**, the key field shows the key KDE accepted, which is empty when it
  refused the chosen one, and the status names the refused key.
- On Wayland, only the compositor sees keys pressed in other windows. Plasma's
  KWin passes them to KGlobalAccel; other desktops may not. Plasma Opus can't
  tell whether they do, so in a Wayland session whose `XDG_CURRENT_DESKTOP`
  doesn't include `KDE` the status says the shortcut works only if the
  desktop supports it. On X11, KGlobalAccel grabs the keys itself.

## Configuration reference

### Window shortcuts

Plasma Opus keeps window shortcuts in the `Shortcuts` group of
`~/.config/opus-file-managerrc`, its configuration file:

```ini
[Shortcuts]
Version=1
open_gallery=F9; Ctrl+K, G
search_folder=none
```

- Each action whose shortcuts differ from its defaults has an entry named
  after the action: its shortcuts in Qt's portable text, primary first,
  separated by a semicolon and a space, or `none` for no shortcut. This is the
  format of KDE's `KActionCollection`.
- Actions without an entry have their defaults, so a changed default reaches
  every action you haven't changed.
- An entry that can't be read, such as one with a misspelt key, is ignored,
  and the action keeps its defaults.
- `Version` marks the format; it isn't an action.
- **Configure Keyboard Shortcuts…** and **Reset All Shortcuts…** save here,
  and every open window takes the saved shortcuts at once. Settings backups
  include the group.
- KDE's editor itself writes the shortcuts into the window's layout file,
  `~/.local/share/kxmlgui5/opus-file-manager/opus-file-managerui.rc`, as
  `shortcut` attributes. Plasma Opus moves them into the group and removes
  them from the file each time the editor saves and whenever it starts, so
  the shortcuts of earlier versions, which were kept only in that file, are
  migrated too. The rest of the file, the customized menus and toolbars,
  stays.
- KDE's own actions that have no layout file save their shortcuts into the
  same group in the same format; Plasma Opus leaves those entries alone.
- Global shortcuts aren't stored here: KGlobalAccel keeps them in
  `~/.config/kglobalshortcutsrc`.

### Related entries

| Entry               | Group      | Type | Default    | Set in                           |
| ------------------- | ---------- | ---- | ---------- | -------------------------------- |
| `DoubleShiftSearch` | `Search`   | Bool | `true`     | **Tools › Search**               |
| `YaziKeyPreset`     | `YaziMode` | Enum | `Standard` | **Yazi Mode › Keyboard**         |
| `GlobalShortcut`    | `Tray`     | Bool | `true`     | **KDE Plasma › Shortcuts**       |
| `TabSwitchesPanes`  | `Panes`    | Bool | `true`     | **Interface › Folders and Tabs** |

The **Set in** column names pages of Preferences.

## Accessibility

- The customized shortcuts and the conflicts have accessible names, and every
  button has a text label.
- Every row of the tab is plain text that a screen reader reads as shown.
- Window shortcuts never take <kbd>Tab</kbd> or <kbd>Shift</kbd>+<kbd>Tab</kbd>
  from a text field, or the arrow keys from a list, so the keyboard focus can
  always move on. Changing a shortcut to <kbd>Escape</kbd>,
  <kbd>F10</kbd>, <kbd>Shift</kbd>+<kbd>F10</kbd>, <kbd>Menu</kbd> or
  <kbd>Tab</kbd> is reported as a conflict, because it no longer closes,
  opens menus or moves the focus.
- Nothing on the tab takes keys: it only shows and resets shortcuts.
  <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> move between its
  controls as in the rest of Preferences.

## Known limitations

The limitations of the keyboard shortcut settings, with recommendations, are
tracked in [Caveats: Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts)
([#771](https://github.com/mikepadiernos/plasma-opus/issues/771)). The
milestone plan is in the roadmap under
[M31: Keyboard shortcut settings](ROADMAP.md#m31-keyboard-shortcut-settings).
