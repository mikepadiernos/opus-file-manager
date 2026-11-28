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
presets and the global shortcuts, and offers extra keys for moving between
panes, tabs and panels. **Help › Keyboard Map…** lists every
action with its shortcuts, searched by name or by pressing keys, and exports
them as a printable reference.

## Contents

- [Overview](#overview)
- [Changing shortcuts](#changing-shortcuts)
- [The Shortcuts tab](#the-shortcuts-tab)
- [Navigation](#navigation)
- [Where shortcuts apply](#where-shortcuts-apply)
- [Search, Yazi mode and global shortcuts](#search-yazi-mode-and-global-shortcuts)
- [Global shortcut status](#global-shortcut-status)
- [Keyboard map](#keyboard-map)
- [Configuration reference](#configuration-reference)
- [Accessibility](#accessibility)
- [Known limitations](#known-limitations)

## Overview

| Where                                        | What it does                                                               |
| -------------------------------------------- | -------------------------------------------------------------------------- |
| **Settings › Configure Keyboard Shortcuts…** | KDE's editor: change, add or remove any action's shortcuts                 |
| **Preferences › Keyboard › Shortcuts**       | Customized shortcuts, conflicts, reset to defaults, search and global keys |
| **Preferences › Keyboard › Navigation**      | Pane, tab, selection, filter and panel focus actions, and their extra keys |
| **Help › Keyboard Map…**                     | Every action and key, searched by name or keys, and a printable reference  |
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
  runs it. An action can have more alternates in the configuration file (see
  [Window shortcuts](#window-shortcuts)); the dialog shows and changes only
  the first two and keeps the others.
- **Conflicts.** The dialog warns before giving a shortcut that another action
  already has, and offers to take it from that action.
- **Defaults.** It restores one action's default shortcuts, or every action's.

The dialog lists every window action, including those on no menu or
toolbar, and their shortcuts work anywhere in the window. It lists each saved
command as **Saved Command:** and its name, so a command saved in **File ›
Scripts and Custom Commands…** or **Preferences › Tools › Automation** can
have its own shortcut. The shortcut opens the command with its preview, as
its toolbar button does, and never runs it straight away. The window where a
command is saved or deleted lists the change at once; deleting a command
removes its shortcut, and saving it under another name makes a new command
without one.

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
- **Keyboard Map…** opens **Help › Keyboard Map…** (see
  [Keyboard map](#keyboard-map)).

Preferences opened without a Plasma Opus window, as tests do, lists nothing
and explains that a window is needed.

## Navigation

**Preferences › Keyboard › Navigation** gathers the actions that move between
panes and tabs, change the selection or the filter, and move the keyboard
focus to the preview panel or the embedded terminal. The actions added for it
have no default shortcut, so they never take a key you already use. They are
on the menus and in **Configure Keyboard Shortcuts…**, where they can be given
any key.

| Area              | Action                      | Menu | Default                                         | Extra key                                       |
| ----------------- | --------------------------- | ---- | ----------------------------------------------- | ----------------------------------------------- |
| Panes             | **Switch Pane**             | Go   | <kbd>Tab</kbd>                                  |                                                 |
| Panes             | **Focus First Pane**        | Go   | None                                            | <kbd>Alt</kbd>+<kbd>1</kbd>                     |
| Panes             | **Focus Second Pane**       | Go   | None                                            | <kbd>Alt</kbd>+<kbd>2</kbd>                     |
| Tabs              | **Next Tab**                | Go   | <kbd>Ctrl</kbd>+<kbd>Tab</kbd>                  | <kbd>Ctrl</kbd>+<kbd>Page Down</kbd>            |
| Tabs              | **Previous Tab**            | Go   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Tab</kbd> | <kbd>Ctrl</kbd>+<kbd>Page Up</kbd>              |
| Tabs              | **Activate Last Tab**       | Go   | None                                            |                                                 |
| Selection         | **Select All**              | Edit | <kbd>Ctrl</kbd>+<kbd>A</kbd>                    |                                                 |
| Selection         | **Deselect All**            | Edit | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>   |
| Selection         | **Invert Selection**        | Edit | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd>   |
| Selection         | **Selection Mode**          | Edit | None                                            |                                                 |
| Filter            | **Filter…**                 | Edit | <kbd>Ctrl</kbd>+<kbd>F</kbd>                    |                                                 |
| Filter            | **Clear Filter**            | Edit | None                                            |                                                 |
| Preview Panel     | **Preview Panel**           | View | <kbd>F3</kbd>                                   |                                                 |
| Preview Panel     | **Focus Preview Panel**     | View | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F3</kbd>  |
| Embedded Terminal | **Show Embedded Terminal**  | View | None                                            |                                                 |
| Embedded Terminal | **Focus Embedded Terminal** | View | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F4</kbd>  |

The defaults are those of a new configuration; <kbd>Tab</kbd> switches panes
only while **Preferences › Interface › Folders and Tabs** says so.

- **Focus First Pane** and **Focus Second Pane** make the left or top pane,
  or the right or bottom one, active and move the keyboard focus to its file
  view, as **Switch Pane** does. A second pane in a tab or its own window is
  brought to the front.
- **Activate Last Tab** shows the active pane's last tab.
- **Deselect All** clears the active pane's selection. **Invert Selection**
  selects the items that aren't selected and deselects the others: every
  item of the folder, and in the details view those of expanded folders.
- **Clear Filter** shows every item of the active pane again and closes its
  filter bar; the status bar says when the pane has no filter to clear.
- **Focus Preview Panel** shows the preview panel and moves the keyboard
  focus into it, so the arrow keys and <kbd>Page Up</kbd> and
  <kbd>Page Down</kbd> scroll a preview. **Focus Embedded Terminal** shows
  the embedded terminal, starting a session if it has none, and moves the
  focus into it. Pressed again while the panel has the focus, either goes
  back to the active pane. The Yazi mode preview column has its own action,
  **Focus Preview Column**.

The tab lists each action under its area with its current shortcuts, its
extra key and the key's status:

| Status                                                        | Meaning                                                        |
| ------------------------------------------------------------- | -------------------------------------------------------------- |
| Off                                                           | The extra keys are off                                         |
| Off; “Action” uses it                                         | The extra keys are off, and another action has the key         |
| Added                                                         | The key is one of the action's defaults and runs it            |
| Added as a default, but this action has shortcuts of your own | Your own shortcuts for the action don't have the key           |
| Not added: “Action” uses it                                   | Another action or a global shortcut has the key                |

**Add the extra navigation shortcuts that no other action uses**, off by
default, adds each extra key as one more default shortcut of its action, so
Configure Keyboard Shortcuts shows it as a default and the keyboard map
lists it. A key is left out while another window action, including a saved
command or an action you gave the key, or a global shortcut that Plasma Opus
registered has it or a sequence that begins with it. The image gallery's keys
don't count, as the gallery is a window of its own. A key left out is added
as soon as it is free again, and every extra key is taken away when the
setting is turned off. Shortcuts of your own for these actions are kept
either way.

The extra keys are free with the default shortcuts and are taken by neither
Yazi mode key preset. <kbd>Ctrl</kbd>+<kbd>Page Down</kbd> and
<kbd>Ctrl</kbd>+<kbd>Page Up</kbd> are KDE's standard keys for the next and
previous tab, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd> is KDE's
standard **Deselect** and <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F4</kbd> is
the key Dolphin uses to focus its terminal. While you type in the embedded
terminal, it keeps keys with a single modifier, such as
<kbd>Alt</kbd>+<kbd>1</kbd> and <kbd>Ctrl</kbd>+<kbd>Page Down</kbd>; keys
with two, such as <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F4</kbd>, which
leaves it, still reach the window.

**Configure Keyboard Shortcuts…** and **Keyboard Map…** below the list open
KDE's editor and the keyboard map. The tab updates when shortcuts are saved
and when the setting is applied.

## Where shortcuts apply

Each shortcut applies in one context, and only shortcuts whose contexts
overlap can conflict:

| Context              | Shortcuts                                    | Overlaps                  |
| -------------------- | -------------------------------------------- | ------------------------- |
| Window               | Every action in Configure Keyboard Shortcuts | All but the image gallery |
| File views and lists | Keys that a pane's view or a list keeps      | Window                    |
| Folder tree          | Keys that the folder tree keeps              | Window                    |
| Text fields          | Keys that a text field keeps                 | Window                    |
| Image gallery        | The gallery's own keys, in its own window    | Image gallery             |
| Global               | KDE's global shortcuts                       | Everything, in every app  |

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

| Key                                      | Action                   |
| ---------------------------------------- | ------------------------ |
| <kbd>Left</kbd>, <kbd>Right</kbd>        | Previous or next image   |
| <kbd>Home</kbd>, <kbd>End</kbd>          | First or last image      |
| <kbd>+</kbd>, <kbd>-</kbd>, <kbd>0</kbd> | Zoom in, out or to fit   |
| <kbd>Space</kbd>                         | Start or stop slideshow  |
| <kbd>I</kbd>                             | Show or hide information |
| <kbd>R</kbd>                             | Rotate right             |
| <kbd>F</kbd>                             | Full screen              |
| <kbd>Ctrl</kbd>+<kbd>C</kbd>             | Copy image               |

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
  Configure Keyboard Shortcuts doesn't list it; the
  [keyboard map](#keyboard-map) does while it is on. The search panel's own keys
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

## Keyboard map

**Help › Keyboard Map…**, also opened by **Keyboard Map…** on
**Preferences › Keyboard › Shortcuts**, lists the keys of a Plasma Opus
window in one place. It has no default shortcut; give it one in Configure
Keyboard Shortcuts. Each window has one map, which stays open beside the
window; choosing it again brings it to the front.

The list has four columns:

| Column      | What it shows                                                             |
| ----------- | ------------------------------------------------------------------------- |
| Action      | The action's name as menus show it                                        |
| Shortcuts   | The shortcut, then each alternate, separated by semicolons, or **None**   |
| Applies in  | Where the keys work (see [Where shortcuts apply](#where-shortcuts-apply)) |
| Description | The action's tooltip, when it says more than the name                     |

It lists, in this order:

- every action of Configure Keyboard Shortcuts, sorted by name, including
  saved commands (**Saved Command:**) and every alternate;
- pressing <kbd>Shift</kbd> twice, while it opens search (see
  [Search](#search-yazi-mode-and-global-shortcuts));
- the image gallery's fixed keys (**Image gallery**);
- the commands of the Yazi mode key preset chosen on **Preferences › Yazi
  Mode › Keyboard** (**Yazi mode columns**), each with every key that runs
  it, written as [Yazi mode › Keyboard](yazi-mode.md#keyboard) writes them,
  such as `G` for <kbd>Shift</kbd>+<kbd>G</kbd>;
- the global shortcuts KDE's global shortcut service has registered
  (**Global**).

To find an action:

- **Search** finds actions whose name, internal name (such as
  `search_folder`), description, shortcuts or **Applies in** contain every
  word typed, in any order and ignoring case. Shortcuts can be typed as
  shown or as `Ctrl+Alt+F`.
- **Find by key**: click the field, then press a key combination or a
  sequence of up to four. The list shows the actions it runs and the longer
  sequences it begins, such as both <kbd>Ctrl</kbd>+<kbd>K</kbd> and
  <kbd>Ctrl</kbd>+<kbd>K</kbd>, <kbd>Ctrl</kbd>+<kbd>C</kbd> for
  <kbd>Ctrl</kbd>+<kbd>K</kbd>. Nothing runs while the field records keys,
  and it doesn't warn about conflicts; its clear button shows every action
  again. Search and **Find by key** combine.
- **Only actions with shortcuts**, on by default, hides actions without a
  key. The line below the list counts the actions listed and says how many
  more a search found that are hidden because they have no shortcut.

The map updates when Configure Keyboard Shortcuts or **Reset All
Shortcuts…** saves, when Preferences changes the Yazi mode preset or
pressing <kbd>Shift</kbd> twice, and when a global shortcut changes. It only
shows keys: **Configure Keyboard Shortcuts…** at the bottom opens KDE's
editor to change them.

**Export…** saves the listed actions, as filtered, as a reference to print:

- an **HTML page** (`.html`), to open and print from a web browser, or a
  **PDF document** (`.pdf`) on A4 pages;
- headed **Plasma Opus Keyboard Shortcuts**, with a table of **Action**,
  **Shortcuts** and **Description** for each **Applies in** context, in the
  order the list shows them;
- suggested as `plasma-opus-shortcuts.html` in the Documents folder; a file
  name without `.html`, `.htm` or `.pdf` gets the chosen type's extension.

The file is replaced only once it is complete, and an error names the file
and the reason. Names in the reference are escaped, so an action or command
name can't add markup to it.

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
  after the action: its shortcuts in Qt's portable text, primary first, then
  every alternate, separated by a semicolon and a space, or `none` for no
  shortcut. This is the
  format of KDE's `KActionCollection`.
- Actions without an entry have their defaults, so a changed default reaches
  every action you haven't changed.
- An entry that can't be read, such as one with a misspelt key, is ignored,
  and the action keeps its defaults.
- `Version` marks the format; it isn't an action.
- Saved commands' entries are named `custom_command_preset_` followed by the
  SHA-256 hash of the command's name.
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

### Extra navigation keys

The setting on **Keyboard › Navigation** is kept in the `Keyboard` group:

```ini
[Keyboard]
ExtraNavigationShortcuts=true
```

| Entry                      | Type | Default | Effect                                             |
| -------------------------- | ---- | ------- | -------------------------------------------------- |
| `ExtraNavigationShortcuts` | Bool | `false` | Adds the free extra keys of the navigation actions |

The extra keys aren't stored: each window works them out from the current
shortcuts whenever it starts, the shortcuts are saved or Preferences are
applied. An action given its extra key and nothing else of your own has no
entry in the `Shortcuts` group.

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
- The navigation actions move the keyboard focus between the panes, the
  preview panel and the embedded terminal without a mouse, and the focus
  actions bring it back to the active pane when pressed again. The
  **Navigation** tab's list has an accessible name, and its areas are
  expandable rows.
- Nothing on the tab takes keys: it only shows and resets shortcuts.
  <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> move between its
  controls as in the rest of Preferences.
- The keyboard map opens with the focus in **Search**, its list has an
  accessible name, and <kbd>Escape</kbd> closes it while **Find by key**
  isn't recording. Only **Find by key**
  takes keys, and only after it is clicked or activated with
  <kbd>Space</kbd>; it then records the next keys pressed, so move the focus
  past it with <kbd>Tab</kbd> before activating it.
- The map and its reference write keys as text, such as
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd>, and print as tables that a
  screen reader can read.

## Known limitations

The limitations of the keyboard shortcut settings, with recommendations, are
tracked in [Caveats: Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts)
([#771](https://github.com/mikepadiernos/plasma-opus/issues/771)). The
milestone plan is in the roadmap under
[M31: Keyboard shortcut settings](ROADMAP.md#m31-keyboard-shortcut-settings).
