# Caveats and limitations

This records current Opus limitations and integration considerations. For
planned features and completion status, see the [roadmap](ROADMAP.md).

## Archives

- Archive creation and integrity testing require local files. Compression
  includes only selected items with local paths; non-local items in a mixed
  selection are skipped.
- Password-protected creation supports AES-encrypted ZIP only. It rejects
  symbolic links and other unsupported file types. Password-protected
  extraction requires a local archive and destination, and rejects links,
  unsafe paths and unsupported entry types. A failed operation can leave a
  partial archive or extraction folder. ZIP entry names are not encrypted.
- Encrypted 7z extraction uses KArchive and can fail for otherwise valid
  archives. Integrity checking uses the `7z` executable when available and
  falls back to KArchive otherwise. Passwords containing a newline, carriage
  return or NUL cannot be passed to the executable.

### Possible Ark integration

Ark is not currently integrated into Opus. An optional handoff to Ark could
improve encrypted 7z compatibility because Ark has a dedicated `7z` backend,
but it adds an Ark and `7z` runtime dependency and needs its own error,
destination and overwrite handling. Ark's
[7z plugin](https://github.com/KDE/ark/blob/master/plugins/cli7zplugin/cliplugin.cpp)
defines a `-p$Password` command-line switch; verify the installed version and
operation before assuming passwords are kept out of process arguments.

## Folder metadata and listings

- Opus supports a subset of Max File Manager's local `.mfm.json` metadata.
  Writes sanitize the file and can discard unsupported fields or values; back
  up existing metadata before editing it. The upstream hide-from-parent flag
  is retained by the sanitizer but is not applied to listings.
- Metadata files are limited to 4,096 bytes, notes to 500 characters and pins
  to 200 per folder. Folder-format rules are stored in local folder metadata;
  remote locations use defaults unless the pane's format is locked.
- Flat view requires the `filenamesearch:/` KIO worker, normally supplied by
  `kio-extras`. Create New, New Folder and Paste are disabled, and drops are
  refused while flat view is active.
- Background folder-size calculations skip symbolic links to directories.
  Automatic calculation can be costly, especially on remote locations. Sizes
  are snapshots; optionally refresh calculated sizes in visible panes
  periodically in View settings to pick up changes outside watched
  directories. Periodic refresh is off by default and can be costly,
  especially on remote locations.

## Previews and gallery

- Document previews need a suitable installed KPart; media playback needs a Qt
  Multimedia backend. Available metadata depends on KDE extraction plugins.
- Preview images are limited to 8 MiB and 40 megapixels; text previews are
  UTF-8 and default to the first 1 MiB. Remote previews are downloaded to a
  temporary file and use those limits, or 128 MiB for other file types.
- The gallery is local-only; gallery images and batch conversion inputs are
  limited to 128 MiB and 40 megapixels. Saving edits and converting images do
  not preserve the original image metadata.
- Batch conversion reads files on disk rather than unsaved gallery edits, and
  converts animated images to still images. It requires destination filesystem
  support for atomic no-replace renames; unsupported files are reported as
  conversion failures. Converted files have owner-only permissions.
- Text lines longer than 16,384 characters are shown without syntax
  highlighting. Rendered HTML and Markdown disable external resources and
  links.

## Editor

- Drafts are limited to 8 MiB of UTF-8 text. Retention is unlimited by
  default; an optional age limit removes expired drafts except those locked by
  an active editor. Legacy drafts from versions without locking are retained
  until explicitly discarded. Restored drafts remain until explicitly
  discarded or expired so recovery cannot delete another editor's changes.
  Drafts are stored in the application's private data directory, not in the
  original file. If draft storage fails, save the document before switching
  files or closing.
- Markdown preview, visual editing and PDF export do not load external or
  local resources; links are disabled in the preview. Visual editing uses
  Qt's Markdown subset and can normalize or remove unsupported markup and
  HTML. It requires confirmation before editing and preserves a recovery
  snapshot when drafts are enabled.

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

## Build, installation and references

- Opus targets Linux and requires the dependencies listed in
  [`CMakeLists.txt`](../CMakeLists.txt), including libarchive. `install.sh
  --deps` prints suggested packages; it does not install them. The installer
  disables tests in its build tree unless reconfigured with
  `-DBUILD_TESTING=ON`. `install.sh --default` changes the default folder
  handler.
- The [parity inventories](README.md#behaviour-references) describe a
  reviewed Max File Manager checkout, not necessarily current Opus behaviour.
  Their upstream bugs and limitations should not be assumed to apply to Opus.

[issue-311]: https://github.com/mikepadiernos/opus-file-manager/issues/311
[issue-312]: https://github.com/mikepadiernos/opus-file-manager/issues/312
