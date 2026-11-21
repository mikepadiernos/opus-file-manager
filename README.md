# Plasma Opus

Plasma Opus is a Linux file manager for KDE Plasma, built with C++20, Qt 6 Widgets
and KDE Frameworks 6. It combines dual-pane browsing and power-user tools
with native KDE file operations, Places and settings.

The latest published MVP is [v0.2.0](https://github.com/mikepadiernos/plasma-opus-file-manager/releases/tag/v0.2.0).
This README describes the current `main` branch, which includes fixes made
after that release.

**[Build and run](#build-and-run)** · **[Features](#features)** ·
**[Safety and limitations](#safety-and-limitations)** · **[Roadmap](docs/ROADMAP.md)** ·
**[Documentation](docs/README.md)**

![Plasma Opus with stacked panes and an image preview](preview.png)

## Build and run

Install a C++20 compiler, CMake 3.25+, Ninja, Qt 6.8+ with Jalali calendar
support, Extra CMake Modules, libarchive and KDE Frameworks 6.20+. See
`CMakeLists.txt` for required components. Baloo development files are optional
and enable indexed search.

```bash
./install.sh --deps  # Print build and optional feature packages; does not install them
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/bin/opus-file-manager
```

Pass folders or URLs as arguments: the first opens in the active pane, the
second in the other pane, and further arguments in tabs in the active pane.
Without arguments, the configured startup or restored session is used.

Run `./install.sh` to build and install for the current user, or
`./install.sh --help` for other options. `./install.sh --default` also changes
the default folder handler. The installer uses `build-plasma` with tests
disabled; set `OPUSFM_BUILD_DIR` to use another build tree. Uninstall using
the same build tree used for installation.
The installer installs the Plasma Opus application icon; file and action icons
continue to use the active desktop theme.

If `build` was created before the project folder was renamed, use a fresh
build directory for the manual build commands above.

Optional integrations include Baloo for indexed search, Konsole for the
embedded terminal and ExifTool for supported media metadata. See
[caveats](docs/CAVEATS.md) for requirements and restrictions, and
[translations](po/README.md) for language setup.

## Features

New windows start with stacked panes, visible tab bars and a right-side
preview placement. Pane navigation starts linked, and activating most files
opens them in the preview panel; both behaviors can be changed in Preferences.

- **Browse:** Use side-by-side or stacked panes with tabs, history and session
  restoration. Choose details, icon, compact or flat views, with filtering
  and per-folder formats.
- **Manage files:** Use KDE file actions, queued transfers, undo, batch rename,
  duplicate finding and listing export. Browse and manage archives, including
  AES-encrypted ZIP creation and protected extraction, checksums and split files.
- **Compare and sync:** Compare files or folders and preview per-item actions
  before synchronizing local folders.
- **Search and automate:** Search local folders or a Baloo index, save searches
  and collections, and preview scripts or commands before running them.
- **Preview and edit:** Preview documents and media, use the image gallery,
  edit text and drafts, and edit supported media metadata. Preview, text and
  metadata panels can be placed beside or below the panes.
- **Terminal:** Embed Konsole below or beside the panes, with optional
  active-folder following. Hiding the panel retains its sessions.

## Customize the interface

- **Panes and panels:** Swap current tabs or whole pane sides, dim the inactive
  pane, and save or reset splitter sizes by orientation. Change combined panel
  section order, visibility and titles; embedded panel titles can be hidden
  separately from navigation-panel titles.
- **Toolbars:** Configure the main and pane toolbars and hamburger menu, choose
  button appearance and lock editing. Toolbar context menus provide editing
  and settings access; the main toolbar also retains KDE's toolbar options.
  Saved scripts and commands can become main-toolbar buttons; running them
  still requires preview and confirmation.
- **Tabs and status:** Position tab bars per pane and layout, or keep them
  together between stacked panes. Show or hide close buttons and configure
  status-bar indicators and their order.
- **Views:** Enable the [file-list minimap](docs/CAVEATS.md#file-list-minimap)
  and set its width. Choose Information panel fields and their order, with
  optional hover information. Set a theme, light, dark or checkerboard
  background for transparent images without changing the image.
- **Backup:** Back up Plasma Opus settings and window and panel layouts, or preview a
  restore that applies on the next launch. Review the
  [backup limitations](docs/CAVEATS.md#settings-backups) before sharing or
  relying on an archive.

Preferences include [tool defaults](docs/ROADMAP.md#m10-tools-settings) and
[language and regional formats](docs/ROADMAP.md#m14-translation-settings).
Path-filter, saved-search, collection, rename-preset and automation management
changes take effect immediately even if Preferences is canceled. KDE File
Associations changes shared desktop settings.

## Safety and limitations

Back up existing `.mfm.json` files before editing them: unsupported metadata
may be discarded. Synchronization is not undoable and can leave partial
results; automatic transfer overwrite can replace files without asking.

See [caveats and limitations](docs/CAVEATS.md) for feature restrictions,
dependencies, precautions and workarounds, and [reported bugs](docs/BUGS.md)
for open issues.
[Edge cases](docs/EDGE.md) lists potential scenarios for investigation; they
are not confirmed bugs.

## Documentation

The [documentation index](docs/README.md) links to the roadmap, issue logs,
design plans and upstream references. The roadmap distinguishes planned work
from completed features.
