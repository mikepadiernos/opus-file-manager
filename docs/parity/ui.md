# Max File Manager UI inventory

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Backend inventory](https://img.shields.io/badge/parity-backend-3daee9?style=flat-square)](backend.md)
[![Roadmap](https://img.shields.io/badge/roadmap-M01%E2%80%93M32-8957e5?style=flat-square)](../ROADMAP.md)
[![Upstream](https://img.shields.io/badge/upstream-Max%20File%20Manager-6e7781?style=flat-square&logo=github)](https://github.com/majidasgari/max-file-manager)

An inventory of the user interface of a reviewed Max File Manager checkout,
used as a behaviour reference. Source: `max-file-manager/src/renderer/src/`
(Electron + React).

> [!IMPORTANT]
> This page describes upstream Max File Manager, not Plasma Opus. This reviewed
> upstream snapshot is not current Plasma Opus behaviour or necessarily the
> latest upstream version. See the [roadmap](../ROADMAP.md) for Plasma Opus
> feature status and KDE adaptations.

## Contents

- [1. Window layout](#1-window-layout): [windows](#windows),
  [main window](#main-window), [pane](#pane), [sidebar](#sidebar),
  [tree pane](#tree-pane), [gallery](#gallery), [editor](#editor),
  [preview panel](#preview-panel), [search overlay](#search-overlay)
- [2. Views and listing](#2-views-and-listing): [view modes](#view-modes),
  [columns](#columns), [sorting](#sorting), [grouping](#grouping),
  [folder and file metadata](#folder-and-file-metadata),
  [filtering and visibility](#filtering-and-visibility),
  [virtual listings](#virtual-listings-and-live-updates),
  [selection](#selection-and-navigation)
- [3. Commands and shortcuts](#3-commands-and-shortcuts):
  [commands](#commands), [fixed keys](#fixed-keys),
  [context menus](#context-menus), [New menu](#new-menu),
  [drag and drop](#drag-and-drop)
- [4. Dialogs](#4-dialogs)
- [5. Preferences (defaults)](#5-preferences-defaults):
  [Preferences pages](#preferences-pages),
  [state without a page](#state-without-a-preferences-page)
- [6. i18n](#6-i18n)
- [7. Specs pinned by renderer tests](#7-specs-pinned-by-renderer-tests)
- [8. Visual design](#8-visual-design)

## 1. Window layout

### Windows

- Main window.
- Standalone gallery window.
- Standalone editor window.
- XDG portal file chooser.
- Tray (**Show**/**Quit**).

Standalone windows follow the main window's prefs live.

### Main window

Top to bottom:

1. **Top bar**: sidebar toggle, settings, single/dual layout, preview panel,
   undo, list/grid, folder size (<kbd>F9</kbd>), hidden files, dark/light.
   Custom window buttons (4 styles, left/right).
2. **Command bar** from `prefs.toolbar` (icon / icon+text / text) with an
   overflow menu.
3. **Body**:
   - Docked sidebar and splitter (160–600 px; double-click resets to 208).
   - Optional tree pane on the left or right (160–520 px; reset 232).
   - Panes: dual (split 0.2–0.8, reset 0.5) or single (active pane only).
     Optional per-pane ancestor/sub-folder tree (`paneTree`).
   - Side slot (preview panel or docked editor): in dual layout it replaces the
     inactive pane; in single layout it is appended. Width share 0.25–0.8, min
     240 px.
   - Floating sidebar mode: overlay with backdrop, movable, resizable.
4. **Jobs bar** while jobs exist (last 8): kind icon, label, progress, %, bytes,
   speed, skipped, errors (tooltip), pause/resume, cancel, clear finished.
5. **Status bar**: item count or **N results**; **N selected - size**; for a
   single selection: name, type, size, date, rwx, label chip, stars, symlink;
   index progress; free space; update chip; running ops; in single layout
   **Destination:** `<other pane>`; transient note (3.5 s).

### Pane

Top to bottom:

- **Tab strip**: hidden for one tab when `hideSingleTab` is on; forced on in
  both panes if either has several.
  - Each tab: pin glyph, lock glyph, folder icon (`tabIcons`), colour dot,
    custom name or basename, close (not on a pinned, locked or only tab); `+`.
  - Drag to reorder, never across the pinned boundary. Middle-click closes.
- **Path row** (always LTR):
  - Back (<kbd>Alt</kbd>+<kbd>Left</kbd>), forward
    (<kbd>Alt</kbd>+<kbd>Right</kbd>), up (<kbd>Backspace</kbd>), refresh
    (<kbd>Ctrl</kbd>+<kbd>R</kbd>).
  - Breadcrumbs that turn into an address field on click of the current crumb,
    double-click or empty space (`~` expands, <kbd>Enter</kbd> commits,
    <kbd>Esc</kbd> cancels).
  - Virtual-listing chip with close.
  - Right side: new tab (when the strip is hidden), sort/group menu, filter
    toggle, in-pane search toggle, bookmark star, bookmarks dropdown.
- **Floating name filter**: <kbd>Esc</kbd> clears, <kbd>Enter</kbd> opens the
  first visible entry.
- **In-pane search bar**: name (wildcards), content text, regex checkbox.
  Limits 5000 results, 2.5 s; excludes `searchExclude` plus dot-entries unless
  hidden files are shown. Results are a virtual listing.
- **List header**: click sorts, edge resize (48–420 px), right-click column
  menu.
- **Rows and tiles**: rubber-band selection (<kbd>Ctrl</kbd> adds),
  <kbd>Ctrl</kbd>+wheel zooms the grid ±8 px, drop target.
  - Empty states: **Loading**, **Nothing found**, **Nothing with this name**,
    **Folder is empty**; errors as text.
- **Preview in other pane**: the pane shows a full preview with prev/next
  image, **n of N** and a gallery button.

### Sidebar

Sections are collapsible (`sidebarCollapsed`). Dropping a folder bookmarks it;
dropping a file shows **only folders can be bookmarked**.

- **Places**: roots (`~` **Home**, `/` **Root**, mounts under `/mnt`,
  `/media/$USER`, `/run/media/$USER`) as lazily loaded trees; the twisty
  expands, the name navigates; expanded nodes persist (`sidebarTreeOpen`).
- **Bookmarks**: folder icon/colour, custom title (inline rename, empty
  reverts), remove, drag reorder; menu **Open** / **Rename** / **Remove**.
- **Labels** used in the current folder: dot, name, count; a click filters
  `#id`.
- **Removable devices** (`sidebarDrives`): unmounted devices are dimmed with
  **Mount**; mounted ones navigate and have eject (moves panes inside the
  device back to home).

### Tree pane

- Dockable, rooted at `/`, follows symlinked folders (badge with
  `showSymlink`).
- Buttons: locate current, auto-follow (`treeAutoFocus`), expand current,
  collapse all, close.
- Click navigates, double-click toggles.

### Gallery

Overlay or standalone window.

- **Header**: name, **n of N**, **Edited WxH**, info (<kbd>I</kbd>), copy image
  (<kbd>Ctrl</kbd>+<kbd>C</kbd>), zoom -/+, reset (<kbd>0</kbd>), slideshow
  (<kbd>Space</kbd>), fullscreen (<kbd>F</kbd>), close.
- **Stage**: prev/next, wheel zoom 0.1–8, drag pan, double-click fit/2x.
- **Filmstrip** (windowed 2000 thumbs).
- **Edit**: crop rectangle; resize W/H/% keeping aspect; save as copy
  (`imageFormat`, `imageQuality`), replace original (own format), discard.
- **Info sidebar** (`galleryInfo`):
  - Summary: name, dims, megapixels, aspect, size, MIME, modified, created,
    path.
  - 5 dominant colours and a 40-bin luminance histogram (160 px downscale).
  - EXIF: camera, date, ISO, aperture, exposure, focal.
  - Edit tools: rotate ±90/180, flip H/V, crop, resize, 25/50/75/100%,
    brightness 50–150, contrast 50–150, saturation 0–200, grayscale 0–100 with
    live preview, **Apply**, **Reset**.
- **Settings**: `galleryLoop`, `slideshowSec`.

### Editor

Docked, maximised, or in its own window.

- **Header**: name, dirty dot, language; Markdown **Visual**/**Source**; wrap;
  font size 9–22 (12.5); split preview (markdown source); PDF export (markdown,
  light theme, `<name>.pdf`); **Save** (<kbd>Ctrl</kbd>+<kbd>S</kbd>);
  maximise; close (<kbd>Esc</kbd>).
- **Visual mode**: WYSIWYG markdown with per-block direction; code always LTR.
- **Source mode**: highlighted with line numbers.
- **Footer**: lines, characters, saved state, key hints.
- **Drafts**: saved 700 ms after edits and on close; on open a banner offers
  **Restore** / **Discard**. Switching files in the docked editor saves the
  previous buffer.

### Preview panel

- **Controls**: prev/next image, HTML **Render**/**Code** toggle, title,
  **n of N**, gallery button, close (<kbd>F3</kbd>).
- **Body**: image (under 8 MB), PDF, office text (docx/xlsx/pptx/odt/ods/odp),
  audio/video player, sanitised HTML, rendered markdown, line-numbered
  highlighted text (capped by `previewMaxKB`), binary "no preview".
- **Footer**: size, date, ID3/EXIF pills, **Truncated**, **Open**.

### Search overlay

Opened with <kbd>Ctrl</kbd>+<kbd>K</kbd> or double <kbd>Shift</kbd> within
420 ms.

- Indexed search, 500 ms debounce, up to 300 hits, index stats and age,
  reindex, settings, progress.
- **Rows**: icon or extension badge, name, parent, size, date.
- **Keys**: <kbd>Up</kbd>/<kbd>Down</kbd>; <kbd>Enter</kbd> reveals (navigate +
  select); <kbd>Ctrl</kbd>+<kbd>Enter</kbd> or double-click opens;
  <kbd>Esc</kbd> closes.

## 2. Views and listing

### View modes

- **List**: name plus columns.
- **Grid**: tile width 80–240 (default 108); thumbnail 0.66 × width; name lines
  1–4; up to 4 caption fields (`gridMeta`). In grid view double-click opens
  images and text in their own windows.

### Columns

The name column is always first. Other columns are user-ordered and their
widths persist.

| Column   | Default width (px) | Content                            |
| -------- | ------------------ | ---------------------------------- |
| `type`   | 88                 | Kind, or **Group N** in duplicates |
| `size`   | 94                 |                                    |
| `date`   | 120                |                                    |
| `mode`   | 92                 | `rwxr-xr-x`                        |
| `owner`  | 72                 | `uid:gid`                          |
| `links`  | 52                 |                                    |
| `dims`   | 90                 | `WxH`                              |
| `rating` | 74                 |                                    |

### Sorting

- Keys: name, size, mtime, ext, mode, owner, links, dims, rating; ascending or
  descending.
- Clicking the same column flips the direction; a new column starts ascending.
- Natural, case-insensitive ordering.
- Folders come first only for name and size sorts when `foldersFirst` is on;
  descending keeps folders first.
- Pinned files float to the top.

### Grouping

| Mode | Groups                                                      |
| ---- | ----------------------------------------------------------- |
| none | No grouping                                                 |
| name | A–Z plus `#`                                                |
| type | Kind                                                        |
| size | Folders, <1 MB, 1–10 MB, 10–100 MB, 100 MB–1 GB, >1 GB      |
| date | Today, yesterday, last 7 days, this month, this year, older |
| ext  | Folders, upper-cased extensions, **No extension**           |

Group headers show the label and count, in semantic order. Setting a group mode
writes the folder format and the global `groupBy`.

### Folder and file metadata

- **Folder format** in the folder's `.mfm.json`:
  `{view, columns, sortKey, sortAsc, group}`, written on every sort, view or
  group change; never for virtual listings or archives.
- **Folder appearance**: colour (red, orange, yellow, green, blue, purple, pink,
  teal, black, grey), icon (`g:<builtin>`, `p:<pure-name>`, or an absolute
  path), `hidden` (hide from the parent unless hidden files are shown).
- **Per file** in the folder's `.mfm.json`: pins, labels (8: red, orange,
  yellow, green, blue, purple, pink, gray), ratings 1–5, notes (500 chars),
  colours.
- **Row decorations**: file-type dot (`fileTypes`), label dot, file colour dot,
  pin, hidden-from-listing glyph (dimmed row), icon, name, symlink badge.

### Filtering and visibility

- **Hidden files**: <kbd>Ctrl</kbd>+<kbd>H</kbd>; also reveals
  `.mfm.json`-hidden folders.
- **Ignore list** (`ignoreFiles`, default `.directory, desktop.ini`): exact or
  glob, case-insensitive; applied to listings, flat view, search and the
  indexer.
- **Filter syntax**: plain substring; `#lbl` label id contains; `*N` rating
  ≥ N.
- **Type-ahead**: unmodified characters append to an 800 ms buffer that becomes
  the filter and selects the first prefix match; <kbd>Backspace</kbd> edits;
  navigating clears.

### Virtual listings and live updates

- **Virtual listings**: flat view (limit 5000), search results, duplicates
  (**N groups**), archive browse (zip/tar/tgz/tar.gz/tar.bz2/tbz2;
  <kbd>F5</kbd> extracts the selection to the other pane).
- **Live updates** for visible folders.

### Selection and navigation

- **Mouse**: click (anchor), <kbd>Ctrl</kbd>-click toggles,
  <kbd>Shift</kbd>-click selects a range, rubber band; right-click selects
  unless already selected, right-click on blank space clears.
- **Keys**: <kbd>Up</kbd>, <kbd>Down</kbd>, <kbd>PgUp</kbd>, <kbd>PgDn</kbd>,
  <kbd>Home</kbd>, <kbd>End</kbd> (<kbd>Shift</kbd> extends); in grid view
  <kbd>Left</kbd>/<kbd>Right</kbd> move between cells; in list view
  <kbd>Left</kbd> = parent, <kbd>Right</kbd> = open (Krusader). <kbd>Esc</kbd>
  clears the selection and filter.
- Moving the selection updates an open preview or docked editor.

## 3. Commands and shortcuts

### Commands

All commands are rebindable. Default toolbar: `newItem`, `copy`, `move`, `edit`,
`rename`, `trash`, `size`, `look`, `previewPane`, `undo`, `newTab`, `flat`,
`dupes`, `terminal`.

| Command                          | ID             | Default key                                   |
| -------------------------------- | -------------- | --------------------------------------------- |
| New menu                         | `newItem`      | <kbd>Ctrl</kbd>+<kbd>N</kbd>                  |
| Copy to other pane               | `copy`         | <kbd>F5</kbd>                                 |
| Move to other pane               | `move`         | <kbd>F6</kbd>                                 |
| Copy files to clipboard          | `copyFiles`    | <kbd>Ctrl</kbd>+<kbd>C</kbd>                  |
| Cut                              | `cutFiles`     | <kbd>Ctrl</kbd>+<kbd>X</kbd>                  |
| Paste (image data saves a PNG)   | `paste`        | <kbd>Ctrl</kbd>+<kbd>V</kbd>                  |
| Edit text                        | `edit`         | <kbd>F4</kbd>                                 |
| Rename (batch if >1)             | `rename`       | <kbd>F2</kbd>                                 |
| Batch rename                     | `batchRename`  | <kbd>Shift</kbd>+<kbd>F2</kbd>                |
| New folder                       | `mkdir`        | <kbd>F7</kbd>                                 |
| Trash (delete if not trash mode) | `trash`        | <kbd>F8</kbd>                                 |
| Delete permanently               | `hardDelete`   | <kbd>Shift</kbd>+<kbd>Delete</kbd>            |
| Calculate size                   | `size`         | <kbd>F9</kbd>                                 |
| Sync with other pane             | `sync`         | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Y</kbd> |
| Compare two files                | `diff`         | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> |
| Undo                             | `undo`         | <kbd>Ctrl</kbd>+<kbd>Z</kbd>                  |
| Select all                       | `selectAll`    | <kbd>Ctrl</kbd>+<kbd>A</kbd>                  |
| Refresh                          | `refresh`      | <kbd>Ctrl</kbd>+<kbd>R</kbd>                  |
| Parent folder                    | `up`           | <kbd>Backspace</kbd>                          |
| Preview panel                    | `previewPane`  | <kbd>F3</kbd>                                 |
| Gallery                          | `gallery`      | <kbd>Space</kbd>                              |
| Flat view                        | `flat`         | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd> |
| Duplicate files                  | `dupes`        | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd> |
| Compress                         | `compress`     | none                                          |
| Extract                          | `extract`      | none                                          |
| Name filter                      | `filter`       | <kbd>Ctrl</kbd>+<kbd>F</kbd>                  |
| Hidden files                     | `hidden`       | <kbd>Ctrl</kbd>+<kbd>H</kbd>                  |
| Bookmark                         | `bookmark`     | <kbd>Ctrl</kbd>+<kbd>D</kbd>                  |
| List/grid                        | `toggleView`   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>V</kbd> |
| Single/dual                      | `toggleLayout` | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>L</kbd> |
| New tab                          | `newTab`       | <kbd>Ctrl</kbd>+<kbd>T</kbd>                  |
| Close tab (not pinned/locked)    | `closeTab`     | <kbd>Ctrl</kbd>+<kbd>W</kbd>                  |
| Reopen closed tab                | `reopenTab`    | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>T</kbd> |
| Next tab                         | `nextTab`      | <kbd>Ctrl</kbd>+<kbd>Tab</kbd>                |
| Address bar                      | `addressBar`   | <kbd>Ctrl</kbd>+<kbd>L</kbd>                  |
| Terminal here                    | `terminal`     | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>T</kbd>   |
| Reveal in system file manager    | `reveal`       | none                                          |
| Check for update                 | `checkUpdate`  | none                                          |
| Folder sizes from index          | `indexSizes`   | none                                          |
| Global search                    | `searchIndex`  | <kbd>Ctrl</kbd>+<kbd>K</kbd>                  |
| Layouts and filters              | `presets`      | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> |
| Save folder display format       | `saveFormat`   | none                                          |
| Folder icon/look                 | `look`         | none                                          |
| Properties                       | `properties`   | <kbd>Alt</kbd>+<kbd>Enter</kbd>               |
| Preferences                      | `prefs`        | <kbd>Ctrl</kbd>+<kbd>,</kbd>                  |

### Fixed keys

| Scope       | Keys                                                                                                                                                                                                                                                    |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Main window | <kbd>Alt</kbd>+<kbd>Left</kbd>/<kbd>Right</kbd> history; <kbd>Tab</kbd> switches pane; <kbd>Enter</kbd> opens; <kbd>Esc</kbd> clears; <kbd>Ctrl</kbd>+<kbd>1</kbd>..<kbd>9</kbd> tab N; <kbd>Menu</kbd> or <kbd>Shift</kbd>+<kbd>F10</kbd> context menu |
| Gallery     | <kbd>Left</kbd>/<kbd>Right</kbd>, <kbd>Home</kbd>/<kbd>End</kbd>, <kbd>+</kbd>/<kbd>=</kbd>, <kbd>-</kbd>, <kbd>0</kbd>, <kbd>R</kbd>, <kbd>Space</kbd>, <kbd>F</kbd>, <kbd>I</kbd>, <kbd>Ctrl</kbd>+<kbd>C</kbd>, <kbd>Esc</kbd>                       |
| Editor      | <kbd>Ctrl</kbd>+<kbd>S</kbd>, <kbd>Esc</kbd>                                                                                                                                                                                                            |

> [!CAUTION]
> Upstream quirks not to copy: type-ahead swallows single-character bindings;
> hard delete is labelled <kbd>Shift</kbd>+<kbd>F8</kbd>; refresh is labelled
> <kbd>F3</kbd>; there is no previous-tab key.

### Context menus

| Menu                     | Entries                                                                                                                                                                                                                                        |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Item                     | **Open**; **Open in new window** (images, text); **Open with** (custom commands, apps, **Other**); **Preview**; **Preview in other pane**; **Properties**; folders: **Terminal here**, **Content chart**; images: **Gallery**; **New** submenu |
| Item: File operations    | Copy, move, browse archive, extract, extract to other pane, rename, batch, compress, trash, delete                                                                                                                                             |
| Item: Folder and view    | Open in new tab, new tab here, calculate size, folder icon, bookmark toggle, hide/show in listing, save display format, pin/unpin                                                                                                              |
| Item: Path and system    | Copy path, create symlink, reveal, terminal, SHA-256 to clipboard, sync, compare (2 selected), run each `prefs.scripts` entry                                                                                                                  |
| Item: Labels and details | Edit text, colour label, rating and note, audio tags (mp3/m4a), convert images                                                                                                                                                                 |
| Item: Listing export     | CSV, PDF                                                                                                                                                                                                                                       |
| Item: bottom             | Quick actions (cut, copy, paste, trash, delete), colour swatch row, folder icon action                                                                                                                                                         |
| Blank area               | **New**, **Refresh**, this folder's commands, **Terminal**, **Flat view**, **Duplicates**, **Size**, **Content chart**, folder icon, **Columns**, **New tab**, quick actions, swatches                                                         |
| Tab                      | **Rename**, **Lock**/**Unlock** (a locked tab opens navigation in a new tab), **Pin**/**Unpin**, **Duplicate**, **Close**, **Close others**, **Close to the right**, **Tab colour** (none plus 8 label colours, keyed by path), **Copy path**  |
| Column header            | 8 columns (at least one on), grid captions (max 4), **Column settings**                                                                                                                                                                        |
| Sort                     | 9 keys, ascending/descending, 6 group modes                                                                                                                                                                                                    |

### New menu

- Folder.
- **Office**: docx, xlsx, pptx, pdf, md, csv, txt.
- **Design**: svg, fig, png.
- **Code**: py, js, ts, html, css, sh, sql.
- **Config**: json, yaml, env.
- **Project**: `README.md`, `AGENTS.md`, `.gitignore`.
- Blank file.

Office and design formats are real binary templates. The name prompt
pre-selects the stem.

### Drag and drop

- Rows dropped on the other pane or on a folder row move (<kbd>Ctrl</kbd>
  copies).
- External drops copy.
- Dragging out provides `text/uri-list`.
- Folders dropped onto the sidebar are bookmarked.
- Bookmarks and tabs reorder by dragging.

## 4. Dialogs

| Dialog                | Contents                                                                                                                                                                                                                                                                                                                                                                                        |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Prompts               | Name prompt (new folder / file / rename), tab rename, delete confirmation (trash vs permanent, lists 3 names), colour label picker (8 + none)                                                                                                                                                                                                                                                   |
| Batch rename          | Template tokens `{name} {ext} {n} {nn}` plus `{title} {artist} {album} {year} {date} {camera}`; start number, zero pad; regex mode with `$1`; preview of the first 8 with changes highlighted; duplicate-name warning                                                                                                                                                                           |
| Archive               | Compress (name, level 1/6/9, here or other pane); extract (destination, never overwrites)                                                                                                                                                                                                                                                                                                       |
| Sync                  | Compare by mtime / ctime / hash, ignore patterns, direction, trash extra files, differences only, summary chips, up to 500 rows, **Run**                                                                                                                                                                                                                                                        |
| Diff                  | Unified, up to 6000 lines, added/removed counts, binary and approximate chips                                                                                                                                                                                                                                                                                                                   |
| Symlink               | Target, link name (default `<name> - link`), same folder, never overwrites                                                                                                                                                                                                                                                                                                                      |
| Presets               | Named layouts (layout, sidebar mode/open, view, columns, toolbar) and named filters                                                                                                                                                                                                                                                                                                             |
| Properties            | **General** (rename, type, MIME, open with + change, location + copy, size with recursive folder size, dims, symlink target, times); **Permissions** (owner/group/others view+modify vs view, executable, ownership, octal + **Apply**, rwx, links, inode/device); **Checksums** (expected hash + paste, MD5/SHA-1/SHA-256/SHA-512 compute/copy, match badge); **Details** (label, stars, note) |
| Open With             | Search, recommended/all, MIME, custom commands, apps with a **Default** badge, "always open with", system default                                                                                                                                                                                                                                                                               |
| Folder look           | Preview, icon pack, category, search, size, built-in glyphs, custom pack path, **Don't show in parent listing**, clear icon. Works on multiple folders at once                                                                                                                                                                                                                                  |
| Rating, tags, convert | Rating and note; ID3 tags (title, artist, album, year); image convert (JPEG / PNG / WebP / BMP, longest side default 1920, quality default 88, output to the other pane)                                                                                                                                                                                                                        |
| Content chart         | Scope this folder / all levels; totals; donuts by kind size, kind count and extension (top 7 + Other); top 10 files (click reveals) and top 10 sub-folders                                                                                                                                                                                                                                      |
| Preferences           | Paged modal (see [section 5](#5-preferences-defaults))                                                                                                                                                                                                                                                                                                                                          |

## 5. Preferences (defaults)

### Preferences pages

Defaults are in parentheses; alternative values are separated by slashes.

| Page                    | Settings                                                                                                                                                                                                                                                                                                                                                                       |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Appearance**          | `lang` en/fa (en); `theme` dark/light/system (dark); `colorTheme` (10 ids); `fontFamily`; `titlebar` app/system; `rounded`; `titlebarButtons`; `titlebarSide`                                                                                                                                                                                                                  |
| **File display**        | `view` list/grid (list); `tileSize` 80–240 (108); `gridNameLines` 1–4 (2); `foldersFirst` (true); `fileIcons` pure/system/line (pure); `ignoreFiles` (`.directory`, `desktop.ini`; ≤100 entries of ≤80 chars, no slashes); `groupBy` (none); `showHidden` (false); `showSymlink` (false); `sizeFormat` human/bytes; `dateFormat` iso/jalali/short (iso); `showSeconds` (false) |
| **Columns**             | `columns` (`type, size, date`); `gridMeta` (`size`, max 4)                                                                                                                                                                                                                                                                                                                     |
| **Toolbar**             | `toolbarMode` icon/icon-text/text (icon-text); `toolbar` list                                                                                                                                                                                                                                                                                                                  |
| **Shortcuts**           | Per-command accelerator, capture, remove, reset, reset all, conflict warning                                                                                                                                                                                                                                                                                                   |
| **Window and layout**   | `layout` single/dual (dual); `sidebar` docked/float; `treePane` off/left/right (off); `treeAutoFocus` (true); `sidebarDrives` (true); `sidebarTone` 1–6; `hideSingleTab` (true); `tabIcons` (true)                                                                                                                                                                             |
| **Agents (MCP)**        | Mode off/read/write, token, client snippet, audit log                                                                                                                                                                                                                                                                                                                          |
| **Search**              | `doubleShiftSearch` (true); `searchRoots` (empty = Documents, Downloads, Desktop, Pictures, Music, Videos); `searchExclude` (`node_modules, .git, .cache, .venv, dist, build, *.tmp, .Trash, snap`)                                                                                                                                                                            |
| **File types**          | Extension → colour from the label palette                                                                                                                                                                                                                                                                                                                                      |
| **Default apps**        | Extension → `.desktop`; `customCommands` `{label, pattern, command}`, where `pattern` is extensions, `folder` or `*`, and `%f` is the path                                                                                                                                                                                                                                     |
| **Archive and gallery** | `zipLevel` 0–9 (6); `imageFormat` png/jpeg/webp (png); `imageQuality` 50–100 (90); `slideshowSec` 1–60 (3); `galleryLoop` (true); `previewMaxKB` 16–8192 (200)                                                                                                                                                                                                                 |
| **Transfer**            | `transferSkipExisting`, `transferNewerOnly` (false); `speedLimitKB` (0)                                                                                                                                                                                                                                                                                                        |
| **File operations**     | `doubleClick` open/gallery/preview; `confirmDelete` (true); `trashByDefault` (true)                                                                                                                                                                                                                                                                                            |
| **Startup**             | `hotkeyEnabled` (false), `hotkey` (<kbd>Super</kbd>+<kbd>E</kbd>); `portalPicker` (false); `singleInstance` (true); `startup` session/folder; `startFolder`; `minimizeToTray`, `closeToTray` (false); set as default file manager                                                                                                                                              |
| **Advanced**            | Export/import settings as JSON; icon directories                                                                                                                                                                                                                                                                                                                               |

### State without a Preferences page

| Kind    | Stored values                                                                                                                                                                                                                                                                                                                  |
| ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| No UI   | `sideWidth` 0.5, `dualSplit` 0.5, `colWidths`, `sidebarOpen`, `sidebarWidth` 208, `sidebarCollapsed`, `sidebarTreeOpen` (≤300), `tabColors`, `newTabPos` next/end, `middleClickClosesTab`, `paneTree`, `treeWidth` 232, `galleryInfo`, `layouts`, `filters`, `scripts` `{name: shell line with {path} {dir}}`, `pickerLastDir` |
| Session | Per pane: tabs, active tab, pin count, names, locks; active pane; global default sort; bookmarks and names (trimmed, ≤80 chars, ≤500)                                                                                                                                                                                          |

## 6. i18n

- English is the default; Persian (RTL) switches live.
- Paths and the address field are always LTR; names use `dir=auto`; code is
  always LTR.
- Persian digits and the `٫` decimal separator in fa.
- Jalali dates are a separate setting.
- About 1,045 strings.

## 7. Specs pinned by renderer tests

<details>
<summary>Behaviour pinned by the upstream renderer tests</summary>

| Spec                          | Pinned behaviour                                                                                                                                                                                                                                                        |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `fmtSize`                     | Folders `''`; `0 B`; one decimal below 10, none at 10 or above (1024 → 1.0 KB, 12345678 → 12 MB); bytes mode                                                                                                                                                            |
| `fmtDate`                     | 0 → `''`; iso `YYYY-MM-DD HH:mm` (+`:ss`); jalali; short `HH:mm`; local time. `toJalali(2024-03-20)` → 1403-01-01                                                                                                                                                       |
| `crumbs`, `baseName`, `dirOf` | `crumbs`; `baseName` (trailing separator ignored); `dirOf('/a')` → `/`                                                                                                                                                                                                  |
| `kindOf`, `typeLabel`         | 12 kinds; unknown extension upper-cased; none → File                                                                                                                                                                                                                    |
| Sorting                       | Natural (file2 < file10); folders first also descending; `foldersFirst=false` mixes; size sort keeps folders first, mtime interleaves; mode/owner/links/dims (area)/rating sorts; filter trimmed, case-insensitive, `#` label substring, `*N` rating; pins float to top |
| `navIndex`                    | Empty → -1; none selected → 0 (End → last); clamped; grid up/down step by columns, page = rows × columns                                                                                                                                                                |
| `opensInEditor`               | txt/log/md/markdown/mdx/rst and extensionless files                                                                                                                                                                                                                     |
| Permissions                   | Setting rw keeps the file x bit; folder x follows read; 644 + others rw → 646; owner r → 444; folder 755 owner r → 555; folder others none → 750. `modeString(755)` → `rwxr-xr-x`                                                                                       |
| `menuTargets`                 | The clicked row wins unless already selected                                                                                                                                                                                                                            |
| `planRename`                  | `photo-{nn}` start 3 pad 3 → `photo-003.txt`; extensionless stays extensionless; `{name}-small.{ext}` does not double the extension; regex `IMG_(\d+)` → `photo-$1`; an invalid regex leaves names unchanged                                                            |
| `openAction`                  | Folders navigate; archives browse; per-extension default app → external; mode open → external; preview → preview; gallery mode: HTML → preview, images → gallery, text → editor, else external; inside archives editor/preview fall back to external                    |
| `terminalDir`                 | Folder itself; file → parent; inside an archive → the archive's parent; no selection → tab folder                                                                                                                                                                       |
| `matchCustomCommand`          | Extension lists split on `, ;` and whitespace, case-insensitive, leading dot allowed; folder keywords; `*`                                                                                                                                                              |
| `reorderItem`                 | Before/after; out of range or same index unchanged                                                                                                                                                                                                                      |
| `groupEntries`                | Name: `#` first, then A–Z; type/ext: folders first; date/size: fixed bucket order; none → one group; empty → `[]`                                                                                                                                                       |
| `isIgnoredName`               | Exact, case-insensitive; `*` `?` globs (`?.tmp` matches `a.tmp` not `ab.tmp`); empty never matches                                                                                                                                                                      |
| Keys                          | Parse/format accelerators, conflicts                                                                                                                                                                                                                                    |
| Image info                    | `aspectText`: known ratios within 1% (1:1, 3:2, 4:3, 5:4, 16:9, 16:10, 21:9, 2:1, 3:1), else reduced fraction, `—` invalid; megapixels; dominant colours (4-bit buckets, alpha < 16 ignored, shares sum to 1); luminance histogram (Rec.709, normalised to tallest bin) |
| Charts                        | Skip values ≤ 0; percent with one decimal, 0 for a zero total; fold slices beyond N into Other                                                                                                                                                                          |
| Direction                     | Persian digits count as RTL                                                                                                                                                                                                                                             |
| New templates                 | Categories office, design, code, config, project; `.gitignore` gets no extension                                                                                                                                                                                        |

</details>

## 8. Visual design

Fully custom CSS theme (dark default, 10 colour themes), custom title bar,
Vazirmatn font, bundled Pure icon theme (GPL-3.0), line-SVG UI glyphs, per-kind
colours, status-bar toast instead of popups. The Qt rewrite replaces all of this
with the Plasma style, colour scheme, fonts, icon theme and KWin decorations.
