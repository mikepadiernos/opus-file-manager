# 🗺️ Plasma Opus roadmap

**[Documentation](README.md)** · **[Scope](philosophy/scope.md)** ·
**[Inspirations](philosophy/inspirations.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

Plasma Opus is a dual-pane KDE Plasma file manager built with C++20, Qt 6
Widgets and KDE Frameworks 6. The
[upstream inventories](README.md#behaviour-references) record the Max File
Manager behaviour that informs its parity goals.

> [!NOTE]
> The latest published MVP is
> [v0.2.0](https://github.com/mikepadiernos/plasma-opus/releases/tag/v0.2.0),
> released after M14. M15 Workspaces, M16 Workspace settings, M17 Themes,
> M18 Theme settings, M19 Branding, M20 Branding settings, M21 Advanced
> search and M22 Advanced search settings are merged into `main`. See
> [pending tasks](TODO.md) for blocked work and branch-specific handoffs.

## At a glance

Status follows each milestone's parent issue on the
[project board](https://github.com/users/mikepadiernos/projects/3).

|     | Milestones | Focus                                        | Status         |
| --- | ---------- | -------------------------------------------- | -------------- |
| 🧱  | M01–M04    | Core dual pane and listing depth             | ✅ Merged      |
| 🖼️  | M05–M08    | Preview, gallery and editor                  | ✅ Merged      |
| 🧰  | M09–M14    | Tools, interface customization, translations | ✅ Merged      |
| 🗂️  | M15–M16    | Workspaces and their settings                | ✅ Merged      |
| 🎨  | M17–M18    | Themes and their settings                    | ✅ Merged      |
| 🏷️  | M19–M20    | Branding and its settings                    | ✅ Merged      |
| 🔎  | M21–M22    | Advanced search and its settings             | ✅ Merged      |
| 🔖  | M23–M26    | Native tagging and Yazi mode                 | 🚧 In progress |
| 🔌  | M27–M31    | Integrations, Nextcloud and shortcuts        | 📋 Planned     |
| ✨  | M32        | Refinements                                  | 📋 Planned     |

## Contents

- [At a glance](#at-a-glance)
- [Architecture](#architecture) ·
  [Plasma-native decisions](#plasma-native-decisions)
- [Milestones](#milestones)
- [M01-M04: Core and listings](#m01-core-dual-pane)
  - [M01](#m01-core-dual-pane) · [M02](#m02-core-dual-pane-settings) ·
    [M03](#m03-listing-depth) · [M04](#m04-listing-depth-settings)
- [M05-M08: Preview and editor](#m05-preview-and-gallery)
  - [M05](#m05-preview-and-gallery) · [M06](#m06-preview-and-gallery-settings) ·
    [M07](#m07-editor) · [M08](#m08-editor-settings)
- [M09-M14: Tools, interface and translations](#m09-tools)
  - [M09](#m09-tools) · [M10](#m10-tools-settings) ·
    [M11](#m11-ui-customizations) · [M12](#m12-ui-customization-settings) ·
    [M13](#m13-translations) · [M14](#m14-translation-settings)
- [M15-M20: Workspaces, themes and branding](#m15-workspaces)
  - [M15](#m15-workspaces) · [M16](#m16-workspace-settings) · [M17](#m17-themes)
    · [M18](#m18-theme-settings) · [M19](#m19-branding) ·
    [M20](#m20-branding-settings)
- [M21-M26: Search, tagging and Yazi](#m21-advanced-search)
  - [M21](#m21-advanced-search) · [M22](#m22-advanced-search-settings) ·
    [M23](#m23-native-tagging) · [M24](#m24-tagging-settings) ·
    [M25](#m25-yazi-mode) · [M26](#m26-yazi-mode-settings)
- [M27-M32: Integrations and refinements](#m27-integrations)
  - [M27](#m27-integrations) · [M28](#m28-integration-settings) ·
    [M29](#m29-nextcloud-integration) · [M30](#m30-nextcloud-settings) ·
    [M31](#m31-keyboard-shortcut-settings) · [M32](#m32-refinements)
- [Feature references](#feature-references) · [Tracking](#tracking)

## Architecture

- `src/core/` is the `opusfm_core` static library for logic without widgets.
- `src/app/` holds the widgets and the `opus-file-manager` executable.
- `tests/` holds QtTest suites run through `ctest`; GUI tests use
  `QT_QPA_PLATFORM=offscreen`.

Rust is a candidate for self-contained core logic, including measured
performance hotspots, not a wholesale rewrite of C++ features. Keep widgets,
settings and file operations in Qt/KDE, including KIO for local and remote
operations. The M21 Rust pilot can inform later work, including consideration of
M09 duplicate detection and rename or sync planning; this evaluation does not
block M09 or change KIO's role. See the [pilot evaluation](rust-prototype.md)
for its findings and the criteria for later Rust components.

## Plasma-native decisions

| Upstream approach                            | Plasma Opus approach                                                                                         |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Custom themes, title bars and window buttons | Plasma widget style, colour scheme, fonts and KWin decorations by default; optional app-local colours in M17 |
| Bundled icon pack                            | Active icon theme through `QIcon::fromTheme`                                                                 |
| Hand-written toolbar and shortcut editors    | KXmlGui, `KEditToolBar` and `KShortcutsDialog`                                                               |
| Settings in SQLite                           | KConfig; session state in the KConfig state file                                                             |
| Custom copy engine and undo                  | KIO jobs and `KIO::FileUndoManager`                                                                          |
| Custom places and device polling             | `KFilePlacesView` and Solid                                                                                  |
| Custom Open With and default-app logic       | `KFileItemActions`, `KOpenWithDialog` and `KApplicationTrader`                                               |
| Custom properties and checksums              | `KPropertiesDialog`                                                                                          |
| Custom clipboard helpers                     | `QClipboard` and `KIO::setClipboardDataCut`                                                                  |

Plasma Opus uses a supported subset of the upstream `.mfm.json` schema for local
per-folder metadata, including pins, labels, ratings, notes, colours and view
formats. See [metadata compatibility](CAVEATS.md#folder-metadata-and-listings)
for limitations on writes and unsupported behaviour.

## Milestones

Checked boxes mark features whose issues are Completed or Merged. Unchecked
features may be incomplete, not started or implemented but still in Review.
Where applicable, feature milestones are followed by their settings in KConfig.
The order below is the intended work sequence. Within M15–M32, remaining
features are ordered by priority, effort and dependencies within their groups.

### M01: Core dual pane

**Project:**
[M01 milestone](https://github.com/mikepadiernos/plasma-opus/issues/12)

- [x] KXmlGui main window with hamburger menu, editable toolbar and shortcuts
- [x] Theme icons for app actions and pane controls, preferring symbolic
      variants when available and preserving file and device emblems
- [x] Two panes with tabs, back/forward history and a `KUrlNavigator`
- [x] Details and icon views over `KDirModel`, with previews
- [x] Places panel (`KFilePlacesView`: devices, mount, eject)
- [x] Open, Open With and service menus (`KFileItemActions`)
- [x] Copy and move to the other pane (F5/F6)
- [x] Cut, copy, paste and copy path via the clipboard
- [x] Drag and drop between panes and onto Places
- [x] Move to trash, delete, rename, new folder and new file templates
- [x] Undo through `KIO::FileUndoManager`
- [x] Show hidden files
- [x] Open terminal here
- [x] Properties dialog (`KPropertiesDialog`, includes checksums)
- [x] Free space in the status bar
- [x] Single instance (`KDBusService`)
- [x] Session restore of panes, tabs and history
- [x] Stacked dual-pane layout: panes split top/bottom with both tab bars
      meeting in the middle (top pane's tabs along its bottom edge, bottom
      pane's tabs along its top edge)

### M02: Core dual pane settings

**Project:**
[M02 milestone](https://github.com/mikepadiernos/plasma-opus/issues/13)

Settings for M01 features, modelled on Dolphin and Orcinus. Plasma Opus settings
are stored in KConfig; Places sections and trash/delete confirmations are shared
with Dolphin.

- **General**

  - [x] **Settings Dialog:** sidebar icon size (default 24 px) and title
        position (to the right of icons by default, or below)

- **Interface**

  - [x] **Folders and Tabs:** startup folders and session; one or two panes in
        new windows; tab bars; Tab switches panes; new-tab placement; navigation
        bars; symbolic-link following
  - [x] **Previews:** enablement, plugins, size limit and remote folders
  - [x] **Confirmations:** trash, delete, multiple tabs; executable handling;
        default drop action
  - [x] **Panels:** Places panel visibility and visible sections; optional focus
        return to the active pane after panel navigation
  - [x] **Status and Location Bars:** status-bar contents and bar density,
        including custom bar spacing

- **View**

  - [x] **General:** default view mode, sorting, hidden files and tooltips
  - [x] **Details:** icon size, columns, item density and dragging from anywhere
        in a row
  - [x] **Icons:** icon and preview size and item density

- **Context Menu**

  - [x] **General:** optional Copy To and Move To submenus, independently
        configurable alongside the additional KDE submenus
  - [x] **Actions:** choose which built-in actions appear (Plasma Opus actions;
        additional KDE transfer submenus are controlled separately)
  - [x] **Service Menus:** choose which installed service actions and plugins
        appear in Plasma Opus separately from Dolphin-shared settings

### M03: Listing depth

**Project:**
[M03 milestone](https://github.com/mikepadiernos/plasma-opus/issues/14)

- [x] Combined Places/Folders panel: combines Places and folder navigation in
      one panel by default instead of showing separate Places and Folders
      panels. Favorites replace bookmarks.
- [x] Extra columns and Dolphin-style grouping
- [x] Filter syntax and type-ahead
- [x] Per-folder view format
- [x] Labels, ratings, notes, pins and colours
- [x] Folder look dialog (icon and colour)
- [x] Ignore list
- [x] Tree pane
- [x] Show Flat (Ctrl+Shift+F): list every file below the current folder in one
      list without folders. Navigation, tabs and history remain rooted at that
      folder; sorting, filtering, previews and file operations continue to work.
      In details view, Type becomes Relative Path. The mode is remembered per
      folder; Create New, New Folder and Paste are disabled, and dropping files
      is refused.
- [x] Browse archives and extract files
- [x] Compress local files: choose an archive name, format and destination (this
      folder or the other pane); never overwrite an existing file. Run in the
      background and report success or errors in the status bar. Supported
      formats are ZIP, uncompressed ZIP, tar.gz, tar.xz and tar.zst.
- [x] Tabs: show folder icons; fill the tab-bar width equally as tabs are added;
      pin/lock behaviour; custom tab names; folder-based colours; restore tabs
      with the session
- [x] Select group: select or deselect by pattern, with saved selection filters
      (Krusader)
- [x] Occupied space of files, folders, archives and remote locations (Krusader)
- [x] Popular folders and a jump-back point (Krusader)
- [x] Archive integrity testing for local ZIP, TAR and 7z files, with password
      prompts for encrypted archives (Krusader)
- [x] Password-protected AES ZIP creation and extraction (Krusader)
- [x] Password-protected 7z extraction
- [x] Display style per pane (Orcinus)
- [x] Expandable folders inline in the file listing (Orcinus)
- [x] Persistent filters (Orcinus)
- [x] File-name highlighting (Orcinus)
- [x] Compact view with names beside icons and entries arranged in columns
      (Dolphin)
- [x] Optional selection markers and a selection mode with an action bar, so
      multiple items can be selected without modifier keys (Dolphin)
- [x] Details-view column-header controls to show or hide columns and fit column
      widths to their contents (Dolphin)
- [x] Folder-format rules for a folder and optionally its descendants, with
      explicit precedence over defaults; show which rule is active and allow a
      pane to temporarily lock its current format
- [x] Restore selection and scroll position when returning through a tab's
      navigation history, ignoring items that no longer exist
- [x] Calculate folder sizes in the background for display in the listing, on
      demand or automatically according to location and folder-format rules;
      optionally refresh calculated sizes periodically

**Caveats:** [Archives](CAVEATS.md#archives) and
[folder metadata and listings](CAVEATS.md#folder-metadata-and-listings). Tracked
in [#36], [#255], [#256], [#257], [#258], [#261], [#262], [#263] and [#265].

### M04: Listing depth settings

**Project:**
[M04 milestone](https://github.com/mikepadiernos/plasma-opus/issues/15)

Settings for M03 listing depth, stored in KConfig.

- **View**

  - [x] **General:** default visible columns and grouping; size format (human or
        bytes), date format and seconds
  - [x] **Icons:** name lines and metadata shown in icon view
  - [x] **Details:** enable inline folder expansion and choose automatic or
        manual column widths by default
  - [x] **Compact:** icon size, item spacing and additional information
  - [x] **Selection:** default selection markers and selection mode
  - [x] **Filters:** default scope, type-ahead activation, filter retention when
        changing folders, and file-name match highlighting
  - [x] **Folder formats:** manage inherited rules and default size calculation;
        allow separate local and remote policies
  - [x] **Folder sizes:** optional periodic refresh of calculated sizes
  - [x] **History:** restore selection and scroll position when navigating back
        or forward

- **Files**

  - [x] **Rules:** ignore list entries and file-type colours by extension
  - [x] **Metadata:** show or hide label, rating, colour and pin indicators;
        choose whether pinned items appear first without changing stored
        metadata
  - [x] **Selection filters:** manage saved selection patterns

- **Panels**

  - [x] **Combined Places/Folders:** show or hide the panel, choose its left or
        right position, configure its auto-focus behaviour, and show or hide
        each section independently
  - [x] **Titles:** independently show or hide navigation and embedded panel
        titles while retaining panel controls and document status

- **Archives**

  - [x] **Default format:** archive format used by Compress (ZIP, uncompressed
        ZIP, tar.gz, tar.xz or tar.zst); the format choice determines
        compression
  - [x] **Destination:** default to the current folder or the other pane when
        creating an archive, retaining the non-overwriting behaviour

- **Tabs**

  - [x] **Appearance:** show tab icons; fill the available tab-bar width and
        divide it equally among tabs as new tabs are created
  - [x] **Colours:** show or hide folder-based tab colours without removing them

**Caveats:**
[Folder metadata and listings](CAVEATS.md#folder-metadata-and-listings), tracked
in [#257], [#258], [#262] and [#263].

### M05: Preview and gallery

**Project:**
[M05 milestone](https://github.com/mikepadiernos/plasma-opus/issues/16)

- [x] Preview panel: images (up to 8 MiB and 40 megapixels) and UTF-8 text
      (first 1 MiB by default), PDF and office documents with an installed
      KPart, and audio/video with an installed Qt Multimedia backend, following
      the active selection (F3)
- [x] Remote previews: files on KIO locations are downloaded to a temporary file
      first, within the same image and text limits and up to 128 MiB for other
      files
- [x] Preview image navigation, gallery and Open controls; formatted HTML and
      Markdown with a source toggle (external resources and links are disabled)
- [x] Local gallery with zoom, a looping slideshow, thumbnails, image
      information, EXIF metadata, dominant colours and a luminance histogram
      (Space); follows the pane's sort order or the selected images
- [x] Animated images and video playback in the gallery
- [x] Rotate, flip, crop, resize and adjust colours (brightness, contrast,
      saturation, greyscale); save a PNG/JPEG/WebP copy or replace the original
- [x] Save-copy format selection and JPEG/WebP quality
- [x] Line-numbered, syntax-highlighted text previews and preview footer
      ID3/EXIF metadata
- [x] Mark images while browsing the gallery and apply file operations to the
      marked selection, without moving files until the user chooses an action
      (Directory Opus)
- [x] Batch image conversion with output format, dimensions and quality, a
      destination preview and non-overwriting names (Max)
- [x] Optional autoplay for audio and video in the preview panel; keep
      click-to-play available independently of gallery playback

**Caveats:** [Previews and gallery](CAVEATS.md#previews-and-gallery), tracked in
[#259], [#260], [#264], [#266], [#267], [#302], [#303] and [#304].

### M06: Preview and gallery settings

**Project:**
[M06 milestone](https://github.com/mikepadiernos/plasma-opus/issues/17)

Settings for M05 preview and gallery, stored in KConfig.

- **Opening**

  - [x] **Activation:** what activating a file opens (application, gallery or
        preview)

- **Preview**

  - [x] **Text:** size limit for text in the preview panel
  - [x] **Presentation:** default to formatted or source view for HTML and
        Markdown; show line numbers and syntax highlighting in text previews
  - [x] **Media:** autoplay audio and video in the preview panel or require play

- **Gallery**

  - [x] **Information:** image info panel shown by default
  - [x] **Display:** initial zoom (fit or actual size) and thumbnail visibility

- **Slideshow**

  - [x] **Playback:** slideshow interval and looping

- **Image conversion**

  - [x] **Output:** default format and quality for saved image copies
  - [x] **Batch:** default dimensions, format, quality and destination for batch
        conversion; keep output names non-overwriting

File activation follows the desktop's single- or double-click setting and also
applies to keyboard activation. Gallery mode opens local images and videos;
other files open in their default application. Folders and executable handling
are unchanged, and the preview panel's Open button opens the application.

**Caveats:** [Previews and gallery](CAVEATS.md#previews-and-gallery), tracked in
[#260], [#264], [#266], [#302], [#303] and [#304].

### M07: Editor

**Project:**
[M07 milestone](https://github.com/mikepadiernos/plasma-opus/issues/18)

- [x] KTextEditor-based editor: use it as a panel alongside Information or
      Preview, or as a separate embedded area below or to the right of both
      panes, like the embedded terminal. Neither embedded placement replaces a
      pane. Also support maximised and standalone editor windows.
- [x] Drafts
- [x] Markdown preview and PDF export

**Caveats:** [Editor](CAVEATS.md#editor), tracked in [#267] and [#307].

### M08: Editor settings

**Project:**
[M08 milestone](https://github.com/mikepadiernos/plasma-opus/issues/19)

Settings for M07 editor, stored in KConfig and KTextEditor configuration.

- **Editor**

  - [x] **Opening:** where files open (panel, separate embedded area, maximised
        or own window)
  - [x] **Embedded placement:** below or to the right of both panes when using a
        separate embedded area
  - [x] **Text:** font size and word wrap
  - [x] **Markdown:** default markdown mode (visual or source)
  - [x] **Drafts:** drafts on or off
  - [x] **Recovery:** draft autosave interval and retention; store drafts
        privately and allow them to be reviewed or discarded without overwriting
        the original files
  - [x] **PDF export:** default page size and margins for Markdown exports

- **KTextEditor**

  - [x] **Configuration:** KTextEditor's own configuration pages

**Caveats:** [Editor](CAVEATS.md#editor), tracked in [#267], [#307] and [#309].

### M09: Tools

**Project:**
[M09 milestone](https://github.com/mikepadiernos/plasma-opus/issues/20)

- [x] Live search and indexed search: choose filename or content search and
      current-folder or indexed scope; indicate when locations are not indexed,
      show progress and allow cancellation (Dolphin, Max)
- [x] Search results: reveal an item in its containing folder and save searches;
      optionally collect references from different locations without moving the
      original files (Dolphin, Directory Opus)
- [x] Duplicate finder: preview groups and comparison criteria before selecting
      files for removal; never delete automatically (Directory Opus)
- [x] Duplicate selection: choose candidates by location or date while keeping
      at least one item per group in automatic selection; warn and require
      explicit confirmation if a manual selection would remove every item in a
      group
- [x] Search and selection filters for full path, parent folder and excluded
      subtrees, with an optional recursion-depth limit
- [x] Content chart (folder breakdown)
- [x] Folder sync: preview differences and planned operations before running;
      allow per-item direction changes and exclusions, with explicit
      confirmation of overwrites and deletions (Krusader)
- [x] Folder comparison and sync filters: include/exclude patterns, recursion,
      symlink handling and optional content comparison; save reusable profiles
      (Krusader)
- [x] File diff
- [x] Batch rename: preview old and new names, detect collisions before
      applying, and support metadata tokens for images and audio (Max)
- [x] Batch rename rules for regular expressions and reusable named presets
- [x] Symlinks
- [x] Listing export to CSV and PDF (Max)
- [x] Edit supported audio tags and image metadata, including a preview of
      changes and clear reporting when a format cannot be written (Directory
      Opus)
- [x] Scripts and custom commands
- [x] Command presets: save named script or custom-command configurations and
      placeholder defaults; let users review commands and arguments before
      running. Keep rename and synchronization profiles specific to their tools.
- [x] Compare folders across the two panes (Krusader)
- [x] Create and verify checksum files (Krusader)
- [x] Split and combine files (Krusader)
- [x] Transfer queue with per-job progress, speed, errors and pause, resume and
      cancellation controls where supported by the underlying KIO job (Krusader,
      Max)
- [x] Embedded terminal alongside both panes: below them or at the right of the
      app for portrait/stacked layouts; showing it never replaces either pane
- [x] Placeholders for scripts and custom commands
- [x] Linked panes and cushion treemap (Orcinus)

**Caveats:** [File comparison](CAVEATS.md#file-comparison), tracked in [#311];
[symbolic links](CAVEATS.md#symbolic-links), tracked in [#312];
[listing export](CAVEATS.md#listing-export), tracked in [#314];
[folder content chart](CAVEATS.md#folder-content-chart), tracked in [#315];
[checksum files](CAVEATS.md#checksum-files), tracked in [#316];
[pane folder comparison](CAVEATS.md#pane-folder-comparison), tracked in [#317];
[split and combine files](CAVEATS.md#split-and-combine-files), tracked in
[#318]; [embedded terminal](CAVEATS.md#embedded-terminal), tracked in [#321];
[linked pane navigation](CAVEATS.md#linked-pane-navigation), tracked in [#322];
[cushion treemap](CAVEATS.md#cushion-treemap), tracked in [#335];
[duplicate finder](CAVEATS.md#duplicate-finder), tracked in [#336];
[search](CAVEATS.md#search), tracked in [#362];
[batch rename](CAVEATS.md#batch-rename), tracked in [#372];
[custom commands](CAVEATS.md#custom-commands), tracked in [#373];
[metadata editing](CAVEATS.md#metadata-editing), tracked in [#374];
[transfer queue](CAVEATS.md#transfer-queue), tracked in [#375];
[filtered folder comparison](CAVEATS.md#filtered-folder-comparison), tracked in
[#376]; [folder synchronization](CAVEATS.md#folder-synchronization), tracked in
[#377].

### M10: Tools settings

**Project:**
[M10 milestone](https://github.com/mikepadiernos/plasma-opus/issues/21)

Settings for M09 tools, stored in KConfig.

- **Embedded terminal**

  - [x] **Placement:** below both panes or at the right of both panes for
        portrait/stacked layouts; the terminal does not replace a pane
  - [x] **Working folder:** optionally follow the active pane's local folder;
        report unsupported locations rather than silently switching folders

- **Search**

  - [x] **Defaults:** search scope and whether to search filenames or contents
  - [x] **Filters:** reusable path and subtree filters and recursion depth for
        search and selection
  - [x] **Saved searches:** manage saved searches and reusable result
        collections

- **Duplicate finder**

  - [x] **Defaults:** comparison criteria and selection preferences; retain the
        safeguard that automatic selection keeps at least one item per group

- **Content chart and cushion treemap**

  - [x] **Display:** choose whether to show the treemap and the default table
        sort order; keep scan errors and skipped entries visible

- **Linked panes**

  - [x] **Startup:** optionally link the initial pane folders; default to
        unlinked, validate local roots and report when linking is unavailable

- **File diff**

  - [x] **Display:** choose whether to wrap lines in the comparison output
        without changing the comparison itself

- **Pane folder comparison**

  - [x] **Results:** choose the initial result categories shown; keep errors and
        incomplete comparisons visible and require explicit selection in the
        panes

- **Folder sync**

  - [x] **Transfers:** skip existing, newer only, speed limit
  - [x] **Comparison:** default recursion, symlink and content-comparison
        options; saved synchronization profiles

- **Batch rename**

  - [x] **Presets:** manage named rename rules and choose a default preset;
        retain the M09 collision preview before applying changes

- **Listing export**

  - [x] **Defaults:** output format and export-specific columns, initially based
        on the current listing columns

- **Symbolic links**

  - [x] **Naming:** default link-name suffix, editable before creation; preserve
        existing destination checks and never overwrite an existing entry

- **Checksum files**

  - [x] **Defaults:** default manifest filename for creation, editable before
        saving; retain the supported SHA-256 format and existing validation

- **Split and combine**

  - [x] **Defaults:** default split-part size, editable for each operation;
        retain part-order review and never overwrite existing outputs

- **Metadata editing**

  - [x] **Fields:** choose the initially displayed editable audio and image
        fields when the editor is available; retain the change preview and
        report unsupported writes

- **Transfer queue**

  - [x] **Behaviour:** queue and concurrency defaults; conflict handling

- **Applications**

  - [x] **Associations:** manage desktop default applications by extension
        through KDE facilities; make changes to shared desktop defaults explicit

- **Automation**

  - [x] **Commands:** scripts, custom commands and presets

**Caveats:** The [M09 tools caveats](#m09-tools) also apply to these settings.

### M11: UI Customizations

**Project:**
[M11 milestone](https://github.com/mikepadiernos/plasma-opus/issues/22)

- [x] Edit the main toolbar beyond the `KEditToolBar` dialog: drag entries on,
      off and around it directly, add separators and spacers, and choose icons,
      text or both per entry
- [x] Custom toolbar buttons for scripts, custom commands and presets
- [x] Pane toolbars instead of a fixed path row: each pane gets its own editable
      toolbar (back, forward, up, reload, location bar, view mode, zoom,
      sorting, previews, hidden files, filter, search) that acts on that pane,
      with the location bar dropping to a second row in a narrow pane
- [x] Move panes: send the second pane to a tab of its own or pop it out into a
      window, carrying its display style
- [x] Drag entries between the main toolbar, pane toolbars and hamburger menu
- [x] Customize the combined Places/Folders panel: reorder its sections, show or
      hide each section, and give sections custom titles
- [x] Back up and restore app-owned settings and layouts with a preview and
      confirmation; exclude recognized credentials and shared KDE settings, and
      avoid overwriting running configuration without a safe reload or restart
- [x] Swap panes: the current tabs or whole sides
- [x] Dim the inactive pane
- [x] Customize folder tab bars: place them above, below, left or right of each
      pane, with a together option for stacked panes; choose close-button
      visibility without bypassing pinned or locked tab protections. Keep
      keyboard and context-menu actions accessible. (Directory Opus)
- [x] Customize status bars: choose a shared bar or one per pane, and arrange
      available item counts, selection size and free-space indicators. Bind each
      pane's values to that pane and keep operation errors visible even when
      optional indicators are hidden. Reuse the existing status-bar preferences.
      (Directory Opus, Dolphin)
- [x] Save and reset pane and panel splitter sizes, including preview, editor
      and terminal areas, independently for side-by-side and stacked layouts.
      Restore sizes safely within the available window; preserve both file panes
      and coordinate with session restore and workspace layouts. (Max)
- [x] Customize the preview and gallery background for transparent images:
      follow the active Plasma theme, or choose light, dark or checkerboard.
      Keep the image data unchanged. (Directory Opus)
- [x] Customize the Information panel's visible metadata fields and their order,
      and optionally show information for a hovered item instead of the current
      selection. Make hover behaviour opt-in and retain selection-based
      fallback; show unavailable metadata without initiating additional remote
      downloads. (Dolphin)
- [x] Optional file-list minimap on the right edge of each explorer pane: show a
      compact overview of the current visible files and folders and the scroll
      viewport, with click or drag navigation. Follow the pane's current tab,
      filter, sort and view; keep normal scrolling and keyboard navigation
      available. Bound rendering work for large or remote listings and do not
      read file contents or trigger additional remote downloads.

**Caveats:** [File-list minimap](CAVEATS.md#file-list-minimap), tracked in
[#434]; [settings backups](CAVEATS.md#settings-backups), tracked in [#433].

### M12: UI Customization settings

**Project:**
[M12 milestone](https://github.com/mikepadiernos/plasma-opus/issues/23)

Settings for M11 UI customizations, stored in KConfig.

- **Toolbars**

  - [x] **Entries:** which entries the main toolbar and each pane toolbar show,
        and in what order; customize hamburger menu entries and their order
  - [x] **Appearance:** toolbar style and icon size
  - [x] **Entry appearance:** icon, text or both for individual toolbar entries
  - [x] **Editing:** lock toolbars

- **Panes**

  - [x] **Moving:** whether a moved pane opens as a tab or window, and whether
        it keeps or resets its display style
  - [x] **Swapping:** swap current tabs or whole pane sides by default
  - [x] **Inactive pane:** enable dimming and adjust its intensity

- **Combined panel**

  - [x] **Sections:** section order, visibility and custom titles, reusing the
        visibility settings from M04

- **Tabs**

  - [x] **Placement and controls:** tab-bar position by pane orientation,
        together placement for stacked panes, and close-button visibility

- **Status bars**

  - [x] **Layout:** shared or per-pane bars, with visible indicators and their
        order; reuse existing item-count, selection-size and free-space settings

- **Splitter sizes**

  - [x] **Restore:** remember sizes per orientation and reset them to defaults,
        without overriding an explicitly applied workspace layout

- **Preview and gallery**

  - [x] **Background:** theme, light, dark or checkerboard for transparent
        images

- **Information panel**

  - [x] **Fields and focus:** visible metadata fields and order; opt-in hover
        information with selection fallback

- **File-list minimap**

  - [x] **Display:** enable or disable the minimap and choose its width

- **Defaults**

  - [x] **Reset:** restore the main toolbar, pane toolbars and hamburger menu
  - [x] **Backup:** export and restore app-owned settings and layouts with a
        preview and confirmation, excluding recognized credentials and shared
        KDE settings

**Caveats:** [File-list minimap](CAVEATS.md#file-list-minimap), tracked in
[#434]; [settings backups](CAVEATS.md#settings-backups), tracked in [#433].

### M13: Translations

**Project:**
[M13 milestone](https://github.com/mikepadiernos/plasma-opus/issues/26)

- [x] Translation catalogues through `ki18n`, fed by the `i18n()` and `i18nc()`
      calls already used throughout the code; provide extraction and
      installation without requiring a translation into any particular language
- [x] Right-to-left layout, mirrored for languages that use it
- [x] Jalali (Persian) calendar dates

**Caveats:** [Jalali dates](CAVEATS.md#jalali-dates), tracked in [#436].

### M14: Translation settings

**Project:**
[M14 milestone](https://github.com/mikepadiernos/plasma-opus/issues/27)

Settings for M13 translations, stored in KConfig.

- **Language**

  - [x] **Interface:** follow the system language or select an interface
        language
  - [x] **Regional formats:** follow the system locale or select a locale for
        dates and numbers independently of the interface language

- **Dates**

  - [x] **Calendar:** calendar for dates: Gregorian or Jalali
  - [x] **Format:** custom date and time formats, with clear precedence over the
        listing date-format defaults from M04

- **Layout**

  - [x] **Direction:** follow the language or force left-to-right or
        right-to-left
  - [x] **Digits:** digit shapes and custom numeral formats, including Western
        or Eastern Arabic-Indic digits

**Caveats:** [Jalali dates](CAVEATS.md#jalali-dates), tracked in [#436];
[translation format overrides](CAVEATS.md#translation-format-overrides), tracked
in [#440].

### M15: Workspaces

**Project:**
[M15 milestone](https://github.com/mikepadiernos/plasma-opus/issues/24)

Organize locations into focused workspace profiles. Each profile can include
features, layouts, a theme and other preferences; see the
[interface design](philosophy/designs.md#workspaces). Tag-group organization
belongs to the native tagging system.

Remaining features are ordered by priority, effort and dependencies.

- [x] Create, switch, rename, duplicate and delete named workspaces using a
      discoverable selector. Give each a full name and short label; duplication
      copies the layout without reassigning locations.
- [x] Assign saved locations to workspaces, with each location assigned to at
      most one workspace. Switching shows the active workspace's assignments
      without moving files; keep unassigned locations accessible.
- [x] Store workspace configuration locally without credentials. Workspaces
      organize the interface and search scope; they are not a security boundary.
- [x] Confirm before overwriting a saved workspace or discarding unsaved layout
      changes. Deleting a workspace removes associations, not the underlying
      files, locations or tags.
- [x] Scope global search to the active workspace's assigned locations by
      default, with an explicit option to search elsewhere.
- [x] Save layouts for workflows and integrations, including pane arrangements,
      views, toolbars, and placement or visibility of search, tagging, preview,
      editor and terminal areas as they become available. Report unavailable
      features or providers when applying the workspace.
- [x] Capture and restore pane and tab locations and active tabs as an explicit
      workspace layout snapshot. Preserve running jobs and confirm before
      replacing unsaved state.
- [x] Restore available components when locations or providers are unavailable.
      Identify components that could not be restored and offer a retry without
      silently replacing saved locations or changing files.
- [x] Show the active workspace and unsaved layout changes, with a
      keyboard-accessible selector and a confirmed action to restore the saved
      layout.
- [x] Extend each pane's location bar with Krusader-inspired breadcrumbs: show
      full segments or abbreviate ancestors to their first letter, retaining
      leading underscores (for example, `__path` becomes `__p`), while always
      showing the current folder's full name. Alternatively, show an editable
      full path. Navigate each breadcrumb using its actual location, reveal full
      names for abbreviations and save each pane's mode in the workspace layout.
- [x] Allow all, selected or none of a workspace's applicable behaviours,
      including location presentation, search scope, layouts, and pane and tab
      restoration. Preserve unselected state and existing defaults and reuse
      existing controls; M27 can apply separate choices per Activity.
- [x] Link reusable saved searches and folder comparison or synchronization
      profiles to workspaces for discovery, without copying profiles or changing
      shared defaults. Preserve and identify missing references; switching a
      workspace never runs a task. Keep synchronization previews and
      confirmations.
- [x] Import and export selected workspace profiles through the M12 backup
      workflow, with preview and collision handling. Exclude credentials and
      never execute commands or modify managed files during import.

**Caveat:** [Workspace layouts](CAVEATS.md#workspace-layouts).

### M16: Workspace settings

**Project:**
[M16 milestone](https://github.com/mikepadiernos/plasma-opus/issues/25)

Settings for M15 workspaces, stored in KConfig.

- [x] **Management:** create, switch, rename, duplicate and delete workspaces;
      manage full names, short labels and locations. Preserve exclusive
      assignments and access to unassigned items; duplicating copies the layout
      without reassigning them. Confirm deletion and retain underlying files and
      locations. Store configuration locally without credentials and explain
      that workspaces are not a security boundary.
- [x] **Startup:** choose a startup workspace or restore the last used one
      without overriding M02 startup folder or session choices
- [x] **Search:** choose workspace-scoped or all-location search as the default,
      reusing M10 settings; allow searches outside the workspace
- [x] **Layouts:** choose which pane arrangements, views, toolbars and available
      integration panels to restore; confirm before overwriting saved layouts.
- [x] **Snapshots:** choose which pane and tab state to restore from an explicit
      workspace snapshot, respecting M02 startup and session choices. Preserve
      running jobs and confirm before replacing unsaved state.
- [x] **Recovery:** choose whether to prompt for retries when locations or
      providers are unavailable; identify what could not be restored and retain
      saved references until explicitly changed
- [x] **Indicators:** show the active workspace and unsaved layout state; choose
      the selector's placement without removing all ways to access it, and
      confirm before returning to the saved layout
- [x] **Pane location bars:** choose full breadcrumbs, abbreviated ancestors or
      an editable full path for each pane; restore these choices with the layout
- [x] **Features:** choose all, selected or none of a workspace's applicable
      behaviours, including location presentation, search scope, layouts, and
      pane and tab restoration. Preserve unselected state and existing defaults;
      reuse settings.
- [x] **Portability:** select profiles for import or export using M12 backup
      controls and review conflicts before applying changes. Exclude
      credentials; never execute commands or modify managed files during import.

**Caveats:** [Workspace settings](CAVEATS.md#workspace-settings), tracked in
[#609].

### M17: Themes

**Project:**
[M17 milestone](https://github.com/mikepadiernos/plasma-opus/issues/341)

Customize the appearance of Plasma Opus without changing the desktop's global
appearance. Every option follows Plasma by default, can be saved in a theme
profile and can be attached to a workspace; see the
[interface design](philosophy/designs.md#workspaces).

- **M17.01: Colour schemes**

  - [x] Follow the active Plasma colour scheme by default. Offer app-local
        light and dark presets and installed KDE schemes without changing the
        desktop's global appearance, widget style or window decorations.
  - [x] Apply app-local colours consistently across supported Plasma Opus
        windows, panes and dialogs, respecting accessibility settings, native
        icon themes and the appearance boundaries of embedded or external
        components.
  - [x] Provide an app-local colour editor with named palette roles and live
        preview. Preserve readable text, selection and focus indicators;
        validate contrast and allow changes to be reverted without saving.
  - [x] Follow Plasma's accent colour by default or choose an app-local accent
        for selection, focus, the active pane, progress and tab indicators.
        Validate contrast against the active colour scheme.
  - [x] Tint the background of all panes, or only the active or inactive pane,
        complementing M11 inactive pane dimming. Keep text, selection and file
        colours readable.

- **M17.02: Effects and shape**

  - [x] Optional background blur behind translucent windows and panels using
        KWin's blur effect through `KWindowEffects`; blur strength follows
        KWin's effect settings. Without KWin compositing, report blur as
        unavailable and keep an opaque background. If translucency cannot be
        changed live, apply it when the window next opens.
  - [x] Adjust background opacity separately for windows, panes and panels,
        with or without blur, while keeping text, icons and selection fully
        opaque.
  - [x] Choose a corner radius (none, small, medium, large or a custom value
        scaled with the display) for item highlights, cards, panels, input
        fields, tabs and tooltips, using custom delegates and app-local style
        rules. Leave window corners to KWin and the window decoration, and
        document controls that the widget style draws without a radius.

- **M17.03: Lines and dividers**

  - [x] Show, thin or hide visible dividers, including splitter handles, panel
        and toolbar separators, header and grid lines and pane frames. Hidden
        splitters keep a resize area, cursor feedback, keyboard resizing and
        the M11 size reset, and can optionally reveal themselves on hover.
  - [x] Show or hide tree lines in the tree pane and M03 inline folder
        expansion, and choose solid or dotted lines. Draw the lines in Plasma
        Opus when the widget style does not provide the chosen style.
  - [x] Colour tree lines with a single colour, a configurable palette by
        nesting depth or each folder's colour. Validate contrast and keep
        nesting distinguishable without relying on colour alone.

- **M17.04: Views**

  - [x] Add a Card view mode alongside Details, Icons and Compact. Show each
        item as a card with a preview or icon, its name and chosen metadata
        such as size, modification date, type, tags, rating and colour label.
        Support card size, columns and density, selection markers, inline
        renaming, drag and drop and keyboard navigation, and remember the mode
        wherever other view modes are remembered, including workspace layouts.
  - [x] Choose row striping in Details view, hover highlighting and a selection
        style such as filled, outlined, rounded or an accent bar. Keep focus and
        selection distinguishable for keyboard users.

- **M17.05: Folder colours**

  - [x] Extend M03 folder colours to tint folder icons consistently across
        views, the tree pane, places, breadcrumbs and tabs. Use the icon
        theme's coloured folder icons, such as Breeze's `folder-red`, when
        available and tint them otherwise; alternatively show a colour badge
        or coloured name.
  - [x] Colour folders by rules that match a path, name pattern or tag, with
        optional inheritance by subfolders. Explicit folder colours take
        precedence over rules, and rules never modify folders or their
        contents.

- **M17.06: Text and spacing**

  - [x] Follow Plasma's fonts by default or override the font family, size and
        weight for file lists, the tree pane, tabs and the status bar, scaled
        consistently with the display.
  - [x] Scale padding and spacing globally, extending M02 item density to
        toolbars, tabs, panels and the status bar.
  - [x] Style tabs and the status bar, including the tab shape, active tab
        indicator and the strength of M03 tab colours.

- **M17.07: Motion and accessibility**

  - [x] Follow Plasma's animation speed by default, or reduce or disable Plasma
        Opus transitions such as view changes, hover fades and panel
        animations.
  - [x] Offer to suspend blur, translucency, tints and custom corner radii with
        high-contrast colour schemes, and validate contrast for every custom
        colour. Provide a one-step reset of global appearance options to Plasma
        defaults from settings and the command line, and a way to start
        without workspace appearance overrides.

- **M17.08: Profiles and stylesheets**

  - [x] Save, import and export versioned app-owned theme profiles with
        palette choices, the appearance options in this milestone and optional
        stylesheets. Validate them, confirm overwrites and offer a safe reset
        to system appearance; do not execute code from theme files. Report
        unavailable themes and preserve saved references for retry.
  - [x] Support an optional app-local Qt Style Sheet (QSS, a CSS-like subset
        rather than full web CSS) entered in a text box or imported from a
        file. Allow preview, application, disabling, reversion and export
        separately or with a theme profile. Report invalid or unsupported
        input, bound resource use and prevent unapproved external resource
        loading.

- **M17.09: Workspaces**

  - [x] Attach a theme profile and selected appearance options, such as Card
        view, folder colour rules, blur, corner radius and divider visibility,
        to a workspace. Choose all, selected or none of them as M15 workspace
        behaviours and apply them live when switching workspaces. Layer them in
        a documented order from Plasma defaults through the theme profile and
        workspace overrides to the custom stylesheet. Preserve unselected
        global state, report unavailable themes or effects, such as blur
        without KWin, and keep saved references for retry; M27 can apply
        separate choices per Activity.

**Caveats:** [Themes](CAVEATS.md#themes), tracked in [#680].

### M18: Theme settings

**Project:**
[M18 milestone](https://github.com/mikepadiernos/plasma-opus/issues/352)

Settings for M17 themes, stored in KConfig. Workspaces can override them; see
M18.08.

- **M18.01: Themes**

  - [x] **Appearance:** follow the system or select an available app-local
        preset or installed KDE colour scheme
  - [x] **Custom colours:** edit, preview, save and reset app-local palette
        roles with contrast feedback and reversible preview
  - [x] **Profiles:** manage, import and export validated app-owned profiles and
        optional stylesheets with overwrite confirmation
  - [x] **Custom stylesheet:** enable or disable QSS, edit it in a text box,
        import or export it, and preview or revert changes
  - [x] **Reset:** restore global appearance options to Plasma defaults with
        confirmation

- **M18.02: Colours**

  - [x] **Accent:** follow Plasma or choose an app-local accent colour
  - [x] **Pane tint:** tint all, active or inactive pane backgrounds and adjust
        the strength
  - [x] **Folder colours:** choose an icon tint, badge or coloured name; manage
        colour rules, their order and subfolder inheritance

- **M18.03: Effects and shape**

  - [x] **Blur:** enable background blur where KWin supports it and show its
        availability
  - [x] **Opacity:** adjust window, pane and panel background opacity
  - [x] **Corner radius:** choose a preset or custom radius for items, cards,
        panels and controls

- **M18.04: Lines and dividers**

  - [x] **Dividers:** show, thin or hide splitter handles, separators, grid
        lines and frames; choose whether hidden splitters appear on hover
  - [x] **Tree lines:** show or hide them; choose solid or dotted lines and a
        single colour, a depth palette or folder colours

- **M18.05: Views**

  - [x] **Cards:** card size, columns, density and shown metadata; add Card to
        the M02 default view mode choices
  - [x] **Rows and selection:** row striping, hover highlighting and selection
        style

- **M18.06: Text and spacing**

  - [x] **Fonts:** follow Plasma or choose app-local fonts for file lists, the
        tree pane, tabs and the status bar
  - [x] **Spacing:** global padding and spacing scale
  - [x] **Tabs and status bar:** tab shape, active tab indicator and tab colour
        strength

- **M18.07: Accessibility**

  - [x] **Motion:** follow Plasma's animation speed or reduce or disable Plasma
        Opus animations
  - [x] **High contrast:** whether to suspend blur, translucency, tints and
        custom corner radii with high-contrast colour schemes

- **M18.08: Workspaces**

  - [x] **Theme:** let each workspace use an available theme profile or the
        global appearance
  - [x] **Overrides:** choose all, selected or none of the appearance options to
        save with a workspace, independently of other workspace features;
        preview them and report unavailable effects

**Caveats:** [Theme settings](CAVEATS.md#theme-settings), tracked in [#687].

### M19: Branding

**Project:**
[M19 milestone](https://github.com/mikepadiernos/plasma-opus/issues/337)

- [x] Use a consistent, translatable application name and description in the
      desktop entry, app metadata, About dialog and documentation.
- [x] Create original, appropriately licensed Plasma Opus logo and application
      icon assets in suitable sizes and light/dark variants. Continue to source
      file, device and action icons from the active desktop icon theme.
- [x] Present branding consistently in About and release materials. Offer an
      optional, accessible startup presentation that does not delay or block
      use.

**Caveats:** [Branding](CAVEATS.md#branding), tracked in [#689].

### M20: Branding settings

**Project:**
[M20 milestone](https://github.com/mikepadiernos/plasma-opus/issues/346)

Settings for M19 branding, stored in KConfig and separate from theme settings.

- **Branding**

  - [x] **Presentation:** enable or disable the optional startup presentation
        and select available branding variants without changing the app identity

**Caveats:** [Branding](CAVEATS.md#branding), tracked in [#689].

### M21: Advanced search

**Project:**
[M21 milestone](https://github.com/mikepadiernos/plasma-opus/issues/30)

Extend the M09 search tools and reuse their search scopes, filters and saved
searches rather than introducing a separate search system. M27 extends the Baloo
integration without blocking this milestone.

- [x] **Indexing:** configure search folders and exclusions, and use existing
      KDE facilities for the search shortcut. Use double Shift where supported,
      report when it is unavailable.
- [x] Provide an embedded search pane using existing supported placements.
      Placing it alongside file panes must not replace either pane.
- [x] Show M09 search results in the embedded panel, reusing progress,
      cancellation, error reporting and open and reveal actions without moving
      files
- [x] Offer an optional main-toolbar search widget to enter queries, search the
      shared scope (initially the active pane's location) and open the advanced
      search panel
- [x] Support keyboard navigation and focus between the toolbar widget, search
      panel and panes
- [x] Prototype query parsing and validation in Rust behind a narrow C++/Rust
      interface. Test boundary error handling, ownership, path handling and
      threading, and evaluate build and maintenance costs before broader Rust
      adoption; keep the search UI and KIO integration in C++.
- [x] Combine filename, path, file type, size, date and content criteria using
      AND, OR and NOT; validate queries and explain unsupported criteria

### M22: Advanced search settings

**Project:**
[M22 milestone](https://github.com/mikepadiernos/plasma-opus/issues/31)

Settings for M21 advanced search, stored in KConfig.

- [x] **Defaults:** share M10 search scope, filter and saved-search settings
      between the toolbar and panel
- [x] **Panel:** choose an embedded-pane placement and whether to restore
      visibility at startup, preserving both file panes in shared placements
- [x] **Results:** choose the default presentation and whether to retain search
      history; allow history to be cleared
- [x] **Toolbar:** show or hide the search widget using M12 toolbar controls and
      choose whether submitting a query opens the panel

**Caveats:** [Search](CAVEATS.md#search), tracked in
[#362](https://github.com/mikepadiernos/plasma-opus/issues/362).

### M23: Native tagging

**Project:**
[M23 milestone](https://github.com/mikepadiernos/plasma-opus/issues/32)

A native KDE tagging workflow inspired by TagStudio, integrated into Plasma Opus
rather than requiring TagStudio or changing the user's folder structure.

- [x] Evaluate Rust for tag relationships, library metadata and search using the
      M21 pilot. Keep Qt/KDE integration in C++ and use C++ for the core if
      build and interoperability costs are not justified.
- [x] Store library metadata in an open, documented format without requiring a
      sidecar per file; preserve existing `.mfm.json` metadata and report
      unsupported or read-only locations
- [x] Create opt-in libraries rooted in existing local folders; index files of
      any type without moving, renaming or duplicating them
- [x] Refresh libraries with progress and cancellation. Track moves and renames
      initiated by Plasma Opus, and offer review and manual relinking for
      missing or externally moved files without silently attaching metadata to
      uncertain matches.
- [x] Manage tags with stable identities, names, aliases, shorthand, colours and
      parent relationships; disambiguate duplicate names and prevent hierarchy
      cycles
- [x] Assign and remove multiple tags on selected files and folders from
      listings or previews, and show assignments in the file interface. Provide
      a searchable, keyboard-accessible tag manager as an embedded pane using
      existing supported placements without replacing file panes.
- [x] Search and filter by tags, inherited parent tags, untagged items, paths
      and file types using AND, OR, NOT and grouped expressions; reuse existing
      search and selection workflows
- [x] Organize tag groups independently of locations and workspaces. Until the
      native tag-group provider is available, keep all groups visible; recommend
      stable identities and no hidden or implicit assignment in
      [#252](https://github.com/mikepadiernos/plasma-opus/issues/252).
- [x] Add editable text and date fields to library entries alongside existing
      labels, ratings and notes, without silently replacing or converting them
- [x] Read supported EXIF image metadata and ID3 audio tags for display and
      search. Let users map selected fields to library tags or fields with
      preview and conflict handling. Reuse M09 metadata editing for supported
      writes, with explicit consent and clear reporting of unsupported formats
      and failures.
- [x] Export and restore library metadata with preview and explicit
      confirmation; never modify or delete original files during either
      operation

**Caveats:** [Native tagging](CAVEATS.md#native-tagging), tracked in
[#714](https://github.com/mikepadiernos/plasma-opus/issues/714).

### M24: Tagging settings

**Project:**
[M24 milestone](https://github.com/mikepadiernos/plasma-opus/issues/33)

Settings for M23 tagging, stored in KConfig.

- [x] **Privacy:** control library metadata and backup locations, explain what
      is stored and require explicit consent before writing tags into file
      metadata
- [x] **Libraries:** manage roots and exclusions, automatic refresh and scan
      limits
- [x] **Tags:** configure colours, display, parent-tag matching defaults and
      embedded-pane placement
- [x] **Search:** choose the default tag-search scope and matching behaviour
- [x] **Fields:** manage available text and date fields
- [x] **Embedded metadata:** select EXIF and ID3 fields to read or map to
      library metadata, and control refresh and conflict handling

**Caveats:** [Tagging settings](CAVEATS.md#tagging-settings), tracked in
[#716](https://github.com/mikepadiernos/plasma-opus/issues/716).

### M25: Yazi mode

**Project:**
[M25 milestone](https://github.com/mikepadiernos/plasma-opus/issues/407)

Offer an optional native Qt/KDE browsing mode inspired by
[Yazi](https://yazi-rs.github.io/docs/quick-start/), a terminal file manager.
Reuse Plasma Opus's file models and KDE services; this is not an embedded Yazi
process or a promise of Yazi configuration or plugin compatibility.

- [x] Allow the mode to be entered or left per pane, keeping it off by default.
      Preserve both panes and their tabs, locations, selections and normal view
      settings when switching; let M15 workspaces restore the mode when
      available.
- [x] Show parent and current folders with an optional preview in the active
      pane. Adapt or hide columns in narrow windows without replacing the other
      pane; keep navigation and preview accessible.
- [x] Provide keyboard-first navigation with optional Vim-style movement and
      multi-key actions. Reuse KDE shortcut facilities; show the active mode and
      do not intercept typing in editors, search fields or the terminal.
- [x] Support focused-item and multi-item selection, copy, move, rename, trash
      and other existing file actions through Plasma Opus and KIO. Retain
      conflict handling, destructive-action confirmations and undo; never add a
      shortcut that silently overwrites or permanently deletes files.
- [x] Reuse filtering, filename and content search, sorting, tabs and history.
      Show active filters and search progress, allow cancellation and report
      backend or remote-location limitations.
- [x] Reuse Plasma Opus previews and file information for the focused item, with
      cancellable work and existing size and remote-download limits. Report
      progress and errors through existing jobs and the transfer queue rather
      than duplicating operations.
- [x] Keep large and remote listings responsive and accessible to keyboard and
      assistive-technology users. Report unavailable operations and preserve
      ordinary browsing when a location is unsupported by the mode.
- [x] Store mode, column and preview choices in KConfig and available workspace
      layouts. Include Yazi mode in workspace snapshots and feature selection.
      Provide a discoverable toggle without changing existing bindings by
      default.

**Caveats:** [Yazi mode](CAVEATS.md#yazi-mode), tracked in
[#723](https://github.com/mikepadiernos/plasma-opus/issues/723).

### M26: Yazi mode settings

**Project:**
[M26 milestone](https://github.com/mikepadiernos/plasma-opus/issues/416)

Settings for M25 Yazi mode, stored in KConfig and coordinated with workspace
layouts.

- [x] **Activation:** choose each pane's default mode and whether to restore its
      previous choice. Respect explicitly restored M15 layouts when available
      and keep the mode off by default.
- [ ] **Columns:** choose visible folder-context and preview columns and their
      sizes; adapt to narrow windows without hiding the other file pane.
- [ ] **Keyboard:** choose a standard or Yazi-inspired shortcut preset and
      review conflicts before applying it. Keep typing and accessibility
      controls usable; reuse KDE shortcut configuration.
- [ ] **Preview:** choose whether to show the focused item's preview or file
      information. Reuse existing providers, size limits and remote-download
      policy rather than introducing conflicting settings.
- [ ] **Workspace layouts:** when M15 is available, choose whether layouts
      restore Yazi mode and its column settings, including the Yazi-specific
      workspace feature choice, without overwriting an explicitly selected
      workspace layout or changing other pane views.
- [ ] **Reset:** restore Yazi mode defaults without erasing saved workspace
      layouts or unrelated settings.

### M27: Integrations

**Project:**
[M27 milestone](https://github.com/mikepadiernos/plasma-opus/issues/34)

- **M27.01: KDE Plasma and desktop services**

  - [x] Default file manager registration
  - [ ] Provide the `org.freedesktop.FileManager1` D-Bus service
  - [ ] Integrate with KDE Plasma's language preferences using M13 translation
        catalogues, M14 language and regional-format preferences, and supported
        KDE and Qt facilities. Follow Plasma when no app override is selected,
        fall back to source text if a Plasma Opus catalogue is unavailable, and
        offer access to Plasma's language settings when supported. Do not change
        shared desktop settings without explicit action or require a particular
        translation.
  - [ ] Plasma notifications and jobs: integrate Plasma Opus-owned long-running
        operations with KDE's notification settings and offer relevant actions
        to return to the operation or inspect its result. Reuse KIO's existing
        progress reporting rather than creating duplicate notifications or
        controls. Respect notification settings and privacy; report failures and
        partial results without exposing sensitive paths unnecessarily.
  - [ ] Provide system-tray integration and a global shortcut
  - [ ] Plasma Activities: optionally associate M15 workspaces with Activities
        and configure default folders for each Activity and workspace
        association. For each association, use all, selected or none of the
        applicable workspace behaviours, including theme and layout, and
        Activity-specific default folders independently of other Activities;
        allow workspace defaults or explicit overrides. Offer to apply the
        selected choices when the Activity changes, without silently replacing
        restored tabs or unsaved layouts. Leave unselected behaviours and state
        unchanged, preserve unassociated workspaces and running operations, and
        never create, delete or switch an Activity without explicit user action.
        Handle an unavailable Activities service, missing folders and removed
        Activities without losing workspace data, and avoid duplicating KDE's
        KIO resource history.
  - [ ] KDE Connect sharing: offer an explicit action to send selected files to
        an available paired device through supported KDE Connect interfaces.
        Show the selected device and transfer outcome; do not pair devices, send
        files or change shared KDE Connect settings automatically. Handle
        missing services, offline devices and unsupported locations visibly.
  - [ ] KRunner integration: provide an optional runner for explicitly saved
        Plasma Opus workspaces and locations, with actions to open the chosen
        item in Plasma Opus. Respect workspace privacy, avoid exposing
        credentials or unselected history, and handle missing or stale targets
        without creating files or running commands. Use Plasma's supported
        runner interfaces and do not replace its existing file search.
  - [ ] Download service menus from the existing Context Menu settings page, as
        in Dolphin, using KDE's KNewStuff service-menu provider. Support
        browsing, installation, updates and removal through the provider, and
        refresh the installed-action list without discarding pending settings
        changes. Explain that downloaded services can run commands and may be
        shared with other KDE applications; require explicit user action and
        report failures. Reuse the existing Plasma Opus-specific and
        Dolphin-shared visibility controls.
  - [ ] Optional system file chooser that replaces application Open and Save
        dialogs, as in [Strata](https://github.com/lgse/strata), by serving the
        XDG portal file chooser. Implement
        `org.freedesktop.impl.portal.FileChooser` (`OpenFile`, `SaveFile` and
        `SaveFiles`) as a D-Bus-activated backend with a compact chooser that
        reuses Plasma Opus views, places and search. Honour multiple and folder
        selection, filters, choices, suggested names and folders, parent
        windows and request cancellation, and confirm overwrites when saving.
        Nothing changes without explicit opt-in through the M28 **File
        manager** settings, `install.sh --file-chooser` or
        `opus-file-manager --install-portal`. Restore the previous chooser from
        the same settings row, `install.sh --uninstall` or
        `opus-file-manager --uninstall-portal`. Setup installs per-user portal
        metadata and an activation service, prefers Plasma Opus ahead of
        `xdg-desktop-portal-kde` in the user's `kde-portals.conf` so the KDE
        chooser remains the fallback, and restarts the portal frontend after
        warning that open file dialogs may close. Restoring puts back the
        previous configuration, or removes only Plasma Opus if the user has
        changed it since. Keep this separate from default file manager
        registration and the `org.freedesktop.FileManager1` service. Explain
        that it affects applications that use the portal, such as Flatpak and
        Snap apps and portal-enabled GTK, Electron and browser apps; native Qt
        and KDE applications keep Plasma's in-process dialog. Do not change
        platform themes or environment variables, and report a missing portal
        service or setup failures.
  - [ ] Mount manager, like Krusader's
        [MountMan](https://docs.kde.org/trunk_kf6/en/krusader/krusader/mount-man.html):
        list mounted and fstab-defined file systems with their device, type,
        mount point, size, free space and a usage chart, optionally showing
        only removable devices. Open a file system in the active pane, and
        mount, unmount or eject it through Solid and UDisks2 with KDE's
        authorization prompts where required. Offer a toolbar quick-mount menu
        that shows each mount point's current state, and include network and
        FUSE mounts such as remote drives. Leave partitioning, formatting and
        fstab editing to KDE Partition Manager. Report busy devices, denied
        authorization and failed operations. (Krusader)
  - [ ] Optional KWin script for window management that Wayland applications
        cannot perform themselves: install, enable, update and remove a Plasma
        Opus KWin script package only through explicit action, using KPackage
        and KWin's scripting D-Bus interface. When applying an M15 workspace or
        Activity association, restore each Plasma Opus window's screen,
        virtual desktop, size, tile and keep-above state. Optionally place
        windows opened from Plasma Opus, such as terminals, editors and
        detached previews, beside the file manager using KWin's tiles, and
        apply an assigned workspace when the virtual desktop changes. Act only
        on Plasma Opus windows and communicate through a versioned D-Bus
        interface, checking the installed script version. Prefer
        KWindowSystem, XDG activation and window rules where they suffice, and
        open KWin's settings instead of editing window rules, other scripts or
        global KWin settings. Report when KWin or scripting is unavailable,
        such as on other compositors, without affecting file management.
  - [ ] Drop-down window, like Yakuake: optionally slide a Plasma Opus window
        in from a chosen screen edge and hide it again, toggled by a KDE global
        shortcut or by a screen edge or touch-screen edge registered by the
        KWin script. Keep it above other windows and out of the taskbar and
        pager, open it on the active screen and optionally hide it when it
        loses focus, but not while a dialog or confirmation needs attention.
        Restore its own layout or an assigned workspace, and fall back to a
        regular window when KWin or the script is unavailable. (Yakuake)

- **M27.02: Archives and application packages**

  - [ ] Integrate deeply with ARK
  - [ ] Extend archive creation through Ark: create 7z, password-protected and
        split archives, encrypting file names where the format supports it,
        and add, rename or remove entries in existing archives. Keep M03's
        non-overwriting behaviour, confirm changes to existing archives and
        report formats that installed Ark backends can only extract, such as
        RAR. (MiXplorer)
  - [ ] Mount and unmount ISO and other disk images through the Mount ISO
        action from Dolphin's plugins when installed, and show mounted images
        in Places through Solid. Report a missing plugin or denied
        authorization. (MiXplorer)
  - [ ] Integrate deeply with AppImage, Flatpak and Snap: identify packages and
        show available application details and actions. Support AppImage
        launching and optional desktop integration, and use the respective
        package tools for supported Flatpak and Snap installation, updates and
        removal. Confirm changes, respect executable and sandbox permissions,
        and report missing tools, unsupported actions and errors.

- **M27.03: Search and indexing**

  - [ ] Enhance Baloo integration using M09 indexed search and M21's search
        interface. Show service status, configured folders and exclusions, and
        available coverage or indexing progress. Distinguish configured scope
        from confirmed coverage and warn about missing or stale results. Offer
        KDE's shared indexing controls, reuse metadata from installed
        KFileMetaData extractors and identify missing extractors when
        detectable. Do not change shared settings, enable indexing or start a
        reindex without explicit action.
  - [ ] Extend Baloo search with supported metadata criteria and optional fuzzy
        ranking. Explain that ranking cannot recover matches missing from the
        index or results truncated by existing limits. Offer an explicit
        live-search fallback for supported local folders, retaining its limits,
        progress, cancellation and error reporting; never silently broaden scope
        or substitute another search.
  - [ ] Show file-type categories in Places, such as documents, images, audio,
        video, archives, ebooks, application packages and large files, as Baloo
        searches alongside KDE's existing **Search For** entries. Explain when
        indexing is off or incomplete and offer a live search of a chosen
        folder instead. (MiXplorer)

- **M27.04: Terminals and command history**

  - [ ] Extend the existing open-terminal action and M09 embedded Konsole
        terminal with explicit separate Konsole, Yakuake or Kwi providers and an
        embedded Kwi KPart when available. Reuse the current Konsole integration
        and pane-preserving layout. Yakuake is a separate drop-down application,
        not an embeddable terminal; report unavailable providers and unsupported
        locations.
  - [ ] Optional [Atuin](https://docs.atuin.sh/) integration for terminal
        command history: let users open interactive Atuin search in a supported
        terminal, without automatically executing a selected command. Respect
        the user's existing shell configuration; do not install or initialize
        Atuin, access its history database, or enable history synchronization
        without consent. Report missing tools and unsupported terminal sessions.

- **M27.05: Editors, documents and media**

  - [ ] Editor integrations: offer KWrite and Kate as separate GUI editors, and
        Neovim, Rune, micro and nano in a supported embedded or separate
        terminal, as alternatives to the built-in editor. KWrite and Kate use
        KTextEditor themselves; they are separate applications, not replacement
        editor components. Preserve the built-in editor and its draft handling,
        pass file paths safely without shell interpolation, and report missing
        providers, launch failures and unsupported locations.
  - [ ] Office documents: integrate viewing and editing for documents,
        spreadsheets and presentations, including OpenDocument and Microsoft
        Office formats. Reuse installed KParts for supported previews and offer
        an explicit action to open files in an available office application,
        such as LibreOffice. Evaluate embedded editing only through a supported
        provider; do not assume an installed viewer supports editing. Report
        unavailable providers, unsupported formats and locations, and launch or
        save failures. Preserve unsaved changes and existing file content; never
        silently convert or overwrite a document.
  - [ ] For any viewable file, offer an edit action when a suitable tool is
        available. Use supported embedded providers or launch a separate editor;
        distinguish viewing from editing capabilities and report unavailable
        tools, unsupported locations and launch failures. Preserve unsaved
        changes and avoid silent conversion or overwrite.
  - [ ] Extend the built-in editor with CommonMark and Obsidian-like wiki links
        and backlinks within a user-selected local vault. Preserve source and
        drafts, respect vault boundaries, and report ambiguous, missing or
        inaccessible targets and incomplete scans. See the
        [pending handoff](TODO.md#markdown-editor-and-backlinks-564).
  - [ ] Let an assigned local location serve as an optional Obsidian-inspired
        vault for the built-in text editor. Reuse #564 for source and visual
        Markdown editing, live preview, note navigation, wiki links, backlinks,
        tags and a note relationship view. Preserve drafts and unsaved edits
        during navigation and workspace switches; index only the chosen vault
        and report incomplete results and ambiguous or missing links. Use
        existing folders without moving notes, executing plugins or changing
        Obsidian configuration. Deleting a workspace must not delete its vault.
        See the [pending handoff](TODO.md#workspace-vault-editing-565).
  - [ ] Offer optional mpv integration for embedded audio and video playback in
        the preview panel and gallery alongside Qt Multimedia, and an explicit
        action to open selected media in the separate mpv application. Preserve
        autoplay and remote preview behavior; report unavailable backends,
        playback errors and unsupported locations. See the
        [pending handoff](TODO.md#mpv-integration-563).
  - [ ] Preview ebooks, comics and help files, such as EPUB, MOBI, FictionBook,
        CBZ, DjVu and CHM, with Okular's KPart when installed, within the M05
        preview limits. Preview fonts with editable sample text through
        Plasma's font viewer part where available, and offer installation
        through KDE's font management. Report missing providers. (MiXplorer)

- **M27.06: Accounts, remote access and synchronization**

  - [ ] Integrate advanced SSH, SFTP and WebDAV support: manage named profiles
        and browse remote locations in either pane using KDE's KIO workers. Use
        the existing transfer queue where available and offer a separate SSH
        terminal for SSH profiles. Support passwords and SSH key/agent
        authentication where available, use KDE credential facilities and
        respect SSH host-key and WebDAV TLS certificate verification. Report
        connection, authentication and unsupported-operation errors explicitly.
  - [ ] Extend remote profiles to SMB and FTP or FTPS through KDE's KIO
        workers, and show services discovered on the local network through
        KIO's `network:/` location. Warn that plain FTP sends credentials
        unencrypted, and use KDE credential facilities. (MiXplorer)
  - [ ] Optional [rclone](https://rclone.org/) integration for cloud providers
        without KDE support, such as Dropbox, OneDrive, Box, MEGA and pCloud.
        List the user's configured remotes through rclone and open one in
        either pane by serving it over WebDAV on the loopback interface or by
        mounting it on request. Never read, store or change rclone credentials
        or configuration; report missing tools, failed mounts and unsupported
        operations, and stop processes that Plasma Opus started. (MiXplorer)
  - [ ] Mount remote storage as local folders, like
        [RaiDrive](https://www.raidrive.com/) drives, so applications without
        KIO support can open, edit and save files in place. Mount saved SSH,
        SFTP, WebDAV, FTP or FTPS and SMB profiles, Nextcloud, S3-compatible
        storage and KDE Online Accounts locations in an empty folder under the
        user's runtime directory or a chosen location, and show them in Places
        until unmounted. Prefer KDE's KIO FUSE, and offer installed backends as
        alternatives to rclone: sshfs for SFTP, davfs2 for WebDAV with its file
        locks, s3fs-fuse for S3 and the kernel CIFS client for SMB. Offer
        read-only mounts, mounting at login or when a workspace is activated,
        and unmounting on exit; stream media from a mount where the backend
        reads ranges rather than whole files. Never write credentials into
        fstab, backend configuration or Plasma Opus settings without explicit
        consent, and use KDE credential facilities where the backend accepts
        them. Report missing FUSE support or tools, mounts that need an fstab
        entry or administrator authorization, stale or busy mounts and failed
        unmounts; unmount only what Plasma Opus mounted. (RaiDrive)
  - [ ] Temporary LAN sharing: serve selected files or a folder over HTTP or
        WebDAV to devices on the local network, with optional TLS, a QR code,
        a random access token, read-only access by default and an optional
        upload folder with conflict handling. Bind only to chosen interfaces,
        show active sessions and transfers, stop after a timeout or on exit,
        and never open firewall ports. Warn that transfers without TLS can be
        observed on the network, and point to KDE Connect and Samba sharing
        for paired devices and persistent shares. (MiXplorer)
  - [ ] Integrate with KDE Online Accounts: discover available accounts and
        their supported file services, let users explicitly connect or open them
        in either pane through the appropriate KDE provider or KIO worker, and
        provide access to KDE's account-management interface. Reuse account
        identities for supported Nextcloud and remote integrations instead of
        duplicating credentials. Handle missing providers, denied access,
        expired authorization and removed accounts visibly; do not assume every
        account offers file access. Respect provider permissions, authentication
        and TLS verification, and never store tokens or passwords in URLs, logs
        or Plasma Opus configuration.
  - [ ] Optional [rsync](https://rsync.samba.org/) backend for M09 folder sync:
        support local folders and SSH locations, reuse sync profiles and
        exclusions, and show an itemized dry run before confirmation. Disable
        deletion by default and explicitly confirm overwrites and deletions.
        Validate that the backend can honor the selected plan, including
        per-item directions; report unsupported options instead of silently
        changing the plan. Show progress, cancellation, errors and possible
        partial results; a preview is a snapshot, not a guarantee against
        subsequent filesystem changes. Use safe argument handling and SSH
        host-key verification.
  - [ ] Two-way cloud sync, like
        [Syncative](https://github.com/Santuzius/syncative): pair any local
        folder, not only a single sync root, with a folder on a Nextcloud,
        WebDAV, SFTP, SMB, S3-compatible or KDE Online Accounts location or an
        rclone remote, and keep both sides up to date. Offer Sync, Cloud backup
        (upload only) and Local backup (download only) modes per pair, with
        deletions copied to a backup only when enabled. Reuse M09 sync filters
        and profiles for exclusions, and ask how to handle a pair nested in
        another. Keep a per-pair state snapshot to tell renames and deletions
        from new files. Never overwrite a file changed on both sides: let the
        user keep the local, remote or both copies, per file or for all.
        Preview the first sync and large deletions before applying them. Sync
        in the background while Plasma Opus or its tray icon runs, optionally
        from login, with pause and resume per pair or account and an option to
        pause on metered connections. Use KIO, or rclone bisync for rclone
        remotes, and keep credentials in KDE's facilities. Refuse pairs that
        overlap folders managed by the Nextcloud desktop client or another
        sync engine. Report quota, offline remotes, permission errors and
        partial runs. (Syncative)
  - [ ] Cloud activity panel, like
        [SDrive](https://github.com/TitanniOpen/SDrive) and
        [DucksDrive](https://patopatetico2-lab.github.io/ducksdrive/): for
        remote drives and sync pairs, show connection state, quota where the
        provider reports it, pending uploads, current transfers with speed,
        remaining time and a speed graph, and recent errors, through the
        existing transfer queue and an optional tray panel. Offer pause and
        resume, a bandwidth limit, the number of parallel transfers, and a size
        limit for the local cache of mounted remotes that keeps recently opened
        files available offline. Before unmounting, exiting or logging out,
        wait for pending uploads with progress and an option to cancel, and
        report uploads that need recovery after a crash. Export connection and
        sync-pair definitions without credentials, and ask the user to sign in
        again after importing them. (SDrive, DucksDrive)

- **M27.07: Access control and automation**

  - [ ] Support administrator mode through KIO's `admin:/` worker
  - [ ] Provide an MCP server
  - [ ] Optional password-gated folders: let users require a password before
        opening selected local folders through Plasma Opus. This is a Plasma
        Opus-only convenience gate, not encryption or filesystem access control:
        clearly warn that other applications and direct filesystem access remain
        unrestricted. Do not change file permissions or contents. Apply the gate
        consistently across Plasma Opus navigation and alternate paths, store no
        plaintext password, and use a trusted password-verification mechanism
        and KDE credential facilities where appropriate. Report unavailable
        protection and failures rather than silently bypassing the gate; provide
        explicit password change and removal workflows.
  - [ ] Plasma Vault integration: list encrypted vaults (CryFS, gocryptfs or
        EncFS) with their lock state, and open, close or create them through
        Plasma Vault's `org.kde.plasmavault` D-Bus interface, revealing an
        opened vault in a pane. Leave passwords and vault configuration to
        Plasma Vault, warn before closing a vault with files in use and report
        an unavailable service. (MiXplorer)
  - [ ] File encryption: offer encrypt, decrypt, sign and verify actions for
        selected files through GnuPG, reusing Kleopatra's file actions when
        installed, and write new files without replacing the originals.
        Identify encrypted and signed files. Support AES Crypt (`.aes`) files,
        as used by MiXplorer, only through an installed `aescrypt` tool and
        without exposing passwords in command lines. (MiXplorer)

- **M27.08: Development tooling**

  - [ ] Use [mise](https://mise.jdx.dev/) for development tooling: manage
        project tool versions and provide configure, build, test and formatting
        tasks. Document setup and system-provided Qt, KDE and other
        dependencies, including versions needed to reproduce builds; keep the
        existing CMake workflow usable without mise.

- **M27.09: Plasma Impresario and mail**

  - [ ] Send selected files by email: offer an explicit action that opens the
        user's preferred mail client, such as
        [Plasma Impresario](https://github.com/mikepadiernos/plasma-impresario),
        through KIO's `KEMailClientLauncherJob` or the XDG Email portal when
        sandboxed. Show the selected files, offer private local copies of
        remote files through KIO with consent, and never send mail. Report
        missing clients and clients that cannot accept attachments; Plasma
        Impresario currently ignores `mailto:` attachments. See the
        [companion workflows](integrations/plasma-impresario.md#compose-with-selected-files).
  - [ ] Mail attachments: verify dragging local files into Plasma Impresario's
        composer and offer private local copies for remote items. Recognize
        its private attachment drag format and explain that the attachment
        must be saved from the mail client instead of saving the reference as
        a file; accept real file content or URLs through the existing KIO drop
        and conflict handling when provided. Never contact a mail server.
  - [ ] Mail file preview: show local `.eml` (`message/rfc822`) headers,
        plain-text body and attachment list in the preview panel without
        loading remote content, running scripts or decrypting messages. Offer
        explicit attachment extraction with normal conflict handling and
        opening through the registered application; never import messages.
  - [ ] Shared context with Plasma Impresario: use Plasma Activities, not a
        private API, as the only shared context with its planned
        account-to-workspace associations. Each application follows Activity
        changes with its own associations; do not read or change the other
        application's workspace, account or mail data.

**Planned caveat:**
[Password-gated folders](CAVEATS.md#planned-password-gated-folders), tracked in
[#406].

### M28: Integration settings

**Project:**
[M28 milestone](https://github.com/mikepadiernos/plasma-opus/issues/35)

Settings for M27 integrations, stored in KConfig.

- **File manager**

  - [ ] **Default:** show the current desktop file-manager association and offer
        an explicit action to set Plasma Opus as the user default; report
        failures
  - [ ] **System file chooser:** show whether Plasma Opus serves the XDG portal
        file chooser, with explicit actions to complete setup or restore the
        previous chooser; report failures

- **Plasma notifications**

  - [ ] **Operations:** choose which Plasma Opus-owned operation events produce
        notifications; use KDE's notification settings for shared controls

- **Mount manager**

  - [ ] **Display:** show only removable devices by default, hide pseudo, snap
        and other system file systems, and show the quick-mount menu on the
        toolbar

- **KWin scripts**

  - [ ] **Window management:** show whether the Plasma Opus KWin script is
        installed and enabled, with explicit actions to install, update,
        disable or remove it; choose which behaviours it applies, such as
        workspace window restoration, tiling opened windows and following
        virtual desktops, and open KWin's settings for shared window rules
  - [ ] **Drop-down window:** enable it and choose its screen edge, width,
        height, screen, animation and trigger, and whether it hides when it
        loses focus

- **Baloo**

  - [ ] **Search:** choose whether to offer fuzzy ranking and live-search
        fallback; reuse M10 and M22 settings and open KDE's shared indexing
        controls rather than duplicating or silently changing them.
  - [ ] **Categories:** choose which file-type categories appear in Places and
        their criteria, such as the large-file size

- **Terminal**

  - [ ] **Provider:** select Konsole, Yakuake or Kwi for separate sessions and
        Konsole or Kwi for embedded sessions, independently of M10 placement

- **Editor**

  - [ ] **Provider:** select the built-in editor or an available KWrite, Kate,
        Neovim, Rune, micro or nano integration, plus the session type for
        terminal editors
  - [ ] **Markdown:** choose a local vault and the Markdown editing and preview
        behavior, retaining source and draft safeguards.
  - [ ] **Workspace vault:** enable vault editing for a workspace and choose an
        assigned local location. Configure available editing, preview,
        navigation, wiki-link, backlink, tag and relationship-view options.
        Preserve drafts and notes and report unavailable roots or features and
        incomplete indexing. See the
        [pending handoff](TODO.md#workspace-vault-editing-565).

- **Office documents**

  - [ ] **Provider:** choose an available office application for editing and use
        installed preview providers where supported; report unavailable choices

- **Ebooks and fonts**

  - [ ] **Previews:** enable ebook, comic, help-file and font previews and set
        the font sample text

- **Media**

  - [ ] **Backend:** choose Qt Multimedia or the optional embedded mpv backend
        for preview and gallery playback.

- **System tray**

  - [ ] **Behaviour:** choose whether to minimise and close to the tray

- **Shortcuts**

  - [ ] **Global:** enable or disable the integration's global shortcut and
        choose its key combination through KDE facilities

- **Remote connections**

  - [ ] **Profiles:** manage SSH, SFTP, SMB, FTP or FTPS and WebDAV hosts,
        ports, usernames and connection defaults; use KDE credential facilities
        instead of storing credentials in KConfig or URLs
  - [ ] **Discovery:** show or hide services discovered on the local network

- **Cloud storage**

  - [ ] **rclone:** enable the integration, choose which remotes to show and
        whether to serve them over WebDAV or mount them
  - [ ] **Remote drives:** choose the mount backend for each protocol from KIO
        FUSE and installed alternatives, the mount folder, read-only by
        default, and whether to mount at login or workspace activation and
        unmount on exit
  - [ ] **Sync pairs:** manage two-way sync pairs and their mode, filters,
        deletion handling and default conflict choice; choose whether to sync
        from login or on metered connections, and how often to check remotes
        that do not report changes
  - [ ] **Transfers and cache:** set the bandwidth limit, parallel transfers,
        cache size and offline retention for mounted remotes, and whether to
        show the tray activity panel

- **LAN sharing**

  - [ ] **Defaults:** protocol, TLS, network interfaces, port, timeout and
        whether uploads are allowed

- **Online accounts**

  - [ ] **Access:** opt in and choose which supported accounts to show in Plasma
        Opus; manage accounts in KDE without deleting shared accounts when
        removing them from Plasma Opus

- **Application packages**

  - [ ] **Integration:** independently enable AppImage, Flatpak and Snap actions
        and configure optional AppImage desktop integration

- **Archives**

  - [ ] **Creation:** default 7z or ZIP format, split size and whether to prompt
        for encryption, extending M04 archive defaults

- **rsync**

  - [ ] **Backend:** enable the optional backend and choose safe sync defaults;
        reuse M10 profiles and preserve preview and confirmation requirements

- **Atuin**

  - [ ] **History:** enable the history action and select a supported terminal
        session without changing Atuin's history or sync settings

- **Plasma Activities**

  - [ ] **Workspaces:** enable Activity associations; manage workspace mappings,
        Activity-specific default folders, feature selections and
        workspace-switch prompts without changing KDE's Activity settings. For
        each association, inherit workspace defaults or select all, individual
        or no behaviours.

- **Mail**

  - [ ] **Client:** show the current default mail client and open Plasma's
        Default Applications settings to change it; enable or disable the
        Send by Email action
  - [ ] **Remote files:** choose whether Send by Email asks before preparing
        local copies of remote files, without allowing silent downloads
  - [ ] **Mail file preview:** enable or disable `.eml` previews, within the
        existing preview size limits

- **KDE Connect**

  - [ ] **Sharing:** enable the sharing action and choose whether to remember a
        device without changing pairing or KDE Connect settings

- **KRunner**

  - [ ] **Discovery:** enable the optional runner and choose which saved
        workspaces and locations it can expose

- **MCP**

  - [ ] **Access:** configure server access (off, read or write), token and
        audit log

- **Password-gated folders**

  - [ ] **Access:** manage protected local folders and their passwords, with a
        clear warning that the gate applies only inside Plasma Opus

- **Encryption**

  - [ ] **Plasma Vault:** show or hide vaults and their lock state
  - [ ] **Files:** enable GnuPG and AES Crypt actions and choose how output
        files are named

### M29: Nextcloud integration

**Project:**
[M29 milestone](https://github.com/mikepadiernos/plasma-opus/issues/586)

Provide optional native Nextcloud navigation, file management and digital asset
management, reusing M27 KDE Online Accounts and KIO WebDAV rather than replacing
the Nextcloud desktop client or its synchronization engine. See the
[Nextcloud integration](integrations/nextcloud.md) for app interfaces and
considered Calendar, Maps, Deck and Notes workflows.

- [ ] Connect supported accounts through KDE Online Accounts or an explicit
      Nextcloud login. Use KDE credential facilities, verify TLS and report
      expired access, unavailable providers and server capabilities.
- [ ] Browse Nextcloud files in either pane and reuse supported KIO operations,
      confirmations, cancellation and error reporting. Respect server
      permissions and report unsupported operations and partial failures.
- [ ] Preview supported remote digital assets and inspect available server
      metadata, respecting existing preview limits and remote-download policy.
- [ ] Search and organize assets using supported server metadata, tags and
      favorites. Keep server tags distinct from native local tags; identify
      unavailable features and incomplete results.
- [ ] Browse Nextcloud Photos albums, shared albums and places as virtual
      folders, sorting and grouping by server capture date and location
      without downloading files. Add photos to albums or remove them with
      confirmation; removing a photo from an album never deletes the file.
      Respect shared-album permissions and report when Photos is unavailable.
- [ ] View and manage supported Nextcloud shares with explicit authorization and
      confirmation before broadening access or revoking a share. Offer to send
      a reviewed share link through the M27 Send by Email action.
- [ ] Open configured local sync folders and the existing desktop client,
      showing available status and distinguishing remote, local, offline and
      unknown state. Never start a second sync engine or change the client's
      configuration.

### M30: Nextcloud settings

**Project:**
[M30 milestone](https://github.com/mikepadiernos/plasma-opus/issues/587)

Settings for M29 Nextcloud integration, stored in KConfig without credentials:

- [ ] Configure account references, supported browsing and preview choices,
      asset metadata and search options, sharing controls and desktop-client
      integration. Reuse KDE Online Accounts, existing WebDAV profiles, remote
      preview policy and KDE credential facilities; explain unavailable server
      features and retain safe defaults.
- [ ] Show or hide Photos albums, shared albums and places, choose their
      default grouping and whether to confirm album changes

### M31: Keyboard shortcut settings

**Project:**
[M31 milestone](https://github.com/mikepadiernos/plasma-opus/issues/29)

- [ ] Configure simultaneous key combinations and sequential key chords with
      conflict detection, clear feedback and reset to defaults; use KDE shortcut
      facilities where supported. Integrate advanced-search shortcuts, Yazi
      navigation and presets, and the integration global shortcut with these
      controls without overriding existing bindings.
- [ ] Scope bindings to the applicable view, folder tree, viewer or application;
      detect conflicts between overlapping contexts and preserve typing and
      accessibility keys in input controls
- [ ] Store Vim-style mode and app-local bindings in KConfig; manage optional
      global shortcuts through KDE facilities and report registration failures
      or platform limitations
- [ ] Allow alternative bindings for an action and configurable shortcuts for
      commands outside a toolbar, including custom commands
- [ ] Show a searchable keyboard map of action names, descriptions, bindings and
      contexts. Search by action name or key combination and export a printable
      reference.
- [ ] Provide shortcuts for pane and tab navigation, selection, filtering,
      preview and terminal focus as features become available, without
      overriding existing bindings by default
- [ ] Offer optional Vim-style navigation in listings and panes with a
      discoverable shortcut reference. Keep it off by default and do not
      intercept typing in text fields or the embedded terminal.
- [ ] Show the active Vim-style mode and provide a clear way to enter filtering
      or text without conflicting with navigation keys
- [ ] Provide a native sliding or floating shortcut hint pane inspired by
      [which-key.nvim](https://github.com/folke/which-key.nvim). Show pending
      key sequences and context-appropriate continuations grouped with action
      descriptions; support keyboard navigation and dismissal.
- [ ] Configure the hint panel's enablement and display delay, and offer an
      on-demand shortcut reference. Cancel or time out incomplete sequences
      without unexpectedly running a conflicting action.
- [ ] Import and export shortcut bindings with conflict preview; do not execute
      commands from imported files

### M32: Refinements

**Project:**
[M32 milestone](https://github.com/mikepadiernos/plasma-opus/issues/28)

Polish the existing interface across menus, settings, layouts and workflows.
Preserve user customizations and use KDE facilities; review future pages as they
become available without blocking this milestone on unimplemented work. See the
[editable M32 plans](refinements/README.md) for proposed sequencing and
subsection details.

- **M32.01: Menus**

  See the [menus plan](refinements/01-menus.md).

  - [ ] Synchronize menu and toolbar action state with the active pane,
        selection and location as tabs change or listings refresh. Keep
        context-menu actions targeted at the selection for which they were
        opened.
  - [ ] Review main, pane and context menus for logical groups, consistent
        action names and discoverable shortcuts. Reuse KXmlGui and KDE actions
        while preserving customized menus and destructive-action confirmations.
  - [ ] Make toolbar and pane actions consistent and discoverable, with overflow
        in narrow windows and a clear indication of the affected pane. Preserve
        toolbar customization and shortcut bindings.
  - [ ] Reorder, hide and restore Plasma Opus actions in context menus,
        extending the M02 action choices, with a reset to defaults. Keep KDE
        service-menu order and Dolphin-shared visibility separate. (MiXplorer)

- **M32.02: Layouts**

  See the [layouts plan](refinements/02-layouts.md).

  - [ ] Restore windows to a usable, visible position after monitor, resolution
        or scale changes while preserving saved geometry for when the display
        returns. Do not override placement decisions owned by the window
        manager.
  - [ ] Review splitter and panel behavior when changing orientation,
        maximizing, hiding panels or swapping panes. Preserve each applicable
        saved layout and make it easy to recover a pane or panel that has become
        too small to use.
  - [ ] Adapt panes, panels, dialogs and menus to small windows, high DPI, long
        translations and right-to-left layouts. Keep both panes and essential
        controls accessible without discarding user-selected layouts.
  - [ ] Implement the five-region [interface design](philosophy/designs.md),
        with file panes in the middle column, sidebar panes in either side
        column and embedded panes in either side column, the middle column or
        footer. Preserve existing placements and saved layouts, and integrate
        the advanced-search panel and tag manager when available; see
        [#583](https://github.com/mikepadiernos/plasma-opus/issues/583).
        Review first-run pane, tab, panel, toolbar and window-layout defaults;
        apply revised defaults only to new configurations or an explicitly
        confirmed reset, never over saved layouts or sessions.
  - [ ] Extend the existing toolbar customizer so users can add, arrange and
        remove custom items, including saved commands and scripts, on the main
        toolbar and either pane toolbar. Preserve per-toolbar placement, labels
        and icons across restarts and report unavailable items without silently
        discarding them.
  - [ ] Provide a way to revert a toolbar's edits made during the current
        customization session and to reset an individual toolbar after
        confirmation. Keep other toolbars, menu entries and saved commands
        unchanged; explain which edits have already been saved.
  - [ ] Make built-in toolbar actions easy to find and add to existing
        customized toolbars, including actions introduced by updates. Offer a
        searchable, labeled reference as a sliding or floating pane without
        requiring a reset or automatically changing saved layouts.
  - [ ] Harmonize spacing, alignment, icons, labels and visual hierarchy across
        windows, panels, menus and dialogs. Follow the active KDE theme and icon
        conventions without hard-coding colours or replacing user-selected
        styling.

- **M32.03: Interaction**

  See the [interaction plan](refinements/03-interaction.md).

  - [ ] Prevent late listing, preview and search results from updating the wrong
        pane, tab or dialog after navigation, cancellation or closure. Retain
        the user's current selection and focus where possible.
  - [ ] Review destructive-operation confirmations to identify affected items,
        destination and consequences where applicable; retain KDE confirmation
        policies and use a safe default without introducing redundant prompts.
  - [ ] Clarify which pane supplies selected items and which location receives
        an operation before it runs, using accessible labels rather than color
        alone. Allow changing the active pane without losing its selection and
        do not change KIO's operation or conflict semantics.
  - [ ] Make partial completion, skipped items and failures in batch operations
        understandable, with actionable details and a clear distinction between
        cancellation and rollback. Reuse existing KIO reporting where
        appropriate.
  - [ ] Explain loading, empty, filtered, unavailable and error states. Show
        progress and cancellation where supported, explain disabled actions and
        avoid duplicate KIO notifications or hidden failures.
  - [ ] Review keyboard navigation, focus restoration, screen-reader names and
        contrast across workflows. Reuse M31 shortcuts and KDE accessibility
        facilities without intercepting text or terminal input.
  - [ ] Give icon-only controls meaningful tooltips and accessible names,
        including relevant shortcuts and pane context where useful. Keep those
        hints current when the user changes shortcuts or toolbar configuration.
  - [ ] Make active file-list filters and their scope visible in the affected
        pane, including visible and hidden counts where available. Provide a
        clear way to remove the quick filter without implying that other filters
        were removed; distinguish filtering the current listing from searching
        other locations.
  - [ ] Display long, similar and bidirectional filenames clearly in listings
        and operation dialogs, with a way to inspect the full original name.
        Preserve the actual filename and avoid truncation that hides a
        meaningful extension.
  - [ ] Improve contextual help and discovery of pane actions, view modes and
        customization without mandatory onboarding or changes to existing
        settings.
  - [ ] Provide context-sensitive help from the main window, settings pages and
        supported dialogs using KDE help facilities and an accessible Help
        action. Open the relevant local documentation where available; explain
        missing help instead of silently failing, and avoid overriding user
        shortcut bindings.
  - [ ] Make adding and editing custom toolbar items discoverable and usable by
        keyboard as well as drag and drop. Pane toolbar items must use the
        intended pane and selection; preserve command preview and confirmation
        and do not delete the underlying saved command when removing a toolbar
        item.
  - [ ] Add subtle, consistent animations for UI transitions such as panels and
        dialogs where they improve orientation. Respect desktop animation and
        reduced-motion preferences where available, provide an app-level way to
        disable motion, and keep interactions responsive and states clear
        without it. Test both animated and motion-disabled behavior.
  - [ ] Add focused interaction tests for menu actions, default and restored
        layouts, keyboard and accessible navigation, responsive controls and
        error states; preserve existing user customizations in regression cases.

- **M32.04: Settings**

  See the [settings plan](refinements/04-settings.md).

  - [ ] Make settings persistence and recovery reliable across reopening the
        dialog, multiple windows and application restarts. Detect and report
        save failures or conflicting external changes without silently
        discarding user edits.
  - [ ] Review closing the settings dialog with pending changes, including
        window close and keyboard dismissal. Give a clear apply, discard or
        cancel choice where needed, without implying that immediate external
        changes are undone.
  - [ ] Explain Apply, OK and Cancel, including pending changes and restoration
        of live previews when changes are cancelled
  - [ ] Validate inputs with actionable inline errors and focus the affected
        control; explain unavailable or dependent options
  - [ ] Offer an explicit choice to restore defaults for the current page or all
        pages, with confirmation before discarding changes
  - [ ] Audit settings that open external KDE dialogs or immediately modify
        saved searches, collections, presets and commands. Make their scope and
        immediate effects clear before the action; do not imply that Cancel can
        undo changes owned by another component.
  - [ ] Clarify the boundaries of reset, backup and restore: preview affected
        app-owned settings, distinguish excluded credentials and shared KDE
        data, and preserve confirmation and restart requirements from M12.
  - [ ] Support keyboard navigation, visible focus, screen-reader labels and
        accessible error messages throughout the dialog
  - [ ] Adapt pages and navigation to small windows, high DPI, long translations
        and right-to-left layouts without clipping controls or labels
  - [ ] Search settings by name and description; show matching pages and open
        the relevant tab and control
  - [ ] Add keyboard-accessible filtering to existing long, searchable
        configuration lists where useful. Show when no entries match and let
        users clear the filter without changing hidden items or losing unsaved
        edits.
  - [ ] Explain setting scope and precedence where defaults can be overridden by
        folder formats, pane locks, operation choices, saved profiles or
        workspace layouts. Distinguish the saved default from the effective
        value without changing the underlying precedence rules.
  - [ ] Make page grouping, labels, descriptions and terminology consistent;
        explain settings that affect shared KDE configuration or require a
        restart
  - [ ] Provide concise contextual help where a setting's effects are not
        obvious
  - [ ] Let users mark frequently used settings pages for quick access and open
        a specific page from an appropriate interface action. Preserve page
        identity across navigation and restarts without changing settings
        values.
  - [ ] Keep settings pages responsive when loading large lists, profiles or
        provider data. Explain missing dependencies and permission failures
        without blocking the dialog or silently replacing saved choices.
  - [ ] Establish reusable search, accessibility, validation and help
        conventions for existing and future pages, including shortcuts, search,
        tagging, integrations, themes and branding. Review pages as implemented
        without blocking M32 on features that remain unavailable.
  - [ ] Add focused tests for settings search, keyboard access, validation,
        Apply/Cancel and defaults, persistence failures and shared or immediate
        changes across representative existing pages. Extend coverage as later
        settings pages are added.

- **M32.05: Performance and startup**

  See the [performance and startup plan](refinements/05-performance.md).

  - [ ] Profile and benchmark cold and warm startup through the first
        interactive window, settings opening and first-page readiness. Record
        representative configurations, timing distributions and memory use, then
        set measurable improvement targets against the baseline.
  - [ ] Reduce GUI-thread startup work by deferring nonessential initialization
        and loading where appropriate. Preserve restored sessions, pane
        readiness, error reporting and user-selected startup behavior.
  - [ ] Speed up opening settings and switching pages through measured
        improvements such as lazy page creation and background loading where
        safe. Preserve search, validation, Apply/Cancel, immediate effects and
        unsaved changes, including for pages not yet opened.
  - [ ] Measure clean and incremental build times, identify bottlenecks and
        evaluate improvements against the same baseline without compromising
        build correctness or the existing CMake workflow.
  - [ ] Prototype and integrate Rust implementations for selected, profiled
        self-contained core hotspots where benchmarks demonstrate a benefit. Use
        the M21 pilot where applicable, a narrow safe C++/Rust interface and
        existing Qt/KDE facilities; account for build, packaging and dependency
        costs without rewriting widgets or replacing KIO.
  - [ ] Add reproducible performance checks and functional regression tests for
        startup, settings and any Rust-backed paths. Compare before and after
        under the same conditions, cover error and cancellation behavior, and
        avoid hardware-dependent pass/fail thresholds in ordinary tests.

- **M32.06: Extensive functional and security testing**

  See the [testing plan](refinements/06-testing.md).

  Use the [edge-case inventory](EDGE.md) as input; its scenarios are not
  confirmed bugs and may overlap existing tests.

  - [ ] Inventory every implemented feature and map its supported entry points
        to reproducible automated or documented manual tests. Record gaps and
        unavailable features explicitly; use coverage reports to identify gaps
        without treating coverage as proof of correctness. Extend the matrix as
        functionality ships without blocking M32 on unimplemented milestones.
  - [ ] Review security-sensitive trust boundaries and test adversarial inputs
        for file operations, archives, paths and symbolic links, external
        commands, terminal integration and imported data. Verify validation,
        permissions, confirmations and error handling without executing
        untrusted code.
  - [ ] Test privacy and secret handling in settings, sessions, backups, logs,
        temporary files and remote connections. Use isolated test config, home
        and trash locations and synthetic credentials; never use real user data.
  - [ ] Run suitable static analysis and dependency checks and perform a
        documented security review. Evaluate tools such as clang-tidy and
        PVS-Studio, including licensing and integration costs, and use the C++
        Core Guidelines where compatible with Qt/KDE ownership and conventions.
        Track reproducible findings, severity and remediation separately; do not
        claim that testing proves the application is secure.
  - [ ] Run focused tests with AddressSanitizer and UndefinedBehaviorSanitizer
        on a supported toolchain. Evaluate ThreadSanitizer separately for
        concurrent paths and leak detection where supported. Consider
        MemorySanitizer or HWAddressSanitizer only where their toolchain and
        platform requirements can be met. Document unsupported environments and
        investigate findings before treating them as defects.
  - [ ] Evaluate running the applicable tests, formatting and analysis in CI
        using documented toolchains and dependencies. Keep sanitizer
        configurations separate where required and report unavailable checks
        explicitly.
  - [ ] Exercise workflows end to end alongside unit and integration tests.
        Cover successful operations, invalid input, cancellation, failures,
        recovery, persistence and feature interactions. Verify destructive
        operations and confirmations using isolated temporary fixtures.
  - [ ] Test accessibility, keyboard and pointer interaction, animations,
        layouts, translations and desktop configurations across a documented
        representative matrix. Include missing optional dependencies and local
        and remote failure scenarios; report environments that could not be
        exercised.
  - [ ] Conduct documented usability walkthroughs of common first-run and
        experienced workflows, including keyboard and assistive-technology use.
        Track confusing steps and regressions with reproducible examples and
        resolve or triage them.

## Feature references

The roadmap adapts workflows to KDE rather than copying platform-specific
interfaces. The additions above draw on these sources:

- [Dolphin view and selection](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/dolphin-view.html)
  and
  [search](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/quick-tips.html).
- [Dolphin panels](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/panels.html)
  and
  [Directory Opus folder tabs](https://docs.dopus.com/doku.php?id=preferences:preferences_categories:folder_tabs:folder_tab_bar),
  [status bars](https://docs.dopus.com/doku.php?id=preferences:preferences_categories:file_displays:status_bar)
  and
  [viewer pane](https://docs.dopus.com/doku.php?id=basic_concepts:the_lister:viewer_pane).
- Directory Opus manual:
  [introduction](https://docs.dopus.com/doku.php?id=introduction),
  [preferences](https://docs.dopus.com/doku.php?id=preferences),
  [customization](https://docs.dopus.com/doku.php?id=customize:the_customize_dialog),
  [source and destination](https://docs.dopus.com/doku.php?id=basic_concepts:source_and_destination)
  and
  [filter bar](https://docs.dopus.com/doku.php?id=basic_concepts:searching_and_filtering:filter_bar).
- [Krusader synchronizer](https://docs.kde.org/trunk_kf6/en/krusader/krusader/synchronizer.html),
  [MountMan](https://docs.kde.org/trunk_kf6/en/krusader/krusader/mount-man.html)
  and
  [features](https://docs.kde.org/trunk_kf6/en/krusader/krusader/features.html).
- [Max File Manager](https://github.com/majidasgari/max-file-manager), alongside
  this project's [UI](parity/ui.md) and [backend](parity/backend.md)
  inventories.
- [KWin scripting](https://develop.kde.org/docs/plasma/kwin/), for window
  placement that Wayland applications cannot perform themselves, and
  [Yakuake](https://apps.kde.org/yakuake/)'s drop-down window.
- [Strata](https://github.com/lgse/strata), especially its opt-in system file
  chooser served through the XDG file chooser portal.
- [RaiDrive](https://www.raidrive.com/), which mounts cloud, S3-compatible and
  NAS storage as local drives, informing remote drives built on KIO FUSE and
  FUSE backends as alternatives to rclone.
- [Syncative](https://github.com/Santuzius/syncative),
  [SDrive](https://github.com/TitanniOpen/SDrive) and
  [DucksDrive](https://patopatetico2-lab.github.io/ducksdrive/), informing
  two-way cloud sync with backup modes and conflict choices, and the cloud
  activity panel for transfers, quota, pending uploads and cache limits.
- [MiXplorer](https://mixplorer.com/) and its
  [MiX Silver listing](https://play.google.com/store/apps/details?id=com.mixplorer.silver),
  an Android file manager whose encrypted containers, file encryption, LAN
  servers, cloud providers, archive editing, file-type categories and menu
  customization are adapted to KDE facilities.
- [Plasma Impresario](https://github.com/mikepadiernos/plasma-impresario), the
  companion KDE mail client, and its
  [Plasma Opus integration notes](https://github.com/mikepadiernos/plasma-impresario/blob/main/docs/integrations/plasma-opus.md).
- [TagStudio](https://github.com/TagStudioDev/TagStudio), especially its
  libraries, tag relationships and search.
- [TagSpaces workspaces](https://docs.tagspaces.org/workspaces/): workspace
  organization of locations, tag groups and search.
- [Directory Opus](https://www.gpsoft.com.au/) and its
  [file collections](https://docs.dopus.com/doku.php?id=basic_concepts:virtual_file_system:file_collections).
- Keyboard references:
  [Directory Opus keyboard map](https://resource.dopus.com/t/list-of-keyboard-shortcuts/26759),
  [Directory Opus shortcut editor](https://docs.dopus.com/doku.php?id=customize:the_customize_dialog:keys),
  [Krusader key bindings](https://docs.kde.org/trunk_kf6/en/krusader/krusader/key_bindings.html)
  and
  [Dolphin commands](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/command-reference.html).
- Directory Opus Resource Centre:
  [folder formats](https://resource.dopus.com/t/folder-formats-detailed-guide/11718),
  [navigation history](https://resource.dopus.com/t/how-to-remember-scroll-positions-and-file-selections/1297),
  [folder sizes](https://resource.dopus.com/t/calculate-folder-sizes-automatically/2064),
  [duplicate selection](https://resource.dopus.com/t/find-duplicate-files/19799),
  [path filters](https://resource.dopus.com/t/how-to-filter-items-by-location-or-sub-folder/7644),
  [rename presets](https://resource.dopus.com/c/rename/30),
  [preview autoplay](https://resource.dopus.com/t/viewer-pane-turn-off-auto-play-for-video-and-audio/33976)
  and
  [configuration backup](https://resource.dopus.com/t/how-to-backup-or-locate-your-opus-configuration-simple/5465).

The existing Orcinus entries reflect earlier project notes; an authoritative
upstream source could not be verified for this review.

## Tracking

Follow the [project instructions](../CLAUDE.md) for issue tracking, completion,
branching and merging. Record blocked work in [pending tasks](TODO.md) and
document feature limitations in [caveats and limitations](CAVEATS.md). Native
dock and column-header spacing follows the
[active widget style](CAVEATS.md#native-title-and-header-padding)
([#560](https://github.com/mikepadiernos/plasma-opus/issues/560)).

[#36]: https://github.com/mikepadiernos/plasma-opus/issues/36
[#255]: https://github.com/mikepadiernos/plasma-opus/issues/255
[#256]: https://github.com/mikepadiernos/plasma-opus/issues/256
[#257]: https://github.com/mikepadiernos/plasma-opus/issues/257
[#258]: https://github.com/mikepadiernos/plasma-opus/issues/258
[#259]: https://github.com/mikepadiernos/plasma-opus/issues/259
[#260]: https://github.com/mikepadiernos/plasma-opus/issues/260
[#261]: https://github.com/mikepadiernos/plasma-opus/issues/261
[#262]: https://github.com/mikepadiernos/plasma-opus/issues/262
[#263]: https://github.com/mikepadiernos/plasma-opus/issues/263
[#264]: https://github.com/mikepadiernos/plasma-opus/issues/264
[#265]: https://github.com/mikepadiernos/plasma-opus/issues/265
[#266]: https://github.com/mikepadiernos/plasma-opus/issues/266
[#267]: https://github.com/mikepadiernos/plasma-opus/issues/267
[#302]: https://github.com/mikepadiernos/plasma-opus/issues/302
[#303]: https://github.com/mikepadiernos/plasma-opus/issues/303
[#304]: https://github.com/mikepadiernos/plasma-opus/issues/304
[#307]: https://github.com/mikepadiernos/plasma-opus/issues/307
[#309]: https://github.com/mikepadiernos/plasma-opus/issues/309
[#311]: https://github.com/mikepadiernos/plasma-opus/issues/311
[#312]: https://github.com/mikepadiernos/plasma-opus/issues/312
[#314]: https://github.com/mikepadiernos/plasma-opus/issues/314
[#315]: https://github.com/mikepadiernos/plasma-opus/issues/315
[#316]: https://github.com/mikepadiernos/plasma-opus/issues/316
[#317]: https://github.com/mikepadiernos/plasma-opus/issues/317
[#318]: https://github.com/mikepadiernos/plasma-opus/issues/318
[#321]: https://github.com/mikepadiernos/plasma-opus/issues/321
[#322]: https://github.com/mikepadiernos/plasma-opus/issues/322
[#335]: https://github.com/mikepadiernos/plasma-opus/issues/335
[#336]: https://github.com/mikepadiernos/plasma-opus/issues/336
[#362]: https://github.com/mikepadiernos/plasma-opus/issues/362
[#372]: https://github.com/mikepadiernos/plasma-opus/issues/372
[#373]: https://github.com/mikepadiernos/plasma-opus/issues/373
[#374]: https://github.com/mikepadiernos/plasma-opus/issues/374
[#375]: https://github.com/mikepadiernos/plasma-opus/issues/375
[#376]: https://github.com/mikepadiernos/plasma-opus/issues/376
[#377]: https://github.com/mikepadiernos/plasma-opus/issues/377
[#406]: https://github.com/mikepadiernos/plasma-opus/issues/406
[#433]: https://github.com/mikepadiernos/plasma-opus/issues/433
[#434]: https://github.com/mikepadiernos/plasma-opus/issues/434
[#436]: https://github.com/mikepadiernos/plasma-opus/issues/436
[#609]: https://github.com/mikepadiernos/plasma-opus/issues/609
[#680]: https://github.com/mikepadiernos/plasma-opus/issues/680
[#687]: https://github.com/mikepadiernos/plasma-opus/issues/687
[#689]: https://github.com/mikepadiernos/plasma-opus/issues/689
