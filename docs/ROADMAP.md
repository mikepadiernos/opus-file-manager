# Opus File Manager roadmap

Opus File Manager is a dual-pane file manager for KDE Plasma, written in C++
with Qt 6 Widgets and KDE Frameworks 6. It targets feature parity with Max File
Manager (Electron), whose behaviour is recorded in `docs/parity/backend.md` and
`docs/parity/ui.md`. It runs on Linux only.

## Architecture

- `src/core/` is the `opusfm_core` static library: pure logic with no widgets
  (tab and history state, session serialisation, listing rules, rename planning,
  sync, breakdown, search index, export). Unit tests cover it.
- `src/app/` holds the widgets and the `opus-file-manager` executable.
- `tests/` holds QtTest suites run through `ctest`. GUI tests run with
  `QT_QPA_PLATFORM=offscreen`.

## Plasma-native decisions

These replace upstream behaviour on purpose.

| Upstream                                       | Opus                                                                           |
| ---------------------------------------------- | ------------------------------------------------------------------------------ |
| Custom CSS themes, title bar, window buttons   | Plasma widget style, colour scheme, fonts and KWin decorations                 |
| Bundled Pure icon pack                         | Active icon theme via `QIcon::fromTheme`; folder colours use `folder-<colour>` |
| Hand-written toolbar and shortcut editors      | KXmlGui (`KEditToolBar`, `KShortcutsDialog`), hamburger menu                   |
| Settings in SQLite                             | KConfig `opusfilemanagerrc`; session in the KConfig state file                 |
| Node copy engine, 20-step undo                 | KIO jobs, `KIO::FileUndoManager`, progress in Plasma notifications             |
| Custom places, drives polling, lsblk/udisksctl | `KFilePlacesView` (Solid devices, mount/eject, hotplug)                        |
| Custom Open With and default-app logic         | `KFileItemActions`, `KOpenWithDialog`, `KApplicationTrader`, service menus     |
| Ratings and notes in `.mfm.json`               | `KFileMetaData::UserMetaData` xattrs, shared with Dolphin and Baloo            |
| Folder icon in `.mfm.json`                     | `.directory` `Icon=`, shared with Dolphin                                      |
| Python FileManager1 bridge, masking Dolphin    | In-process `org.freedesktop.FileManager1`; Dolphin is never masked             |
| Custom properties and checksum dialog          | `KPropertiesDialog` (includes checksums)                                       |
| Clipboard helper processes                     | `QClipboard` with `KIO::setClipboardDataCut`                                   |
| Hard-coded template blobs                      | `KNewFileMenu` templates plus Opus templates                                   |

Pins, colour labels, file colour dots, the hide-from-parent flag and the
per-folder view format have no Plasma equivalent. They stay in `.mfm.json` with
the upstream schema, so existing folders keep their data.

## Milestones

Each milestone ends with a working, tested app.

1. **M01 core dual pane.** KXmlGui main window; two panes with tabs, history and
   a `KUrlNavigator`; details and icon views over `KDirModel`; previews; Places
   panel; open, Open With and service menus; copy/move to the other pane
   (F5/F6), clipboard, drag and drop, trash/delete, rename, new folder/file,
   undo; hidden files; terminal; properties; free space in the status bar;
   single instance; session restore; stacked top/bottom layout with both tab
   bars meeting in the middle.
2. **M02 settings.** Settings for each M01 feature, modelled on Dolphin and its
   fork Orcinus and stored in KConfig: preferences dialog; startup folders and
   session; one or two panes for new windows and tabs, tab bar, per-pane
   navigation bar; following symbolic links; view mode, sizes, columns, sorting,
   hidden files; item and bar density; previews and their plugins and size
   limits; Places panel; status bar contents; what opening an executable does;
   confirmations, default drop action and dragging from anywhere in a row;
   context menu contents.
3. **M03 listing depth.** A combined panel that replaces the separate Places and
   Folders panels by default, in one scrolling column: a Favorites section
   (favourite folders with custom titles, renamed, reordered and dropped onto
   it, in place of bookmarks), the KDE Places sections, and a folder tree with
   Home and Root that follows the active pane's folder, shows hidden folders
   when the pane does, and takes drops. Collapsed sections and expanded folders
   are remembered, and View > Panels switches back to separate Places and
   Folders panels. Also extra columns, Dolphin-style grouping, filter syntax,
   type-ahead, per-folder format, labels, ratings, notes, pins, colours, folder
   look dialog, ignore list, flat view, archive browsing and extract/compress,
   tab pin/lock/colour.

   Done so far: the combined panel, the filter bar with type-ahead, the ignore
   list, tab pin/lock/colour and names, archive browsing with extract to the
   other pane, compress (zip, stored zip, tar.gz, tar.xz or tar.zst; KArchive
   has no compression levels, so the format stands in for one), and Show Flat
   with a Relative Path column.

4. **M04 preview and gallery.** Preview panel (images, PDF and office via
   KParts, text, media), gallery with zoom, slideshow, filmstrip, image info,
   edits, convert.
5. **M05 editor.** KTextEditor-based editor (docked, maximised, standalone),
   drafts, markdown preview and PDF export.
6. **M06 tools.** Live search, indexed search, duplicates, content chart, folder
   sync, file diff, batch rename, symlinks, listing export, audio tags, scripts
   and custom commands, presets.
7. **M07 desktop integration.** FileManager1 D-Bus service, default file
   manager, tray, global shortcut, XDG portal file chooser, MCP server, Persian
   translation and RTL, Jalali dates.
