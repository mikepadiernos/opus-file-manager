# Opus File Manager roadmap

Opus is a dual-pane file manager for KDE Plasma, written in C++20 with Qt 6
Widgets and KDE Frameworks 6. Its [scope](philosophy/scope.md) and
[inspirations](philosophy/inspirations.md) are documented separately. It
targets feature parity with Max File Manager; upstream behaviour is recorded in
`docs/parity/backend.md` and `docs/parity/ui.md`.

## Architecture

- `src/core/` is the `opusfm_core` static library for logic without widgets.
- `src/app/` holds the widgets and the `opus-file-manager` executable.
- `tests/` holds QtTest suites run through `ctest`; GUI tests use
  `QT_QPA_PLATFORM=offscreen`.

## Plasma-native decisions

| Upstream approach | Opus approach |
| --- | --- |
| Custom themes, title bars and window buttons | Plasma widget style, colour scheme, fonts and KWin decorations |
| Bundled icon pack | Active icon theme through `QIcon::fromTheme` |
| Hand-written toolbar and shortcut editors | KXmlGui, `KEditToolBar` and `KShortcutsDialog` |
| Settings in SQLite | KConfig; session state in the KConfig state file |
| Custom copy engine and undo | KIO jobs and `KIO::FileUndoManager` |
| Custom places and device polling | `KFilePlacesView` and Solid |
| Custom Open With and default-app logic | `KFileItemActions`, `KOpenWithDialog` and `KApplicationTrader` |
| Custom properties and checksums | `KPropertiesDialog` |
| Custom clipboard helpers | `QClipboard` and `KIO::setClipboardDataCut` |

Pins, colour labels, file colour dots, the hide-from-parent flag and per-folder
view formats have no Plasma equivalent. They remain in `.mfm.json` using the
upstream schema.

## Milestones

Strikethrough marks completed features. Unstruck features are incomplete or not
started. Odd-numbered milestones add features; the even-numbered milestone
after each adds their settings in KConfig.

### M01 Core dual pane

- ~~KXmlGui main window with hamburger menu, editable toolbar and shortcuts~~
- ~~Theme icons for app actions and pane controls, preferring symbolic variants
  when available~~
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

Settings for M01 features, modelled on Dolphin and Orcinus. Opus settings are
stored in KConfig; Places sections and trash/delete confirmations are shared
with Dolphin.

- **General**
  - ~~**Settings Dialog:** page titles beside or below sidebar icons, and icon
    size~~
- **Interface**
  - ~~**Folders and Tabs:** startup folders and session; one or two panes in new
    windows; tab bars; Tab switches panes; new-tab placement; navigation bars;
    symbolic-link following~~
  - ~~**Previews:** enablement, plugins, size limit and remote folders~~
  - ~~**Confirmations:** trash, delete, multiple tabs; executable handling;
    default drop action~~
  - ~~**Panels:** Places panel and visible sections~~
  - ~~**Status and Location Bars:** status-bar contents and bar density~~
- **View**
  - ~~**General:** default view mode, sorting, hidden files and tooltips~~
  - ~~**Details:** icon size, columns, item density and dragging from anywhere
    in a row~~
  - ~~**Icons:** icon and preview size and item density~~
- **Context Menu**
  - ~~**General:** optional Copy To and Move To submenus~~
  - ~~**Actions:** choose which built-in actions appear~~
  - ~~**Service Menus:** choose which installed service actions and plugins
    appear~~

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
  prompts for encrypted 7z archives (Krusader)~~
- ~~Display style per pane (Orcinus)~~
- ~~Expandable folders (Orcinus)~~
- ~~Persistent filters (Orcinus)~~
- ~~File-name highlighting (Orcinus)~~

### M04 Listing depth settings

Settings for M03 listing depth, stored in KConfig.

- **View**
  - ~~**General:** default visible columns and grouping; size format (human or
    bytes), date format and seconds~~
  - ~~**Icons:** name lines and metadata shown in icon view~~
- **Files**
  - ~~**Rules:** ignore list entries and file-type colours by extension~~
- **Panels**
  - ~~**Combined Places/Folders:** show or hide the panel, choose its left or
    right position, configure its auto-focus behaviour, and show or hide each
    section independently~~
  - ~~**Titles:** show or hide the title bars on all panels~~
- **Archives**
  - ~~**Default format:** archive format used by Compress (ZIP, uncompressed
    ZIP, tar.gz, tar.xz or tar.zst); the format choice determines compression~~
- **Tabs**
  - ~~**Appearance:** show tab icons; fill the available tab-bar width and
    divide it equally among tabs as new tabs are created~~

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

Saving does not preserve the original metadata. Gallery images are limited to
128 MiB and 40 megapixels. Text lines over 16,384 characters are shown without
highlighting. Metadata depends on installed KDE extraction plugins.

### M06 Preview and gallery settings

Settings for M05 preview and gallery, stored in KConfig.

- **Opening**
  - ~~**Double-click:** what activating a file opens (application, gallery or
    preview)~~
- **Preview**
  - ~~**Text:** size limit for text in the preview panel~~
  - ~~**Information:** image info panel shown by default~~
- **Slideshow**
  - ~~**Playback:** slideshow interval and looping~~
- **Image conversion**
  - ~~**Output:** default format and quality for saved image copies~~

File activation follows the desktop's single- or double-click setting and also
applies to keyboard activation. Gallery mode opens local images and videos;
other files open in their default application. Folders and executable handling
are unchanged, and the preview panel's Open button opens the application.

### M07 Editor

- KTextEditor-based editor (docked, maximised, standalone)
- Drafts
- Markdown preview and PDF export

### M08 Editor settings

Settings for M07 editor, stored in KConfig and KTextEditor configuration.

- **Editor**
  - **Opening:** where files open (docked, maximised or own window)
  - **Text:** font size and word wrap
  - **Markdown:** default markdown mode (visual or source)
  - **Drafts:** drafts on or off
- **KTextEditor**
  - **Configuration:** KTextEditor's own configuration pages

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
- Compare folders across the two panes (Krusader)
- Create and verify checksum files (Krusader)
- Split and combine files (Krusader)
- Transfer queue (Krusader)
- Embedded terminal alongside both panes: below them or at the right of the app
  for portrait/stacked layouts; showing it never replaces either pane
- Placeholders for scripts and custom commands
- Linked panes, cushion treemap and profiles (Orcinus)

### M10 Tools settings

Settings for M09 tools, stored in KConfig.

- **Embedded terminal**
  - **Placement:** below both panes or at the right of both panes for
    portrait/stacked layouts; the terminal does not replace a pane
- **Search**
  - **Indexing:** search shortcut (double Shift), search folders and exclusions
- **Folder sync**
  - **Transfers:** skip existing, newer only, speed limit
- **Applications**
  - **Associations:** default applications by extension
- **Automation**
  - **Commands:** scripts, custom commands and presets

### M11 UI Customizations

- Edit the main toolbar beyond the `KEditToolBar` dialog: drag entries on, off
  and around it directly, add separators and spacers, and choose icons, text or
  both per entry
- Custom toolbar buttons for scripts, custom commands and presets
- Pane toolbars instead of a fixed path row: each pane gets its own editable
  toolbar (back, forward, up, reload, location bar, view mode, zoom, sorting,
  previews, hidden files, filter, search) that acts on that pane, with the
  location bar dropping to a second row in a narrow pane
- Move panes: send the second pane to a tab of its own or pop it out into a
  window, carrying its display style
- Drag entries between the main toolbar, pane toolbars and hamburger menu
- Swap panes: the current tabs or whole sides
- Dim the inactive pane

### M12 UI Customization settings

Settings for M11 UI customizations, stored in KConfig.

- **Toolbars**
  - **Entries:** which entries the main toolbar and each pane toolbar show,
    and in what order
  - **Appearance:** toolbar style and icon size
  - **Editing:** lock toolbars
- **Panes**
  - **Moving:** whether a moved pane opens as a tab or window, and whether it
    keeps or resets its display style
- **Defaults**
  - **Reset:** restore the main toolbar, pane toolbars and hamburger menu

### M13 Integrations

- `org.freedesktop.FileManager1` D-Bus service
- Deep integration with ARK
- Default file manager registration
- System tray and global shortcut
- XDG portal file chooser
- MCP server
- Administrator mode through KIO's `admin:/` worker

### M14 Integration settings

Settings for M13 integrations, stored in KConfig.

- **System tray**
  - **Behaviour:** minimise and close to tray
- **Shortcuts**
  - **Global:** global shortcut on or off and its key combination
- **File manager**
  - **Portal:** serve the XDG portal file chooser on or off
  - **Default:** set as default file manager
- **MCP**
  - **Access:** server access (off, read or write), token and audit log

### M15 Translations

- Translation catalogues through `ki18n`, fed by the `i18n()` and `i18nc()`
  calls already used throughout the code
- Persian translation
- Right-to-left layout, mirrored for Arabic and Hebrew too
- Jalali (Persian) calendar dates

### M16 Translation settings

Settings for M15 translations, stored in KConfig.

- **Language**
  - **Interface:** interface language, overriding the system locale
- **Dates**
  - **Calendar:** calendar for dates: Gregorian or Jalali
  - **Format:** custom date and time formats
- **Layout**
  - **Direction:** follow the language or force left-to-right or right-to-left
  - **Digits:** digit shapes and custom numeral formats, including Western or
    Eastern Arabic-Indic digits

## Tracking

There is no GitHub remote yet. Milestones stand in for issues, and feature
branches use the `feature/M##-name` naming convention.
