# Opus File Manager

Opus is a dual-pane file manager for Linux and KDE Plasma. It combines
side-by-side browsing and power-user tools with native KDE file operations,
Places, properties and settings. It is built with C++20, Qt 6 Widgets and
KDE Frameworks 6.

The latest published MVP is [v0.2.0](https://github.com/mikepadiernos/opus-file-manager/releases/tag/v0.2.0).
This README describes the current `main` branch, which includes fixes made
after that release.

**[Build and run](#build-and-run)** · **[Features](#features)** ·
**[Caveats](#caveats)** · **[Roadmap](docs/ROADMAP.md)** ·
**[Documentation](docs/README.md)**

## Build and run

To build, install a C++20 compiler, CMake 3.25 or newer, Ninja, Qt 6.8 or
newer, Extra CMake Modules, libarchive and KDE Frameworks 6.20 or newer. The
required Qt and KDE components are listed in `CMakeLists.txt`. Baloo development
files are optional at build time and enable indexed search.
Qt must be built with Jalali calendar support.

```bash
./install.sh --deps  # Print suggested distribution packages; does not install them
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/bin/opus-file-manager
```

Pass folders or URLs as arguments: the first opens in the active pane, the
second in the other pane, and further arguments in tabs in the active pane.
Without arguments, the configured startup or restored session is used.

Preferences → Translation provides independent interface language, regional
locale, calendar, custom date and time format, layout direction and digit
choices. Restart Opus to fully apply these changes. Custom formats override
listing defaults; an explicit calendar overrides the ISO listing's automatic
Gregorian calendar. Catalogues may be incomplete; untranslated text falls
back to the source language. See [translation limitations](docs/CAVEATS.md#translation-format-overrides)
and [Jalali date limitations](docs/CAVEATS.md#jalali-dates).

Run `./install.sh` to build and install for the current user, or
`./install.sh --help` for other options. `./install.sh --default` also changes
the default folder handler. The installer disables tests in its build tree;
reconfigure with `-DBUILD_TESTING=ON` to run them.

## Features

- **Browse:** Use side-by-side or stacked panes with tabs, history and session
  restoration. Details, icon, compact and flat views support grouping,
  filtering and per-folder formats. The combined Places/Folders panel provides
  navigation; format inheritance, pane locks and background folder sizes are
  available from the View menu.
- **Manage files:** Use KDE file actions, drag and drop, clipboard operations
  and undo. Queue copy and move jobs; preview batch renames, find duplicates,
  create symbolic links, and export listings to CSV or PDF.
- **Archives and tools:** Browse, extract, create and test archives, including
  AES-encrypted ZIP creation and password-protected extraction. Create and
  verify SHA-256 checksums, split and combine files, and inspect folder sizes
  with a chart and cushion treemap.
- **Compare and sync:** Compare local files using a unified diff, compare
  immediate pane entries using metadata, or compare local folders recursively
  with filters and profiles. Preview per-item actions before synchronizing
  local folders.
- **Search and automate:** Search local folders live or query Baloo's index;
  save searches and collect references without moving files. Filter searches
  and selections by path, parent, exclusions and depth. Preview and run
  scripts and commands with selection placeholders and saved presets.
- **Preview and edit:** Preview documents and media, browse and edit images in
  a gallery, and edit text in embedded or standalone KTextEditor views. Recover
  drafts, edit or preview Markdown, export it to PDF and edit supported JPEG,
  TIFF and MP3 metadata fields.
- **Terminal and navigation:** Show an embedded Konsole terminal below or beside
  both panes and link navigation between corresponding local folders. The
  terminal panel menu and close button hide the panel without ending sessions;
  its messages can be dismissed. Optional terminal folder following switches
  between retained sessions instead of changing the directory of a running shell.

## Customize the interface

- **Panes and panels:** Swap current tabs or whole pane sides, dim the inactive
  pane, and save or reset splitter sizes by orientation. Change combined panel
  section order, visibility and titles.
- **Toolbars:** Configure the main and pane toolbars and hamburger menu, choose
  button appearance and lock editing. Saved scripts and commands can become
  toolbar buttons; running them still requires preview and confirmation.
- **Tabs and status:** Position tab bars per pane and layout, or keep them
  together between stacked panes. Show or hide close buttons and configure
  status-bar indicators and their order.
- **Views:** Enable the [file-list minimap](docs/CAVEATS.md#file-list-minimap)
  and set its width. Choose Information panel fields and their order, with
  optional hover information. Set a theme, light, dark or checkerboard
  background for transparent images without changing the image.
- **Backup:** Back up Opus settings and window and panel layouts, or preview a
  restore that applies on the next launch. Review the
  [backup limitations](docs/CAVEATS.md#settings-backups) before sharing or
  relying on an archive.

## Tool preferences

Preferences → Tools contains defaults and management controls for the file
tools:

- **Search:** Set the initial scope and filename/content mode, manage saved
  searches, collection snapshots and reusable path filters, or open KDE File
  Search to configure the shared Baloo index. Saved searches override the
  initial defaults. Loading a collection replaces its references, not files.
- **Comparison and sync:** Set duplicate-selection criteria, chart display,
  linked-pane startup, diff wrapping and pane comparison categories. Folder
  sync defaults cover comparison options, copy policy and speed; profiles can
  override comparison options. Changing copy policy clears the plan, and
  overwrites and deletions still require review and confirmation.
- **File tools:** Set the link-name suffix, checksum filename and split-part
  size; edit them during each operation. Choose listing-export format and
  columns, batch-rename presets, and initially displayed metadata fields.
  Hidden metadata fields are preserved.
- **Automation and transfers:** Manage saved scripts and command presets;
  configure transfer concurrency and conflict handling. See the
  [transfer queue limitations](docs/CAVEATS.md#transfer-queue) before choosing
  an overwrite policy.
- **Applications:** Open KDE File Associations to manage filename patterns and
  default applications; changes affect shared desktop settings.

Path-filter, saved-search, collection, rename-preset and automation management
changes take effect immediately, even if Preferences is canceled.

## Caveats

Back up existing `.mfm.json` files before editing them: unsupported metadata
may be discarded. Synchronization is not undoable and can leave partial
results; automatic transfer overwrite can replace files without asking.

See [caveats and limitations](docs/CAVEATS.md) for feature restrictions,
dependencies, precautions and workarounds, and [reported bugs](docs/BUGS.md)
for open issues, including the toolbar context-menu and icon report.
[Edge cases](docs/EDGE.md) lists potential scenarios for investigation; they
are not confirmed bugs.

## Planned work

The next planned milestones cover [advanced search](docs/ROADMAP.md#m15-advanced-search)
and [native tagging](docs/ROADMAP.md#m17-native-tagging), followed by an optional
[Yazi-inspired browsing mode](docs/ROADMAP.md#m19-yazi-mode),
[workspaces](docs/ROADMAP.md#m21-workspaces) and
[desktop integrations](docs/ROADMAP.md#m23-integrations), including enhanced
Baloo support. These are planned, not yet available; see the roadmap for their
scope and status. [Keyboard shortcuts](docs/ROADMAP.md#m29-keyboard-shortcut-settings)
and [UI/UX polish](docs/ROADMAP.md#m30-ui-and-ux-polish) follow later; the
latter also plans accessibility, motion controls, startup and settings
performance, measured Rust-backed optimizations, and extensive functional and
security testing.

## Documentation

| Document | Purpose |
| --- | --- |
| [Roadmap](docs/ROADMAP.md) | Architecture, feature plans and milestone status |
| [Pending tasks](docs/TODO.md) | Blocked work and handoffs |
| [Reported bugs](docs/BUGS.md) | Bug reports and completion status |
| [Edge cases](docs/EDGE.md) | Potential boundary conditions and test scenarios |
| [Scope](docs/philosophy/scope.md) | Product goals and design boundaries |
| [Inspirations](docs/philosophy/inspirations.md) | Sources and their influence |
| [Documentation index](docs/README.md) | All project documents and behaviour references |
