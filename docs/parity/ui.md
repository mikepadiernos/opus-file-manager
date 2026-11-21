# Max File Manager UI inventory

Source: `max-file-manager/src/renderer/src/` (Electron + React). This reviewed
Max snapshot is an upstream reference, not current Opus behavior or
necessarily the latest upstream version. See the [roadmap](../ROADMAP.md)
for Opus's feature status and KDE adaptations.

## 1. Window layout

Windows: main; standalone gallery; standalone editor; XDG portal file chooser;
tray (show/quit). Standalone windows follow the main window's prefs live.

Main window, top to bottom:

1. Top bar: sidebar toggle, settings, single/dual layout, preview panel, undo,
   list/grid, folder size (F9), hidden files, dark/light. Custom window buttons
   (4 styles, left/right).
2. Command bar from `prefs.toolbar` (icon / icon+text / text) with overflow
   menu.
3. Body:
   - Docked sidebar + splitter (160-600 px, double-click resets to 208).
   - Optional tree pane left or right (160-520 px, reset 232).
   - Panes: dual (split 0.2-0.8, reset 0.5) or single (active pane only).
     Optional per-pane ancestor/sub-folder tree (`paneTree`).
   - Side slot (preview panel or docked editor): in dual layout it replaces the
     inactive pane; in single layout it is appended. Width share 0.25-0.8, min
     240 px.
   - Floating sidebar mode: overlay with backdrop, movable, resizable.
4. Jobs bar while jobs exist (last 8): kind icon, label, progress, %, bytes,
   speed, skipped, errors (tooltip), pause/resume, cancel, clear finished.
5. Status bar: item count or "N results"; "N selected - size"; single selection:
   name, type, size, date, rwx, label chip, stars, symlink; index progress; free
   space; update chip; running ops; in single layout "Destination:
   `<other pane>`"; transient note (3.5 s).

Pane, top to bottom:

- Tab strip (hidden for one tab when `hideSingleTab`; forced on in both panes if
  either has several). Tab: pin glyph, lock glyph, folder icon (`tabIcons`),
  colour dot, custom name or basename, close (not on pinned/locked/only tab),
  `+`. Drag reorder, never across the pinned boundary. Middle-click closes.
- Path row (always LTR): back (Alt+Left), forward (Alt+Right), up (Backspace),
  refresh (Ctrl+R); breadcrumbs that turn into an address field on click of the
  current crumb / double-click / empty space (`~` expands, Enter commits, Esc
  cancels); virtual-listing chip with close. Right side: new tab (when strip
  hidden), sort/group menu, filter toggle, in-pane search toggle, bookmark star,
  bookmarks dropdown.
- Floating name filter: Esc clears, Enter opens first visible entry.
- In-pane search bar: name (wildcards), content text, regex checkbox. Limits
  5000 results, 2.5 s; excludes `searchExclude` plus dot-entries unless hidden
  shown. Results are a virtual listing.
- List header: click sorts, edge resize (48-420 px), right-click column menu.
- Rows/tiles: rubber-band selection (Ctrl adds), Ctrl+wheel zooms grid +-8 px,
  drop target. Empty states: Loading, Nothing found, Nothing with this name,
  Folder is empty; errors as text.
- "Preview in other pane": the pane shows a full preview with prev/next image,
  "n of N", gallery button.

Sidebar (sections collapsible, `sidebarCollapsed`; dropping a folder bookmarks
it, a file shows "only folders can be bookmarked"):

- Places: roots (`~` Home, `/` Root, mounts under /mnt, /media/$USER,
  /run/media/$USER) as lazily loaded trees; twisty expands, name navigates;
  expanded nodes persist (`sidebarTreeOpen`).
- Bookmarks: folder icon/colour, custom title (inline rename, empty reverts),
  remove, drag reorder; menu Open / Rename / Remove.
- Labels used in the current folder: dot, name, count; click filters `#id`.
- Removable devices (`sidebarDrives`): unmounted dimmed with Mount; mounted
  navigate and have eject (moves panes inside the device back to home).

Tree pane: dockable, rooted at `/`, follows symlinked folders (badge with
`showSymlink`). Buttons: locate current, auto-follow (`treeAutoFocus`), expand
current, collapse all, close. Click navigates, double-click toggles.

Gallery (overlay or standalone window):

- Header: name, "n of N", "Edited WxH", info (I), copy image (Ctrl+C), zoom -/+,
  reset (0), slideshow (Space), fullscreen (F), close.
- Stage: prev/next, wheel zoom 0.1-8, drag pan, double-click fit/2x.
- Filmstrip (windowed 2000 thumbs).
- Edit: crop rectangle; resize W/H/% keeping aspect; save as copy
  (`imageFormat`, `imageQuality`), replace original (own format), discard.
- Info sidebar (`galleryInfo`): summary (name, dims, megapixels, aspect, size,
  MIME, modified, created, path); 5 dominant colours + 40-bin luminance
  histogram (160 px downscale); EXIF (camera, date, ISO, aperture, exposure,
  focal); edit tools (rotate +-90/180, flip H/V, crop, resize, 25/50/75/100%,
  brightness 50-150, contrast 50-150, saturation 0-200, grayscale 0-100 with
  live preview, Apply, Reset).
- `galleryLoop`, `slideshowSec`.

Editor (docked, maximised, or own window):

- Header: name, dirty dot, language; Markdown Visual/Source; wrap; font size
  9-22 (12.5); split preview (markdown source); PDF export (markdown, light
  theme, `<name>.pdf`); Save (Ctrl+S); maximise; close (Esc).
- Visual mode: WYSIWYG markdown with per-block direction; code always LTR.
  Source mode: highlighted with line numbers.
- Footer: lines, characters, saved state, key hints.
- Drafts: saved 700 ms after edits and on close; on open a banner offers Restore
  / Discard. Switching files in the docked editor saves the previous buffer.

Preview panel: prev/next image, HTML Render/Code toggle, title, "n of N",
gallery button, close (F3). Body: image (under 8 MB), PDF, office text
(docx/xlsx/pptx/odt/ods/odp), audio/video player, sanitised HTML, rendered
markdown, line-numbered highlighted text (capped by `previewMaxKB`), binary "no
preview". Footer: size, date, ID3/EXIF pills, Truncated, Open.

Search overlay (Ctrl+K or double Shift within 420 ms): indexed search, 500 ms
debounce, up to 300 hits, index stats and age, reindex, settings, progress.
Rows: icon or extension badge, name, parent, size, date. Up/Down, Enter reveals
(navigate + select), Ctrl+Enter or double-click opens, Esc closes.

## 2. Views and listing

- View modes: list (name + columns) and grid (tile width 80-240, default 108;
  thumbnail 0.66 x width; name lines 1-4; up to 4 caption fields `gridMeta`). In
  grid view double-click opens images and text in their own windows.
- Columns (name always first), default widths: type 88 (kind, or "Group N" in
  duplicates), size 94, date 120, mode 92 (`rwxr-xr-x`), owner 72 (`uid:gid`),
  links 52, dims 90 (`WxH`), rating 74. User-ordered; widths persist.
- Sorting: name, size, mtime, ext, mode, owner, links, dims, rating; asc/desc.
  Same column flips, new column starts ascending. Natural, case-insensitive.
  Folders first only for name/size sorts when `foldersFirst`; descending keeps
  folders first. Pinned files float to the top.
- Grouping: none; name (A-Z plus `#`); type (kind); size (folders, <1 MB, 1-10
  MB, 10-100 MB, 100 MB-1 GB, >1 GB); date (today, yesterday, last 7 days, this
  month, this year, older); ext (folders, upper-cased extensions, "No
  extension"). Headers show label and count; semantic order. Setting it writes
  the folder format and the global `groupBy`.
- Folder format in the folder's `.mfm.json`:
  `{view, columns, sortKey, sortAsc, group}`, written on every sort/view/group
  change; never for virtual listings or archives.
- Folder appearance: colour (red, orange, yellow, green, blue, purple, pink,
  teal, black, grey), icon (`g:<builtin>`, `p:<pure-name>`, or absolute path),
  `hidden` (hide from parent unless hidden files shown).
- Per file in the folder's `.mfm.json`: pins, labels (8: red, orange, yellow,
  green, blue, purple, pink, gray), ratings 1-5, notes (500 chars), colours.
- Row decorations: file-type dot (`fileTypes`), label dot, file colour dot, pin,
  hidden-from-listing glyph (dimmed row), icon, name, symlink badge.
- Hidden files: Ctrl+H; also reveals `.mfm.json`-hidden folders.
- Ignore list (`ignoreFiles`, default `.directory, desktop.ini`): exact or glob,
  case-insensitive; applied to listings, flat view, search, indexer.
- Filter syntax: plain substring; `#lbl` label id contains; `*N` rating >= N.
- Type-ahead: unmodified characters append to an 800 ms buffer that becomes the
  filter and selects the first prefix match; Backspace edits; navigating clears.
- Virtual listings: flat view (limit 5000), search results, duplicates ("N
  groups"), archive browse (zip/tar/tgz/tar.gz/tar.bz2/tbz2; F5 extracts the
  selection to the other pane).
- Live updates for visible folders.
- Selection: click (anchor), Ctrl-click toggle, Shift-click range, rubber band,
  right-click selects unless already selected, right-click blank clears.
  Up/Down/PgUp/PgDn/Home/End (+Shift extends); grid Left/Right move cells; list
  Left = parent, Right = open (Krusader). Esc clears selection and filter.
  Moving the selection updates an open preview / docked editor.

## 3. Commands and shortcuts

All rebindable. Default toolbar: newItem, copy, move, edit, rename, trash, size,
look, previewPane, undo, newTab, flat, dupes, terminal.

| Command (id)                              | Key          |
| ----------------------------------------- | ------------ |
| New menu (`newItem`)                      | Ctrl+N       |
| Copy to other pane (`copy`)               | F5           |
| Move to other pane (`move`)               | F6           |
| Copy files to clipboard (`copyFiles`)     | Ctrl+C       |
| Cut (`cutFiles`)                          | Ctrl+X       |
| Paste (`paste`; image data saves a PNG)   | Ctrl+V       |
| Edit text (`edit`)                        | F4           |
| Rename (`rename`; batch if >1)            | F2           |
| Batch rename (`batchRename`)              | Shift+F2     |
| New folder (`mkdir`)                      | F7           |
| Trash (`trash`; delete if not trash mode) | F8           |
| Delete permanently (`hardDelete`)         | Shift+Delete |
| Calculate size (`size`)                   | F9           |
| Sync with other pane (`sync`)             | Ctrl+Shift+Y |
| Compare two files (`diff`)                | Ctrl+Shift+Z |
| Undo (`undo`)                             | Ctrl+Z       |
| Select all (`selectAll`)                  | Ctrl+A       |
| Refresh (`refresh`)                       | Ctrl+R       |
| Parent folder (`up`)                      | Backspace    |
| Preview panel (`previewPane`)             | F3           |
| Gallery (`gallery`)                       | Space        |
| Flat view (`flat`)                        | Ctrl+Shift+F |
| Duplicate files (`dupes`)                 | Ctrl+Shift+D |
| Compress (`compress`)                     | none         |
| Extract (`extract`)                       | none         |
| Name filter (`filter`)                    | Ctrl+F       |
| Hidden files (`hidden`)                   | Ctrl+H       |
| Bookmark (`bookmark`)                     | Ctrl+D       |
| List/grid (`toggleView`)                  | Ctrl+Shift+V |
| Single/dual (`toggleLayout`)              | Ctrl+Shift+L |
| New tab (`newTab`)                        | Ctrl+T       |
| Close tab (`closeTab`; not pinned/locked) | Ctrl+W       |
| Reopen closed tab (`reopenTab`)           | Ctrl+Shift+T |
| Next tab (`nextTab`)                      | Ctrl+Tab     |
| Address bar (`addressBar`)                | Ctrl+L       |
| Terminal here (`terminal`)                | Ctrl+Alt+T   |
| Reveal in system file manager (`reveal`)  | none         |
| Check for update (`checkUpdate`)          | none         |
| Folder sizes from index (`indexSizes`)    | none         |
| Global search (`searchIndex`)             | Ctrl+K       |
| Layouts and filters (`presets`)           | Ctrl+Shift+P |
| Save folder display format (`saveFormat`) | none         |
| Folder icon/look (`look`)                 | none         |
| Properties (`properties`)                 | Alt+Enter    |
| Preferences (`prefs`)                     | Ctrl+,       |

Fixed keys: Alt+Left/Right history; Tab switches pane; Enter opens; Esc clears;
Ctrl+1..9 tab N; Menu or Shift+F10 context menu. Gallery: Left/Right, Home/End,
+/=, -, 0, R, Space, F, I, Ctrl+C, Esc. Editor: Ctrl+S, Esc.

Upstream quirks not to copy: type-ahead swallows single-character bindings; hard
delete labelled Shift+F8; refresh labelled F3; no previous-tab key.

Context menu (item): Open; Open in new window (images, text); Open with (custom
commands, apps, Other); Preview; Preview in other pane; Properties; folders:
Terminal here, Content chart; images: Gallery; New submenu. File operations:
copy, move, browse archive, extract, extract to other pane, rename, batch,
compress, trash, delete. Folder and view: open in new tab, new tab here,
calculate size, folder icon, bookmark toggle, hide/show in listing, save display
format, pin/unpin. Path and system: copy path, create symlink, reveal, terminal,
SHA-256 to clipboard, sync, compare (2 selected), run each `prefs.scripts`
entry. Labels and details: edit text, colour label, rating and note, audio tags
(mp3/m4a), convert images. Listing export: CSV, PDF. Bottom: quick actions (cut,
copy, paste, trash, delete), colour swatch row, folder icon action.

Blank-area menu: New, Refresh, this folder's commands, Terminal, Flat view,
Duplicates, Size, Content chart, folder icon, Columns, New tab, quick actions,
swatches.

Tab menu: Rename, Lock/Unlock (locked tab opens navigation in a new tab),
Pin/Unpin, Duplicate, Close, Close others, Close to the right, Tab colour (none

- 8 label colours, keyed by path), Copy path.

Column header menu: 8 columns (at least one on), grid captions (max 4), Column
settings. Sort menu: 9 keys, asc/desc, 6 group modes.

New menu: folder; office docx, xlsx, pptx, pdf, md, csv, txt; design svg, fig,
png; code py, js, ts, html, css, sh, sql; config json, yaml, env; project
README.md, AGENTS.md, .gitignore; blank file. Office/design formats are real
binary templates. The name prompt pre-selects the stem.

Drag and drop: rows to other pane or folder row move (Ctrl copies); external
drops copy; drag out as `text/uri-list`; folders onto the sidebar bookmark;
bookmarks and tabs reorder.

## 4. Dialogs

- Name prompt (new folder / file / rename), tab rename, delete confirm (trash vs
  permanent, lists 3 names), colour label picker (8 + none).
- Batch rename: template tokens `{name} {ext} {n} {nn}` plus
  `{title} {artist} {album} {year} {date} {camera}`; start number, zero pad;
  regex mode with `$1`; preview of first 8 with changes highlighted;
  duplicate-name warning.
- Archive: compress (name, level 1/6/9, here or other pane); extract (dest,
  never overwrites).
- Sync: compare by mtime / ctime / hash, ignore patterns, direction, trash extra
  files, differences only, summary chips, up to 500 rows, Run.
- Diff: unified, up to 6000 lines, added/removed counts, binary and approximate
  chips.
- Symlink: target, link name default `<name> - link`, same folder, never
  overwrites.
- Presets: named layouts (layout, sidebar mode/open, view, columns, toolbar) and
  named filters.
- Properties: General (rename, type, MIME, open with + change, location + copy,
  size with recursive folder size, dims, symlink target, times); Permissions
  (owner/group/others view+modify vs view, executable, ownership, octal + Apply,
  rwx, links, inode/device); Checksums (expected hash + paste, MD5/SHA-1/
  SHA-256/SHA-512 compute/copy, match badge); Details (label, stars, note).
- Open With: search, recommended/all, MIME, custom commands, apps with Default
  badge, "always open with", system default.
- Folder look: preview, icon pack, category, search, size, built-in glyphs,
  custom pack path, "Don't show in parent listing", clear icon. Multiple folders
  at once.
- Rating and note; ID3 tags (title, artist, album, year); image convert (JPEG /
  PNG / WebP / BMP, longest side default 1920, quality default 88, output to
  other pane).
- Content chart: scope this folder / all levels; totals; donuts by kind size,
  kind count, extension (top 7 + Other); top 10 files (click reveals) and top 10
  sub-folders.
- Preferences: paged modal (section 5).

## 5. Preferences (defaults)

Appearance: lang en|fa (en); theme dark|light|system (dark); colorTheme (10
ids); fontFamily; titlebar app|system; rounded; titlebarButtons; titlebarSide.

File display: view list|grid (list); tileSize 80-240 (108); gridNameLines 1-4
(2); foldersFirst (true); fileIcons pure|system|line (pure); ignoreFiles
(`.directory`, `desktop.ini`; <=100 entries of <=80 chars, no slashes); groupBy
(none); showHidden (false); showSymlink (false); sizeFormat human|bytes;
dateFormat iso|jalali|short (iso); showSeconds (false).

Columns: columns (`type, size, date`), gridMeta (`size`, max 4).

Toolbar: toolbarMode icon|icon-text|text (icon-text); toolbar list.

Shortcuts: per-command accelerator, capture, remove, reset, reset all, conflict
warning.

Window and layout: layout single|dual (dual); sidebar docked|float; treePane
off|left|right (off); treeAutoFocus (true); sidebarDrives (true); sidebarTone
1-6; hideSingleTab (true); tabIcons (true).

Agents (MCP): mode off|read|write, token, client snippet, audit log.

Search: doubleShiftSearch (true); searchRoots (empty = Documents, Downloads,
Desktop, Pictures, Music, Videos); searchExclude
(`node_modules, .git, .cache, .venv, dist, build, *.tmp, .Trash, snap`).

File types: ext -> colour from the label palette.

Default apps: ext -> .desktop; customCommands `{label, pattern, command}` where
pattern is extensions, `folder`, or `*`, and `%f` is the path.

Archive and gallery: zipLevel 0-9 (6); imageFormat png|jpeg|webp (png);
imageQuality 50-100 (90); slideshowSec 1-60 (3); galleryLoop (true);
previewMaxKB 16-8192 (200).

Transfer: transferSkipExisting, transferNewerOnly (false); speedLimitKB (0).

File operations: doubleClick open|gallery|preview; confirmDelete (true);
trashByDefault (true).

Startup: hotkeyEnabled (false), hotkey (Super+E); portalPicker (false);
singleInstance (true); startup session|folder; startFolder; minimizeToTray,
closeToTray (false); set as default file manager.

Advanced: export/import settings as JSON; icon directories.

No UI: sideWidth 0.5, dualSplit 0.5, colWidths, sidebarOpen, sidebarWidth 208,
sidebarCollapsed, sidebarTreeOpen (<=300), tabColors, newTabPos next|end,
middleClickClosesTab, paneTree, treeWidth 232, galleryInfo, layouts, filters,
scripts `{name: shell line with {path} {dir}}`, pickerLastDir.

Session: per pane tabs, active tab, pin count, names, locks; active pane; global
default sort; bookmarks and names (trimmed, <=80 chars, <=500).

## 6. i18n

English default, Persian (RTL) switches live. Paths and address field always
LTR; names `dir=auto`; code always LTR. Persian digits and `٫` decimal in fa.
Jalali dates are a separate setting. About 1,045 strings.

## 7. Specs pinned by renderer tests

- `fmtSize`: folders ''; `0 B`; one decimal below 10, none at 10 or above (1024
  -> 1.0 KB, 12345678 -> 12 MB); bytes mode.
- `fmtDate`: 0 -> ''; iso `YYYY-MM-DD HH:mm` (+`:ss`); jalali; short `HH:mm`;
  local time. `toJalali(2024-03-20)` -> 1403-01-01.
- `crumbs`, `baseName` (trailing separator ignored), `dirOf('/a')` -> `/`.
- `kindOf`/`typeLabel`: 12 kinds; unknown extension upper-cased; none -> File.
- Sorting: natural (file2 < file10); folders first also descending;
  `foldersFirst=false` mixes; size sort keeps folders first, mtime interleaves;
  mode/owner/links/dims(area)/rating sorts; filter trimmed, case-insensitive,
  `#` label substring, `*N` rating; pins float to top.
- `navIndex`: empty -> -1; none selected -> 0 (End -> last); clamped; grid
  up/down step by columns, page = rows x columns.
- `opensInEditor`: txt/log/md/markdown/mdx/rst and extensionless files.
- Permissions: setting rw keeps file x bit; folder x follows read; 644 + others
  rw -> 646; owner r -> 444; folder 755 owner r -> 555; folder others none
  -> 750. `modeString(755)` -> `rwxr-xr-x`.
- `menuTargets`: clicked row wins unless already selected.
- `planRename`: `photo-{nn}` start 3 pad 3 -> `photo-003.txt`; extensionless
  stays extensionless; `{name}-small.{ext}` does not double the extension; regex
  `IMG_(\d+)` -> `photo-$1`; invalid regex leaves names unchanged.
- `openAction`: folders navigate; archives browse; per-extension default app ->
  external; mode open -> external; preview -> preview; gallery mode: HTML ->
  preview, images -> gallery, text -> editor, else external; inside archives
  editor/preview fall back to external.
- `terminalDir`: folder itself; file -> parent; inside archive -> archive's
  parent; no selection -> tab folder.
- `matchCustomCommand`: extension lists split on `, ;` whitespace,
  case-insensitive, leading dot allowed; folder keywords; `*`.
- `reorderItem`: before/after; out of range or same index unchanged.
- `groupEntries`: name `#` first then A-Z; type/ext folders first; date/size
  fixed bucket order; none -> one group; empty -> [].
- `isIgnoredName`: exact, case-insensitive; `*` `?` globs (`?.tmp` matches
  `a.tmp` not `ab.tmp`); empty never matches.
- Keys: parse/format accelerators, conflicts.
- Image info: `aspectText` known ratios within 1% (1:1, 3:2, 4:3, 5:4, 16:9,
  16:10, 21:9, 2:1, 3:1) else reduced fraction, `—` invalid; megapixels;
  dominant colours (4-bit buckets, alpha < 16 ignored, shares sum to 1);
  luminance histogram (Rec.709, normalised to tallest bin).
- Charts: skip values <= 0; percent one decimal, 0 for zero total; fold slices
  beyond N into Other.
- Direction: Persian digits count as RTL.
- New templates: categories office, design, code, config, project; `.gitignore`
  gets no extension.

## 8. Visual design

Fully custom CSS theme (dark default, 10 colour themes), custom title bar,
Vazirmatn font, bundled Pure icon theme (GPL-3.0), line-SVG UI glyphs, per-kind
colours, status-bar toast instead of popups. The Qt rewrite replaces all of this
with the Plasma style, colour scheme, fonts, icon theme and KWin decorations.
