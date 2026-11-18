# Opus File Manager

A dual-pane file manager for KDE Plasma, written in C++20 with Qt 6 Widgets and
KDE Frameworks 6. Linux only. Inspired by Max File Manager.

## Objective

Bring the Directory Opus experience to Linux. GPSoft's Directory Opus is the
benchmark for power-user file management on Windows (dual panes, folder tabs,
viewer pane, flat view, scriptable commands, metadata everywhere), and Opus File
Manager aims to get as close to it as a native Plasma app can.

It does this as a native Plasma application rather than a cross-platform shell.
Wherever a file manager would normally roll its own solution (themes, copy
engine, places, Open With, properties, undo), Opus uses the KDE Frameworks
equivalent instead, so it shares icons, colour schemes, shortcuts, service
menus, ratings and folder icons with Dolphin and the rest of the desktop.

It should also be trivial to install: one script, no extra toolchains, no
AppImage, no hand-edited `.desktop` files.

The architecture, Plasma-native decisions and milestones live in
`docs/ROADMAP.md`.

## Features

Struck-through items are implemented. Everything else is planned.

Milestones alternate between features and settings: each odd-numbered milestone
adds features, and the even-numbered milestone after it adds the settings for
those features.

### M01 Core dual pane

- ~~KXmlGui main window with hamburger menu, editable toolbar and shortcuts~~
- ~~Two panes with tabs, back/forward history and a `KUrlNavigator`~~
- ~~Details and icon views over `KDirModel`, with previews~~
- ~~Places panel (`KFilePlacesView`: devices, mount, eject)~~
- ~~Open, Open With and service menus (`KFileItemActions`)~~
- ~~Copy and move to the other pane (F5/F6)~~
- ~~Cut, copy, paste and copy path via the clipboard~~
- ~~Drag and drop between panes and onto Places~~
- ~~Move to trash, delete, rename, new folder and new file templates~~
- ~~Undo through `KIO::FileUndoManager`~~
- ~~Show hidden files~~
- ~~Open terminal here~~
- ~~Properties dialog (`KPropertiesDialog`, includes checksums)~~
- ~~Free space in the status bar~~
- ~~Single instance (`KDBusService`)~~
- ~~Session restore of panes, tabs and history~~
- ~~Stacked dual-pane layout: panes split top/bottom with both tab bars meeting
  in the middle (top pane's tabs along its bottom edge, bottom pane's tabs along
  its top edge)~~

### M02 Core dual pane settings

Settings for each M01 feature, modelled on Dolphin's settings and on Orcinus, a
Dolphin fork for power users, and stored in KConfig (`opus-file-managerrc`):

- Preferences dialog with one page per feature
- ~~Startup: home folder, restore the last session or open fixed folders~~
- ~~Panes and tabs: open new windows and tabs in one or two panes (as last used,
  always or never), always show the tab bar, Tab switches panes, where new tabs
  open, show or hide each pane's navigation bar~~
- ~~Navigation: always follow symbolic links to their target~~
- Views: default view mode, icon and preview sizes, visible columns, sort order,
  folders first, hidden files, tooltips
- Density: space around items for each view mode (normal, compact, very compact
  or custom) and height of the toolbars, location bars and status bar
- Previews: on or off, which preview plugins run, size limits for local and
  remote files
- Places panel: shown or hidden, which sections appear
- Status bar: free space, item counts and selection size
- Opening files: what opening an executable does (ask, open in its application,
  run, or run in a terminal that stays open or closes)
- File operations: confirm trash, delete and closing multiple tabs; default drop
  action; drag from anywhere in a details row
- Context menu: Copy To and Move To submenus, which actions and service menus
  appear

### M03 Listing depth

- Extra columns and Dolphin-style grouping
- Filter syntax and type-ahead
- Per-folder view format
- Labels, ratings, notes, pins and colours
- Folder look dialog (icon and colour)
- Ignore list
- Tree pane and flat view
- Archive browsing, extract and compress
- Bookmarks with custom titles
- Tab pin, lock and colour

### M04 Listing depth settings

- Default visible columns and grouping
- Size format (human or bytes), date format and seconds
- Name lines and metadata shown in icon view
- Ignore list entries
- File type colours by extension
- Tree pane position (off, left or right) and auto-focus
- Archive compression level
- Tab icons

### M05 Preview and gallery

- Preview panel: images, PDF and office via KParts, text, media
- Gallery with zoom, slideshow and filmstrip
- Image info, edits and convert

### M06 Preview and gallery settings

- What double-click opens (application, gallery or preview)
- Size limit for text in the preview panel
- Slideshow interval and looping
- Image info panel shown by default
- Default format and quality for image convert

### M07 Editor

- KTextEditor-based editor (docked, maximised, standalone)
- Drafts
- Markdown preview and PDF export

### M08 Editor settings

- Where files open (docked, maximised or own window)
- Font size and word wrap
- Default markdown mode (visual or source)
- Drafts on or off
- KTextEditor's own configuration pages

### M09 Tools

- Live search and indexed search
- Duplicate finder
- Content chart (folder breakdown)
- Folder sync
- File diff
- Batch rename
- Symlinks
- Listing export
- Audio tags
- Scripts and custom commands
- Presets

### Packaging

- ~~One-step `install.sh`: builds, installs for the current user or system-wide,
  prints the dependency packages for the detected distro, optionally registers
  Opus as the default folder handler, and cleanly uninstalls~~
- Distro packages (AUR, Flatpak)

### M10 Tools settings

- Search shortcut (double Shift), search folders and exclusions
- Folder sync: skip existing, newer only, speed limit
- Default applications by extension
- Scripts and custom commands
- Presets

### M11 Desktop integration

- `org.freedesktop.FileManager1` D-Bus service
- Default file manager registration
- System tray and global shortcut
- XDG portal file chooser: Opus serves `org.freedesktop.impl.portal.FileChooser`
  so Open and Save dialogs across the desktop are replaced by Opus
- MCP server
- Persian translation, RTL layout and Jalali dates

### M12 Desktop integration settings

- Minimise and close to tray
- Global shortcut on or off and its key combination
- Serve the XDG portal file chooser on or off
- Set as default file manager
- MCP server access (off, read or write), token and audit log
- Language and calendar (Gregorian or Jalali)

## Installing

```bash
./install.sh --deps      # prints the package command for your distro; run it
./install.sh             # build + install into ~/.local, no root needed
./install.sh --default   # ... and make Opus the default folder handler
```

Other options: `--system` installs under `/usr/local` with `sudo`,
`--prefix DIR` installs anywhere else, `--uninstall` removes exactly what was
installed and hands the folder association back to Dolphin. The script is
re-runnable and never masks or removes Dolphin. The installed `.desktop` entry
carries the absolute binary path, so a user install works even when
`~/.local/bin` is not on `PATH`.

## Building from source

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/bin/opus-file-manager [path...]
```

Format all C++ sources with `cmake --build build --target clang-format`.

## Layout

- `src/core/` — `opusfm_core` static library: widget-free logic (tabs, history,
  session serialisation) covered by QtTest suites in `tests/`.
- `src/app/` — widgets and the `opus-file-manager` executable.
- `docs/` — roadmap and feature inventories.
