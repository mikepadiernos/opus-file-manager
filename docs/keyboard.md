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
- [Search, Yazi mode and global shortcuts](#search-yazi-mode-and-global-shortcuts)
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

Shortcuts take effect as soon as the dialog saves them. Preferences can stay
open meanwhile: the **Keyboard** and **Yazi Mode** pages update when the
shortcuts are saved.

## The Shortcuts tab

**Preferences › Keyboard › Shortcuts** has these parts:

- **Customized Shortcuts** lists each action whose shortcuts differ from its
  defaults, with its current shortcuts and its defaults. When <kbd>Tab</kbd>
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
    service takes the key first.

  With the default shortcuts, the list is empty.

- **Configure Keyboard Shortcuts…** opens KDE's editor.
- **Reset All Shortcuts…** asks, then gives every action its default
  shortcuts and saves them right away. It is available only while some
  shortcuts are customized. Global shortcuts are kept; change those where
  they are configured.

Preferences opened without a Plasma Opus window, as tests do, lists nothing
and explains that a window is needed.

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

## Configuration reference

### Window shortcuts

KDE's editor saves window shortcuts with the window's menus and toolbars, in
the `ActionProperties` element of
`~/.local/share/kxmlgui5/opus-file-manager/opus-file-managerui.rc`. Each
action whose shortcuts differ from its defaults has a `shortcut` attribute:
its shortcuts in Qt's portable text, separated by a semicolon and a space, or
an empty value for none. **Reset All Shortcuts…** writes the same file.

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
- Nothing on the tab takes keys: it only shows and resets shortcuts.
  <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> move between its
  controls as in the rest of Preferences.

## Known limitations

The limitations of the keyboard shortcut settings, with recommendations, are
tracked in [Caveats: Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts)
([#771](https://github.com/mikepadiernos/plasma-opus/issues/771)). The
milestone plan is in the roadmap under
[M31: Keyboard shortcut settings](ROADMAP.md#m31-keyboard-shortcut-settings).
