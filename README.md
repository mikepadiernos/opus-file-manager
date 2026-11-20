# Opus File Manager

Opus is a dual-pane file manager for Linux and KDE Plasma. It combines
side-by-side browsing and power-user tools with native KDE file operations,
Places, properties and settings. It is built with C++20, Qt 6 Widgets and
KDE Frameworks 6.

## Build and run

To build, install a C++20 compiler, CMake 3.25 or newer, Ninja, Qt 6.8 or
newer, Extra CMake Modules, libarchive and KDE Frameworks 6.20 or newer. The
required Qt and KDE components are listed in `CMakeLists.txt`. Baloo development
files are optional at build time and enable indexed search.

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
  both panes and link navigation between corresponding local folders. Optional
  terminal folder following switches between retained sessions instead of
  changing the directory of a running shell.

## Customize the interface

The View menu can swap current tabs or entire pane sides, dim the inactive
pane, and save or reset pane and panel sizes separately for side-by-side and
stacked layouts. **Customize Combined Panel** changes section order, visibility
and titles.

Use **Configure Toolbars** to add saved scripts and commands as buttons; they
still require preview and confirmation before running.

Use **Active Pane Tab Bar Position** to choose a position for each pane and
layout. In stacked mode, **Keep Stacked Tab Bars Together** puts the bars
between the panes; choosing an individual position turns that option off.
**Show Tab Close Buttons** applies to both panes. Choose a theme, light, dark or
checkerboard background for transparent images under Preferences → Gallery →
Display; this does not change the image.

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
  configure transfer concurrency and conflict handling. Automatic overwrite
  replaces files and merges folders without asking. Paste and drag-and-drop
  bypass the queue and its defaults.
- **Applications:** Open KDE File Associations to manage filename patterns and
  default applications; changes affect shared desktop settings.

Path-filter, saved-search, collection, rename-preset and automation management
changes take effect immediately, even if Preferences is canceled.

## Caveats

- Back up existing `.mfm.json` metadata before editing: Opus sanitizes the
  supported subset and may discard unsupported fields.
- Flat view requires the `filenamesearch:/` KIO worker, usually in
  `kio-extras`. Document previews require a suitable KPart, media playback
  requires a Qt Multimedia backend, and the embedded terminal requires the
  Konsole KPart. Gallery edits do not preserve image metadata.
- Encrypted 7z extraction can fail in KArchive; integrity checks use `7z` when
  available. Archive operations have local-file and entry-type restrictions.
- Live search, linked navigation, file comparison, symbolic-link creation,
  checksums and splitting or combining files are local-only. Search and
  recursive comparisons have limits; duplicate results require manual review.
  Synchronization is local-only, is not undoable and can leave partial results.
  Combined files cannot be checked for missing or misordered parts.
- Metadata editing requires ExifTool and has format and write limitations.
  Custom commands run with the user's permissions. Visual Markdown editing
  requires confirmation because it can rewrite unsupported markup.

See [caveats and limitations](docs/CAVEATS.md) for details and recommendations.

## Roadmap

The [roadmap](docs/ROADMAP.md) contains 28 milestones. Items are struck through
when their issues reach Completed; unstruck items can include implemented work
still in Review.

- [M01 Core dual pane](docs/ROADMAP.md#m01-core-dual-pane): browsing, tabs and
  essential file operations.
- [M02 Core dual pane settings](docs/ROADMAP.md#m02-core-dual-pane-settings):
  startup, navigation, views and desktop integration preferences.
- [M03 Listing depth](docs/ROADMAP.md#m03-listing-depth): richer views,
  filtering, metadata, archives and navigation.
- [M04 Listing depth settings](docs/ROADMAP.md#m04-listing-depth-settings):
  listing, panel, archive and tab preferences.
- [M05 Preview and gallery](docs/ROADMAP.md#m05-preview-and-gallery): file
  previews, media browsing and image editing.
- [M06 Preview and gallery settings](docs/ROADMAP.md#m06-preview-and-gallery-settings):
  preview, slideshow and image output preferences.
- [M07 Editor](docs/ROADMAP.md#m07-editor): embedded and standalone editing,
  drafts and Markdown export.
- [M08 Editor settings](docs/ROADMAP.md#m08-editor-settings): editor placement,
  text, draft and export preferences.
- [M09 Tools](docs/ROADMAP.md#m09-tools): search, comparison, synchronization,
  automation and other file tools.
- [M10 Tools settings](docs/ROADMAP.md#m10-tools-settings): tool defaults,
  profiles, terminal placement and transfer preferences.
- [M11 UI Customizations](docs/ROADMAP.md#m11-ui-customizations): configurable
  toolbars, tabs, status bars, panes, panels and layouts.
- [M12 UI Customization settings](docs/ROADMAP.md#m12-ui-customization-settings):
  customization, layout backup and reset controls.
- [M13 Workspaces](docs/ROADMAP.md#m13-workspaces): focused locations, tag
  groups, saved workflow layouts and per-pane breadcrumb modes.
- [M14 Workspace settings](docs/ROADMAP.md#m14-workspace-settings): workspace
  management, startup, search, layout and location-bar preferences.
- [M15 Translations](docs/ROADMAP.md#m15-translations): translations,
  right-to-left layout and Jalali dates.
- [M16 Translation settings](docs/ROADMAP.md#m16-translation-settings):
  language, regional formats and layout preferences.
- [M17 Settings dialog polish](docs/ROADMAP.md#m17-settings-dialog-polish):
  discoverability, accessibility and consistent settings controls.
- [M18 Keyboard shortcut settings](docs/ROADMAP.md#m18-keyboard-shortcut-settings):
  customizable bindings, Vim-style navigation and shortcut hints.
- [M19 Advanced search](docs/ROADMAP.md#m19-advanced-search): an embedded
  search panel, toolbar widget and combined search criteria.
- [M20 Advanced search settings](docs/ROADMAP.md#m20-advanced-search-settings):
  panel, toolbar and search behaviour preferences.
- [M21 Native tagging](docs/ROADMAP.md#m21-native-tagging): tag libraries,
  relationships, metadata and search.
- [M22 Tagging settings](docs/ROADMAP.md#m22-tagging-settings): library,
  tag, field and privacy preferences.
- [M23 Integrations](docs/ROADMAP.md#m23-integrations): desktop services,
  downloadable service menus, terminals, alternative editors, rsync, Atuin,
  Nextcloud and KDE Online Accounts.
- [M24 Integration settings](docs/ROADMAP.md#m24-integration-settings):
  provider, account, sync, terminal, editor and tray controls.
- [M25 Themes](docs/ROADMAP.md#m25-themes): optional app-local colour schemes,
  custom Qt stylesheets, profiles and accessible previews.
- [M26 Theme settings](docs/ROADMAP.md#m26-theme-settings): colours,
  stylesheets and theme profiles.
- [M27 Branding](docs/ROADMAP.md#m27-branding): application identity, icon
  assets and presentation.
- [M28 Branding settings](docs/ROADMAP.md#m28-branding-settings): branding
  presentation preferences.

## Inspirations

Max File Manager is the main behaviour reference; Dolphin guides KDE
integration, and Directory Opus and Krusader inform power-user workflows.
Earlier project notes also cite Orcinus; TagSpaces, TagStudio and
which-key.nvim inspire planned features. See
[Inspirations](docs/philosophy/inspirations.md) for details.

## Project documentation

- [Scope and objective](docs/philosophy/scope.md)
- [Roadmap and milestones](docs/ROADMAP.md)
- [Caveats and limitations](docs/CAVEATS.md)
- [Documentation index](docs/README.md)
