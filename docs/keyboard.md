# Keyboard shortcuts

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M31 Keyboard shortcut settings](https://img.shields.io/badge/roadmap-M31%20Keyboard%20shortcut%20settings-8957e5?style=flat-square)](ROADMAP.md#m31-keyboard-shortcut-settings)
[![Caveats: Keyboard shortcuts](https://img.shields.io/badge/caveats-Keyboard%20shortcuts-bf8700?style=flat-square)](CAVEATS.md#keyboard-shortcuts)

![Milestone M31](https://img.shields.io/badge/milestone-M31-8957e5?style=flat-square)
![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)

Plasma Opus uses KDE's own shortcut editor, **Settings › Configure Keyboard
Shortcuts…**, for every window action. **Preferences › Keyboard** (M31) adds a
summary around it: which shortcuts you have changed, which ones get in each
other's way, a reset to the defaults, and the keys that the editor doesn't
show, such as pressing <kbd>Shift</kbd> twice to search, the Yazi mode key
presets and the global shortcuts, and offers extra keys for moving between
panes, tabs and panels. It also exports your shortcuts to a keymap file and
imports one after showing what it changes. **Help › Keyboard Map…** lists
every action with its shortcuts, searched by name or by pressing keys, and
exports them as a printable reference. **Help › Command Palette…** finds an
action of the menus and toolbars, or a saved command, by name and runs it.
Optional Vim-style keys move through the file views of panes that aren't in
Yazi mode. While a key sequence waits for its next key, a popup lists the keys
that can follow it, and **Help › Shortcut Hints** lists the keys that work
where the keyboard focus is. **Go › Keyboard Hints** labels the files,
folders, location segments, tabs and places in view with a few keys each, so
typing a label goes there without the mouse.

## Contents

- [Overview](#overview)
- [Changing shortcuts](#changing-shortcuts)
- [The Shortcuts tab](#the-shortcuts-tab)
- [Navigation](#navigation)
- [Where shortcuts apply](#where-shortcuts-apply)
- [Search, Yazi mode and global shortcuts](#search-yazi-mode-and-global-shortcuts)
- [Global shortcut status](#global-shortcut-status)
- [Keyboard map](#keyboard-map)
- [Command palette](#command-palette)
- [Vim-style navigation](#vim-style-navigation)
- [Shortcut hints](#shortcut-hints)
- [Keyboard hints](#keyboard-hints)
- [Import and export](#import-and-export)
- [Configuration reference](#configuration-reference)
- [Accessibility](#accessibility)
- [Known limitations](#known-limitations)

## Overview

| Where                                        | What it does                                                               |
| -------------------------------------------- | -------------------------------------------------------------------------- |
| **Settings › Configure Keyboard Shortcuts…** | KDE's editor: change, add or remove any action's shortcuts                 |
| **Preferences › Keyboard › Shortcuts**       | Customized shortcuts, conflicts, reset to defaults, search and global keys |
| **Keyboard › Shortcuts › Import…, Export…**  | Keymap files: export your shortcuts, preview and apply another set         |
| **Preferences › Keyboard › Navigation**      | Pane, tab, selection, filter and panel focus actions, and their extra keys |
| **Help › Keyboard Map…**                     | Every action and key, searched by name or keys, and a printable reference  |
| **Help › Command Palette…**                  | Find a menu, toolbar or saved-command action by name and run it            |
| **Preferences › Keyboard › Command Palette** | The palette's shortcut, saved commands in it and its recent actions        |
| **Preferences › Keyboard › Vim Mode**        | Vim-style keys in file views outside Yazi mode, and what they shadow       |
| **View › Vim-Style Navigation**              | Turns the Vim-style keys in file views on or off                           |
| **Preferences › Keyboard › Hints**           | Shortcut hints, their delay and how long a key sequence waits              |
| **Help › Shortcut Hints**                    | The keys that work where the keyboard focus is, over the active pane       |
| **Go › Keyboard Hints**                      | Labels items, location segments, tabs and places with keys to type         |
| **Preferences › Keyboard › Keyboard Hints**  | The keys labels are made of, and whether a file label selects or opens     |
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
- **Import…** and **Export…**, under **Import and Export**, read and write
  keymap files (see [Import and export](#import-and-export)).

Preferences opened without a Plasma Opus window, as tests do, lists nothing
and explains that a window is needed.

## Navigation

**Preferences › Keyboard › Navigation** gathers the actions that move between
panes and tabs, change the selection or the filter, and move the keyboard
focus to the preview panel or the embedded terminal. The actions added for it
have no default shortcut, so they never take a key you already use. They are
on the menus and in **Configure Keyboard Shortcuts…**, where they can be given
any key.

| Area              | Action                      | Menu | Default                                         | Extra key                                      |
| ----------------- | --------------------------- | ---- | ----------------------------------------------- | ---------------------------------------------- |
| Panes             | **Switch Pane**             | Go   | <kbd>Tab</kbd>                                  |                                                |
| Panes             | **Focus First Pane**        | Go   | None                                            | <kbd>Alt</kbd>+<kbd>1</kbd>                    |
| Panes             | **Focus Second Pane**       | Go   | None                                            | <kbd>Alt</kbd>+<kbd>2</kbd>                    |
| Tabs              | **Next Tab**                | Go   | <kbd>Ctrl</kbd>+<kbd>Tab</kbd>                  | <kbd>Ctrl</kbd>+<kbd>Page Down</kbd>           |
| Tabs              | **Previous Tab**            | Go   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Tab</kbd> | <kbd>Ctrl</kbd>+<kbd>Page Up</kbd>             |
| Tabs              | **Activate Last Tab**       | Go   | None                                            |                                                |
| Selection         | **Select All**              | Edit | <kbd>Ctrl</kbd>+<kbd>A</kbd>                    |                                                |
| Selection         | **Deselect All**            | Edit | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>  |
| Selection         | **Invert Selection**        | Edit | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd>  |
| Selection         | **Selection Mode**          | Edit | None                                            |                                                |
| Filter            | **Filter…**                 | Edit | <kbd>Ctrl</kbd>+<kbd>F</kbd>                    |                                                |
| Filter            | **Clear Filter**            | Edit | None                                            |                                                |
| Preview Panel     | **Preview Panel**           | View | <kbd>F3</kbd>                                   |                                                |
| Preview Panel     | **Focus Preview Panel**     | View | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F3</kbd> |
| Embedded Terminal | **Show Embedded Terminal**  | View | None                                            |                                                |
| Embedded Terminal | **Focus Embedded Terminal** | View | None                                            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F4</kbd> |

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

| Status                                                        | Meaning                                                |
| ------------------------------------------------------------- | ------------------------------------------------------ |
| Off                                                           | The extra keys are off                                 |
| Off; “Action” uses it                                         | The extra keys are off, and another action has the key |
| Added                                                         | The key is one of the action's defaults and runs it    |
| Added as a default, but this action has shortcuts of your own | Your own shortcuts for the action don't have the key   |
| Not added: “Action” uses it                                   | Another action or a global shortcut has the key        |

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
| Vim-style navigation | The Vim-style keys, while they are on        | Window                    |
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
- the [Vim-style navigation](#vim-style-navigation) keys, written the same
  way, as **File views with Vim-style navigation**, or with **(off:
  Preferences › Keyboard › Vim Mode)** added while they are off; their
  internal names begin with `vim-`, such as `vim-move-down`;
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
Shortcuts…** saves, when Preferences changes the Yazi mode preset, the
Vim-style keys or pressing <kbd>Shift</kbd> twice, and when a global shortcut
changes. It only
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

## Command palette

**Help › Command Palette…**, or <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>I</kbd>,
opens KDE's command bar at the top of the window, as **Find Action…** does in
Dolphin and Kate. Type part of an action's name, then press <kbd>Enter</kbd>
or click the action to run it. <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>I</kbd> is
the key Dolphin and Kate use; no other Plasma Opus action, text field or Yazi
mode key preset uses it. Change it in Configure Keyboard Shortcuts like any
other.

The palette lists:

- the actions of the menu bar's menus and their submenus, after the menu
  path in grey, such as **File › Password Protection: Protect with
  Password…**;
- the actions of the window's toolbars that no menu has, after the toolbar's
  name;
- the saved commands, as **Saved Command:** and the command's name, unless
  **List saved commands** is off on **Preferences › Keyboard › Command
  Palette**;
- KDE's **Command Bar: Clear History**, which forgets the recently used
  actions.

Each action is listed once, with its icon and its primary shortcut. Actions
that are unavailable where you are, such as **Copy** with nothing selected,
hidden actions and the palette itself aren't listed. Each entry of a
submenu, including choices such as a tab bar position, is its own row. The
panes' own toolbars and the context menus aren't searched; their actions are
in the menus.

Typing narrows the list with KDE's fuzzy matching on the menu path and name,
so `nwfl` finds **New Folder…**, and highlights the matched letters.
<kbd>Up</kbd>, <kbd>Down</kbd>, <kbd>Page Up</kbd> and <kbd>Page Down</kbd>
move through the list while you type.

**Where actions run.** The palette remembers the widget that had the keyboard
focus when it opened, and the active pane. When you choose an action, the
palette closes, the focus goes back to that widget, the pane stays active and
then the action runs. **Select All**, **Rename…** or **Copy to Other Pane**
act on the pane you were in, even when it is the second pane.
<kbd>Escape</kbd>, or clicking elsewhere, closes the palette without running
anything, and <kbd>Escape</kbd> also gives the focus back.

**Recently used actions.** The last six actions run from the palette come
first, most recent at the top, until you type; typing sorts by how well the
names match instead. The **Recently Used** list on **Preferences › Keyboard ›
Command Palette** shows them, and **Clear History** there forgets them.

**Saved commands** open in **Scripts and Custom Commands** with their
preview, as their own shortcuts and toolbar buttons do, and run only once you
start them there. They're listed only where they can run: in a local folder,
with only local items selected.

The palette doesn't change shortcuts: it shows each one as it is. It doesn't
take keys from text fields either; letters typed in a field go to the field.
<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>I</kbd> isn't a typing key, so it opens
the palette from a field too, and the focus returns to the field afterwards.
In the embedded terminal, the terminal decides whether it keeps the key.

The **Command Palette** tab of **Preferences › Keyboard** shows the
palette's current shortcut, the **List saved commands** setting and the
recently used actions, with **Clear History** and **Configure Keyboard
Shortcuts…**. The list updates when the tab is shown and when shortcuts are
saved.

## Vim-style navigation

**Use Vim-style keys in file views**, on **Preferences › Keyboard › Vim
Mode**, moves through the file views of the panes with keys taken from Vim, as
ranger and Yazi do. It is off by default. **View › Vim-Style Navigation**
(`toggle_vim_navigation`, no default shortcut) turns it on or off too, saves
the setting at once and says so in the status bar.

The keys work in every view mode, but only while a pane's file view has the
keyboard focus. A pane in [Yazi mode](yazi-mode.md#keyboard) uses its Yazi
mode key preset instead, the standard or the Yazi-style keys, and gets the
Vim-style navigation keys back when it leaves Yazi mode. The two settings are
independent: **View › Vim-Style Keys in Yazi Mode** chooses the Yazi mode
preset, and **View › Vim-Style Navigation** the keys outside Yazi mode.

| Keys                                                 | Command                                          |
| ---------------------------------------------------- | ------------------------------------------------ |
| <kbd>j</kbd>, <kbd>k</kbd>                           | Move down or up                                  |
| <kbd>g</kbd> <kbd>g</kbd>, <kbd>G</kbd>              | Move to the first or last item                   |
| <kbd>Ctrl</kbd>+<kbd>D</kbd>, <kbd>J</kbd>           | Move half a page down                            |
| <kbd>Ctrl</kbd>+<kbd>U</kbd>, <kbd>K</kbd>           | Move half a page up                              |
| <kbd>h</kbd>                                         | Open the parent folder, focusing the folder left |
| <kbd>l</kbd>                                         | Open the focused item, as <kbd>Enter</kbd> does  |
| <kbd>H</kbd>, <kbd>L</kbd>                           | Go back or forward in the tab's history          |
| <kbd>g</kbd> <kbd>h</kbd>                            | Open **Go › Home**'s folder                      |
| <kbd>g</kbd> <kbd>t</kbd>, <kbd>g</kbd> <kbd>T</kbd> | Go to the next or previous tab of the pane       |
| <kbd>/</kbd>, <kbd>i</kbd>                           | Enter the filter, as **Edit › Filter…** does     |
| <kbd>\\</kbd>                                        | Clear the filter and close the filter bar        |

Capital letters are typed with <kbd>Shift</kbd>, so <kbd>G</kbd> is
<kbd>Shift</kbd>+<kbd>g</kbd>. <kbd>J</kbd> and <kbd>K</kbd> come from
ranger; they also work when a window shortcut keeps
<kbd>Ctrl</kbd>+<kbd>D</kbd> or <kbd>Ctrl</kbd>+<kbd>U</kbd>.

- **Counts.** Digits typed first repeat a movement, so <kbd>5</kbd>
  <kbd>j</kbd> moves five items down and <kbd>3</kbd> <kbd>J</kbd> one and a
  half pages. A count is at most 999. While the keys are on, digits 1 to
  9 begin a count instead of finding an item by its first characters.
- **Sequences.** <kbd>g</kbd> waits for the next key. A pending sequence or
  count is cancelled after the key sequence timeout without a key, one
  second by default (see [Hint settings](#hint-settings)), by
  <kbd>Escape</kbd>, or by a key that doesn't continue it, which then works on
  its own. While the [shortcut hints](#shortcut-hints) show, it waits for the
  next key instead of expiring.
- **Moving** sends the view its own arrow, <kbd>Home</kbd> and
  <kbd>End</kbd> keys, so the focus and the selection move as they always do,
  in lists, details, icons and cards alike. A movement stops at the first or
  last item.
- **Messages.** A key that can't do anything, such as <kbd>h</kbd> in a
  folder without a parent, <kbd>L</kbd> without later history,
  <kbd>g</kbd> <kbd>t</kbd> with one tab, <kbd>\\</kbd> without a filter or a
  movement in an empty folder, says why in the status bar and to screen
  readers instead of doing nothing silently.
- **Typing elsewhere.** Other letters still find items by their first
  characters, as without the keys. In the file view, the arrow keys,
  <kbd>Enter</kbd>, <kbd>Tab</kbd>, <kbd>Escape</kbd> and every key with
  <kbd>Ctrl</kbd> other than <kbd>Ctrl</kbd>+<kbd>D</kbd> and
  <kbd>Ctrl</kbd>+<kbd>U</kbd> work as before.

**Modes.** While the keys are on, each pane shows where its keys go in a
highlighted label at its bottom, as Vim shows its mode:

| Indicator        | Where the keyboard focus is                                 | Keys                                   |
| ---------------- | ----------------------------------------------------------- | -------------------------------------- |
| **NORMAL**       | The file view, or outside the pane                          | Move, open and filter, as listed above |
| **NORMAL · 5g…** | The file view, with a count or sequence begun               | Wait for the rest of the sequence      |
| **FILTER**       | The filter bar                                              | Type the filter                        |
| **INSERT**       | Another of the pane's text fields, such as the location bar | Type in the field                      |

- <kbd>/</kbd> or <kbd>i</kbd> enters the filter: the filter bar opens, or
  takes the focus again with its text, and the indicator shows **FILTER**.
  Every key, including <kbd>j</kbd>, <kbd>k</kbd> and <kbd>g</kbd>, is typed
  into the filter there.
- <kbd>Escape</kbd> in the filter bar returns to the items and keeps the
  filter, so the keys move through the filtered items. Press <kbd>\\</kbd> in
  the items to clear it, or <kbd>/</kbd> to change it. <kbd>Escape</kbd> in
  an empty filter bar closes it, and <kbd>Enter</kbd> still opens the first
  match.
- Typing a letter that isn't bound, with **Filter as you type** on, opens the
  filter bar with that letter as before; the indicator then shows **FILTER**.
- The keys of a field in **INSERT**, such as <kbd>Escape</kbd> in the
  location bar or a rename editor, are the field's own.
- In a pane in Yazi mode, the Yazi mode indicator shows instead, and
  <kbd>Escape</kbd> in the filter bar clears the filter as described in
  [Yazi mode › Filtering](yazi-mode.md#filtering).

**Typing is never taken.** The keys apply only to the file view itself, so
they never reach the filter bar, the location bar, search fields, the
embedded terminal, an inline rename editor or any other text field. While an
item is being renamed in place, every key goes to the editor.

**Window shortcuts.** Plain keys, those without <kbd>Ctrl</kbd>,
<kbd>Alt</kbd> or <kbd>Meta</kbd>, take over a window shortcut that uses them
while a file view has the focus, as Yazi mode's keys do: if **Open Gallery**
were given <kbd>G</kbd>, <kbd>G</kbd> would move to the last item there and
open the gallery elsewhere. Keys with <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or
<kbd>Meta</kbd> never take over a shortcut: the shortcut keeps running, and
the Vim-style key works only while no shortcut uses it. With the default
shortcuts, **Add to Favorites** keeps <kbd>Ctrl</kbd>+<kbd>D</kbd>, so use
<kbd>J</kbd> to move half a page down, and no plain key is taken.

The **Vim Mode** tab shows:

- **Use Vim-style keys in file views**, the setting;
- **Keys**, every command with all its keys, in a list with an accessible
  name, a note on counts and typing, and a note on the modes;
- **Conflicts**, the window shortcuts that the plain keys take over, as
  [Yazi mode › Keyboard](yazi-mode.md#keyboard) lists them, such as
  “G runs “Move to the last item” instead of “Open Gallery””, and the
  shortcuts with <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or <kbd>Meta</kbd> that keep
  their key, such as “Ctrl+D runs “Add to Favorites”, so it doesn't run “Move
  half a page down””. The list is worked out whether the keys are on or off,
  and updates when the shortcuts are saved or Preferences applied;
- **Configure Keyboard Shortcuts…** and **Keyboard Map…**, which open KDE's
  editor and the [keyboard map](#keyboard-map);
- a note on where the keys never apply.

The [Shortcuts tab](#the-shortcuts-tab)'s **Conflicts** list doesn't include
the Vim-style keys; the **Vim Mode** tab lists them.

## Shortcut hints

While a key sequence waits for its next key, a hint popup lists the keys that
can follow it and what they do, as
[which-key.nvim](https://github.com/folke/which-key.nvim) does in Neovim. It
floats at the bottom of the active pane's file view, or of its columns in
[Yazi mode](yazi-mode.md), half a second after the sequence begins by default
(see [Hint settings](#hint-settings)), so a sequence typed quickly never shows
it. It lists:

- after the first keys of a [Vim-style](#vim-style-navigation) or
  [Yazi mode](yazi-mode.md#keyboard) sequence, such as <kbd>g</kbd>, the keys
  that complete it, such as <kbd>g</kbd> for **Move to the first item**;
- after a count, such as <kbd>5</kbd>, the commands that a count repeats;
- after the first key combination of a window shortcut of several, such as
  <kbd>Ctrl</kbd>+<kbd>K</kbd> of <kbd>Ctrl</kbd>+<kbd>K</kbd>,
  <kbd>G</kbd>, the key combinations that can follow it and their actions.

The title shows the keys typed so far, such as **g…** or **5…**. The hints
are grouped under headings: the Vim-style and Yazi mode keys under **Move**,
**Folders and history**, **View**, **Selection**, **Files**, **Filter and
search**, **Sort** and **Tabs**, and window shortcuts under their menu, such
as **File** or **Go**, or **Other actions**. Keys that begin longer sequences
are one hint, such as **+3 commands**, under **More keys** when those
commands are in different groups. The hints fill as many columns as fit; a
list taller than about two-fifths of the view is split into pages, and the
footer shows the page, such as **1 of 3**.

- **The focus stays.** The popup never takes the keyboard focus or a typed
  key: the file view keeps both, and the next key goes to the sequence as
  without the popup.
- **No hurry.** While the popup shows, a sequence doesn't expire; it waits
  for the next key. Without the popup, a sequence is cancelled after the key
  sequence timeout.
- **Completing or continuing.** A key that completes the sequence runs it and
  closes the popup; a key that continues it updates the popup at once.
- **Cancelling.** <kbd>Escape</kbd> cancels the sequence and closes the
  popup, as does moving the focus elsewhere. Before the popup shows, a key
  that doesn't continue a Vim-style or Yazi mode sequence still cancels it
  and works on its own.
- **Turning pages.** While the popup lists Vim-style or Yazi mode keys,
  <kbd>Page Up</kbd> and <kbd>Page Down</kbd>, and the arrow keys, turn its
  pages instead of cancelling the sequence. When the sequence itself uses
  them, as a count uses <kbd>Page Down</kbd> to move pages down, they work as
  keys of the sequence, and the footer says to scroll instead. Scrolling over
  the popup always turns its pages.
- **Window shortcuts.** Qt matches window shortcuts of several key
  combinations itself and takes every key while one waits, so the popup
  can't take keys then: scroll to turn its pages. A key combination that
  doesn't continue the shortcut ends it, as without the popup, and
  <kbd>Escape</kbd> or moving the focus ends Qt's sequence too, so the next
  key doesn't complete it; so does the key sequence timeout while the popup
  doesn't show. Only the window shortcuts that work where the
  keyboard focus is are listed, without those whose first key the focused
  text field or list keeps (see [Where shortcuts apply](#where-shortcuts-apply))
  and those of disabled actions.
- **Typing is never taken.** The Vim-style and Yazi mode keys never apply in
  text fields or the embedded terminal, so their hints don't either. A
  window shortcut that a text field or the terminal lets through shows its
  hints over the active pane, but the keys still go where they always do.
- **Motion.** The popup slides up into place when **Animations** in
  **Preferences › Themes › Motion and Accessibility** follows Plasma and
  Plasma's animations are on; with reduced or no animations, it appears in
  place.

### Hint settings

The **Hints** tab of **Preferences › Keyboard** has:

- **Show shortcut hints while a key sequence waits**, on by default. When it
  is off, sequences work as before, without the popup.
- **Show hints after**, how long a sequence waits before the popup shows:
  500 ms by default, from **At once** to 5000 ms.
- **Cancel incomplete sequences after**, the key sequence timeout: 1000 ms by
  default, up to 10000 ms, or **Never**. It applies to the Vim-style and Yazi
  mode keys and to window shortcuts of several key combinations, which Qt
  would otherwise keep waiting for. A sequence that times out runs nothing,
  and the next key works on its own instead of completing it. While the
  popup shows, a sequence waits instead. With **Never**, a sequence waits
  until <kbd>Escape</kbd>, a key that doesn't continue it, or the focus
  moves.
- a note when the hints are on and the delay is as long as the timeout or
  longer, since sequences would then be cancelled before their hints show;
- **Shortcut Reference**, the shortcut of **Help › Shortcut Hints**, **None**
  by default, with **Configure Keyboard Shortcuts…** to give it one and
  **Keyboard Map…**.

The settings take effect when Preferences are applied, and keymap files
include them (see [Keymap files](#keymap-files)).

### Shortcut reference

**Help › Shortcut Hints** shows the hint popup on demand over the active
pane, listing the keys that work where the keyboard focus is, under a title
such as **Shortcuts: File views and lists**:

- in a pane in Yazi mode, the keys of its key preset, then the window
  shortcuts they leave;
- in a file view with the Vim-style keys on, those keys, then the window
  shortcuts. A Vim-style key with <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or
  <kbd>Meta</kbd> that a window shortcut keeps isn't listed, nor is a window
  shortcut whose key a plain Vim-style key takes over;
- elsewhere, the window shortcuts that work there, grouped by menu, without
  those whose first key the focused text field or list keeps.

Each shortcut is listed in full, such as <kbd>Ctrl</kbd>+<kbd>K</kbd>,
<kbd>G</kbd>, rather than as the beginning of longer sequences.

- **In a file view or the folder tree,** <kbd>Page Up</kbd>,
  <kbd>Page Down</kbd> and the arrow keys turn the popup's pages without
  moving through the items, and <kbd>Escape</kbd> closes it and does nothing
  else.
- **Elsewhere,** as in a text field or the embedded terminal, every key goes
  where it always does; scroll over the popup to turn its pages.
- **Closing.** Any other key closes the popup and works as usual, as do
  choosing **Shortcut Hints** again and moving the focus.
- **No default shortcut,** so it takes no key from another action or a text
  field; give it one in **Configure Keyboard Shortcuts…**, or run it from
  the [command palette](#command-palette). It works whether or not the hints
  are on.

For every action and key, including those of other places, use the
[keyboard map](#keyboard-map).

## Keyboard hints

**Go › Keyboard Hints**, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>J</kbd> by
default, puts a short label of one or more letters on everything in the
active pane that you could click to move around, as the link hints of
[Lektra](philosophy/inspirations.md#lektra--document-viewer-and-keyboard-control)
and of keyboard-driven web browsers do. Type a label's letters to go there
without the mouse. It labels:

- the files and folders visible in the active pane's file view, or in the
  current column of a pane in [Yazi mode](yazi-mode.md);
- the segments of the pane's location bar, such as a parent folder;
- the pane's other tabs, on their tabs;
- the entries visible in the **Places** panel.

The labels are made of the home-row keys of your keyboard layout, chosen on
the [Keyboard Hints tab](#keyboard-hint-settings), such as <kbd>f</kbd>,
<kbd>j</kbd>, <kbd>d</kbd> and <kbd>k</kbd> on a QWERTY keyboard. No label
begins another, so a label is complete as soon as it is typed, and labels are
as short as their number allows: files and folders get the shortest ones,
then location segments, tabs and places, each in the order they are shown.
The same view always gets the same labels.

- **Files and folders.** Typing an item's label selects it, makes it the
  current item and scrolls to it, with the keyboard focus in the file view.
  Hold <kbd>Shift</kbd> on the label's last key to open it instead, as
  pressing <kbd>Enter</kbd> does. **Typing a file or folder label** on the
  tab can make labels open items, and <kbd>Shift</kbd> then selects them.
- **Location segments and places** open their folder in the active pane.
- **Tabs** switch to the tab.
- **Narrowing.** Each key typed hides the labels that don't begin with the
  keys typed so far and dims the typed part of the others.
  <kbd>Backspace</kbd> takes the last key back, or cancels when nothing is
  typed. Letters match in either case.
- **Cancelling.** <kbd>Escape</kbd>, <kbd>Tab</kbd>, a key that doesn't
  continue any label, a key with <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or
  <kbd>Meta</kbd>, or **Keyboard Hints** again removes the labels. The key
  that cancels does nothing else. So does anything that would move the
  labels away from their targets: moving the keyboard focus, scrolling,
  resizing the window or a labelled part of it, the listing changing, as
  when a folder loads more items, the pane going to another folder, opening
  or closing a tab, or another pane becoming active. The keyboard focus then
  returns to where it was.
- **Typing is never taken.** While the keyboard focus is in a text field,
  such as the location bar or the filter bar, an inline rename editor, the
  embedded terminal or a field that takes an input method,
  **Keyboard Hints** shows no labels and says so in the status bar; move
  the focus out of the text first, as with <kbd>Escape</kbd>. While labels
  show, they take only the keys described above, and input methods are off
  until they close.
- **Vim-style keys and Yazi mode.** Labels work the same with the
  [Vim-style keys](#vim-style-navigation) on or off and in Yazi mode. While
  labels show, typed letters go to the labels rather than to those keys; once
  a label is typed or the labels close, the keys work as before.
- **Motion.** Labels appear and disappear at once, without animation.

When nothing in the window can be labelled, the status bar says so.

### Keyboard hint settings

The **Keyboard Hints** tab of **Preferences › Keyboard** has:

- a summary of what the labels do, and **Shortcut**, the shortcut of **Go ›
  Keyboard Hints**, with **Configure Keyboard Shortcuts…** to change it;
- **Label keys**, the keys labels are made of, by keyboard layout:

  | Choice           | Keys                  |
  | ---------------- | --------------------- |
  | QWERTY home row  | `f j d k s l a g h`   |
  | AZERTY home row  | `f j d k s l q m g h` |
  | Dvorak home row  | `u h e t o n a s i d` |
  | Colemak home row | `t n s e r i a o d h` |

  QWERTY is the default. Keys earlier in the list are used first, so the
  strongest fingers type most labels.
- **Typing a file or folder label**, **Select it** by default, or **Open
  it**. Holding <kbd>Shift</kbd> on the last key does the other.

The settings take effect when Preferences are applied, and keymap files
include them (see [Keymap files](#keymap-files)).

## Import and export

**Import…** and **Export…** under **Import and Export** on **Preferences ›
Keyboard › Shortcuts** copy your shortcuts to another computer or keep a copy
of them, as a keymap file (see [Keymap files](#keymap-files)).

**Export…** asks where to save, suggesting `plasma-opus-keymap.json` in the
Documents folder; a file name without an extension gets `.json`. The file
has:

- each action whose shortcuts differ from its defaults, with every
  shortcut and alternate, including actions you gave no shortcut and saved
  commands;
- the saved values of the keyboard settings: the extra navigation keys, the
  Vim-style navigation keys, the Yazi mode key preset, pressing
  <kbd>Shift</kbd> twice to search, whether <kbd>Tab</kbd> switches
  panes, the [hint settings](#hint-settings) and the
  [keyboard hint settings](#keyboard-hint-settings). Changes not yet applied
  in Preferences aren't exported.

Actions that have their defaults and global shortcuts aren't included. The
file is replaced only once it is complete, and the line below the buttons
says what was exported, or names the file and the reason it failed.

**Import…** reads a keymap file and, before changing anything, shows what it
would do:

- **Shortcuts That Change** lists each action whose shortcuts differ from
  the file's, with its shortcuts **Now**, the **Imported** ones and where it
  applies. Actions that already have the file's shortcuts are counted, not
  listed.
- **Give actions that the file doesn't list their default shortcuts**, off
  by default, also gives every other customized action its defaults, so the
  shortcuts become the same as where the file was exported. Otherwise only
  the actions the file lists change.
- **Conflicts After Importing** lists the conflicts the import adds, found as
  the [Shortcuts tab](#the-shortcuts-tab) finds them, including keys that
  text fields and lists keep, and says how many it resolves and how many stay.
  An extra navigation key that an imported shortcut takes gives way, as in
  the window.
- **Keyboard Settings** lists each setting the file changes, with its current
  and imported values.
- **Not Imported** lists the actions this window doesn't have, such as a
  saved command that isn't saved here, and anything else in the file that is
  skipped.

**Cancel** changes nothing. **Apply** gives the actions the file's shortcuts
and saves them as **Configure Keyboard Shortcuts…** does: they take effect at
once in every open Plasma Opus window, and the **Keyboard** and **Yazi Mode**
pages and the keyboard map update. Conflicts don't stop an import; resolve
them afterwards on the Shortcuts tab. The keyboard settings are shown in their
places in Preferences without being saved: **Apply** or **OK** saves them,
as when you change them yourself, asking first about a Yazi mode preset that
takes window shortcuts, and **Cancel** discards them.

A file that isn't a keymap this version of Plasma Opus reads is refused with
the reason, and nothing changes. Importing never runs anything: a keymap file
only names actions, keys and keyboard settings, and a saved command's
shortcut still opens the command with its preview.

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
- **Configure Keyboard Shortcuts…**, **Reset All Shortcuts…** and
  **Import…** save here, and every open window takes the saved shortcuts at
  once. Settings backups
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

### Vim-style navigation

The setting on **Keyboard › Vim Mode** is kept in the `Keyboard` group:

```ini
[Keyboard]
VimNavigation=true
```

| Entry           | Type | Default | Effect                                                      |
| --------------- | ---- | ------- | ----------------------------------------------------------- |
| `VimNavigation` | Bool | `false` | Vim-style keys in the file views of panes outside Yazi mode |

**View › Vim-Style Navigation** writes the same entry. The keys themselves are
fixed and aren't stored. The setting is one of the keymap file settings.

### Shortcut hints

The settings on **Keyboard › Hints** are kept in the `Keyboard` group:

```ini
[Keyboard]
ShortcutHints=true
ShortcutHintDelay=500
KeySequenceTimeout=1000
```

| Entry                | Type | Default | Effect                                                           |
| -------------------- | ---- | ------- | ---------------------------------------------------------------- |
| `ShortcutHints`      | Bool | `true`  | Shows the hint popup while a key sequence waits                  |
| `ShortcutHintDelay`  | Int  | `500`   | Milliseconds before the popup shows, from 0 to 5000              |
| `KeySequenceTimeout` | Int  | `1000`  | Milliseconds an incomplete sequence waits, up to 10000; 0, never |

A value outside its range is read as the nearest one allowed. **Help ›
Shortcut Hints** stores only its shortcut, as `show_shortcut_hints` in the
[`Shortcuts` group](#window-shortcuts). The settings are keymap file
settings.

### Keyboard hints

The settings on **Keyboard › Keyboard Hints** are kept in the `Keyboard`
group:

```ini
[Keyboard]
KeyboardHintKeys=Dvorak
KeyboardHintItemAction=Open
```

| Entry                    | Type | Default  | Effect                                                         |
| ------------------------ | ---- | -------- | -------------------------------------------------------------- |
| `KeyboardHintKeys`       | Enum | `Qwerty` | Label keys: `Qwerty`, `Azerty`, `Dvorak` or `Colemak` home row |
| `KeyboardHintItemAction` | Enum | `Select` | Typing a file or folder label: `Select` or `Open` it           |

**Go › Keyboard Hints** stores only its shortcut, as `keyboard_hints` in the
[`Shortcuts` group](#window-shortcuts). The settings are keymap file
settings.

### Command palette

The setting on **Keyboard › Command Palette** is kept in the `Keyboard`
group:

```ini
[Keyboard]
CommandPaletteSavedCommands=false
```

| Entry                         | Type | Default | Effect                                              |
| ----------------------------- | ---- | ------- | --------------------------------------------------- |
| `CommandPaletteSavedCommands` | Bool | `true`  | Lists saved commands, which open with their preview |

The palette's action has KDE's name, `open_kcommand_bar`, so a changed
shortcut is stored under that name in the [`Shortcuts` group](#window-shortcuts)
and in keymap files. The setting isn't one of the keymap file settings.

KDE's command bar keeps the recently used actions in the `General` group of
`~/.local/state/opus-file-managerstaterc`, the state file:

```ini
[General]
CommandBarLastUsedActions=Select &All,&New Folder…
```

- It writes the entry when a palette closes: up to six action names as the
  menus show them, with their accelerator markers, most recent first. An
  action run twice is listed twice.
- It finds the actions by name, so an action that is renamed, or shown in
  another language, loses its place.
- **Clear History**, on the tab or in the palette, empties the entry.

### Keymap files

**Import…** and **Export…** use keymap files, JSON documents in UTF-8:

```json
{
  "format": "plasma-opus-keymap",
  "version": 1,
  "application": "opus-file-manager",
  "exported": "2026-11-28T12:00:00Z",
  "actions": {
    "open_gallery": ["F9", "Ctrl+K, G"],
    "search_folder": []
  },
  "settings": {
    "ExtraNavigationShortcuts": true,
    "YaziKeyPreset": "Vim"
  }
}
```

| Member        | Type    | Required | Meaning                                                   |
| ------------- | ------- | -------- | --------------------------------------------------------- |
| `format`      | String  | Yes      | Always `plasma-opus-keymap`                               |
| `version`     | Integer | Yes      | The format version, `1`                                   |
| `application` | String  | Yes      | Whose actions the file names: `opus-file-manager`         |
| `exported`    | String  | No       | When the file was exported, in ISO 8601 UTC; only for you |
| `actions`     | Object  | Yes      | Each action's shortcuts, by action name                   |
| `settings`    | Object  | No       | Keyboard settings, by configuration entry name            |

- **Actions** are named as in the [`Shortcuts` group](#window-shortcuts),
  such as `search_folder` or `custom_command_preset_` and a hash for a saved
  command. Each has an array of its shortcuts in Qt's portable text, the
  primary first, then each alternate; an empty array means no shortcut. A
  shortcut is one to four key combinations separated by a comma and a space,
  such as `Ctrl+K, G`, each with a key other than a modifier. An action
  that isn't listed keeps its shortcuts, unless the import is told to give it
  its defaults.
- **Settings** can be these, and no others:

  | Entry                      | Value                                             | Set in                           |
  | -------------------------- | ------------------------------------------------- | -------------------------------- |
  | `ExtraNavigationShortcuts` | `true` or `false`                                 | **Keyboard › Navigation**        |
  | `VimNavigation`            | `true` or `false`                                 | **Keyboard › Vim Mode**          |
  | `YaziKeyPreset`            | `"Standard"` or `"Vim"`                           | **Yazi Mode › Keyboard**         |
  | `DoubleShiftSearch`        | `true` or `false`                                 | **Tools › Search**               |
  | `TabSwitchesPanes`         | `true` or `false`                                 | **Interface › Folders and Tabs** |
  | `ShortcutHints`            | `true` or `false`                                 | **Keyboard › Hints**             |
  | `ShortcutHintDelay`        | A number, 0 to 5000                               | **Keyboard › Hints**             |
  | `KeySequenceTimeout`       | A number, 0 to 10000                              | **Keyboard › Hints**             |
  | `KeyboardHintKeys`         | `"Qwerty"`, `"Azerty"`, `"Dvorak"` or `"Colemak"` | **Keyboard › Keyboard Hints**    |
  | `KeyboardHintItemAction`   | `"Select"` or `"Open"`                            | **Keyboard › Keyboard Hints**    |

Plasma Opus reads a file strictly and refuses all of it when:

- it is larger than 256 KiB, isn't JSON, or isn't an object;
- `format` isn't `plasma-opus-keymap`, `version` isn't a whole number from 1
  to the newest version this Plasma Opus reads, or `application` isn't
  `opus-file-manager`;
- `exported` is there and isn't a string, `actions` isn't an object, or
  `settings` is there and isn't an object;
- it lists more than 4,096 actions, an action has more than 16 shortcuts, an
  action name is empty, longer than 256 characters, has control characters
  or begins or ends with a space, or its shortcuts aren't an array of
  strings;
- a shortcut isn't a valid key sequence as described above, or is longer
  than 256 characters;
- a known setting has a value of the wrong type, a choice that doesn't
  exist, or a number that isn't a whole number within its range.

These are skipped, and **Not Imported** lists them:

- members other than those above, so a later version can add some;
- settings other than the keyboard settings above, including any other
  Preferences entry;
- a shortcut listed twice for one action, which is used once;
- actions this window doesn't have.

**Version policy.** Version 1 is the first. Members or settings that older
versions can safely skip are added without a new version. A change that
older versions would misread, such as a new meaning for an existing member,
gets a new version; older versions then refuse the file and say that it
needs a newer Plasma Opus, while newer versions keep reading every earlier
version.

**Other applications.** `application` names whose actions the file
describes, so another KDE application could use the same format with its own
name and actions. Plasma Opus refuses a keymap for another application, and
doesn't yet share keymaps with Plasma Impresario; see
[Share Vim mode and shortcuts](integrations/plasma-impresario.md#share-vim-mode-and-shortcuts).

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
- Nothing on the tab takes keys: it only shows, resets, imports and exports
  shortcuts.
  <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> move between its
  controls as in the rest of Preferences.
- The import preview opens with the focus in the list of changes; its lists
  have accessible names, every part is plain text, and the preview changes
  nothing until **Apply** is chosen. The result of an import or export is
  reported in words below the buttons.
- The keyboard map opens with the focus in **Search**, its list has an
  accessible name, and <kbd>Escape</kbd> closes it while **Find by key**
  isn't recording. Only **Find by key**
  takes keys, and only after it is clicked or activated with
  <kbd>Space</kbd>; it then records the next keys pressed, so move the focus
  past it with <kbd>Tab</kbd> before activating it.
- The map and its reference write keys as text, such as
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd>, and print as tables that a
  screen reader can read.
- The command palette opens with the focus in its search field. The palette,
  the field and the list of actions have accessible names, every row is the
  action's name with its menu path and shortcut as text, and
  <kbd>Escape</kbd> closes it and returns the focus to where it was. The
  arrow keys move through the list without leaving the field.
- The **Command Palette** tab's recently used actions are plain text in a
  list with an accessible name.
- The Vim-style keys are off by default and add to the usual keys rather
  than replacing them: in the file views, the arrow keys, <kbd>Home</kbd>,
  <kbd>End</kbd>, <kbd>Enter</kbd>, <kbd>Tab</kbd> and <kbd>Escape</kbd> work
  as before, and no key is taken from text fields or the terminal.
  <kbd>Escape</kbd> always leaves the filter bar. A key that can't do
  anything says why in words, which screen readers announce. The **Vim
  Mode** tab's keys and conflicts are plain text in lists with accessible
  names.
- Each pane's mode indicator has an accessible name that says the mode in
  words and how to leave it, such as “Vim-style navigation, filter mode:
  typing filters the items; press Escape to return to the items”, or the
  keys a sequence waits after. A new mode, and keys waiting for the rest of
  a sequence, are announced to screen readers in the pane being typed in;
  moving between panes isn't. The indicator uses the highlight colours with
  its mode written out, so it doesn't rely on colour.
- The shortcut hint popup never takes the keyboard focus, and it has the
  accessible name “Shortcut hints”. Its accessible description, which is
  also announced to screen readers when it appears, changes or turns a page,
  reads the title, each hint's keys and what they do, and the page, such as
  “g…: g, Move to the first item; …”. <kbd>Escape</kbd> closes it, and it
  turns pages with keys as well as the mouse wheel where it can. It slides
  only while Plasma Opus's animations follow Plasma's.
- **Help › Shortcut Hints** shows the same popup, read the same way, without
  taking the focus or a typed key outside file views and the folder tree.
  <kbd>Escape</kbd> closes it. The **Hints** tab's delay and timeout are
  spin boxes with their units, and a longer timeout or **Never** gives more
  time to type a sequence.
- Each keyboard hint label has an accessible name of its keys and its
  target's name, such as “fj: Documents”, and an accessible description of
  the kind of target, such as “File or folder”, “Location segment”, “Tab” or
  “Place”. Screen readers are told how many labels show and how to cancel
  them. The labels use the tooltip colours with a frame and bold text, so
  they don't rely on colour; labels that no longer match are hidden rather
  than only dimmed, and they never animate. **Keyboard Hints** has a
  shortcut and is in the Go menu and the command palette, so it can be
  reached without the mouse, and <kbd>Escape</kbd> always closes it and
  returns the focus.

## Known limitations

The limitations of the keyboard shortcut settings, with recommendations, are
tracked in [Caveats: Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts)
([#771](https://github.com/mikepadiernos/plasma-opus/issues/771)). The
milestone plan is in the roadmap under
[M31: Keyboard shortcut settings](ROADMAP.md#m31-keyboard-shortcut-settings).
