# Opus File Manager

Opus is a dual-pane file manager for Linux and KDE Plasma. It keeps source and
destination folders in view while providing tabs, flexible listings and
power-user file operations.

Built with C++20, Qt 6 Widgets and KDE Frameworks 6, Opus uses KDE services
for file operations, Places, properties, settings and desktop integration.

## Features

- Browse in side-by-side or stacked panes, with tabs, history and session
  restoration.
- Use details, icon, compact and flat views with grouping, filtering and
  per-folder formats. The View menu also offers format inheritance, temporary
  pane locks and background folder sizes.
- Navigate with the combined Places and folder-tree panel; use KDE file
  actions, drag and drop, clipboard operations and undo.
- Browse, extract, create and test archives, including AES-encrypted ZIP
  creation and password-protected extraction.
- Preview files and browse images and media in a gallery, with image editing
  and save-copy options.
- Edit text in embedded or standalone KTextEditor views, recover drafts, and
  edit or preview Markdown and export it to PDF. Visual Markdown editing
  requires confirmation because it can rewrite unsupported markup.
- Compare two selected local files in one pane or across both panes using a
  unified diff.
- Create symbolic links next to local files and folders without overwriting.
- Export the filtered listing and configured details columns to CSV or PDF.
- Inspect a folder's content sizes and relative shares in a cancellable chart.
- Create and verify SHA-256 checksum files for local files.
- Compare immediate entries across pane folders and select differences.
- Split local files into numbered parts and combine selected parts in a chosen
  order.
- Show an embedded Konsole terminal alongside the panes.
- Link navigation between corresponding local pane folders.
- Explore folder sizes with a cushion treemap and folder drilldown.
- Search local folders live or query Baloo's index; save searches and collect
  references without moving files. Filter searches and selections by path,
  parent, excluded folders and recursion depth.
- Preview local batch renames with numbering, metadata tokens, regex rules and
  named presets.
- Preview edits to selected JPEG, TIFF and MP3 metadata fields.
- Preview and run scripts or commands with selection placeholders and saved
  command presets.
- Preview local duplicates, select candidates by date or folder, and move
  checked files to Trash after confirmation.

## Caveats

Opus reads and writes a supported subset of Max File Manager's local
`.mfm.json` metadata. Writing sanitizes the file and can discard unsupported
fields or values, so back up existing metadata before editing it with Opus.

Flat view requires the `filenamesearch:/` KIO worker, normally supplied by
`kio-extras`. Document previews require a suitable KPart, and media playback
requires a Qt Multimedia backend. Saving gallery edits does not preserve the
original image metadata.

Encrypted 7z extraction can fail in KArchive; integrity checks use the `7z`
executable when available. Archive creation and extraction also have local-file
and entry-type restrictions. File comparison, symbolic links, checksum files,
and splitting and combining are local-only. Folder comparison checks immediate
entries by metadata, and combined files cannot be checked for missing or
misordered parts. The embedded terminal requires the Konsole KPart, and linked
navigation supports local folders only. Duplicate-finder results require manual
review and verification. Live search is local-only and has scan and content
limits. Metadata editing requires ExifTool and has format and write limitations.
Custom commands run with the user's permissions and have local-context and
output limits. See [caveats and limitations](docs/CAVEATS.md) for details.

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

## Roadmap

The [roadmap](docs/ROADMAP.md) contains 28 milestones. Its strikethroughs
distinguish completed features from planned or in-progress work.

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
  groups and saved workflow layouts.
- [M14 Workspace settings](docs/ROADMAP.md#m14-workspace-settings): workspace
  management, startup, search and layout preferences.
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
  downloadable service menus, terminals, alternative editors, rsync, Atuin and
  Nextcloud, and KDE Online Accounts.
- [M24 Integration settings](docs/ROADMAP.md#m24-integration-settings):
  integration, editor, terminal, tray and access controls.
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
