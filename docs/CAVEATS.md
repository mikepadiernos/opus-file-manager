# Caveats and limitations

This document describes current limitations and integration considerations.
Links beside tracked caveats lead to their project issues. For planned work
and completion status, see the [roadmap](ROADMAP.md). Recommendations describe
current workarounds or possible improvements, not features already available.
If a recommended tool is not installed, see
[optional dependencies](#installing-optional-alternatives) for package names.
These tools are optional dependencies of the suggested workarounds, not
required dependencies of Opus as a whole; some are also needed for specific
Opus features, as noted below.

## Archives

- **Local files:** Archive creation and integrity testing require local files.
  When compressing a mixed selection, Opus skips items without local paths.
  Password-protected extraction also requires a local archive and destination.
  ([#265][issue-265])

  **Recommendation:** Copy remote inputs to a local folder before creating,
  testing or extracting an archive; copy the result back afterward. For
  supported remote services, `rclone` is an alternative for transferring files.
  (Comment: [#265][comment-265])
- **Password-protected archives:** Creation supports AES-encrypted ZIP only
  and rejects symbolic links and other unsupported file types. ZIP entry names
  are not encrypted. Extraction rejects links, unsafe paths and unsupported
  entry types. If creation or extraction fails, a partial archive or extraction
  folder may remain. ([#255][issue-255], [#256][issue-256])

  **Recommendation:** Avoid sensitive ZIP entry names, and check for partial
  output after failures before retrying. Use another trusted archive tool if
  the required format or input type is unsupported, such as 7-Zip for formats
  it supports. For encrypted filenames, 7-Zip can encrypt 7z headers; ZIP
  still exposes entry names. Check its handling of links and paths before use.
  (Comments: [#255][comment-255], [#256][comment-256])
- **Encrypted 7z:** Extraction uses KArchive and can fail even for valid
  archives. Integrity testing uses the `7z` executable when available and
  otherwise falls back to KArchive. Passwords containing a newline, carriage
  return or NUL cannot be passed to `7z`.
  ([#36][issue-36], [#255][issue-255])

  **Recommendation:** If extraction fails, try a trusted archive tool that
  supports encrypted 7z. For archives you create and intend to test with `7z`,
  choose a password without those characters. 7-Zip is one external option;
  enter passwords at its prompt rather than putting them in command arguments.
  (Comments: [#36][comment-36], [#255][comment-255])

### Possible Ark integration

Opus does not currently integrate with Ark. An optional handoff could improve
encrypted 7z compatibility through Ark's dedicated `7z` backend, but would
require Ark and `7z` at runtime as well as error, destination and overwrite
handling. Ark's
[7z plugin](https://github.com/KDE/ark/blob/master/plugins/cli7zplugin/cliplugin.cpp)
defines a `-p$Password` command-line switch. Verify the installed version and
operation before assuming passwords are not exposed in process arguments.
([#313][issue-313])

**Recommendation:** Investigate an optional Ark handoff with explicit dependency
checks and safe error, destination, overwrite and password handling. Until then,
open archives with 7-Zip or another trusted external tool when needed.
(Comment: [#313][comment-313])

## Folder metadata and listings

- **Metadata compatibility:** Opus supports a subset of Max File Manager's
  local `.mfm.json` metadata. Writing sanitizes the file and may discard
  unsupported fields or values, so back up existing metadata before editing.
  The sanitizer retains the upstream hide-from-parent flag, but Opus does not
  apply it to listings. ([#257][issue-257], [#258][issue-258])

  **Recommendation:** Back up `.mfm.json` before editing it in Opus and do not
  rely on hide-from-parent to hide entries in listings. A text editor can
  inspect a backup, but cannot make Opus apply unsupported metadata.
  (Comments: [#257][comment-257], [#258][comment-258])
- **Metadata limits:** Metadata files are limited to 4,096 bytes, notes to
  500 characters and pins to 200 per folder. Folder-format rules are stored
  in local metadata; remote locations use defaults unless the pane's format
  is locked. ([#263][issue-263])

  **Recommendation:** Keep metadata within these limits; lock the pane format
  when the same format is needed while browsing remote locations.
  (Comment: [#263][comment-263])
- **Flat view:** The `filenamesearch:/` KIO worker is required and is normally
  supplied by `kio-extras`. Create New, New Folder and Paste are disabled,
  and drops are refused while flat view is active. ([#261][issue-261])

  **Recommendation:** Install the worker if flat view is unavailable, and
  leave flat view before creating, pasting or dropping files. For local
  filename searches, `fd` or GNU `find` is an alternative; neither replaces KIO
  flat view.
  (Comment: [#261][comment-261])
- **Folder sizes:** Background calculations skip symbolic links to
  directories. Calculated sizes are snapshots and can become stale when
  changes occur outside watched directories. View settings offer optional
  periodic refresh for visible panes; it is off by default. Automatic
  calculation and periodic refresh can be costly, especially for remote
  locations. ([#262][issue-262])

  **Recommendation:** Enable periodic refresh only when its cost is acceptable;
  GNU `du -L` can follow directory symlinks for local paths. Be mindful of
  cycles and repeated targets.
  (Comment: [#262][comment-262])

## Previews and gallery

- **Preview dependencies:** Document previews require a suitable installed
  KPart, media playback requires a Qt Multimedia backend, and available
  metadata depends on KDE extraction plugins. ([#259][issue-259])

  **Recommendation:** Install the relevant KPart, multimedia backend or
  extraction plugin for the file types you need. For supported media, FFmpeg's
  `ffplay`, when included in the installed build, is an external playback
  alternative. (Comment: [#259][comment-259])
- **Preview limits:** Images are limited to 8 MiB and 40 megapixels. Text
  previews use UTF-8 and default to the first 1 MiB; lines longer than 16,384
  characters are shown without syntax highlighting. Remote previews download
  to a temporary file and use the same limits, or 128 MiB for other file
  types. ([#259][issue-259], [#264][issue-264])

  **Recommendation:** Open files in a suitable external application when
  previews are incomplete, exceed these limits or require another encoding.
  For local text files, `bat` or Neovim may be useful.
  (Comments: [#259][comment-259], [#264][comment-264])
- **Rendered previews:** HTML and Markdown previews disable links and
  external resources. ([#267][issue-267])

  **Recommendation:** Open trusted documents in an appropriate external
  application if links or external resources are needed.
  (Comment: [#267][comment-267])
- **Gallery:** The gallery supports local files only. Gallery images and batch
  conversion inputs are limited to 128 MiB and 40 megapixels. Saving edits or
  converting images does not preserve the original image metadata.
  ([#266][issue-266], [#260][issue-260])

  **Recommendation:** Work on local copies within the limits and keep the
  originals when their metadata must be preserved. ExifTool can inspect and
  copy supported metadata separately; verify the result for the target format.
  ImageMagick can process supported local images outside Opus's gallery,
  subject to its own resource and format limits.
  (Comments: [#266][comment-266], [#260][comment-260])
- **Batch conversion:** Conversion reads files on disk, not unsaved gallery
  edits, and turns animated images into still images. The conversion dialog
  warns about both limitations. ([#303][issue-303])

  **Recommendation:** Save gallery edits before conversion; use an
  animation-capable tool such as FFmpeg or ImageMagick when animation must be
  retained, and verify that the chosen output format supports it.
  (Comment: [#303][comment-303])
- **Conversion output:** The destination filesystem must support atomic
  no-replace renames, which prevent overwriting files, including those
  created after preview. Unsupported filesystems cause conversion failures.
  Converted files have owner-only permissions; change permissions if needed
  before sharing them. ([#302][issue-302], [#304][issue-304])

  **Recommendation:** Convert to a filesystem that supports atomic no-replace
  renames, then copy the output if needed. Review permissions before sharing;
  for local files, `chmod` can grant only the access needed.
  (Comments: [#302][comment-302], [#304][comment-304])

## Editor

- **Draft storage:** Drafts are limited to 8 MiB of UTF-8 text and are stored
  in the application's private data directory, not in the original file. If
  draft storage fails, save the document before switching files or closing.
  ([#307][issue-307])

  **Recommendation:** Save work to the original file regularly and use an
  external editor such as Neovim for documents that exceed the draft limit.
  (Comment: [#307][comment-307])
- **Draft retention:** Drafts are retained indefinitely by default. An
  optional age limit removes expired drafts unless they are locked by an
  active editor. Legacy drafts from versions without locking remain until
  explicitly discarded. Restored drafts remain until discarded or expired,
  protecting another editor's changes during recovery. ([#307][issue-307])

  **Recommendation:** Set an age limit if appropriate and explicitly discard
  unneeded legacy or restored drafts after checking their contents.
  (Comment: [#307][comment-307])
- **Markdown resources:** Markdown preview, visual editing and PDF export do
  not load local or external resources. Links are also disabled in the
  preview. ([#267][issue-267])

  **Recommendation:** Use a trusted external renderer when a document or PDF
  needs embedded resources or working links. Pandoc can render local resources;
  PDF output requires an appropriate PDF engine. (Comment: [#267][comment-267])
- **Visual Markdown editing:** Qt's Markdown subset can normalize the source
  or remove unsupported markup and HTML. Visual editing requires confirmation
  and preserves a recovery snapshot when drafts are enabled. ([#309][issue-309])

  **Recommendation:** Keep a backup and use source editing when preserving
  unsupported markup is important; Neovim is one external source editor.
  (Comment: [#309][comment-309])

## File comparison

File comparison requires the `diff` executable and two local regular files.
Unified output is decoded as UTF-8, and comparison stops if output exceeds
4 MiB. Binary differences are reported without a visual binary diff.
([#311][issue-311])

**Recommendation:** Copy remote files locally and use another comparison tool
for non-UTF-8 text, large output or a visual binary diff. Difftastic can
compare supported text files; `cmp` can check whether binary files differ,
but does not provide a visual diff. (Comment: [#311][comment-311])

## Symbolic links

Symbolic-link creation supports existing local files and folders only. The new
link is created beside its target; another destination cannot be chosen.
([#312][issue-312])

**Recommendation:** Use a suitable external tool when a different link
destination or remote symbolic-link support is needed. For local paths,
GNU `ln -s` can create a link at a chosen destination; remote support depends
on the server and access method. (Comment: [#312][comment-312])

## Listing export

- Listing export uses the current filtered rows and configured details columns
  with their displayed values, even in icon or compact view. Output is local
  only, and wide PDF tables may wrap or clip. CSV prefixes cells that could be
  interpreted as spreadsheet formulas with an apostrophe.

  **Recommendation:** Check the filter and details columns before exporting,
  and reduce columns for wide PDF tables. Keep CSV formula protection when
  opening untrusted data in a spreadsheet; copy local output elsewhere if needed.
  GNU `find -print0` can produce a local, NUL-delimited filename list for
  compatible tools, but does not reproduce Opus filters, columns or CSV output.
  (Comment: [#314][comment-314])

## Folder content chart

- The chart shows measured logical sizes by immediate entry, not kind or
  extension breakdowns or a largest-file drilldown. It includes hidden
  entries, skips top-level symbolic links and entries without a known size,
  and relies on KIO workers for recursive traversal. Scans are cancellable but
  have no entry or time limit and can be costly; results are snapshots.
  ([#315](https://github.com/mikepadiernos/opus-file-manager/issues/315))

  **Recommendation:** Scan smaller folders when possible, cancel expensive
  scans and rescan when current results are needed. Use another tool for
  extension breakdowns, largest-file analysis or linked-directory sizes.
  `ncdu` can explore local directory usage; GNU `du -L` can follow directory
  symlinks, subject to cycles and repeated targets.
  (Comment: [#315][comment-315])

## Checksum files

- Checksum files use UTF-8 GNU-style SHA-256 manifests and local regular files
  in the same folder. Other algorithms, remote files, symbolic links, path
  entries and non-UTF-8 names are unsupported. Manifests are limited to 10,000
  entries. Files can change during hashing; cancellation near completion can
  race with publication of the checksum file.
  ([#316](https://github.com/mikepadiernos/opus-file-manager/issues/316))

  **Recommendation:** Use local regular files in the same folder and keep
  manifests within 10,000 entries. Avoid changing files while hashing, and
  check whether an output file was published after cancellation. Use a trusted
  external checksum tool for unsupported algorithms or inputs. GNU coreutils
  provides `sha256sum` and other checksum commands for local files.
  (Comment: [#316][comment-316])

## Pane folder comparison

- Comparison covers immediate entries only. Files are compared by size and
  modification time, not contents; subfolders are not compared. Results are
  snapshots, and hidden or filtered entries may not be selectable. Large or
  remote listings may be costly.
  ([#317](https://github.com/mikepadiernos/opus-file-manager/issues/317))

  **Recommendation:** Use file comparison for individual files, or a trusted
  recursive comparison tool when content verification or subfolder comparison
  is needed. For local folders, GNU `diff -r` compares directory trees. Show
  hidden files and clear pane filters before selecting differences; compare
  again after changes.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/317#issuecomment-7021904155))

## Split and combine files

- Split and combine supports local regular files. Parts are raw numbered files;
  combining uses the displayed order and cannot detect missing, altered or
  incorrectly ordered parts. Inputs can change during processing. Failures or
  cancellation can leave incomplete output, and many small parts can be
  expensive. Existing outputs are never overwritten and new files are
  owner-only. ([#318](https://github.com/mikepadiernos/opus-file-manager/issues/318))

  **Recommendation:** Check the complete part list and order before combining,
  keep originals and verify the reconstructed file with a checksum. Avoid
  changing inputs while processing. Inspect and remove unwanted partial outputs
  after errors or cancellation, choose a sensible part size, and adjust output
  permissions if sharing is needed. GNU `split` and `cat` are external
  alternatives; verify part order and handle output safely to avoid overwriting
  existing files. GNU `sha256sum` can verify reconstructed files against a
  trusted checksum.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/318#issuecomment-7022199214))

## Embedded terminal

- The embedded terminal requires the Konsole KPart. New sessions start in the
  active local folder, or in the home folder with a visible notice for remote
  locations. Pane navigation does not change a running session's directory.
  Hiding the panel keeps the session running; closing the window ends it and
  can affect running programs.
  ([#321](https://github.com/mikepadiernos/opus-file-manager/issues/321))

  **Recommendation:** Install the Konsole terminal component for embedded
  sessions. For remote locations, connect using a suitable terminal tool or use
  a separate terminal configured for that service. Check the terminal working
  directory before running commands, and finish or safely stop programs before
  closing the window.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/321#issuecomment-7022392302))

## Linked pane navigation

- Linked navigation supports existing local folder trees rooted at the two
  starting folders. It unlinks when navigation leaves either tree, a matching
  folder is missing, a symbolic link resolves outside its tree, or a pane is
  hidden. Tab and history changes also participate and may unlink. Filesystem
  changes can invalidate folders between checks and navigation.
  ([#322](https://github.com/mikepadiernos/opus-file-manager/issues/322))

  **Recommendation:** Start from corresponding local folders with matching
  subfolder names. Check both panes after navigating, especially when
  directories may change; relink at a new pair of roots when needed. Navigate
  remote folders independently.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/322#issuecomment-7022490696))

## Cushion treemap

- The treemap uses the folder content chart measurements: tiles represent
  measured immediate entries and their relative logical sizes. Nested folders
  are aggregated rather than shown simultaneously; activate a folder in the
  list to rescan it. Empty, unmeasured and skipped entries have no visible
  tile, and very small shares may be difficult to see. Measurements are
  snapshots and the [folder content chart](#folder-content-chart) scan limits
  also apply. ([#335](https://github.com/mikepadiernos/opus-file-manager/issues/335))

  **Recommendation:** Activate folders in the list to inspect their contents,
  and use the size table for empty, skipped or very small entries. Scan smaller
  folders and rescan when current results are needed; use another disk-usage
  tool when simultaneous nested visualization is required.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/335#issuecomment-7022647720))

## Duplicate finder

- The duplicate finder recursively scans local folders for known regular
  files, groups equal sizes and SHA-256 hashes, and allows checked file URLs
  to be copied for manual review. Candidates can be selected by modification
  time or folder and moved to Trash after confirmation. It rechecks affected
  groups before asking, but files can change afterward. Symbolic links are
  skipped; hard links may appear as separate paths, and unknown entry types are
  skipped. Errors are shown, but scans are unbounded and matching hashes are
  not a guarantee of byte identity.
  ([#336](https://github.com/mikepadiernos/opus-file-manager/issues/336))

  **Recommendation:** Review groups and errors, verify current file contents
  and paths independently, and keep at least one confirmed copy and backups
  before moving files to Trash. Scan smaller local folders if a scan is
  expensive.
  ([Comment](https://github.com/mikepadiernos/opus-file-manager/issues/336#issuecomment-7022793474))

## Build, installation and references

- **Dependencies:** Opus targets Linux and requires the dependencies listed in
  [`CMakeLists.txt`](../CMakeLists.txt), including libarchive.

  **Recommendation:** Install the required development packages for your Linux
  distribution before building.
- **Installer:** `install.sh --deps` prints suggested packages but does not
  install them. The installer disables tests in its build tree unless
  reconfigured with `-DBUILD_TESTING=ON`. `install.sh --default` changes the
  default folder handler.

  **Recommendation:** Review the suggested packages and install them separately;
  enable tests explicitly when needed, and use `--default` only if you want to
  change the default folder handler.
- **Parity inventories:** The [inventories](README.md#behaviour-references)
  describe a reviewed Max File Manager checkout, not necessarily current Opus
  behaviour. Do not assume upstream bugs or limitations apply to Opus.

  **Recommendation:** Check Opus itself and the roadmap when evaluating
  current behaviour or planned parity.

## Installing optional alternatives

These are independent tools, not Opus integrations. Install only what you need
for a recommended workaround if it is not already available, and check each
tool's format, platform and security limitations. For example, `difftastic`
provides `difft` for text comparison and `ncdu` provides `ncdu` for browsing
directory usage. On Arch Linux,
install an official package with `sudo pacman -S <package>`; `paru -S <package>`
can also install repository packages. With Homebrew, use `brew install
<formula>`. For the listed mise registry names, `mise use -g <name>@latest`
installs the tool and adds it to your global mise configuration; ensure mise is
activated in your shell. A dash means no mise option is listed here, not that
installation through another backend is impossible.

| Tool | Arch package | Homebrew formula | mise name |
| --- | --- | --- | --- |
| 7-Zip | `7zip` | `sevenzip` | `7zip` |
| rclone | `rclone` | `rclone` | `rclone` |
| fd | `fd` | `fd` | `fd` |
| bat | `bat` | `bat` | `bat` |
| Neovim | `neovim` | `neovim` | `neovim` |
| Difftastic | `difftastic` | `difftastic` | `difftastic` |
| Pandoc | `pandoc-cli` | `pandoc` | `pandoc` |
| ImageMagick | `imagemagick` | `imagemagick` | - |
| FFmpeg | `ffmpeg` | `ffmpeg` | - |
| ExifTool | `perl-image-exiftool` | `exiftool` | - |
| ncdu | `ncdu` | `ncdu` | - |
| GNU coreutils | `coreutils` | `coreutils` | - |
| GNU diffutils | `diffutils` | `diffutils` | - |
| GNU findutils | `findutils` | `findutils` | - |

Homebrew's GNU coreutils commands are normally prefixed with `g`, for example
`gdu` and `gsha256sum`; Homebrew's GNU diffutils commands likewise use names
such as `gdiff` and `gcmp`, and GNU findutils uses `gfind`. KDE components
such as Ark, KParts and KIO workers
should be installed through the system's KDE packages, not treated as
standalone CLI replacements. The PDF engine used by Pandoc is a separate
dependency and varies with the chosen output method.

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

[comment-36]: https://github.com/mikepadiernos/opus-file-manager/issues/36#issuecomment-7021740511
[comment-255]: https://github.com/mikepadiernos/opus-file-manager/issues/255#issuecomment-7021741358
[comment-256]: https://github.com/mikepadiernos/opus-file-manager/issues/256#issuecomment-7021742119
[comment-257]: https://github.com/mikepadiernos/opus-file-manager/issues/257#issuecomment-7021742864
[comment-258]: https://github.com/mikepadiernos/opus-file-manager/issues/258#issuecomment-7021743599
[comment-259]: https://github.com/mikepadiernos/opus-file-manager/issues/259#issuecomment-7021744368
[comment-260]: https://github.com/mikepadiernos/opus-file-manager/issues/260#issuecomment-7021745029
[comment-261]: https://github.com/mikepadiernos/opus-file-manager/issues/261#issuecomment-7021745774
[comment-262]: https://github.com/mikepadiernos/opus-file-manager/issues/262#issuecomment-7021746538
[comment-263]: https://github.com/mikepadiernos/opus-file-manager/issues/263#issuecomment-7021748197
[comment-264]: https://github.com/mikepadiernos/opus-file-manager/issues/264#issuecomment-7021749088
[comment-265]: https://github.com/mikepadiernos/opus-file-manager/issues/265#issuecomment-7021749894
[comment-266]: https://github.com/mikepadiernos/opus-file-manager/issues/266#issuecomment-7021750778
[comment-267]: https://github.com/mikepadiernos/opus-file-manager/issues/267#issuecomment-7021751490
[comment-302]: https://github.com/mikepadiernos/opus-file-manager/issues/302#issuecomment-7021752278
[comment-303]: https://github.com/mikepadiernos/opus-file-manager/issues/303#issuecomment-7021753023
[comment-304]: https://github.com/mikepadiernos/opus-file-manager/issues/304#issuecomment-7021753785
[comment-307]: https://github.com/mikepadiernos/opus-file-manager/issues/307#issuecomment-7021754467
[comment-309]: https://github.com/mikepadiernos/opus-file-manager/issues/309#issuecomment-7021755164
[comment-311]: https://github.com/mikepadiernos/opus-file-manager/issues/311#issuecomment-7021755780
[comment-312]: https://github.com/mikepadiernos/opus-file-manager/issues/312#issuecomment-7021756400
[comment-313]: https://github.com/mikepadiernos/opus-file-manager/issues/313#issuecomment-7021757012
[comment-314]: https://github.com/mikepadiernos/opus-file-manager/issues/314#issuecomment-7021757620
[comment-315]: https://github.com/mikepadiernos/opus-file-manager/issues/315#issuecomment-7021758275
[comment-316]: https://github.com/mikepadiernos/opus-file-manager/issues/316#issuecomment-7021845379
