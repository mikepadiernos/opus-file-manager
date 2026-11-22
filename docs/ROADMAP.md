# Plasma Opus roadmap

**[Documentation](README.md)** · **[Scope](philosophy/scope.md)** ·
**[Inspirations](philosophy/inspirations.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

Plasma Opus is a dual-pane KDE Plasma file manager built with C++20, Qt 6
Widgets and KDE Frameworks 6. The [upstream inventories](README.md#behaviour-references)
record the Max File Manager behaviour that informs its parity goals.

> [!NOTE]
> The latest published MVP is
> [v0.2.0](https://github.com/mikepadiernos/plasma-opus-file-manager/releases/tag/v0.2.0).
> This roadmap tracks `main` and future work; M21 Workspaces is next.

## Architecture

- `src/core/` is the `opusfm_core` static library for logic without widgets.
- `src/app/` holds the widgets and the `opus-file-manager` executable.
- `tests/` holds QtTest suites run through `ctest`; GUI tests use
  `QT_QPA_PLATFORM=offscreen`.

Rust is a candidate for self-contained core logic, including measured
performance hotspots, not a wholesale rewrite of C++ features. Keep widgets,
settings and file operations in Qt/KDE, including KIO for local and remote
operations. The M15 Rust pilot can inform later work, including consideration
of M09 duplicate detection and rename or sync planning; this evaluation does
not block M09 or change KIO's role.

## Plasma-native decisions

| Upstream approach | Plasma Opus approach |
| --- | --- |
| Custom themes, title bars and window buttons | Plasma widget style, colour scheme, fonts and KWin decorations by default; optional app-local colours in M25 |
| Bundled icon pack | Active icon theme through `QIcon::fromTheme` |
| Hand-written toolbar and shortcut editors | KXmlGui, `KEditToolBar` and `KShortcutsDialog` |
| Settings in SQLite | KConfig; session state in the KConfig state file |
| Custom copy engine and undo | KIO jobs and `KIO::FileUndoManager` |
| Custom places and device polling | `KFilePlacesView` and Solid |
| Custom Open With and default-app logic | `KFileItemActions`, `KOpenWithDialog` and `KApplicationTrader` |
| Custom properties and checksums | `KPropertiesDialog` |
| Custom clipboard helpers | `QClipboard` and `KIO::setClipboardDataCut` |

Plasma Opus uses a supported subset of the upstream `.mfm.json` schema for local
per-folder metadata, including pins, labels, ratings, notes, colours and view
formats. See [metadata compatibility](CAVEATS.md#folder-metadata-and-listings)
for limitations on writes and unsupported behaviour.

## Milestones

Strikethrough marks features whose issues are Completed or Merged. Unstruck
features may be incomplete, not started or implemented but still in Review.
Where applicable, feature milestones are followed by their settings in KConfig.
Milestone numbers are stable identifiers; the order below is the intended
work sequence.

### M01 Core dual pane

- ~~KXmlGui main window with hamburger menu, editable toolbar and shortcuts~~
- ~~Theme icons for app actions and pane controls, preferring symbolic variants
  when available and preserving file and device emblems~~
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
  in the middle (top pane's tabs along its bottom edge, bottom pane's tabs
  along its top edge)~~

### M02 Core dual pane settings

Settings for M01 features, modelled on Dolphin and Orcinus. Plasma Opus settings are
stored in KConfig; Places sections and trash/delete confirmations are shared
with Dolphin.

- **General**
  - ~~**Settings Dialog:** sidebar icon size (default 24 px) and title position
    (to the right of icons by default, or below)~~
- **Interface**
  - **Folders and Tabs:** ~~startup folders and session; one or two panes in new
    windows; tab bars; Tab switches panes; new-tab placement; navigation bars;
    symbolic-link following~~
  - **Previews:** ~~enablement, plugins, size limit and remote folders~~
  - **Confirmations:** ~~trash, delete, multiple tabs; executable handling;
    default drop action~~
  - **Panels:** ~~Places panel visibility and visible sections; optional focus
    return to the active pane after panel navigation~~
  - **Status and Location Bars:** ~~status-bar contents and bar density,
    including custom bar spacing~~
- **View**
  - **General:** ~~default view mode, sorting, hidden files and tooltips~~
  - **Details:** ~~icon size, columns, item density and dragging from anywhere
    in a row~~
  - **Icons:** ~~icon and preview size and item density~~
- **Context Menu**
  - **General:** ~~optional Copy To and Move To submenus, independently
    configurable alongside the additional KDE submenus~~
  - **Actions:** ~~choose which built-in actions appear~~ (Plasma Opus actions;
    additional KDE transfer submenus are controlled separately)
  - **Service Menus:** ~~choose which installed service actions and plugins
    appear in Plasma Opus separately from Dolphin-shared settings~~

### M03 Listing depth

- ~~Combined Places/Folders panel: combines Places and folder navigation in one
  panel by default instead of showing separate Places and Folders panels.
  Favorites replace bookmarks.~~
- ~~Extra columns and Dolphin-style grouping~~
- ~~Filter syntax and type-ahead~~
- ~~Per-folder view format~~
- ~~Labels, ratings, notes, pins and colours~~
- ~~Folder look dialog (icon and colour)~~
- ~~Ignore list~~
- ~~Tree pane~~
- ~~Show Flat (Ctrl+Shift+F): list every file below the current folder in one
  list without folders. Navigation, tabs and history remain rooted at that
  folder; sorting, filtering, previews and file operations continue to work.
  In details view, Type becomes Relative Path. The mode is remembered per
  folder; Create New, New Folder and Paste are disabled, and dropping files is
  refused.~~
- ~~Browse archives and extract files~~
- ~~Compress local files: choose an archive name, format and destination (this
  folder or the other pane); never overwrite an existing file. Run in the
  background and report success or errors in the status bar. Supported formats
  are ZIP, uncompressed ZIP, tar.gz, tar.xz and tar.zst.~~
- ~~Tabs: show folder icons; fill the tab-bar width equally as tabs are added;
  pin/lock behaviour; custom tab names; folder-based colours; restore tabs with
  the session~~
- ~~Select group: select or deselect by pattern, with saved selection filters
  (Krusader)~~
- ~~Occupied space of files, folders, archives and remote locations (Krusader)~~
- ~~Popular folders and a jump-back point (Krusader)~~
- ~~Archive integrity testing for local ZIP, TAR and 7z files, with password
  prompts for encrypted archives (Krusader)~~
- ~~Password-protected AES ZIP creation and extraction (Krusader)~~
- ~~Password-protected 7z extraction~~
- ~~Display style per pane (Orcinus)~~
- ~~Expandable folders inline in the file listing (Orcinus)~~
- ~~Persistent filters (Orcinus)~~
- ~~File-name highlighting (Orcinus)~~
- ~~Compact view with names beside icons and entries arranged in columns
  (Dolphin)~~
- ~~Optional selection markers and a selection mode with an action bar, so
  multiple items can be selected without modifier keys (Dolphin)~~
- ~~Details-view column-header controls to show or hide columns and fit
  column widths to their contents (Dolphin)~~
- ~~Folder-format rules for a folder and optionally its descendants, with
  explicit precedence over defaults; show which rule is active and allow a
  pane to temporarily lock its current format~~
- ~~Restore selection and scroll position when returning through a tab's
  navigation history, ignoring items that no longer exist~~
- ~~Calculate folder sizes in the background for display in the listing, on
  demand or automatically according to location and folder-format rules;
  optionally refresh calculated sizes periodically~~

**Caveats:** [Archives](CAVEATS.md#archives) and
[folder metadata and listings](CAVEATS.md#folder-metadata-and-listings).
Tracked in [#36], [#255], [#256], [#257], [#258], [#261], [#262], [#263]
and [#265].

### M04 Listing depth settings

Settings for M03 listing depth, stored in KConfig.

- **View**
  - ~~**General:** default visible columns and grouping; size format (human or
    bytes), date format and seconds~~
  - ~~**Icons:** name lines and metadata shown in icon view~~
  - ~~**Details:** enable inline folder expansion and choose automatic or manual
    column widths by default~~
  - ~~**Compact:** icon size, item spacing and additional information~~
  - ~~**Selection:** default selection markers and selection mode~~
  - ~~**Filters:** default scope, type-ahead activation, filter retention when
    changing folders, and file-name match highlighting~~
  - ~~**Folder formats:** manage inherited rules and default size calculation;
    allow separate local and remote policies~~
  - ~~**Folder sizes:** optional periodic refresh of calculated sizes~~
  - ~~**History:** restore selection and scroll position when navigating back
    or forward~~
- **Files**
  - ~~**Rules:** ignore list entries and file-type colours by extension~~
  - ~~**Metadata:** show or hide label, rating, colour and pin indicators; choose
    whether pinned items appear first without changing stored metadata~~
  - ~~**Selection filters:** manage saved selection patterns~~
- **Panels**
  - ~~**Combined Places/Folders:** show or hide the panel, choose its left or
    right position, configure its auto-focus behaviour, and show or hide each
    section independently~~
  - ~~**Titles:** independently show or hide navigation and embedded panel
    titles while retaining panel controls and document status~~
- **Archives**
  - ~~**Default format:** archive format used by Compress (ZIP, uncompressed
    ZIP, tar.gz, tar.xz or tar.zst); the format choice determines compression~~
  - ~~**Destination:** default to the current folder or the other pane when
    creating an archive, retaining the non-overwriting behaviour~~
- **Tabs**
  - ~~**Appearance:** show tab icons; fill the available tab-bar width and
    divide it equally among tabs as new tabs are created~~
  - ~~**Colours:** show or hide folder-based tab colours without removing them~~

**Caveats:** [Folder metadata and listings](CAVEATS.md#folder-metadata-and-listings),
tracked in [#257], [#258], [#262] and [#263].

### M05 Preview and gallery

- ~~Preview panel: images (up to 8 MiB and 40 megapixels) and UTF-8 text (first
  1 MiB by default), PDF and office documents with an installed KPart, and
  audio/video with an installed Qt Multimedia backend, following the active
  selection (F3)~~
- ~~Remote previews: files on KIO locations are downloaded to a temporary file
  first, within the same image and text limits and up to 128 MiB for other
  files~~
- ~~Preview image navigation, gallery and Open controls; formatted HTML and
  Markdown with a source toggle (external resources and links are disabled)~~
- ~~Local gallery with zoom, a looping slideshow, thumbnails, image
  information, EXIF metadata, dominant colours and a luminance histogram
  (Space); follows the pane's sort order or the selected images~~
- ~~Animated images and video playback in the gallery~~
- ~~Rotate, flip, crop, resize and adjust colours (brightness, contrast,
  saturation, greyscale); save a PNG/JPEG/WebP copy or replace the original~~
- ~~Save-copy format selection and JPEG/WebP quality~~
- ~~Line-numbered, syntax-highlighted text previews and preview footer
  ID3/EXIF metadata~~
- ~~Mark images while browsing the gallery and apply file operations to the
  marked selection, without moving files until the user chooses an action
  (Directory Opus)~~
- ~~Batch image conversion with output format, dimensions and quality, a
  destination preview and non-overwriting names (Max)~~
- ~~Optional autoplay for audio and video in the preview panel; keep click-to-play
  available independently of gallery playback~~

**Caveats:** [Previews and gallery](CAVEATS.md#previews-and-gallery), tracked
in [#259], [#260], [#264], [#266], [#267], [#302], [#303] and [#304].

### M06 Preview and gallery settings

Settings for M05 preview and gallery, stored in KConfig.

- **Opening**
  - ~~**Activation:** what activating a file opens (application, gallery or
    preview)~~
- **Preview**
  - ~~**Text:** size limit for text in the preview panel~~
  - ~~**Presentation:** default to formatted or source view for HTML and
    Markdown; show line numbers and syntax highlighting in text previews~~
  - ~~**Media:** autoplay audio and video in the preview panel or require play~~
- **Gallery**
  - ~~**Information:** image info panel shown by default~~
  - ~~**Display:** initial zoom (fit or actual size) and thumbnail visibility~~
- **Slideshow**
  - ~~**Playback:** slideshow interval and looping~~
- **Image conversion**
  - ~~**Output:** default format and quality for saved image copies~~
  - ~~**Batch:** default dimensions, format, quality and destination for batch
    conversion; keep output names non-overwriting~~

File activation follows the desktop's single- or double-click setting and also
applies to keyboard activation. Gallery mode opens local images and videos;
other files open in their default application. Folders and executable handling
are unchanged, and the preview panel's Open button opens the application.

**Caveats:** [Previews and gallery](CAVEATS.md#previews-and-gallery), tracked
in [#260], [#264], [#266], [#302], [#303] and [#304].

### M07 Editor

- ~~KTextEditor-based editor: use it as a panel alongside Information or
  Preview, or as a separate embedded area below or to the right of both panes,
  like the embedded terminal. Neither embedded placement replaces a pane.
  Also support maximised and standalone editor windows.~~
- ~~Drafts~~
- ~~Markdown preview and PDF export~~

**Caveats:** [Editor](CAVEATS.md#editor), tracked in [#267] and [#307].

### M08 Editor settings

Settings for M07 editor, stored in KConfig and KTextEditor configuration.

- **Editor**
  - ~~**Opening:** where files open (panel, separate embedded area, maximised or
    own window)~~
  - ~~**Embedded placement:** below or to the right of both panes when using a
    separate embedded area~~
  - ~~**Text:** font size and word wrap~~
  - ~~**Markdown:** default markdown mode (visual or source)~~
  - ~~**Drafts:** drafts on or off~~
  - ~~**Recovery:** draft autosave interval and retention; store drafts
    privately and allow them to be reviewed or discarded without overwriting
    the original files~~
  - ~~**PDF export:** default page size and margins for Markdown exports~~
- **KTextEditor**
  - ~~**Configuration:** KTextEditor's own configuration pages~~

**Caveats:** [Editor](CAVEATS.md#editor), tracked in [#267], [#307] and [#309].

### M09 Tools

- ~~Live search and indexed search: choose filename or content search and
  current-folder or indexed scope; indicate when locations are not indexed,
  show progress and allow cancellation (Dolphin, Max)~~
- ~~Search results: reveal an item in its containing folder and save searches;
  optionally collect references from different locations without moving the
  original files (Dolphin, Directory Opus)~~
- ~~Duplicate finder: preview groups and comparison criteria before selecting
  files for removal; never delete automatically (Directory Opus)~~
- ~~Duplicate selection: choose candidates by location or date while keeping at
  least one item per group in automatic selection; warn and require explicit
  confirmation if a manual selection would remove every item in a group~~
- ~~Search and selection filters for full path, parent folder and excluded
  subtrees, with an optional recursion-depth limit~~
- ~~Content chart (folder breakdown)~~
- ~~Folder sync: preview differences and planned operations before running;
  allow per-item direction changes and exclusions, with explicit confirmation
  of overwrites and deletions (Krusader)~~
- ~~Folder comparison and sync filters: include/exclude patterns, recursion,
  symlink handling and optional content comparison; save reusable profiles
  (Krusader)~~
- ~~File diff~~
- ~~Batch rename: preview old and new names, detect collisions before applying,
  and support metadata tokens for images and audio (Max)~~
- ~~Batch rename rules for regular expressions and reusable named presets~~
- ~~Symlinks~~
- ~~Listing export to CSV and PDF (Max)~~
- ~~Edit supported audio tags and image metadata, including a preview of changes
  and clear reporting when a format cannot be written (Directory Opus)~~
- ~~Scripts and custom commands~~
- ~~Command presets: save named script or custom-command configurations and
  placeholder defaults; let users review commands and arguments before
  running. Keep rename and synchronization profiles specific to their tools.~~
- ~~Compare folders across the two panes (Krusader)~~
- ~~Create and verify checksum files (Krusader)~~
- ~~Split and combine files (Krusader)~~
- ~~Transfer queue with per-job progress, speed, errors and pause, resume and
  cancellation controls where supported by the underlying KIO job (Krusader,
  Max)~~
- ~~Embedded terminal alongside both panes: below them or at the right of the app
  for portrait/stacked layouts; showing it never replaces either pane~~
- ~~Placeholders for scripts and custom commands~~
- ~~Linked panes and cushion treemap (Orcinus)~~

**Caveats:** [File comparison](CAVEATS.md#file-comparison), tracked in [#311];
[symbolic links](CAVEATS.md#symbolic-links), tracked in [#312]; [listing export](CAVEATS.md#listing-export),
tracked in [#314]; [folder content chart](CAVEATS.md#folder-content-chart), tracked in [#315];
[checksum files](CAVEATS.md#checksum-files), tracked in [#316];
[pane folder comparison](CAVEATS.md#pane-folder-comparison), tracked in [#317];
[split and combine files](CAVEATS.md#split-and-combine-files), tracked in [#318];
[embedded terminal](CAVEATS.md#embedded-terminal), tracked in [#321];
[linked pane navigation](CAVEATS.md#linked-pane-navigation), tracked in [#322];
[cushion treemap](CAVEATS.md#cushion-treemap), tracked in [#335];
[duplicate finder](CAVEATS.md#duplicate-finder), tracked in [#336];
[search](CAVEATS.md#search), tracked in [#362];
[batch rename](CAVEATS.md#batch-rename), tracked in [#372];
[custom commands](CAVEATS.md#custom-commands), tracked in [#373];
[metadata editing](CAVEATS.md#metadata-editing), tracked in [#374];
[transfer queue](CAVEATS.md#transfer-queue), tracked in [#375];
[filtered folder comparison](CAVEATS.md#filtered-folder-comparison), tracked in [#376];
[folder synchronization](CAVEATS.md#folder-synchronization), tracked in [#377].

### M10 Tools settings

Settings for M09 tools, stored in KConfig.

- **Embedded terminal**
  - ~~**Placement:** below both panes or at the right of both panes for
    portrait/stacked layouts; the terminal does not replace a pane~~
  - ~~**Working folder:** optionally follow the active pane's local folder;
    report unsupported locations rather than silently switching folders~~
- **Search**
  - ~~**Defaults:** search scope and whether to search filenames or contents~~
  - ~~**Filters:** reusable path and subtree filters and recursion depth for
    search and selection~~
  - ~~**Saved searches:** manage saved searches and reusable result collections~~
- **Duplicate finder**
  - ~~**Defaults:** comparison criteria and selection preferences; retain the
    safeguard that automatic selection keeps at least one item per group~~
- **Content chart and cushion treemap**
  - ~~**Display:** choose whether to show the treemap and the default table
    sort order; keep scan errors and skipped entries visible~~
- **Linked panes**
  - ~~**Startup:** optionally link the initial pane folders; default to unlinked,
    validate local roots and report when linking is unavailable~~
- **File diff**
  - ~~**Display:** choose whether to wrap lines in the comparison output without
    changing the comparison itself~~
- **Pane folder comparison**
  - ~~**Results:** choose the initial result categories shown; keep errors and
    incomplete comparisons visible and require explicit selection in the panes~~
- **Folder sync**
  - ~~**Transfers:** skip existing, newer only, speed limit~~
  - ~~**Comparison:** default recursion, symlink and content-comparison options;
    saved synchronization profiles~~
- **Batch rename**
  - ~~**Presets:** manage named rename rules and choose a default preset; retain
    the M09 collision preview before applying changes~~
- **Listing export**
  - ~~**Defaults:** output format and export-specific columns, initially based
    on the current listing columns~~
- **Symbolic links**
  - ~~**Naming:** default link-name suffix, editable before creation; preserve
    existing destination checks and never overwrite an existing entry~~
- **Checksum files**
  - ~~**Defaults:** default manifest filename for creation, editable before
    saving; retain the supported SHA-256 format and existing validation~~
- **Split and combine**
  - ~~**Defaults:** default split-part size, editable for each operation; retain
    part-order review and never overwrite existing outputs~~
- **Metadata editing**
  - ~~**Fields:** choose the initially displayed editable audio and image fields
    when the editor is available; retain the change preview and report
    unsupported writes~~
- **Transfer queue**
  - ~~**Behaviour:** queue and concurrency defaults; conflict handling~~
- **Applications**
  - ~~**Associations:** manage desktop default applications by extension through
    KDE facilities; make changes to shared desktop defaults explicit~~
- **Automation**
  - ~~**Commands:** scripts, custom commands and presets~~

**Caveats:** The [M09 tools caveats](#m09-tools) also apply to these settings.

### M11 UI Customizations

- ~~Edit the main toolbar beyond the `KEditToolBar` dialog: drag entries on, off
  and around it directly, add separators and spacers, and choose icons, text or
  both per entry~~
- ~~Custom toolbar buttons for scripts, custom commands and presets~~
- ~~Pane toolbars instead of a fixed path row: each pane gets its own editable
  toolbar (back, forward, up, reload, location bar, view mode, zoom, sorting,
  previews, hidden files, filter, search) that acts on that pane, with the
  location bar dropping to a second row in a narrow pane~~
- ~~Move panes: send the second pane to a tab of its own or pop it out into a
  window, carrying its display style~~
- ~~Drag entries between the main toolbar, pane toolbars and hamburger menu~~
- ~~Customize the combined Places/Folders panel: reorder its sections, show or
  hide each section, and give sections custom titles~~
- ~~Back up and restore app-owned settings and layouts with a preview and
  confirmation; exclude recognized credentials and shared KDE settings, and avoid
  overwriting running configuration without a safe reload or restart~~
- ~~Swap panes: the current tabs or whole sides~~
- ~~Dim the inactive pane~~
- ~~Customize folder tab bars: place them above, below, left or right of each
  pane, with a together option for stacked panes; choose close-button
  visibility without bypassing pinned or locked tab protections. Keep
  keyboard and context-menu actions accessible. (Directory Opus)~~
- ~~Customize status bars: choose a shared bar or one per pane, and arrange
  available item counts, selection size and free-space indicators. Bind each
  pane's values to that pane and keep operation errors visible even when
  optional indicators are hidden. Reuse the existing status-bar preferences.
  (Directory Opus, Dolphin)~~
- ~~Save and reset pane and panel splitter sizes, including preview, editor and
  terminal areas, independently for side-by-side and stacked layouts. Restore
  sizes safely within the available window; preserve both file panes and
  coordinate with session restore and workspace layouts. (Max)~~
- ~~Customize the preview and gallery background for transparent images:
  follow the active Plasma theme, or choose light, dark or checkerboard.
  Keep the image data unchanged. (Directory Opus)~~
- ~~Customize the Information panel's visible metadata fields and their order,
  and optionally show information for a hovered item instead of the current
  selection. Make hover behaviour opt-in and retain selection-based fallback;
  show unavailable metadata without initiating additional remote downloads.
  (Dolphin)~~
- ~~Optional file-list minimap on the right edge of each explorer pane: show a
  compact overview of the current visible files and folders and the scroll
  viewport, with click or drag navigation. Follow the pane's current tab,
  filter, sort and view; keep normal scrolling and keyboard navigation
  available. Bound rendering work for large or remote listings and do not
  read file contents or trigger additional remote downloads.~~

**Caveats:** [File-list minimap](CAVEATS.md#file-list-minimap), tracked in
[#434]; [settings backups](CAVEATS.md#settings-backups), tracked in [#433].

### M12 UI Customization settings

Settings for M11 UI customizations, stored in KConfig.

- **Toolbars**
  - ~~**Entries:** which entries the main toolbar and each pane toolbar show,
    and in what order; customize hamburger menu entries and their order~~
  - ~~**Appearance:** toolbar style and icon size~~
  - ~~**Entry appearance:** icon, text or both for individual toolbar entries~~
  - ~~**Editing:** lock toolbars~~
- **Panes**
  - ~~**Moving:** whether a moved pane opens as a tab or window, and whether it
    keeps or resets its display style~~
  - ~~**Swapping:** swap current tabs or whole pane sides by default~~
  - ~~**Inactive pane:** enable dimming and adjust its intensity~~
- **Combined panel**
  - ~~**Sections:** section order, visibility and custom titles, reusing the
    visibility settings from M04~~
- **Tabs**
  - ~~**Placement and controls:** tab-bar position by pane orientation, together
    placement for stacked panes, and close-button visibility~~
- **Status bars**
  - ~~**Layout:** shared or per-pane bars, with visible indicators and their
    order; reuse existing item-count, selection-size and free-space settings~~
- **Splitter sizes**
  - ~~**Restore:** remember sizes per orientation and reset them to defaults,
    without overriding an explicitly applied workspace layout~~
- **Preview and gallery**
  - ~~**Background:** theme, light, dark or checkerboard for transparent images~~
- **Information panel**
  - ~~**Fields and focus:** visible metadata fields and order; opt-in hover
    information with selection fallback~~
- **File-list minimap**
  - ~~**Display:** enable or disable the minimap and choose its width~~
- **Defaults**
  - ~~**Reset:** restore the main toolbar, pane toolbars and hamburger menu~~
  - ~~**Backup:** export and restore app-owned settings and layouts with a
    preview and confirmation, excluding recognized credentials and shared KDE
    settings~~

**Caveats:** [File-list minimap](CAVEATS.md#file-list-minimap), tracked in
[#434]; [settings backups](CAVEATS.md#settings-backups), tracked in [#433].

### M13 Translations

- ~~Translation catalogues through `ki18n`, fed by the `i18n()` and `i18nc()`
  calls already used throughout the code; provide extraction and installation
  without requiring a translation into any particular language~~
- ~~Right-to-left layout, mirrored for languages that use it~~
- ~~Jalali (Persian) calendar dates~~

**Caveats:** [Jalali dates](CAVEATS.md#jalali-dates), tracked in [#436].

### M14 Translation settings

Settings for M13 translations, stored in KConfig.

- **Language**
  - ~~**Interface:** follow the system language or select an interface language~~
  - ~~**Regional formats:** follow the system locale or select a locale for
    dates and numbers independently of the interface language~~
- **Dates**
  - ~~**Calendar:** calendar for dates: Gregorian or Jalali~~
  - ~~**Format:** custom date and time formats, with clear precedence over the
    listing date-format defaults from M04~~
- **Layout**
  - ~~**Direction:** follow the language or force left-to-right or right-to-left~~
  - ~~**Digits:** digit shapes and custom numeral formats, including Western or
    Eastern Arabic-Indic digits~~

**Caveats:** [Jalali dates](CAVEATS.md#jalali-dates), tracked in [#436];
[translation format overrides](CAVEATS.md#translation-format-overrides),
tracked in [#440].

### M21 Workspaces

Organize locations and tag groups into focused workspace profiles, inspired by
TagSpaces. Each profile can include features, layouts, a theme and other
preferences; see the [interface design](philosophy/designs.md#workspaces).

- Create, switch, rename, duplicate and delete named workspaces, with a full
  name and short label; duplicating copies the layout without reassigning
  locations or groups. Provide a discoverable workspace selector.
- Assign saved locations and, when available, tag groups to a workspace;
  each location or group belongs to at most one workspace. Switching shows
  the active workspace's locations and groups without moving files or
  changing tags; provide access to unassigned locations and groups.
- Scope global search to the active workspace's assigned locations by default,
  with an explicit way to search outside it. If no tag groups are assigned,
  show all groups as in TagSpaces; explain the fallback in the interface.
- Save layouts for different workflows and integrations, including pane
  arrangements, views, toolbars and panel placement or visibility for search,
  tagging, preview, editor and terminal areas as they become available. Save
  the workspace's theme choice, using app-local theme profiles when available.
  Report unavailable features, themes or providers when applying a workspace.
- Extend each pane's existing location bar with Krusader-inspired breadcrumbs:
  show full path segments or abbreviate ancestor segments to their first
  letter while retaining leading underscores (for example, `__path` becomes
  `__p`); always show the current folder's full name. Alternatively, show an
  editable full path. Keep each breadcrumb navigable using its actual location,
  reveal full names for abbreviated segments, and save each pane's chosen mode
  in the workspace layout.
- Show the active workspace and unsaved layout changes clearly, with a
  keyboard-accessible selector and a way to return to the saved layout after
  confirmation.
- Capture and restore a workspace's pane and tab locations, active tabs and
  Yazi mode choices as an explicit layout snapshot; preserve running jobs and
  confirm before replacing unsaved state.
- Restore available workspace components when locations or providers are
  unavailable; identify what could not be restored and offer a retry without
  silently replacing saved locations or changing files.
- Export and import selected workspace profiles using the existing M12 backup
  workflow, with a preview and collision handling; exclude credentials and
  never execute commands or modify managed files during import.
- Allow all, selected or none of a workspace's applicable behaviours to be
  enabled, including location and tag-group presentation, search scope,
  layouts, theme, pane and tab restoration, and Yazi mode. Preserve unselected
  state and existing defaults, and reuse existing controls; M23 can apply
  separate choices per Activity.
- Confirm before overwriting a saved workspace or discarding unsaved layout
  changes; deleting a workspace removes its associations, not the underlying
  files, locations or tags
- Keep workspace configuration local and exclude credentials. Workspaces
  organize the interface and search scope; they are not a security boundary.

### M22 Workspace settings

Settings for M21 workspaces, stored in KConfig.

- **Management:** manage workspaces, their locations and tag-group assignments
- **Vault:** enable vault-oriented editing for a workspace, choose an assigned
  local location and configure available editor, preview and note navigation
  behavior. Keep notes and Obsidian configuration unchanged by settings
  changes; report unavailable roots. See the [pending handoff](TODO.md#workspace-vault-settings-566).
- **Startup:** choose a startup workspace or restore the last-used workspace
  without overriding M02 startup folder and session choices
- **Search:** choose whether workspace-scoped or all-location search is the
  default, reusing the search settings from M10
- **Layouts:** choose which workspace layout components to restore and confirm
  before overwriting saved layouts
- **Pane location bars:** choose full breadcrumbs, abbreviated ancestor
  breadcrumbs or an editable full path independently for each pane; restore
  these choices with the workspace layout
- **Indicators:** show the active workspace and unsaved layout state; choose
  where the workspace selector appears without hiding all ways to access it
- **Snapshots:** choose which pane, tab and Yazi mode state a workspace
  snapshot restores, respecting M02 startup and session choices
- **Recovery:** choose whether to prompt to retry unavailable locations or
  providers; keep saved references until the user explicitly changes them
- **Portability:** select workspace profiles for export or import with the M12
  backup controls and review conflicts before applying changes
- **Features:** choose a workspace's default enabled behaviours individually,
  or select all or none; reuse existing search, layout and theme settings and
  expose per-Activity overrides through M24 when available

### M25 Themes

- Follow the active Plasma colour scheme by default. Offer optional app-local
  light and dark colour presets and installed KDE colour schemes without
  changing the desktop's global appearance, widget style or window decorations.
- Provide an app-local colour editor with named palette roles and a live
  preview. Preserve readable text, selection and focus indicators; validate
  contrast and offer a way to revert without saving.
- Save, import and export versioned app-owned theme profiles, including palette
  choices and optional custom stylesheets, with validation, explicit overwrite
  confirmation and a safe reset to the system appearance; do not execute code
  from theme files.
- Apply app-local colours consistently to supported Plasma Opus windows, panes and
  dialogs while respecting accessibility settings, native icon themes and the
  appearance boundaries of embedded or external components.
- Support an optional app-local custom Qt Style Sheet (QSS, a CSS-like subset,
  not full web CSS) entered in a text box or imported from a file. Preview,
  apply, disable and revert it; export it separately or as part of a theme
  profile. Report unsupported or invalid input, bound resource use and prevent
  unapproved external resource loading.

### M26 Theme settings

Settings for M25 themes, stored in KConfig.

- **Themes**
  - **Appearance:** follow the system or choose an available app-local preset
    or installed KDE colour scheme
  - **Custom colours:** edit, preview, save and reset app-local palette roles,
    with contrast feedback and a reversible preview
  - **Custom stylesheet:** enable or disable QSS, edit it in a text box, import
    or export a file, and preview or revert changes
  - **Profiles:** manage, import and export validated app-owned theme profiles,
    including optional custom stylesheets, with explicit overwrite confirmation;
    allow workspaces to select an available theme profile

### M15 Advanced search

Extend the M09 search tools and reuse their search scopes, filters and saved
searches rather than introducing a separate search system. M23 extends the
Baloo integration without blocking this milestone.

- **Indexing:** search folders and exclusions; configure the search shortcut
  through existing KDE facilities, using double Shift where supported and
  reporting when it is unavailable. Integrate with M29 when available.
- Provide an embedded search pane with the placements defined in the
  [interface design](philosophy/designs.md#panes). Placement alongside the
  file panes must not replace either pane; integrate with the five-region
  layout when available.
- Offer an optional search widget for the main toolbar: enter a query, search
  using the shared scope (initially the active pane's location) and open the
  advanced search panel
- Combine filename, path, file type, size, date and content criteria with
  AND, OR and NOT; validate queries and explain unsupported criteria
- Prototype the query parser and validation in Rust behind a narrow C++/Rust
  interface. Test error handling, ownership, path handling and threading at
  the boundary, and evaluate the build and maintenance costs before adopting
  Rust more broadly; keep the search UI and KIO integration in C++.
- Present M09 search results in the embedded panel, reusing its progress,
  cancellation, error reporting, open and reveal actions without moving files
- Support keyboard navigation and focus between the toolbar widget, search
  panel and panes; integrate with M29 shortcut configuration when available

### M16 Advanced search settings

Settings for M15 advanced search, stored in KConfig.

- **Panel:** choose an embedded-pane placement and whether to restore its
  visibility on startup, preserving both file panes in shared placements
- **Toolbar:** use the M12 toolbar controls to show or hide the search widget;
  choose whether submitting a query opens the panel
- **Results:** choose the default result presentation and whether to retain
  search history; allow history to be cleared
- **Defaults:** reuse the M10 search scope, filters and saved-search settings
  across the toolbar and panel

### M17 Native tagging

A native KDE tagging workflow inspired by TagStudio, integrated into Plasma Opus
rather than requiring TagStudio or changing the user's folder structure.

- Opt-in libraries rooted at existing local folders; index files of any type
  without moving, renaming or duplicating them
- Evaluate Rust for tag relationships, library metadata and search logic
  based on the M15 pilot; retain Qt/KDE integration in C++ and use C++ for the
  core if the added build and interoperability costs are not justified
- Create and manage tags with stable identities, names, aliases, shorthand,
  colours and parent relationships; support duplicate names with clear
  disambiguation and prevent cycles in the hierarchy
- Organize tags into groups independently of workspaces; allow M21 workspaces
  to reference these groups without changing the underlying tags
- Assign and remove multiple tags on selected files and folders from the
  listing or preview; provide a searchable, keyboard-accessible tag manager
  as an embedded pane using the [interface design](philosophy/designs.md#panes)
  placements, and show assigned tags in the file interface. Integrate with the
  five-region layout when available.
- Search and filter by tags, inherited parent tags, untagged items, paths and
  file types, with AND, OR, NOT and grouped expressions; integrate with the
  existing search and selection workflows
- Add editable text and date fields to library entries, alongside existing
  labels, ratings and notes without silently replacing or converting them
- Read supported EXIF image metadata and ID3 audio tags for display and search;
  allow users to map selected fields to library tags or fields with a preview
  and conflict handling. Reuse M09 metadata editing for supported writes, with
  explicit consent and clear reporting of unsupported formats or failures.
- Store library metadata in an open, documented format without requiring a
  sidecar for each file; preserve existing `.mfm.json` metadata and report
  unsupported or read-only locations
- Refresh libraries with progress and cancellation; track Plasma Opus-initiated moves
  and renames, and offer review and manual relinking for missing or externally
  moved files without silently attaching metadata to an uncertain match
- Export and restore library metadata with a preview and explicit confirmation;
  never modify or delete the original files during export or restore

### M18 Tagging settings

Settings for M17 tagging, stored in KConfig.

- **Libraries:** manage library roots and exclusions, and configure automatic
  refresh and scanning limits
- **Tags:** configure tag colours, display, parent-tag matching defaults and
  embedded-pane placement
- **Fields:** manage available text and date fields
- **Embedded metadata:** choose which EXIF and ID3 fields to read or map to
  library metadata; control refresh and conflict handling
- **Search:** choose default tag search scope and matching behaviour
- **Privacy:** control library metadata storage and backup locations; make
  clear what is stored and do not write tags into file metadata without
  explicit consent

### M19 Yazi mode

Offer an optional native Qt/KDE browsing mode inspired by
[Yazi](https://yazi-rs.github.io/docs/quick-start/), a terminal file manager.
Reuse Plasma Opus's file models and KDE services; this is not an embedded Yazi process
or a promise of Yazi configuration or plugin compatibility.

- Enable or leave the mode per pane, keeping it off by default. Preserve both
  panes and their tabs, locations, selections and normal view settings when
  switching modes; allow M21 workspaces to restore the mode when available.
- Present parent and current folders alongside an optional preview in the
  active pane. Adapt or hide columns in narrow windows without replacing the
  other pane; keep navigation and preview accessible.
- Provide keyboard-first navigation, including optional Vim-style movement and
  multi-key actions. Reuse existing KDE shortcut facilities and integrate with
  M29 when available; show the active mode and avoid intercepting typing in
  editors, search fields or the terminal.
- Support focused-item and multi-item selection, copy, move, rename, trash and
  other existing file actions through Plasma Opus and KIO. Keep conflict handling,
  destructive-action confirmations and undo behaviour; never introduce a
  shortcut that silently overwrites or permanently deletes files.
- Reuse existing filtering, filename and content search, sorting, tabs and
  history. Show active filters and search progress, allow cancellation, and
  report backend or remote-location limitations.
- Reuse Plasma Opus previews and file information for the focused item, with
  cancellable work and existing size and remote-download limits. Use existing
  job and transfer-queue interfaces for progress and errors rather than
  duplicating operations.
- Keep large and remote listings responsive and accessible to keyboard and
  assistive-technology users; report unavailable operations and preserve
  ordinary browsing when the mode cannot support a location.
- Store mode, column and preview choices in KConfig and, when available,
  workspace layouts; provide a discoverable way to toggle the mode without
  changing existing bindings by default.

### M20 Yazi mode settings

Settings for M19 Yazi mode, stored in KConfig and coordinated with workspace
layouts.

- **Activation:** choose the default mode independently for each pane and
  whether to restore the previous choice; respect explicitly restored M21
  workspace layouts when available and keep the mode off by default.
- **Columns:** choose which folder-context and preview columns are shown and
  their sizes; adapt to narrow windows without hiding the other file pane.
- **Preview:** choose whether to show the focused item's preview or file
  information. Reuse existing preview providers, size limits and remote
  download policy rather than creating conflicting settings.
- **Keyboard:** choose a standard or Yazi-inspired shortcut preset, review
  conflicts before applying it, and keep typing and accessibility controls
  usable. Reuse existing KDE shortcut configuration and integrate with M29
  when available.
- **Workspace layouts:** when M21 is available, choose whether workspace
  layouts restore Yazi mode and its column settings, without overwriting an
  explicitly selected workspace layout or altering other pane views.
- **Reset:** restore Yazi mode preferences to their defaults without erasing
  saved workspace layouts or unrelated settings.

### M23 Integrations

- `org.freedesktop.FileManager1` D-Bus service
- Deep integration with ARK
- Enhanced Baloo integration: reuse M09 indexed search and M15's search
  interface. Show service status, configured folders and exclusions, and
  coverage or indexing progress when available; distinguish configured scope
  from confirmed coverage and warn that results may be missing or stale. Offer
  access to KDE's shared indexing controls, reuse metadata from installed
  KFileMetaData extractors, and identify missing extractors when detectable.
  Do not change shared settings, enable indexing or start a reindex without
  explicit user action.
- Extend Baloo search with supported metadata criteria and optional fuzzy
  ranking of returned results. Make clear that ranking cannot recover matches
  absent from the index or results truncated by existing limits. Offer an
  explicit live-search fallback for supported local folders, retaining its
  own limits, progress, cancellation and error reporting; never silently
  broaden scope or substitute a different search.
- KDE Plasma language integration: reuse M13 translation catalogues and M14
  language and regional-format preferences with supported KDE and Qt
  facilities. Follow Plasma's language preferences when no app-specific
  override is selected, fall back to source text when a Plasma Opus catalogue is
  unavailable, and offer access to Plasma's language settings when supported.
  Do not modify shared desktop settings without explicit user action or
  require a translation into a particular language.
- Deep AppImage, Flatpak and Snap integration: identify packages and show
  available application details and actions in the file manager. Support
  AppImage launching and optional desktop integration, and use the respective
  package tools for supported Flatpak and Snap installation, updates and
  removal. Require confirmation for changes, respect executable and sandbox
  permissions, and report missing tools, unsupported actions and errors.
- Advanced SSH, SFTP and WebDAV integration: manage named connection profiles
  and browse remote locations in either pane using KDE's KIO workers; support
  transfers through the existing queue where available and open a separate SSH
  terminal session for SSH profiles. Support password and SSH key/agent
  authentication where available, use KDE's credential facilities, and
  respect SSH host-key and WebDAV TLS certificate verification. Report
  connection, authentication and unsupported-operation errors explicitly.
- Use [mise](https://mise.jdx.dev/) for Plasma Opus development tooling: manage
  project tool versions and provide configure, build, test and formatting
  tasks. Document setup and system-provided Qt, KDE and other dependencies;
  keep the existing CMake workflow usable without mise.
- ~~Default file manager registration~~
- System tray and global shortcut
- XDG portal file chooser
- MCP server
- Administrator mode through KIO's `admin:/` worker
- Extend the existing open-terminal action and M09 embedded Konsole terminal
  with explicit separate Konsole, Yakuake or Kwi providers and an embedded Kwi
  KPart when available. Reuse the current Konsole integration and
  pane-preserving layout. Yakuake is a separate drop-down application, not an
  embeddable terminal; report unavailable providers and unsupported locations.
- Editor integrations: offer KWrite and Kate as separate GUI editors, and
  Neovim, Rune, micro and nano in a supported embedded or separate terminal,
  as alternatives to the built-in editor. KWrite and Kate use KTextEditor
  themselves; they are separate applications, not replacement editor
  components. Preserve the built-in editor and its draft handling, pass file
  paths safely without shell interpolation, and report missing providers,
  launch failures and unsupported locations.
- Optional [rsync](https://rsync.samba.org/) backend for M09 folder sync:
  support local folders and SSH locations, reuse sync profiles and exclusions,
  and show an itemized dry run before confirmation. Disable deletion by
  default and explicitly confirm overwrites and deletions. Validate that the
  backend can honor the selected plan, including per-item directions; report
  unsupported options instead of silently changing the plan. Show progress,
  cancellation, errors and possible partial results; a preview is a snapshot,
  not a guarantee against subsequent filesystem changes. Use safe argument
  handling and SSH host-key verification.
- Optional [Atuin](https://docs.atuin.sh/) integration for terminal command
  history: let users open interactive Atuin search in a supported terminal,
  without automatically executing a selected command. Respect the user's
  existing shell configuration; do not install or initialize Atuin, access
  its history database, or enable history synchronization without consent.
  Report missing tools and unsupported terminal sessions.
- Optional [Nextcloud](https://nextcloud.com/) integration: browse an account
  through the existing KIO WebDAV support and open configured local sync
  folders in the panes. When the desktop client exposes supported status
  information, show sync state and provide an explicit action to open the
  client; otherwise report that status is unavailable. Reuse supported KDE
  Online Accounts, connection profiles, KDE credential facilities and TLS
  verification. Do not start a second synchronization process or change client
  configuration automatically.
- Download service menus from the existing Context Menu settings page, as in
  Dolphin, using KDE's KNewStuff service-menu provider. Support browsing,
  installation, updates and removal through the provider, and refresh the
  installed-action list without discarding pending settings changes. Explain
  that downloaded services can run commands and may be shared with other KDE
  applications; require explicit user action and report failures. Reuse the
  existing Plasma Opus-specific and Dolphin-shared visibility controls.
- Integrate with KDE Online Accounts: discover available accounts and their
  supported file services, let users explicitly connect or open them in either
  pane through the appropriate KDE provider or KIO worker, and provide access
  to KDE's account-management interface. Reuse account identities for supported
  Nextcloud and remote integrations instead of duplicating credentials. Handle
  missing providers, denied access, expired authorization and removed accounts
  visibly; do not assume every account offers file access. Respect provider
  permissions, authentication and TLS verification, and never store tokens or
  passwords in URLs, logs or Plasma Opus configuration.
- Plasma Activities: optionally associate M21 workspaces with Activities and
  configure default folders for each Activity and workspace association. For
  each association, use all, selected or none of the applicable workspace
  behaviours, including theme and layout, and Activity-specific default
  folders independently of other Activities; allow workspace defaults or
  explicit overrides. Offer to apply the selected choices when the Activity
  changes, without silently replacing
  restored tabs or unsaved layouts. Leave unselected behaviours and state
  unchanged, preserve unassociated workspaces and running operations, and never
  create, delete or switch an Activity without explicit user action.
  Handle an unavailable Activities service, missing folders and removed
  Activities without losing workspace data, and avoid duplicating KDE's KIO
  resource history.
- Plasma notifications and jobs: integrate Plasma Opus-owned long-running operations
  with KDE's notification settings and offer relevant actions to return to the
  operation or inspect its result. Reuse KIO's existing progress reporting
  rather than creating duplicate notifications or controls. Respect notification
  settings and privacy; report failures and partial results without exposing
  sensitive paths unnecessarily.
- KDE Connect sharing: offer an explicit action to send selected files to an
  available paired device through supported KDE Connect interfaces. Show the
  selected device and transfer outcome; do not pair devices, send files or
  change shared KDE Connect settings automatically. Handle missing services,
  offline devices and unsupported locations visibly.
- KRunner integration: provide an optional runner for explicitly saved Plasma Opus
  workspaces and locations, with actions to open the chosen item in Plasma Opus.
  Respect workspace privacy, avoid exposing credentials or unselected history,
  and handle missing or stale targets without creating files or running
  commands. Use Plasma's supported runner interfaces and do not replace its
  existing file search.
- Optional password-gated folders: let users require a password before opening
  selected local folders through Plasma Opus. This is a Plasma Opus-only convenience gate,
  not encryption or filesystem access control: clearly warn that other
  applications and direct filesystem access remain unrestricted. Do not change
  file permissions or contents. Apply the gate consistently across Plasma Opus
  navigation and alternate paths, store no plaintext password, and use a
  trusted password-verification mechanism and KDE credential facilities where
  appropriate. Report unavailable protection and failures rather than silently
  bypassing the gate; provide explicit password change and removal workflows.

**Planned caveat:** [Password-gated folders](CAVEATS.md#planned-password-gated-folders),
tracked in [#406].

### M24 Integration settings

Settings for M23 integrations, stored in KConfig.

- **System tray**
  - **Behaviour:** minimise and close to tray
- **Shortcuts**
  - **Global:** enable or disable the integration's global shortcut and choose
    its key combination through KDE facilities; integrate with M29's shortcut
    configuration when available
- **File manager**
  - **Portal:** serve the XDG portal file chooser on or off
  - **Default:** show the current desktop file-manager association and offer
    an explicit action to set Plasma Opus as the user default; report failures
- **MCP**
  - **Access:** server access (off, read or write), token and audit log
- **Terminal**
  - **Provider:** choose Konsole, Yakuake or Kwi for separate sessions and
    Konsole or Kwi for embedded sessions, independently of placement in M10
- **Editor**
  - **Provider:** choose the built-in editor or an available KWrite, Kate,
    Neovim, Rune, micro or nano integration, and the terminal session type for
    terminal editors
- **Application packages**
  - **Integration:** enable or disable AppImage, Flatpak and Snap actions
    independently and configure optional AppImage desktop integration
- **Remote connections**
  - **Profiles:** manage SSH, SFTP and WebDAV hosts, ports, usernames and
    connection defaults; store credentials through KDE's credential facilities,
    not in KConfig or URLs
- **rsync**
  - **Backend:** enable the optional backend and choose safe sync defaults,
    reusing M10 profiles and preserving preview and confirmation requirements
- **Atuin**
  - **History:** enable the terminal history action and choose a supported
    terminal session; leave Atuin's own history and sync settings unchanged
- **Nextcloud**
  - **Integration:** enable account and client integration, manage references
    to local sync folders and reuse existing WebDAV profiles without storing
    passwords or tokens in KConfig
- **Online accounts**
  - **Access:** opt in to integration and choose which supported accounts to
    show in Plasma Opus; manage accounts in KDE without deleting shared accounts when
    removing them from Plasma Opus
- **Baloo**
  - **Search:** choose whether to offer fuzzy ranking and a live-search fallback;
    reuse M10 and M16 search settings. Open KDE's shared indexing controls
    rather than duplicating or silently changing them.
- **Plasma Activities**
  - **Workspaces:** enable Activity associations; manage workspace mappings,
    Activity-specific default folders, feature selections and workspace-switch
    prompts without changing KDE's Activity settings. For each association,
    inherit workspace defaults or select all, individual or no behaviours.
- **Plasma notifications**
  - **Operations:** choose which Plasma Opus-owned operation events produce
    notifications and open KDE's notification settings for shared controls
- **KDE Connect**
  - **Sharing:** enable the sharing action and choose whether to remember a
    device, without changing pairing or KDE Connect settings
- **KRunner**
  - **Discovery:** enable the optional runner and choose which saved workspaces
    and locations it can expose
- **Password-gated folders**
  - **Access:** manage protected local folders and their passwords, with a
    clear warning that the gate applies only inside Plasma Opus

### M27 Branding

- Establish a consistent, translatable application name and description across
  the desktop entry, app metadata, About dialog and documentation.
- Create original, appropriately licensed Plasma Opus logo and application-icon assets
  with suitable sizes and light/dark variants. Keep file, device and action
  icons sourced from the active desktop icon theme.
- Present the branding consistently in About and release materials; provide an
  optional, accessible startup presentation that does not delay or block use.

### M28 Branding settings

Settings for M27 branding, stored in KConfig and separate from theme settings.

- **Branding**
  - **Presentation:** enable or disable the optional startup presentation and
    choose among available branding variants without changing the app identity

### M29 Keyboard shortcut settings

- Optional Vim-style navigation in file listings and panes, with a discoverable
  shortcut reference; keep it off by default and do not intercept typing in
  text fields or the embedded terminal
- Show the active Vim-style mode and provide a clear way to enter filtering or
  text input without conflicting with navigation keys
- Configure custom shortcuts using simultaneous key combinations and
  sequential key chords, with conflict detection, clear feedback and reset to
  defaults; integrate with KDE shortcut facilities where supported
- Allow alternative bindings for an action and configurable shortcuts for
  commands that are not on a toolbar, including custom commands
- Show a searchable keyboard map with action names, descriptions, bindings and
  contexts; search by typing an action name or pressing a key combination, and
  export a printable reference
- Scope bindings to the applicable view, folder tree, viewer or application;
  detect conflicts where contexts overlap and preserve typing and accessibility
  keys in input controls
- Native shortcut hint panel, shown as a sliding or floating pane, inspired by
  [which-key.nvim](https://github.com/folke/which-key.nvim): show pending key
  sequences and context-appropriate continuations grouped with action
  descriptions; support keyboard navigation and dismissal
- Configure the hint panel's enablement and display delay, and provide an
  on-demand shortcut reference; cancel or time out incomplete sequences
  without unexpectedly running a conflicting action
- Provide shortcuts for pane and tab navigation, selection, filtering, preview
  and terminal focus as those features become available, without overriding
  existing bindings by default
- Import and export shortcut bindings with a conflict preview; do not execute
  commands from imported files
- Save the Vim-style mode and app-local shortcut bindings in KConfig; manage
  optional global shortcuts through KDE facilities and report registration
  failures or platform limitations

<a id="m30-ui-and-ux-polish"></a>
### M30 Refinements

Polish the existing interface across menus, settings, layouts and workflows.
Preserve user customizations and use KDE facilities; review future pages as
they become available without blocking this milestone on unimplemented work.
See the [editable M30 plans](refinements/README.md) for proposed sequencing
and subsection details.

#### M30.1 Menus

See the [menus plan](refinements/01-menus.md).

- Review main, pane and context menus for logical grouping, consistent action
  names and discoverable shortcuts. Reuse KXmlGui and existing KDE actions;
  preserve user-customized menus and destructive-action confirmations.
- Make toolbar and pane actions consistent and discoverable, including
  overflow in narrow windows and clear indication of the pane they affect.
  Preserve existing toolbar customization and shortcut bindings.
- Keep menu and toolbar action state synchronized with the active pane, current
  selection and location as tabs change or listings refresh. Context-menu
  actions must continue to target the selection for which they were opened.

#### M30.2 Layouts

See the [layouts plan](refinements/02-layouts.md).

- Implement the five-region [interface design](philosophy/designs.md), with
  file panes in the middle column, sidebar panes in either side column and
  embedded panes in either side column, the middle column or footer. Preserve
  existing placements and saved layouts, and integrate future embedded panes
  as they become available. Review first-run pane, tab, panel, toolbar and
  window-layout defaults; apply revised defaults only to new configurations or
  an explicitly confirmed reset, never over saved layouts or sessions.
- Harmonize spacing, alignment, icons, labels and visual hierarchy across
  windows, panels, menus and dialogs. Follow the active KDE theme and icon
  conventions without hard-coding colours or replacing user-selected styling.
- Adapt panes, panels, dialogs and menus to small windows, high DPI, long
  translations and right-to-left layouts. Keep both panes and essential
  controls accessible without discarding user-selected layouts.
- Restore windows to a usable visible position after monitor, resolution or
  scale changes, while preserving saved geometry when the display returns.
  Avoid overriding placement decisions owned by the window manager.
- Review splitter and panel behavior when changing orientation, maximizing,
  hiding panels or swapping panes. Preserve each applicable saved layout and
  make it easy to recover a pane or panel that has become too small to use.
- Extend the existing toolbar customizer so users can add, arrange and remove
  custom items, including saved commands and scripts, on the main toolbar and
  either pane toolbar. Preserve per-toolbar placement, labels and icons across
  restarts and report unavailable items without silently discarding them.
- Make built-in toolbar actions easy to find and add to existing customized
  toolbars, including actions introduced by updates. Offer a searchable,
  labeled reference as a sliding or floating pane without requiring a reset
  or automatically changing saved layouts.
- Provide a way to revert a toolbar's edits made during the current
  customization session and to reset an individual toolbar after confirmation.
  Keep other toolbars, menu entries and saved commands unchanged; explain
  which edits have already been saved.

#### M30.3 Interaction

See the [interaction plan](refinements/03-interaction.md).

- Review keyboard navigation, focus restoration, screen-reader names and
  contrast across workflows. Reuse M29 shortcuts and KDE accessibility
  facilities without intercepting text entry or terminal input.
- Make loading, empty, filtered, unavailable and error states understandable;
  show operation progress and cancellation where supported, explain disabled
  actions, and avoid duplicating KIO notifications or hiding failures.
- Improve contextual help and discovery of pane actions, view modes and
  customization without mandatory onboarding or changing existing settings.
- Provide context-sensitive help from the main window, settings pages and
  supported dialogs using KDE help facilities and an accessible Help action.
  Open the relevant local documentation where available; explain missing help
  instead of silently failing, and avoid overriding user shortcut bindings.
- Clarify which pane supplies selected items and which location receives an
  operation before it runs, using accessible labels rather than color alone.
  Allow changing the active pane without losing its selection and do not
  change KIO's operation or conflict semantics.
- Make active file-list filters and their scope visible in the affected pane,
  including visible and hidden counts where available. Provide a clear way to
  remove the quick filter without implying that other filters were removed;
  distinguish filtering the current listing from searching other locations.
- Give icon-only controls meaningful tooltips and accessible names, including
  relevant shortcuts and pane context where useful. Keep those hints current
  when the user changes shortcuts or toolbar configuration.
- Make adding and editing custom toolbar items discoverable and usable by
  keyboard as well as drag and drop. Pane toolbar items must use the intended
  pane and selection; preserve command preview and confirmation and do not
  delete the underlying saved command when removing a toolbar item.
- Display long, similar and bidirectional filenames clearly in listings and
  operation dialogs, with a way to inspect the full original name. Preserve
  the actual filename and avoid truncation that hides a meaningful extension.
- Review destructive-operation confirmations to identify affected items,
  destination and consequences where applicable; retain KDE confirmation
  policies and use a safe default without introducing redundant prompts.
- Make partial completion, skipped items and failures in batch operations
  understandable, with actionable details and a clear distinction between
  cancellation and rollback. Reuse existing KIO reporting where appropriate.
- Prevent late listing, preview and search results from updating the wrong
  pane, tab or dialog after navigation, cancellation or closure. Retain the
  user's current selection and focus where possible.
- Add subtle, consistent animations for UI transitions such as panels and
  dialogs where they improve orientation. Respect desktop animation and
  reduced-motion preferences where available, provide an app-level way to
  disable motion, and keep interactions responsive and states clear without it.
  Test both animated and motion-disabled behavior.
- Add focused interaction tests for menu actions, default and restored layouts,
  keyboard and accessible navigation, responsive controls and error states;
  preserve existing user customizations in regression cases.

#### M30.4 Settings

See the [settings plan](refinements/04-settings.md).

- Search settings by name and description, showing matching pages and opening
  the relevant tab and control
- Review page grouping, labels, descriptions and terminology for consistency;
  explain settings that affect shared KDE configuration or require a restart
- Make Apply, OK and Cancel behaviour clear, including pending changes and
  restoration of live previews when changes are cancelled
- Restore defaults for the current page or all pages with an explicit choice
  and confirmation before discarding changes
- Validate inputs with actionable inline errors and focus the affected control;
  explain unavailable or dependent options
- Support keyboard navigation, visible focus, screen-reader labels and
  accessible error messages throughout the dialog
- Adapt pages and navigation to small windows, high DPI, long translations and
  right-to-left layouts without clipping controls or labels
- Provide concise contextual help for settings whose effects are not obvious
- Let users mark frequently used settings pages for quick access and open a
  specific page from an appropriate interface action. Preserve page identity
  across navigation and restarts without changing settings values.
- Add keyboard-accessible filtering to existing long, searchable configuration
  lists where useful. Show when no entries match and let users clear the
  filter without changing hidden items or losing unsaved edits.
- Review closing the settings dialog with pending changes, including window
  close and keyboard dismissal. Give a clear apply, discard or cancel choice
  where needed, without implying that immediate external changes are undone.
- Explain setting scope and precedence where defaults can be overridden by
  folder formats, pane locks, operation choices, saved profiles or workspace
  layouts. Distinguish the saved default from the effective value without
  changing the underlying precedence rules.
- Audit settings that open external KDE dialogs or immediately modify saved
  searches, collections, presets and commands. Make their scope and immediate
  effects clear before the action; do not imply that Cancel can undo changes
  owned by another component.
- Make settings persistence and recovery reliable across reopening the dialog,
  multiple windows and application restarts. Detect and report save failures
  or conflicting external changes without silently discarding user edits.
- Clarify the boundaries of reset, backup and restore: preview affected
  app-owned settings, distinguish excluded credentials and shared KDE data, and
  preserve confirmation and restart requirements from M12.
- Keep settings pages responsive when loading large lists, profiles or
  provider data. Explain missing dependencies and permission failures without
  blocking the dialog or silently replacing saved choices.
- Establish reusable search, accessibility, validation and help conventions for
  existing and future pages, including shortcuts, search, tagging, integrations,
  themes and branding. Review pages as implemented without blocking M30 on
  features that remain unavailable.
- Add focused tests for settings search, keyboard access, validation,
  Apply/Cancel and defaults, persistence failures and shared or immediate
  changes across representative existing pages. Extend coverage as later
  settings pages are added.

#### M30.5 Performance and startup

See the [performance and startup plan](refinements/05-performance.md).

- Profile and benchmark cold and warm application startup through the first
  interactive window, settings opening and first-page readiness. Record
  representative configurations, timing distributions and memory use, then
  set measurable improvement targets against the baseline.
- Reduce startup work on the GUI thread, deferring nonessential initialization
  and loading where appropriate. Preserve restored sessions, pane readiness,
  error reporting and user-selected startup behavior.
- Speed up opening settings and switching pages through measured improvements
  such as lazy page creation and background loading where safe. Preserve
  search, validation, Apply/Cancel, immediate effects and unsaved changes,
  including for pages not yet opened.
- Prototype and integrate Rust implementations for selected, profiled
  self-contained core hotspots where benchmarks demonstrate a benefit. Use
  the M15 pilot where applicable, a narrow safe C++/Rust interface and existing
  Qt/KDE facilities; account for build, packaging and dependency costs without
  rewriting widgets or replacing KIO.
- Add reproducible performance checks and functional regression tests for
  startup, settings and any Rust-backed paths. Compare before and after under
  the same conditions, cover error and cancellation behavior, and avoid
  hardware-dependent pass/fail thresholds in ordinary tests.

#### M30.6 Extensive functional and security testing

See the [testing plan](refinements/06-testing.md).

Use the [edge-case inventory](EDGE.md) as input; its scenarios are not
confirmed bugs and may overlap existing tests.

- Inventory every implemented functionality and map it to reproducible
  automated or documented manual tests, including its supported entry points.
  Record gaps and unavailable features explicitly; extend the matrix as new
  functionality ships without blocking M30 on unimplemented milestones.
- Exercise workflows end to end alongside unit and integration tests, covering
  successful operations, invalid input, cancellation, failures, recovery,
  persistence and interactions between features. Verify destructive operations
  and their confirmations using isolated temporary fixtures.
- Test accessibility, keyboard and pointer interaction, animations, layouts,
  translations and desktop configurations across a documented representative
  matrix. Include missing optional dependencies and local and remote failure
  scenarios; report environments that could not be exercised.
- Conduct documented usability walkthroughs of common first-run and experienced
  workflows, including keyboard and assistive-technology use. Track confusing
  steps and regressions with reproducible examples and resolve or triage them.
- Review security-sensitive trust boundaries and test adversarial inputs for
  file operations, archives, paths and symbolic links, external commands,
  terminal integration and imported data. Verify validation, permissions,
  confirmations and error handling without executing untrusted code.
- Test privacy and secret handling in settings, sessions, backups, logs,
  temporary files and remote connections. Use isolated test config, home and
  trash locations and synthetic credentials; never use real user data.
- Run suitable static analysis and dependency checks and perform a documented
  security review. Track reproducible findings, severity and remediation
  separately; do not claim that testing proves the application is secure.

## Feature references

The roadmap adapts workflows to KDE rather than copying platform-specific
interfaces. The additions above draw on these sources:

- [Dolphin view and selection](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/dolphin-view.html)
  and [search](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/quick-tips.html).
- [Dolphin panels](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/panels.html)
  and [Directory Opus folder tabs](https://docs.dopus.com/doku.php?id=preferences:preferences_categories:folder_tabs:folder_tab_bar),
  [status bars](https://docs.dopus.com/doku.php?id=preferences:preferences_categories:file_displays:status_bar)
  and [viewer pane](https://docs.dopus.com/doku.php?id=basic_concepts:the_lister:viewer_pane).
- Directory Opus manual: [introduction](https://docs.dopus.com/doku.php?id=introduction),
  [preferences](https://docs.dopus.com/doku.php?id=preferences),
  [customization](https://docs.dopus.com/doku.php?id=customize:the_customize_dialog),
  [source and destination](https://docs.dopus.com/doku.php?id=basic_concepts:source_and_destination)
  and [filter bar](https://docs.dopus.com/doku.php?id=basic_concepts:searching_and_filtering:filter_bar).
- [Krusader synchronizer](https://docs.kde.org/trunk_kf6/en/krusader/krusader/synchronizer.html)
  and [features](https://docs.kde.org/trunk_kf6/en/krusader/krusader/features.html).
- [Max File Manager](https://github.com/majidasgari/max-file-manager), alongside
  this project's [UI](parity/ui.md) and [backend](parity/backend.md) inventories.
- [TagStudio](https://github.com/TagStudioDev/TagStudio), especially its
  libraries, tag relationships and search.
- [TagSpaces workspaces](https://docs.tagspaces.org/workspaces/): workspace
  organization of locations, tag groups and search.
- [Directory Opus](https://www.gpsoft.com.au/) and its
  [file collections](https://docs.dopus.com/doku.php?id=basic_concepts:virtual_file_system:file_collections).
- Keyboard references: [Directory Opus keyboard map](https://resource.dopus.com/t/list-of-keyboard-shortcuts/26759),
  [Directory Opus shortcut editor](https://docs.dopus.com/doku.php?id=customize:the_customize_dialog:keys),
  [Krusader key bindings](https://docs.kde.org/trunk_kf6/en/krusader/krusader/key_bindings.html)
  and [Dolphin commands](https://docs.kde.org/stable_kf6/en/dolphin/dolphin/command-reference.html).
- Directory Opus Resource Centre: [folder formats](https://resource.dopus.com/t/folder-formats-detailed-guide/11718),
  [navigation history](https://resource.dopus.com/t/how-to-remember-scroll-positions-and-file-selections/1297),
  [folder sizes](https://resource.dopus.com/t/calculate-folder-sizes-automatically/2064),
  [duplicate selection](https://resource.dopus.com/t/find-duplicate-files/19799),
  [path filters](https://resource.dopus.com/t/how-to-filter-items-by-location-or-sub-folder/7644),
  [rename presets](https://resource.dopus.com/c/rename/30),
  [preview autoplay](https://resource.dopus.com/t/viewer-pane-turn-off-auto-play-for-video-and-audio/33976)
  and [configuration backup](https://resource.dopus.com/t/how-to-backup-or-locate-your-opus-configuration-simple/5465).

The existing Orcinus entries reflect earlier project notes; an authoritative
upstream source could not be verified for this review.

## Tracking

Follow the [project instructions](../CLAUDE.md) for issue tracking, completion,
branching and merging. Record blocked work in [pending tasks](TODO.md) and
document feature limitations in [caveats and limitations](CAVEATS.md).
Native dock and column-header spacing follows the
[active widget style](CAVEATS.md#native-title-and-header-padding) ([#560](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/560)).

[#36]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/36
[#255]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/255
[#256]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/256
[#257]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/257
[#258]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/258
[#259]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/259
[#260]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/260
[#261]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/261
[#262]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/262
[#263]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/263
[#264]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/264
[#265]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/265
[#266]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/266
[#267]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/267
[#302]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/302
[#303]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/303
[#304]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/304
[#307]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/307
[#309]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/309
[#311]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/311
[#312]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/312
[#314]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/314
[#315]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/315
[#316]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/316
[#317]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/317
[#318]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/318
[#321]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/321
[#322]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/322
[#335]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/335
[#336]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/336
[#362]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/362
[#372]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/372
[#373]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/373
[#374]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/374
[#375]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/375
[#376]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/376
[#377]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/377
[#406]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/406
[#433]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/433
[#434]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/434
[#436]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/436
