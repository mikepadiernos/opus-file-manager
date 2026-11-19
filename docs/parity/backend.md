# Max File Manager: backend feature inventory (src/main, src/preload, src/shared)

This is a reference snapshot of a reviewed Max checkout, not a description of
implemented Opus features or necessarily the latest upstream version. KDE/Qt
replacement notes are design suggestions, not implementation status; see the
[roadmap](../ROADMAP.md).

**Context.** The app is Electron 45 with a React renderer. It is a dual-pane
file manager targeting KDE and Windows (this checkout also has macOS code
paths). The renderer has no `fs` access: everything goes through the
`window.api` preload bridge. Paths from the renderer are checked with
`path.isAbsolute`. Local files reach the renderer over two privileged schemes:

- `mfm://<abs path>` streams any local file and adds CORS `*` so canvases are
  not tainted.
- `thumb://<abs path>?s=<side>&v=<mtime-size>` returns cached thumbnails. A
  versioned URL gets `immutable, max-age=1y`; an unversioned one gets
  `no-cache`.

UI strings and error messages in main are mostly Persian. The app version is
1.0.8. The only runtime dependency is `dbus-next`; fflate is also used.

## Global storage map (userData = `~/.config/max-file-manager/`)

| File                                                         | Content                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `search.db` (SQLite, WAL)                                    | Index tables `files(path PK,name,ext,dir,size,mtime,root)`, `dirs(path PK,mtime,root)`, FTS5 `fts(name)` (tokenizer `unicode61 remove_diacritics 2`), `meta(k,v)` (key `lastIndex`). **App settings live in table `app(k,v)`** as JSON strings: `state`, `window`, `draftIndex`, `draft:<abs path>`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| `app['state']`                                               | `{panes:[{tabs,tabIds,ti,pinCount,names,locks}], active, sort, marks:[abs paths], markNames, prefs:{…}, chrome:{customTitlebar,rounded}}`. Main itself reads `prefs.titlebar` ('system'/'app'), `prefs.rounded`, `prefs.singleInstance`, `prefs.portalPicker`, and writes `prefs.pickerLastDir`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `app['window']`                                              | `{x,y,width,height,maximized}` (the normal, un-maximized bounds)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Prefs keys (defined in renderer `prefs.ts`, stored verbatim) | prefsVersion, lang, theme, colorTheme, fontFamily, view, foldersFirst, showHidden, showSymlink, sideWidth, dualSplit, columns, colWidths, gridMeta, sizeFormat, dateFormat, showSeconds, gridNameLines, tileSize, fileIcons, titlebar, titlebarButtons, titlebarSide, rounded, layout, sidebar, sidebarOpen, sidebarWidth, sidebarTone, sidebarCollapsed, sidebarDrives, sidebarTreeOpen, shortcuts, toolbar, toolbarMode, newTabPos, middleClickClosesTab, hideSingleTab, tabIcons, tabColors, zipLevel, imageFormat, imageQuality, slideshowSec, galleryLoop, previewMaxKB, layouts, filters, fileTypes, searchRoots, searchExclude, doubleShiftSearch, paneTree, treePane, treeWidth, treeAutoFocus, galleryInfo, groupBy, ignoreFiles, scripts, transferSkipExisting, transferNewerOnly, speedLimitKB, doubleClick, confirmDelete, trashByDefault, portalPicker, pickerLastDir, singleInstance, minimizeToTray, closeToTray, startup, hotkey, hotkeyEnabled, startFolder, defaultApps, customCommands, iconDirs, iconTheme |
| Legacy `state.json`, `window.json`                           | Imported once into SQLite by `migrateLegacyStore` and left on disk as a backup                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `mcp.json` (mode 600)                                        | `{mode:'off'\|'read'\|'write', port (default 47321, valid range 1025–65534), token (48 hex chars)}`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `mcp.log` (mode 600)                                         | JSONL audit: `{ts, tool, ok, args-summary\|reason}`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `thumbs/<sha1(path\|mtime\|size\|side)>.png`                 | Thumbnail cache                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| `clipboard-owner`                                            | PID of the clipboard helper process                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `drag-debug.log`                                             | Written only when `MFM_DRAG_DEBUG=1`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| `appicons/`                                                  | macOS only                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Per folder: `<dir>/.mfm.json`                                | Folder metadata (see meta.ts)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |

Settings export file format:
`{app:'max-file-manager', format:1, exportedAt, prefs}`. Import also accepts a
bare prefs object.

---

## Modules

### index.ts (main process: windows, IPC, protocols)

#### Capabilities

- **Directory listing** (`list`):
  - readdir plus a stat per entry, in batches of 128. Each entry gets name,
    path, dir, size, mtime, ext, mode, uid, gid, nlink and a symlink flag.
  - Dot-files are hidden unless `showHidden` is on.
  - For up to 1000 subfolders it reads `.mfm.json` for color, icon and `hidden`.
    If there is no MFM icon or color, it falls back to the native look from
    Dolphin's `.directory` or Windows' `desktop.ini`.
  - Folders whose `.mfm.json` says `hidden` are dropped unless hidden files are
    shown.
  - Optional pixel dimensions for up to 400 images under 20 MB (png, jpg, gif,
    webp, bmp, avif).
  - The listing also returns the folder's pins, labels, ratings, notes, format
    and per-file colors.
- **Live refresh**: one `fs.watch` per visible folder (no polling). Events are
  debounced 300 ms, then `fs:changed` is sent. Watchers are dropped when a
  folder leaves the screen.
- **Roots**: `/`, `~`, and the subfolders of `/mnt`, `/media/$USER` and
  `/run/media/$USER`. On Windows, drive letters.
- **Folder size**: recursive walk that stops at 200k files or 4 s and returns a
  `capped` flag.
- **statfs**: free and total bytes of a filesystem.
- **Properties**:
  - lstat/stat, permission bits, R/W/X access checks.
  - Owner and group names via `getent passwd|group` (cached).
  - nlink, inode, device, birth/m/a/ctime, symlink target, MIME, child count for
    folders, image dimensions.
- **chmod** limited to mode 0..0o7777.
- **Raw bytes** of a file (≤32 MB), used to cache drag-out data for web drop
  zones.
- **Image as data URL** (≤40 MB).
- **Clipboard image paste**: saves PNG bytes (≤50 MB) with a sanitized name, a
  free (non-colliding) name and an atomic `.part` → rename write.
- **User scripts** (`app:runScript`): runs `sh -c` with `{path}`/`%f` and
  `{dir}`/`%d` shell-quoted. cwd is the given dir or home; 60 s timeout; output
  trimmed to 2 KB.
- **Global hotkey**: one accelerator via Electron `globalShortcut`; it shows and
  focuses the window. It fails on Wayland and the failure is reported.
- **Windows**:
  - Saved bounds are written with a 400 ms debounce and re-fitted to the current
    displays at startup (`window-bounds.ts`).
  - Frameless/transparent mode controlled by `chrome`.
  - Min size 780×480, default 1320×860.
  - Standalone gallery and editor windows (`?gallery=1&path=` /
    `?editor=1&path=`) are tracked and closed on quit.
- **Tray**: optional minimize-to-tray and close-to-tray with a Show/Quit menu.
  Only active when single-instance is on. If no tray host exists, both are
  turned off so the window can never be hidden with no way back.
- **Single instance**: the lock is taken before `ready`; a second instance calls
  `process.exit(0)`.
  - A second launch restores and focuses the window and sends `app:open-path`
    with the target.
  - `--portal-backend` in the second argv starts the portal server.
- **Launch path**: `targetPath(argv)` is sent as `app:open-path` after load.
  macOS `open-file` events are queued until the window exists.
- **Search index runner**: one run at a time, with a global status object.
  Progress is broadcast as `search:progress` to the window even when the run was
  started over MCP.
- **Session state**: get/set/setPrefs/patchPrefs. patchPrefs merges a patch and
  broadcasts `state:prefs-patch` to all windows.
- **Bookmarks** ("marks"): add/remove absolute paths and broadcast `state:marks`
  to all windows.
- **Smoke mode**: `SMOKE_SHOT` and `SMOKE_JS` env vars capture a screenshot and
  quit.
- **MCP handler**: maps about 91 tool names to the same functions the IPC
  handlers use. Tools that drive the window are forwarded to the renderer as
  `mcp:ui`.

#### KDE/Qt replacement

- Listing and refresh: KDirLister/KDirModel + KFileItem, which bring KDirWatch
  updates for free.
- Size: KIO::DirectorySizeJob. Free space: KIO::FileSystemFreeSpaceJob.
- Properties and chmod: KPropertiesDialog / KFileItem / KIO::chmod.
- Roots: KFilePlacesModel.
- Hotkey: KGlobalAccel. Tray: KStatusNotifierItem. Single instance:
  KDBusService(Unique).
- Window size: KWindowConfig. Settings: KConfig/KSharedConfig.

### jobs.ts (transfer engine, undo, rename/mkdir/new file); tests: jobs.test.ts

#### Capabilities

- **Queue**: copy and move jobs run strictly one after another (a promise
  chain).
- **Job fields**: `id` (`j<n>`), `kind`, `label`, `state` (queued, running,
  paused, done, cancelled, error), `bytes`, `done`, `files`, `filesDone`,
  `speed` (B/s, every 500 ms), `current`, `errors[]`, `skipped`.
  - Updates are throttled to 120 ms (forced on state changes) and sent as
    `job:update`.
- **Pre-scan** measures total bytes and file count per source.
- **Copy**: streamed in 1 MB chunks with backpressure. Pause/resume polls every
  100 ms. On cancel the partial file is deleted, but folders already created are
  left behind.
- **Preserved**: atime and mtime only (no mode or owner). Symlinks are recreated
  as symlinks; directories are copied recursively.
- **Move** is copy + `rm -rf` of the source. There is no `rename()` fast path,
  even on the same filesystem.
- **Conflict policy**: there are no prompts.
  - Default: the destination gets a free name, `name (2).ext`, `name (3).ext`, …
    (`freeName`). Copying into the same folder therefore duplicates.
  - `skipExisting`: an existing destination name is left alone and counted in
    `skipped`.
  - `newerOnly`: the destination is overwritten only if the source mtime is
    newer; otherwise it is skipped.
  - `limitBps`: a rough throttle (sleeps after each chunk, ≤500 ms per sleep).
- **Undo stack**: LIFO, maximum 20 records. It covers:
  - move (rename back; on EXDEV, copy back then delete),
  - batch rename (reverse rename),
  - mkdir (`rmdir`),
  - createFile (`unlink`).
  - It does **not** cover copy, trash, delete, extract, or a `newerOnly`
    overwrite.
  - Undo returns `{done,label}`. One item that cannot be restored does not stop
    the rest.
- **renameMany(pairs)**: `fs.rename` per pair, with per-item errors. The batch
  becomes one undo record.
  - Note: POSIX rename silently overwrites an existing target. Collision checks
    happen in the renderer (`planRename`).
- **makeDir(dir,name)**.
- **createFile(dir,name,content)**: exclusive create (flag `wx`). EEXIST
  produces an error. Content prefixed `base64:` is written as binary; this is
  how the template system creates valid docx/xlsx/pptx/pdf/png/svg/fig files.

#### Tests pin down

- Copy reports exact byte totals and keeps the source.
- Move deletes the source; undo depth is 1; undo restores the source and removes
  the destination.
- Cancelling a 24 MB copy leaves no partial file and the state becomes
  `cancelled`.
- renameMany returns `{done:2,errors:[]}` and undo reverses it.
- mkdir followed by undo removes the folder.
- createFile writes text, refuses to overwrite, writes base64 binary (bytes
  1,2,3,4 and a `PK` header), and each create is undoable.
- A relative destination makes the job fail with an error mentioning
  "destination".
- skipExisting leaves the old file, `skipped=1`, and creates no `(2)` file.
- newerOnly replaces an older destination file and skips a newer one.

#### KDE/Qt replacement

- KIO::copy/move (CopyJob) and KIO::rename/mkdir/`KIO::storedPut`.
- KIO::FileUndoManager, which covers more than the current stack (copy, link,
  trash, mkdir, put).
- KJobTrackerInterface / KUiServerV2JobTracker / KWidgetJobTracker for progress.
- KIO::AskUserActionInterface / RenameDialog for conflicts. Automatic `(2)`
  naming can be done with `KFileUtils::suggestName`; check that its numbering
  format matches.
- KIO has no bandwidth limit, and no skip-existing or newer-only filters built
  in. Use the `CopyJob` auto-skip flags (`setAutoSkip`), or a custom ask-user
  interface that answers per item.

### ops.ts (delete, free names, symlinks); tests: ops.test.ts

#### Capabilities

- `remove(paths,{trash?})`: uses the injected trash function (`shell.trashItem`,
  i.e. the freedesktop Trash) or `fs.rm -rf --force`. It never throws; errors
  are reported per item.
- `freeName`: `base (N).ext` starting at N=2.
- `createSymlink(target, link)`: absolute paths only. Refuses when target equals
  link, when the target is missing, when the link's parent folder is missing, or
  when the link path already exists (`lstat`). Windows uses junctions for
  folders. EPERM gets a friendly message.

#### Tests pin down

- The trash callback owns the file (it is not deleted directly).
- Relative paths are refused.
- freeName produces `(2)`, then `(3)`.
- A missing path counts as done (force) and the batch continues.
- symlinkType per platform.
- File and folder links work and are never overwritten.
- A missing target or a relative path is refused.

**KDE/Qt replacement**: KIO::trash / KIO::del / KIO::DeleteOrTrashJob;
KIO::symlink / KIO::link; KFileUtils::suggestName.

### dirs.ts (sidebar tree children); tests: dirs.test.ts

#### Capabilities

- Lists subfolders only, capped at 1000. `showHidden` covers dot-folders and
  `.mfm.json` `hidden`.
- `followLinks` includes symlinked folders (marked `link:true`) when the link
  resolves to a directory.

#### Tests pin down

- Default output is real folders only.
- `followLinks` adds and marks linked folders.
- Broken links and links to files are never listed.
- Hidden dot-entries follow `showHidden`.
- A missing folder returns `[]`.

**KDE/Qt replacement**: KDirModel + KDirSortFilterProxyModel with
`setDirOnlyMode`.

### search.ts (live walk/search, duplicates); tests: search.test.ts

#### Capabilities

- **`walk(root, opts)`**: breadth-first, used for the flat view and folder
  search.
  - Name matching, in order of precedence:
    - Regex (case-insensitive). An invalid regex falls back to substring
      matching.
    - `*`/`?` wildcards anchored to the whole name.
    - Otherwise a case-insensitive substring.
  - Filters: `minSize`, `maxSize`, `newerThan`, and `content` (case-insensitive
    substring in text files ≤2 MB; files with a NUL in the first 4 KB are
    skipped).
  - `includeDirs`; `exclude` patterns (same syntax as the index exclusions).
  - Limits: `limit` 5000 results, `maxScan` 200k files, `budgetMs` 4 s; returns
    a `capped` flag.
  - Results carry a `name` relative to the root.
- **`findDupes(root)`**: groups files by size, then sha256. Zero-byte files are
  ignored; each group gets a 1-based `group` number; the scan is capped at 4000
  entries.

#### Tests pin down

- Literal, wildcard and regex matching, and the broken-regex fallback.
- Wildcard search is recursive with relative names and absolute paths.
- Content search is case-insensitive.
- Size filter works.
- Hitting `limit` sets `capped`.
- Duplicate grouping puts three identical files in group 1.

**KDE/Qt replacement**: KIO `filenamesearch:/` (kio-extras; supports content
search) or a custom QDirIterator worker; QCryptographicHash for duplicates.

### searchdb.ts (indexed search plus the settings KV); tests: searchdb.test.ts

#### Capabilities

- `indexRoots({roots, exclude})`: incremental indexing.
  - A folder whose mtime has not changed is skipped together with its whole
    subtree.
  - Progress events (scanned, added, done/total roots, current root) are emitted
    every 400 entries.
  - Roots that are no longer configured are purged.
  - `lastIndex` is stored.
- `excluded(path, patterns)`: glob patterns matched against the base name or the
  full path.
- **`searchIndex(q, limit≤1000)`**:
  - Tokens are split on whitespace and on `._-/\`. Each becomes an FTS5 prefix
    term (`"tok"*`), ANDed together in any order.
  - Results are ordered folders first, then by name.
  - If fewer than 20 hits come back, a LIKE `%tok%` substring fallback adds
    more.
- `indexStats` (files, dirs, lastIndex).
- `indexSizes(dirs)`: instant folder sizes computed with SUM over the index.
- `clearIndex` empties files, fts, dirs and meta, but keeps the `app` table.
- `getAppKV` / `setAppKV`.

#### Tests pin down

- Dots, dashes and spaces are interchangeable word boundaries ("mim for me",
  "MIM FOR ME" and "me mim for" all find `mim.for.me.4.3.1.md`).
- Clearing the index keeps the settings in `app`.

**KDE/Qt replacement**: Baloo (`Baloo::Query`, `baloosearch:/`) for system-wide
search. For parity with the user-chosen roots and exclusions, keep a private
SQLite FTS5 index via QtSql. Settings themselves move to KConfig.

### store.ts

**Capabilities**: `readStore` / `writeStore` are JSON values in `search.db.app`.
`migrateLegacyStore` imports `state.json` and `window.json` once. **KDE/Qt
replacement**: KSharedConfig (`max-file-managerrc`) plus a KConfig state file
(`KSharedConfig::openStateConfig`).

### drafts.ts (editor crash safety); tests: drafts.test.ts

#### Capabilities

- One KV entry per file (`draft:<path>` → `{text, at}`) plus an index
  `draftIndex` → `[{path, at}]`.
- `prune` on boot keeps the newest 200 and drops anything older than 60 days.
- IPC refuses texts over 8 MB.

#### Tests pin down

- Drafts round-trip per path and clear independently.
- Setting the same path again replaces the draft (the index keeps one row).
- Prune keeps the newest N and expires old ones.
- Junk JSON returns null.

**KDE/Qt replacement**: a QSettings/KConfig group or files under
`QStandardPaths::AppDataLocation`. KTextEditor has its own swap files
(`KTextEditor::Document` swap) if a Kate part is used.

### archives.ts (zip/tar browse, extract, create); tests: archives.test.ts

#### Capabilities

- `isArchive` recognises `.zip .tar .tgz .tar.gz .tar.bz2 .tbz2`. **bz2 is
  claimed but not implemented**: only gunzip is applied, so it fails.
- Reading:
  - The whole archive is read into memory, capped at 256 MB.
  - zip via fflate; zip mtime is always 0.
  - tar via a minimal built-in ustar parser: name plus prefix, size, mtime,
    directory flag. No pax or GNU long names.
- `listArchive` returns a flat list with `path = "<archive>!<inner>"` and skips
  `__MACOSX`.
- **`extract(archive, names[], dest)`**:
  - Writes the selected entries, or everything when `names` is empty.
  - Zip-slip guard: resolved paths must stay inside `dest`.
  - Never overwrites: an existing file gets a `name (2)` sibling.
- **`createZip(dir, name, sources, level 0–9)`**:
  - Streams the zip (AsyncZipDeflate) into `freeName(dir/name.zip)`.
  - Paths inside the zip are relative to each source's base name.
  - An empty source list returns an error. A failure removes the partial zip.
  - Formats: zip only (no 7z, rar or encryption).

#### Tests pin down

- isArchive recognises zip, tar.gz and tgz.
- Zip listing returns sizes and `archive!inner` paths.
- Selective extraction writes only the chosen entries.
- Extraction with an empty name list writes everything.
- The built-in tar parser works.
- createZip round-trips with a Persian archive name; `files=2`; inner paths
  `pack/one.txt` and `pack/deep/two.txt`.
- createZip never overwrites (`a (2).zip`) and returns the empty-source error.
- Extraction never overwrites (`a (2).txt`).

**KDE/Qt replacement**: KArchive (KZip, KTar, K7Zip, KCompressionDevice for
gz/bz2/xz/zstd), or the KIO `zip:/` and `tar:/` protocols for browsing
(kio-extras). Ark's `.desktop` service menus can also be used.

### meta.ts (per-folder `.mfm.json`, which travels with the folder); tests: meta.test.ts

**Schema** (`.mfm.json`, at most 4096 bytes on read):

| Key            | Content / validation                                                                                                                                 |
| -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `color`        | `#hex` 3–8 digits                                                                                                                                    |
| `icon`         | `g:<glyph>`, `p:<pure icon>`, or an absolute path                                                                                                    |
| `pins`         | Bare names, at most 200                                                                                                                              |
| `labels`       | name → id; ids are `red, orange, yellow, green, blue, purple, pink, gray`, colors `#e0564d #e08b39 #d8b93f #4fb56a #4f8ff0 #9a6ee0 #dd6b9c #8a93a3`  |
| `colors`       | name → `#hex` (the colored dot before a file name)                                                                                                   |
| `ratings`      | name → 1..5                                                                                                                                          |
| `notes`        | name → text, at most 500 characters                                                                                                                  |
| `format`       | `{view:list\|grid, columns ⊂ {type,size,date,mode,owner,links,dims,rating}, sortKey (<20 chars), sortAsc, group: none\|name\|type\|size\|date\|ext}` |
| `hidden: true` | Hides the folder from its parent's listing                                                                                                           |

#### Capabilities

- Name validation rejects `/`, `\`, `.`, `..` and `.mfm.json`.
- Writes go through a per-folder serialized queue, so concurrent
  read-modify-write is safe.
- A patch that leaves nothing deletes the file.
- Setters: `setMeta` (merge), `setEntryColor`, `togglePin`, `setLabel`,
  `setRating` (0 clears, values clamped to 1..5), `setNote` (trimmed; blank
  clears), `setFormat`.

#### Tests pin down

- Write then read works.
- Merging works, and an empty result deletes the file.
- Junk JSON, non-hex colors and relative icon paths are dropped.
- Pins toggle in order and reject escaping names; the file disappears when
  nothing is left.
- Labels reject unknown ids and unsafe names, and garbage is dropped on read.
- Ratings clamp to 5 and 0 clears; notes trim and blank clears.
- A concurrent rating and note on the same file both survive.
- The `hidden` flag merges and unsets correctly.
- The same test file also covers `iconsIn` (prefers scalable) and `themeDirs`
  (user packs are included).

#### KDE/Qt replacement

- Ratings, tags and comments map to `KFileMetaData::UserMetaData` (xattrs
  `user.baloo.rating`, `user.xdg.tags`, `user.xdg.comment`), which is
  Dolphin-compatible. Keep `.mfm.json` for parity and portability.
- Folder icons can use `KDesktopFile(".directory")` `Icon=`.
- Dolphin hides entries through a `.hidden` file in the parent directory.
- KFileItem/KDirModel already honour `.directory` icons.

### folderlook.ts; tests: folderlook.test.ts

**Capabilities**: read-only import of other file managers' folder icons.

- KDE `.directory` `Icon=` (last one wins), resolved through the icon theme.
- Windows `desktop.ini` `IconResource` / `IconFile`: index and quotes stripped;
  absolute or relative to the folder.

**Tests pin down**: parsing of `.directory` Icon values, and desktop.ini
IconResource with an index, quoting, and IconFile.

**KDE/Qt replacement**: KDesktopFile::readIcon; KFileItem::iconName handles this
natively.

### icons.ts

#### Capabilities

- `themeDirs`:
  - Scans `/usr/share/icons`, `/usr/local/share/icons`, `~/.local/share/icons`
    and `~/.icons`, plus extra user packs.
  - Sorts the bundled `Pure` pack first, then the active theme, then breeze,
    then others, then hicolor.
- `iconsIn(dir)`: every svg/png name, capped at 4000, preferring scalable, then
  64, 48, 32; cached.
- `resolveIconName(name)`:
  - Gets the active theme from `~/.config/kdeglobals` `[Icons] Theme`.
  - Tries sizes scalable…16 across the contexts places, apps, mimetypes,
    devices, categories and actions, in both size/context and context/size
    layouts, in the first 8 themes.
  - Skips generic folder names.
- `iconSetPaths`: picks one icon per file kind (dir, image, video, audio,
  archive, code, text, doc, pdf, sheet, slides, file).

**KDE/Qt replacement**: KIconLoader / QIcon::fromTheme / KIconTheme; KIconDialog
for the picker.

### appicons.ts

macOS only: converts `.app` bundle `.icns` icons with `plutil` and `sips` and
caches PNGs. **Drop.**

### apps.ts (Open with, default apps, launching, default file manager); tests: apps.test.ts

#### Capabilities (Linux)

- **MIME detection**: a hard-coded table of about 50 extensions; unknown files
  get `application/octet-stream`.
- **`.desktop` parsing**: only the `[Desktop Entry]` group; the last value wins.
  - App directories: `~/.local/share/applications`, `/usr/share/applications`,
    and `$XDG_DATA_DIRS/applications`.
- **`listApps(path, all)`**:
  - Default mode: apps whose `MimeType` includes the file's MIME. `all` mode:
    every visible Application.
  - Entries with `NoDisplay`, `Hidden` or a non-Application `Type` are excluded;
    duplicates by Name are removed.
  - The default handler comes from `xdg-mime query default <mime>`; it is sorted
    first, then the rest by name.
  - Icons are resolved from pixmaps and a fixed list of themes (hicolor, breeze,
    Adwaita, Papirus, …).
- **`execFor(Exec, file)`**: `%f %F %u %U` → the file; other field codes are
  removed; quotes are honoured.
- **`launchApp(desktopPath, file)`**: detached spawn.
- **`openWithDefault`**:
  1. Launch the default `.desktop` directly. Electron's `shell.openPath` reports
     success even when `xdg-open` fails, so it is avoided.
  2. Otherwise try `gio open`, `xdg-open`, `kioclient5 exec`, `kde-open5` in
     turn. Success means the process exited 0 or was still alive after 1.2 s.
- **`setDefaultApp`**: only apps that `listApps` knows; runs
  `xdg-mime default <x.desktop> <mime>`.
- **`isDefaultFileManager`**: `xdg-mime query default inode/directory` contains
  `max-file-manager`.
- **`setDefaultFileManager`** (invasive):
  - Writes `~/.local/share/applications/max-file-manager.desktop` (MimeType
    `inode/directory; x-scheme-handler/file; application/x-gnome-saved-search`).
  - Writes D-Bus service files `org.freedesktop.FileManager1.service` and
    `org.kde.dolphin.FileManager1.service` in `~/.local/share/dbus-1/services`,
    both pointing at `filemanager1-bridge.py`.
  - **Runs `systemctl --user stop` and then `mask` on
    `plasma-dolphin.service`.**
  - Spawns the bridge.
  - Runs `xdg-mime default` for `inode/directory` and `x-scheme-handler/file`.
- The Windows (PowerShell registry) and macOS (osascript/LaunchServices) paths
  are irrelevant here.

#### Tests pin down

- The parser reads only the Desktop Entry group; action groups do not leak in.
- MIME claim matching, with NoDisplay, Hidden and Type=Link excluded.
- Exec tokenization (quoted path with spaces, `%i %c %k` dropped, `%%`).
- MIME map (`PHOTO.JPG` → `image/jpeg`; docx; unknown →
  `application/octet-stream`).
- `systemOpenCommand` returns null on Linux.
- Windows and mac parsers (ignore).

#### KDE/Qt replacement

- KApplicationTrader::queryByMimeType / preferredService / setPreferredService.
- KIO::ApplicationLauncherJob, KIO::OpenUrlJob (default open), KOpenWithDialog.
- QMimeDatabase instead of the extension table.
- Default file manager: install a proper `.desktop` and set the
  `inode/directory` association through KApplicationTrader / mimeapps.list. Do
  not mask Dolphin.

### filemanager1-bridge.py

#### Capabilities

- Python + GLib (gi) service. Owns `org.freedesktop.FileManager1` on the session
  bus with the REPLACE flag, at `/org/freedesktop/FileManager1`.
- Methods `ShowFolders`, `ShowItems` and `ShowItemProperties` take
  `(as uris, s startupId)`. All three decode `file://` URIs and spawn
  `max-file-manager <path>` (or `electron <repo>` in dev), one process per URI.
  Single-instance forwarding does the rest.
- ShowItems therefore passes the file path itself; the renderer handles
  revealing it. ShowItemProperties does not open a properties dialog.

**KDE/Qt replacement**: implement FileManager1 in-process with
QDBusAbstractAdaptor, as Dolphin's `DBusInterface` does. Register through
KDBusService and an `org.freedesktop.FileManager1.service` activation file.
Implement ShowItems as open-parent-and-select, and ShowItemProperties as
KPropertiesDialog.

### desktop.ts

#### Capabilities

- **Terminal here**: the first that works of `konsole --workdir`,
  `gnome-terminal --working-directory`, `xfce4-terminal`,
  `x-terminal-emulator -e bash -l`. Detached; success means the process spawned.
- **Reveal in the system file manager**: Electron `shell.showItemInFolder`.
- **Drag-out**: `webContents.startDrag({file, files[], icon})` produces a native
  OS drag (`text/uri-list`) to Dolphin, Telegram and browsers.
- **Custom command**: `sh -c` with `%f`/`{path}` and `%d`/`{dir}` quoted; if
  there is no placeholder, the path is appended. cwd is the file's folder;
  detached, fire-and-forget.

**KDE/Qt replacement**: KTerminalLauncherJob; KIO::highlightInFileManager (here
that is the app itself); QDrag with `QMimeData::setUrls` / KUrlMimeData;
KIO::CommandLauncherJob or QProcess::startDetached.

### fileclip.ts + wayland-clip/ (C helper)

**Capabilities**: puts a file list on the system clipboard (copy or cut) even
when the app window is unfocused.

- **On Wayland**:
  - Spawns the `wayland-clip` binary, built by `npm run build` with gcc and
    `-lwayland-client` from the bundled `ext-data-control-v1` and
    `wlr-data-control-unstable-v1` protocol sources.
  - It owns the selection and offers `text/uri-list`,
    `x-special/gnome-copied-files` (`copy|cut\n` + URIs),
    `application/x-kde-cutselection` (0/1), and `text/plain` (newline-separated
    paths).
  - When exactly one image is copied it also serves the image bytes under
    `image/png|jpeg|webp|gif|bmp`.
- **Otherwise**: falls back to a PySide6 helper run with `python3 -c` (QMimeData
  with the same formats).
- **Helper protocol**:
  - stdin carries JSON lines `{paths, cut}` or `{clear:true}`; the helper
    answers `ready`, `ok` or `err`.
  - The helper keeps serving for 10 minutes after the app exits.
  - Its PID is written to `userData/clipboard-owner`. A stale owner is killed
    only after `/proc/<pid>/cmdline` confirms it is our helper.
  - If no helper is available, the paths go to the clipboard as plain text.
  - `releaseFiles` clears the selection before the renderer writes an image.

**KDE/Qt replacement**: QClipboard + QMimeData::setUrls +
`KIO::setClipboardDataCut` / `KUrlMimeData`. A native, focused Qt window can
write the clipboard directly, so the helper is unnecessary. Klipper keeps the
data after the app exits.

### portal.ts and friends (XDG file-chooser backend)

Files: portal.ts, portal-core.ts, portal-install.ts, shared/portal-filters.ts;
tests: portal-core.test.ts.

#### Capabilities

- MFM can act as the system **file picker** for other apps.
- **D-Bus service**:
  - Name `org.freedesktop.impl.portal.desktop.mfm`, object
    `/org/freedesktop/portal/desktop`, interface
    `org.freedesktop.impl.portal.FileChooser`.
  - Methods `OpenFile`, `SaveFile` and `SaveFiles`, signature `osssa{sv}` →
    `ua{sv}`.
  - A per-request `org.freedesktop.impl.portal.Request.Close` at the handle path
    cancels the request.
- **Options parsed**: multiple, directory, current_folder / current_file /
  current_name (`ay` NUL-terminated byte paths), accept_label, filters
  `a(sa(us))` (glob=0, MIME=1; at most 50 filters × 100 rules), and `files` for
  SaveFiles.
- **Picker window**: each request opens a picker window (`?portal=1`) whose
  renderer reads `portal:request` and answers through `portal:respond`.
- **Response handling**:
  - Validates absolute paths, single versus multiple selection, and (for open)
    that each path exists and is a file or directory as requested.
  - Returns `uris` and `current_filter`. Cancel or window close returns
    response 1.
  - The window is destroyed 80 ms after the reply so the reply reaches the bus.
- **Export order**: the object is exported before the name is requested, so a
  call arriving right after activation does not fail.
- **Install** (writes user-scope files atomically, then runs
  `systemctl --user restart xdg-desktop-portal.service`):
  - `$XDG_DATA_HOME/xdg-desktop-portal/portals/mfm.portal`
  - `$XDG_DATA_HOME/dbus-1/services/<busname>.service`, with
    `Exec=<APPIMAGE|execPath> [appPath] --portal-backend` quoted GLib-style.
  - `$XDG_CONFIG_HOME/xdg-desktop-portal/portals.conf`:
    `[preferred] org.freedesktop.impl.portal.FileChooser=mfm;<existing…>`.
- **Uninstall**: removes only the `mfm` token and deletes the config file only
  if it would be empty.
- **Headless mode**: `--portal-backend` runs with no main window.
  `prefs.portalPicker` also starts the server at boot. The last picker folder is
  remembered in `prefs.pickerLastDir`.
- **Filter matching** (`matchesPortalFilter`): glob with `* ? [..] [!..]`,
  case-insensitive; MIME rules match exact types or `type/*` via an
  extension→MIME table.

#### Tests pin down

- `portalPath` decodes NUL-terminated bytes and rejects relative paths.
- Glob wildcards and character classes work, and `+` is literal.
- Glob and MIME filters both work.
- `portals.conf` edits are surgical: they prepend `mfm;`, keep `default=kde;`,
  are idempotent, and disabling restores the original. A `[preferred]` section
  is created if missing; disabling an empty file yields an empty file.
- dbusExecQuote handles spaces and `'`; `fileUri` escapes spaces.

**KDE/Qt replacement**: a QDBusAbstractAdaptor implementing
`impl.portal.FileChooser` around the app's own picker dialog
(xdg-desktop-portal-kde's FileChooser is the reference). Matching:
QMimeDatabase + `QRegularExpression::wildcardToRegularExpression`.

### preview.ts (+ hash tests); tests: preview.test.ts

#### Capabilities

- Preview kinds:
  - `dir`
  - `image` (png jpg jpeg gif webp svg bmp avif ico)
  - `pdf`
  - `audio` (mp3 flac wav ogg m4a opus)
  - `video` (mp4 mkv webm mov avi)
  - `office`: text extracted from docx, xlsx (first sheet with shared strings,
    tab-separated), pptx (slides in order), and odt/ods/odp (content.xml);
    capped at 200 KB.
  - `text`: first `maxKB` (default 200, clamped 16..8192); a NUL in the first 4
    KB means binary; `capped` flag.
  - `binary`
- For media types main returns only the kind; the renderer loads the file via
  `mfm://`.
- The docx extractor turns paragraphs into lines, table cells into tabs, and
  keeps `w:tab` and `w:br`. It drops `instrText` and `delText` and decodes XML
  entities.

#### Tests pin down

- Detection of text, binary (NUL), image and dir.
- A 300 KB log is capped to exactly 200 KB with `capped=true`.
- sha256 of "hello" is stable; a missing file gives a null hash.
- docx: paragraphs, hard breaks, table cells become `a\tb`, field codes and
  deleted text are dropped, entities are decoded.
- xlsx: shared strings and rich runs are joined; rows become tab-separated
  lines.

#### KDE/Qt replacement

- KIO::PreviewJob for images and video thumbnails.
- KFileMetaData::ExtractorCollection with `ExtractPlainText` for office text.
- KParts (Okular part for PDF, Gwenview part, KTextEditor part) for an embedded
  viewer.
- QMediaPlayer for audio and video.

### thumbs.ts

#### Capabilities

- Thumbnails with the longest side 32..512 px (default 256), PNG.
- Cache key: sha1(path|mtime|size|side). A cache hit touches the file's mtime so
  pruning works as LRU.
- Decoding uses Electron nativeImage (synchronous, on the main thread); macOS
  uses `sips` with 3 in parallel.
- Prune runs every 50 writes: at most 4000 files and 300 MB, oldest first.
- `clearThumbs` returns `{removed}`.

**KDE/Qt replacement**: KIO::PreviewJob, which uses the freedesktop shared cache
in `~/.cache/thumbnails` and the KIO thumbnailer plugins (video, PDF, office,
fonts, …). Clearing becomes a matter of `~/.cache/thumbnails` or is dropped.

### hash.ts

**Capabilities**: streamed digest of any Node/OpenSSL algorithm (default
sha256), returning `[{path, hash|null}]`. **KDE/Qt replacement**:
QCryptographicHash (md5, sha1, sha2, sha3, blake2 in Qt 6).

### breakdown.ts (folder content charts); tests: breakdown.test.ts

#### Capabilities

- Modes: `all` (recursive) or `level1` (only the folder itself).
- Returns:
  - totals: bytes, files, folders, scanned, `capped`;
  - `byExt` (count and bytes per extension, top 80, sorted by bytes);
  - `largest` (top N files, default 15);
  - `children` (bytes and files per direct child folder).
- Symlinked directories are not descended into; files are stat'ed.
- Budgets: `maxEntries` 250k (clamped 1k..2M) and `budgetMs` 6 s (clamped
  0.2..60 s).

#### Tests pin down

- In `all` mode: totals 5 files, 3 folders, 3900 bytes; jpg count and bytes;
  order of the largest files; child bytes, with the biggest child first.
- In `level1` mode only the folder itself is counted, and child folders appear
  with 0 bytes.
- `topFiles` limit.
- An empty folder gives zeros.
- Exceeding maxEntries sets `capped` and scanned ≤ cap.

**KDE/Qt replacement**: custom QDirIterator / KIO::listRecursive worker
(Filelight-like). KIO::DirectorySizeJob covers only the totals.

### sync.ts (folder sync and file diff); tests: sync.test.ts

#### Capabilities

- **`compareTrees(left, right, deep, {compareBy: mtime|ctime|hash, ignore[]})`**:
  - Recursive lstat walk that skips `.mfm.json`.
  - Row states:
    `same, leftOnly, rightOnly, leftNewer, rightNewer, sizeDiff, contentDiff, typeDiff`.
  - Times differing by 2 s or less count as equal; a size difference with equal
    times is `sizeDiff`, which is ambiguous.
  - The hash mode re-checks file contents with sha256.
  - At most 20k rows; ignore patterns (≤50) match the base name or the relative
    path.
- **`planRows(rows, ltr|rtl, deleteExtra)`** and **`applySync`**:
  - Copies with `fs.copyFile` (mkdir -p first).
  - `typeDiff` rows become errors.
  - Extra files are deleted only through the injected trash function; this
    module never hard-deletes.
  - Returns `{copied, deleted, skipped, errors}`.
- **`diffFiles(a, b)`**:
  - Binary files report the first differing byte offset.
  - Text files use an LCS line diff with line numbers (`same/add/del`).
  - If lines(a) × lines(b) > 400k, a positional "coarse" diff is used instead.

#### Tests pin down

- Classification of same, leftOnly, rightOnly and leftNewer; a folder present on
  both sides is `same`.
- planRows direction for ltr and rtl, including deleteExtra.
- applySync copies 2 and trashes 1 through the callback; a second compare is
  clean.
- diffLines numbering.
- Coarse fallback at 900×900 lines.
- An equal-time `sizeDiff` is offered in both directions.

**KDE/Qt replacement**: custom logic (Krusader's Synchronizer is the model);
KIO::copy / KIO::trash to apply. For diffs, libkomparediff2 / Kompare KPart or a
custom Myers diff.

### editor.ts

#### Capabilities

- `readTextFile`: at most 8 MB; NUL in the first 4 KB means "not text".
- `writeTextFile`: atomic (tmp file `p.mfm-tmp-<pid>` → rename, keeping the
  original mode); at most 8 MB.
- `htmlToPdf(html, out, theme)`: an offscreen BrowserWindow with JavaScript
  disabled. Embeds the Vazirmatn woff2 fonts (read from `cwd/node_modules`,
  which is fragile), A4 page, 18/16 mm margins, RTL document, LTR code blocks,
  highlight.js colors for light and dark themes.

**KDE/Qt replacement**: QSaveFile; KTextEditor::Document / KSyntaxHighlighting;
QTextDocument + QPdfWriter (or QWebEngine `printToPdf`) for PDF.

### export.ts + shared/export.ts (listing export); tests: shared/export.test.ts

#### Capabilities

- Rows: `{name, size, mtime, type, label, rating, note}`.
- CSV: UTF-8 BOM, CRLF line endings, header
  `name,size,modified,type,label,rating,note`, quoting when a value contains
  `,`, `"`, newline or `;`, ISO dates.
- PDF: an HTML table (human-readable sizes, ★ ratings, escaped values) rendered
  through htmlToPdf.
- The output file is `freeName(dir/<sanitized name>.csv|.pdf)`.
- `rowsOf` merges the listing with the folder metadata.

**Tests pin down**: BOM, quoting and doubled quotes; HTML escaping, stars,
`2.0 KB`; the metadata join.

**KDE/Qt replacement**: QFile/QTextStream; QTextDocument → QPdfWriter.

### convert.ts + shared/bmp.ts (batch image conversion); tests: shared/bmp.test.ts

#### Capabilities

- Converts to jpeg, png or bmp.
- `maxSide` resizes the longest side; `quality` 1..100 applies to jpeg; output
  goes to `dest` or next to the source with a free name; at most 500 files.
- WebP output is refused in main (only the renderer canvas can produce it).
- The BMP encoder writes 24-bit bottom-up rows with 4-byte padding, composites
  alpha over white, and treats a size of 0 as 1×1.

**Tests pin down**: BMP header fields, row order and BGR padding, alpha
compositing, the zero-size case.

**KDE/Qt replacement**: QImage / QImageWriter (with kimageformats, WebP, AVIF,
HEIF and more work). This removes the WebP limitation.

### image.ts (gallery editor saves); tests: image.test.ts

#### Capabilities

- `saveImage(src, dataUrl, overwrite|copy)`:
  - Accepts png, jpeg, webp and bmp data URLs.
  - copy writes `<base>-edit.<ext>` with a free name; overwrite replaces
    atomically through a `.mfm-save-*` temp file.
- `saveImageAs(src, dataUrl, dest?)`: writes `<base>.<ext>` with a free name
  into `dest` or next to the source.

#### Tests pin down

- Copy creates a sibling `photo-edit.png` and leaves the original untouched; a
  second copy gets `photo-edit (2).png`.
- Overwrite writes in place and leaves no temp file.
- A jpeg data URL produces `.jpg`.
- Bad data and relative paths are refused.

**KDE/Qt replacement**: QImage + QSaveFile. Rotate, flip, crop and resize are
QImage transforms.

### metadata.ts (ID3 and EXIF); tests: metadata.test.ts

#### Capabilities

- **Read** (`readTags`):
  - Whole-file read for files ≤60 MB.
  - ID3v2.2/2.3/2.4 text frames: title, artist, album, year, track. Encodings:
    latin1/utf8, UTF-16, UTF-16BE.
  - JPEG EXIF (IFD0 + ExifIFD): camera (make + model), date (normalized to
    `YYYY-MM-DD hh:mm:ss`), ISO, f-number, exposure, focal length.
  - The tags also feed the batch-rename tokens
    `{title} {artist} {album} {year} {date} {camera}`.
- **Write** (`writeId3`): rewrites an ID3v2.3 UTF-8 tag (TIT2, TPE1, TALB, TYER,
  TRCK; ≤200 characters each). The old tag, including its footer, is dropped and
  the audio bytes are copied untouched. Atomic tmp → rename.

**Tests pin down**: replacing a tag keeps the audio bytes and round-trips
Persian values; a tag is created when none existed.

**KDE/Qt replacement**: KFileMetaData (ExtractorCollection for reading;
WriterCollection with the TagLib writer for writing), or TagLib and libexiv2
directly.

### drives.ts (removable media); tests: drives.test.ts

#### Capabilities

- `listDrives`: runs
  `lsblk -J -o NAME,LABEL,MOUNTPOINT,RM,HOTPLUG,SIZE,TYPE,FSTYPE,MODEL,VENDOR,TRAN`.
  - Keeps removable, hotplug or USB devices, or devices with a child mounted
    under `/media` or `/run/media`.
  - Skips swap and LUKS containers.
  - Display name: label, else "model (size)", else "USB sdX (size)".
- `mountDrive` / `unmountDrive`:
  `udisksctl mount|unmount -b /dev/X --no-user-interaction`.
  - The mount point is parsed from the output; "already mounted" is treated as
    success.
  - Unmount accepts a mount point and maps it back to the device.
  - Device paths are validated against `^/dev/[A-Za-z0-9_-]+$`.

#### Tests pin down

- Empty or invalid JSON returns `[]`.
- An internal NVMe disk is skipped.
- USB partitions are named by label or by "model (size)", with correct mounted
  flags and fs types.
- An unpartitioned USB stick is listed as the whole disk.

**KDE/Qt replacement**: Solid (StorageVolume / StorageAccess
`setup()`/`teardown()`, `Solid::OpticalDrive::eject`) with KFilePlacesModel;
`Solid::DeviceNotifier` gives hotplug notifications, which the current code
lacks.

### launch-path.ts; tests: launch-path.test.ts

**Capabilities**: picks the first absolute or `file://` argument from argv,
skipping flags and skipping the app directory once (the dev launcher passes
`electron <appDir> %U`).

**Tests pin down**: the app dir alone gives null; a packaged launch takes the
first argument; `file://` URIs are percent-decoded; flags are ignored; the app's
own folder can still be opened explicitly when given twice.

**KDE/Qt replacement**: QCommandLineParser + `QUrl::fromUserInput`; KDBusService
`activateRequested(args, cwd)`.

### window-bounds.ts; tests: window-bounds.test.ts

**Capabilities**: `fitWindow` restores a saved rectangle if at least 120×80 px
of it is on some display's work area; it is clamped inside that area. Otherwise
the saved size is centred on the primary display. With no saved rectangle, the
default size is centred.

**Tests pin down**: no saved bounds gives centred 1320×860; exact restore on the
primary or second display; an unplugged display recentres; a rectangle half off
the edge is pulled back; an oversized rectangle is clamped; a sliver counts as
gone.

**KDE/Qt replacement**: KWindowConfig::saveWindowSize / restoreWindowSize.
Position is mostly ignored on Wayland.

### updates.ts + updates-core.ts; tests: updates.test.ts

#### Capabilities

- Fetches
  `GET https://api.github.com/repos/majidasgari/max-file-manager/releases`,
  cached for 1 h.
- Picks the highest non-draft, non-prerelease tag; numeric dotted comparison
  with a leading `v` and a `-suffix` tolerated; notes limited to 600 characters.
- `openRelease` only opens URLs under
  `https://github.com/majidasgari/max-file-manager/`.

**Tests pin down**: 1.2.10 > 1.2.9; the `v` prefix; equal versions; `-beta` is
not newer; missing segments count as 0; drafts and prereleases are skipped.

**KDE/Qt replacement**: QNetworkAccessManager, or drop it for distro packaging.

### mcp.ts (agent API, off by default)

#### Capabilities

- Loopback HTTP server on `127.0.0.1:<port>`:
  - `GET /tools` lists the tools; `POST /call {tool, args}` runs one.
  - Requires a Bearer token (timing-safe comparison).
  - Rejects requests with any `Origin` header and any `Host` other than
    `127.0.0.1:port` / `localhost:port`.
  - Body limit 4 MB.
- **Modes**: `off`, `read` (only tools marked read), `write`.
- **Audit log**: tool name and argument shapes only (text becomes `<n chars>`,
  arrays `[n]`, strings truncated to 160).
- A listen failure is logged, not shown as a dialog.
- The stdio bridge is `bin/mfm-mcp.mjs` (outside the scope of this inventory).

#### Tools (about 91)

- Read:
  - roots, drives_list, list, dirs, walk, properties, statfs, size, breakdown,
    hash, dupes
  - search, search_stats, search_status, search_sizes
  - preview, read_text, archive_list, diff, sync_compare, tags
  - meta_get, labels, folder_look, icons
  - jobs_list, undo_depth, state, mcp_status
  - apps, app_default, desktop_default_manager, portal_status, update_check,
    image_get
- Write: write_text, mkdir, create_file, rename, rename_many, copy, move, trash,
  delete (needs `confirm:true`), symlink, chmod, extract, create_zip,
  tags_write, export_listing, pdf, image_save, image_save_as, image_convert,
  clipboard_image, thumbs_clear, label_set, file_color_set, rating_set,
  note_set, meta_set, sort_set, pin_set, job_start, job_pause, job_resume,
  job_cancel, job_clear, undo, sync_apply, run_script, run_custom_command,
  search_index (`wait:false` returns immediately), search_clear, prefs_set,
  settings_export, settings_import, open, reveal, terminal, apps_launch,
  app_set_default, set_default_manager, update_open, hotkey, drive_mount,
  drive_unmount.
- Forwarded to the UI (`mcp:ui`): command, goto, select, filter, tab (new,
  close, next, prev, reopen, select, rename, lock, color), bookmark (list is
  answered in main), open_editor, open_gallery, open_preview, properties_dialog,
  note. `window` (minimize, maximize, close, show, fullscreen, chrome) is
  handled in main.

**KDE/Qt replacement**: QHttpServer (Qt 6.4+) or QTcpServer bound to
QHostAddress::LocalHost.

### types.ts

Shared types: `Entry` fields: name, path, dir, size, mtime, ext, and optional
group, color, icon, mode, uid, gid, nlink, w, h, hidden, link. `DirRef`,
`Listing`, and `Result {done, errors[]}`.

---

## Preload IPC surface (`window.api`)

All calls are `invoke` unless marked otherwise.

### Filesystem: listing and inspection

- `fs:list(dir, showHidden, dims)`: entries plus pins, labels, ratings, notes,
  format.
- `fs:watch(dirs[])`: replaces the set of watched folders.
- `fs:dirs(dir, showHidden, followLinks)`: subfolders for the tree.
- `fs:roots`, `fs:home`, `fs:statfs(p)`, `fs:size(p)`: roots, home folder,
  free/total space, recursive size.
- `fs:breakdown({path, mode})`.
- `fs:properties(p)`, `fs:chmod({path, mode})`.
- `fs:walk(dir, opts)`, `fs:dupes(dir)`: flat view and search; duplicate finder.
- `fs:preview(p)`, `fs:readText(p)`, `fs:writeText({path, text})`,
  `fs:readBytes({path, max})`, `fs:imageData(p)`.
- `fs:hash(paths, algo)`, `fs:diff({a, b})`, `fs:tags(paths)` (at most 200
  paths).
- `debug:log(msg)`: drag debug log.

### Filesystem: mutations

- `fs:remove({paths, permanent})`: trash or permanent delete.
- `fs:mkdir(dir, name)`, `fs:createFile(dir, name, content)` (undoable).
- `fs:symlink(target, link)`, `fs:renameMany(pairs)` (undoable).
- `fs:archive(p)`: list an archive;
  `fs:archiveCreate({dir, name, sources, level})`;
  `fs:extract({archive, names, dest})`.

### Jobs and undo

- `job:start({kind, sources, dest, opts})`, `job:pause(id)`, `job:resume(id)`,
  `job:cancel(id)`, `job:list`, `job:clear`.
- `undo:run`, `undo:depth`.

### Folder metadata (.mfm.json)

- `meta:get(dir)`, `meta:set({dir, meta})`, `meta:setFormat`, `meta:setRating`,
  `meta:setNote`, `meta:setLabel`, `meta:setEntryColor`, `meta:togglePin`,
  `meta:labels`.
- `meta:writeTags({path, fields})`: writes ID3 tags.

**Icons**: `icons:themes(extra)`, `icons:in(themeDir)`, `icons:set`,
`icons:folderLook(p)`.

**Images**: `image:save({src, dataUrl, mode})`,
`image:saveAs({src, dataUrl, dest})`, `image:batchConvert({files, opts})`,
`thumbs:clear`.

**Export and PDF**: `export:listing({dir, name, rows, format, theme})`,
`pdf:html({html, out, theme})`.

**Sync**: `sync:compare({left, right, deep?, compareBy?, ignore?})`,
`sync:apply({left, right, rows, mode, deleteExtra})`.

**Editor drafts**: `drafts:get(p)`, `drafts:set({path, text})`,
`drafts:clear(p)`.

**Search index**: `search:index(cfg)`, `search:query({query, limit})`,
`search:stats`, `search:status`, `search:sizes(dirs)`, `search:clear`.

### Apps and desktop

- `shell:open(p)`: open with the default app.
- `apps:list(p)`, `apps:openWith({path, all})` → `{mime, canSetDefault, apps}`,
  `apps:setDefault({path, id})`, `apps:default(p)`, `apps:launch(id, p)`.
- `desktop:isDefaultFileManager`, `desktop:setDefaultFileManager`.
- `app:terminal(dir)`, `app:reveal(p)`, `app:runScript({command, path, dir})`,
  `app:runCustomCommand({command, path})`.
- `app:dragOut(files)` (**send**, not invoke).
- `hotkey:register(accel)`.

**Clipboard**: `clipboard:copyFiles({paths, cut})` → `'files'|'text'`,
`clipboard:releaseFiles`, `clipboard:writeText(text)`,
`clipboard:saveImage({dir, name, data})`.

**Drives**: `drives:list`, `drives:mount(device)`,
`drives:unmount(deviceOrMount)`.

### App, window and state

- `app:launchPath`, `app:openViewer({kind: gallery|editor, path})`.
- `window:control(minimize|maximize|close)` (acts on the sender's window),
  `window:isMax`, `window:fullscreen(on)`, `window:getChrome`,
  `window:setChrome({customTitlebar, rounded})` (rebuilds the window),
  `window:setTray({minimize, close, show, quit})`.
- `state:get`, `state:set(s)`, `state:setPrefs(prefs)`,
  `state:patchPrefs(patch)`, `state:marks({path, on}?)`.
- `settings:export({prefs})` and `settings:import`: native save/open dialogs.

**Updates**: `update:check(force)`, `update:open(url)`.

**MCP**: `mcp:status`, `mcp:configure({mode, port})` (restarts the server),
`mcp:token` (generates a new token and restarts).

**Portal**: `portal:request` (the request for this window),
`portal:respond(result|null)`, `portal:rememberDir(dir)`, `portal:status`,
`portal:configure(enabled)` (install or uninstall plus start or stop).

**Non-IPC**: `pathForFile(File)` uses `webUtils.getPathForFile` to turn a
dropped file into a path.

**Events (main → renderer)**: `fs:changed(dir)`, `job:update(job)`,
`search:progress(status)`, `mcp:ui({tool, args, …})` (buffered in preload until
the renderer subscribes), `app:open-path(p)`, `window:maximized(bool)`,
`state:prefs-patch(patch)`, `state:marks(marks[])`.

---

## Bugs and quirks to note (do not replicate unknowingly)

1. **`window:setChrome` is broken.** The handler re-runs
   `protocol.handle('thumb')` and `ipcMain.handle('thumbs:clear')` inside
   itself; both throw on a second registration, so the window rebuild that
   follows them is never reached. It looks like a bad merge (index.ts
   ~1667–1692).
2. **`renameMany` can overwrite.** It uses plain `rename`, which silently
   replaces an existing target on Linux, although the MCP tool claims "never
   overwrites". Collision checks exist only in the renderer.
3. **Move is always copy + delete**, with no same-filesystem rename. Cancelling
   a job leaves partial folders.
4. **Copy, trash, delete and extract are not undoable**; the undo depth is 20.
5. **`.tar.bz2` and `.tbz2` are advertised but fail.** Zip entry mtimes are
   lost. The whole archive is held in memory (256 MB cap).
6. **`setDefaultFileManager` masks `plasma-dolphin.service`** and points
   Dolphin's FileManager1 activation file at the Python bridge.
7. **The FileManager1 bridge does not reveal items.** ShowItems and
   ShowItemProperties just open the path, one process per URI.
8. **`openWithDefault` falls back to the KF5-era `kioclient5` and `kde-open5`.**
9. **`readTags` reads whole files (≤60 MB); thumbnails decode synchronously** on
   the main thread on Linux.
10. **Hard caps a native app could raise:** listing dims for 400 images, 1000
    folders checked for meta, folder size 200k entries / 4 s, walk 5000 results
    / 200k scanned / 4 s, dupes 4000 entries, sync 20k rows, diff LCS up to 400k
    cells, editor 8 MB, preview text up to 8 MB.

## Main files

- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/index.ts`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/preload/index.ts`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/jobs.ts`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/mcp.ts`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/portal*.ts`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/fileclip.ts`
  and `src/main/wayland-clip/wayland-clip.c`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/main/filemanager1-bridge.py`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/src/renderer/src/prefs.ts`
  (the full Prefs schema and defaults, outside this scope)
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/scripts/install-integration.sh`
- `/home/mikepadiernos/Library/Projects/personal/max-file-manager/docs/`
  (FEATURES.md, PREFERENCES.md, MCP.md, INTEGRATION.md, GAPS.md)
