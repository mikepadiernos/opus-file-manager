# Caveats and limitations

This document describes current limitations and integration considerations.
Links beside tracked caveats lead to their project issues. For planned work
and completion status, see the [roadmap](ROADMAP.md).

## Archives

- **Local files:** Archive creation and integrity testing require local files.
  When compressing a mixed selection, Opus skips items without local paths.
  Password-protected extraction also requires a local archive and destination.
  ([#265][issue-265])
- **Password-protected archives:** Creation supports AES-encrypted ZIP only
  and rejects symbolic links and other unsupported file types. ZIP entry names
  are not encrypted. Extraction rejects links, unsafe paths and unsupported
  entry types. If creation or extraction fails, a partial archive or extraction
  folder may remain. ([#255][issue-255], [#256][issue-256])
- **Encrypted 7z:** Extraction uses KArchive and can fail even for valid
  archives. Integrity testing uses the `7z` executable when available and
  otherwise falls back to KArchive. Passwords containing a newline, carriage
  return or NUL cannot be passed to `7z`.
  ([#36][issue-36], [#255][issue-255])

### Possible Ark integration

Opus does not currently integrate with Ark. An optional handoff could improve
encrypted 7z compatibility through Ark's dedicated `7z` backend, but would
require Ark and `7z` at runtime as well as error, destination and overwrite
handling. Ark's
[7z plugin](https://github.com/KDE/ark/blob/master/plugins/cli7zplugin/cliplugin.cpp)
defines a `-p$Password` command-line switch. Verify the installed version and
operation before assuming passwords are not exposed in process arguments.
([#313][issue-313])

## Folder metadata and listings

- **Metadata compatibility:** Opus supports a subset of Max File Manager's
  local `.mfm.json` metadata. Writing sanitizes the file and may discard
  unsupported fields or values, so back up existing metadata before editing.
  The sanitizer retains the upstream hide-from-parent flag, but Opus does not
  apply it to listings. ([#257][issue-257], [#258][issue-258])
- **Metadata limits:** Metadata files are limited to 4,096 bytes, notes to
  500 characters and pins to 200 per folder. Folder-format rules are stored
  in local metadata; remote locations use defaults unless the pane's format
  is locked. ([#263][issue-263])
- **Flat view:** The `filenamesearch:/` KIO worker is required and is normally
  supplied by `kio-extras`. Create New, New Folder and Paste are disabled,
  and drops are refused while flat view is active. ([#261][issue-261])
- **Folder sizes:** Background calculations skip symbolic links to
  directories. Calculated sizes are snapshots and can become stale when
  changes occur outside watched directories. View settings offer optional
  periodic refresh for visible panes; it is off by default. Automatic
  calculation and periodic refresh can be costly, especially for remote
  locations. ([#262][issue-262])

## Previews and gallery

- **Preview dependencies:** Document previews require a suitable installed
  KPart, media playback requires a Qt Multimedia backend, and available
  metadata depends on KDE extraction plugins. ([#259][issue-259])
- **Preview limits:** Images are limited to 8 MiB and 40 megapixels. Text
  previews use UTF-8 and default to the first 1 MiB; lines longer than 16,384
  characters are shown without syntax highlighting. Remote previews download
  to a temporary file and use the same limits, or 128 MiB for other file
  types. ([#259][issue-259], [#264][issue-264])
- **Rendered previews:** HTML and Markdown previews disable links and
  external resources. ([#267][issue-267])
- **Gallery:** The gallery supports local files only. Gallery images and batch
  conversion inputs are limited to 128 MiB and 40 megapixels. Saving edits or
  converting images does not preserve the original image metadata.
  ([#266][issue-266], [#260][issue-260])
- **Batch conversion:** Conversion reads files on disk, not unsaved gallery
  edits, and turns animated images into still images. The conversion dialog
  warns about both limitations. ([#303][issue-303])
- **Conversion output:** The destination filesystem must support atomic
  no-replace renames, which prevent overwriting files, including those
  created after preview. Unsupported filesystems cause conversion failures.
  Converted files have owner-only permissions; change permissions if needed
  before sharing them. ([#302][issue-302], [#304][issue-304])

## Editor

- **Draft storage:** Drafts are limited to 8 MiB of UTF-8 text and are stored
  in the application's private data directory, not in the original file. If
  draft storage fails, save the document before switching files or closing.
  ([#307][issue-307])
- **Draft retention:** Drafts are retained indefinitely by default. An
  optional age limit removes expired drafts unless they are locked by an
  active editor. Legacy drafts from versions without locking remain until
  explicitly discarded. Restored drafts remain until discarded or expired,
  protecting another editor's changes during recovery. ([#307][issue-307])
- **Markdown resources:** Markdown preview, visual editing and PDF export do
  not load local or external resources. Links are also disabled in the
  preview. ([#267][issue-267])
- **Visual Markdown editing:** Qt's Markdown subset can normalize the source
  or remove unsupported markup and HTML. Visual editing requires confirmation
  and preserves a recovery snapshot when drafts are enabled. ([#309][issue-309])

## File comparison

File comparison requires the `diff` executable and two local regular files.
Unified output is decoded as UTF-8, and comparison stops if output exceeds
4 MiB. Binary differences are reported without a visual binary diff.
([#311][issue-311])

## Symbolic links

Symbolic-link creation supports existing local files and folders only. The new
link is created beside its target; another destination cannot be chosen.
([#312][issue-312])

## Listing export

- Listing export uses the current filtered rows and configured details columns
  with their displayed values, even in icon or compact view. Output is local
  only, and wide PDF tables may wrap or clip. CSV prefixes cells that could be
  interpreted as spreadsheet formulas with an apostrophe.

## Folder content chart

- The chart shows measured logical sizes by immediate entry, not kind or
  extension breakdowns or a largest-file drilldown. It includes hidden
  entries, skips top-level symbolic links and entries without a known size,
  and relies on KIO workers for recursive traversal. Scans are cancellable but
  have no entry or time limit and can be costly; results are snapshots.
  ([#315](https://github.com/mikepadiernos/opus-file-manager/issues/315))

## Build, installation and references

- **Dependencies:** Opus targets Linux and requires the dependencies listed in
  [`CMakeLists.txt`](../CMakeLists.txt), including libarchive.
- **Installer:** `install.sh --deps` prints suggested packages but does not
  install them. The installer disables tests in its build tree unless
  reconfigured with `-DBUILD_TESTING=ON`. `install.sh --default` changes the
  default folder handler.
- **Parity inventories:** The [inventories](README.md#behaviour-references)
  describe a reviewed Max File Manager checkout, not necessarily current Opus
  behaviour. Do not assume upstream bugs or limitations apply to Opus.

[issue-36]: https://github.com/mikepadiernos/opus-file-manager/issues/36
[issue-255]: https://github.com/mikepadiernos/opus-file-manager/issues/255
[issue-256]: https://github.com/mikepadiernos/opus-file-manager/issues/256
[issue-257]: https://github.com/mikepadiernos/opus-file-manager/issues/257
[issue-258]: https://github.com/mikepadiernos/opus-file-manager/issues/258
[issue-259]: https://github.com/mikepadiernos/opus-file-manager/issues/259
[issue-260]: https://github.com/mikepadiernos/opus-file-manager/issues/260
[issue-261]: https://github.com/mikepadiernos/opus-file-manager/issues/261
[issue-262]: https://github.com/mikepadiernos/opus-file-manager/issues/262
[issue-263]: https://github.com/mikepadiernos/opus-file-manager/issues/263
[issue-264]: https://github.com/mikepadiernos/opus-file-manager/issues/264
[issue-265]: https://github.com/mikepadiernos/opus-file-manager/issues/265
[issue-266]: https://github.com/mikepadiernos/opus-file-manager/issues/266
[issue-267]: https://github.com/mikepadiernos/opus-file-manager/issues/267
[issue-302]: https://github.com/mikepadiernos/opus-file-manager/issues/302
[issue-303]: https://github.com/mikepadiernos/opus-file-manager/issues/303
[issue-304]: https://github.com/mikepadiernos/opus-file-manager/issues/304
[issue-307]: https://github.com/mikepadiernos/opus-file-manager/issues/307
[issue-309]: https://github.com/mikepadiernos/opus-file-manager/issues/309
[issue-311]: https://github.com/mikepadiernos/opus-file-manager/issues/311
[issue-312]: https://github.com/mikepadiernos/opus-file-manager/issues/312
[issue-313]: https://github.com/mikepadiernos/opus-file-manager/issues/313
