# Caveats and limitations

This guide records limitations, precautions and practical recommendations for
current features and, where marked, planned features. Tracked caveats link to
their issues and recommendation comments. Alternatives supplement the primary
recommendation; external tools are separate applications. Proposed
improvements are not implemented features. See the [roadmap](ROADMAP.md) for
scope and status.

## Find a caveat

- **Browsing and viewing:** [Archives](#archives),
  [folder metadata and listings](#folder-metadata-and-listings),
  [planned password-gated folders](#planned-password-gated-folders),
  [previews and gallery](#previews-and-gallery), [editor](#editor),
  [Jalali dates](#jalali-dates),
  [translation format overrides](#translation-format-overrides)
- **Search and operations:** [Search](#search), [batch rename](#batch-rename),
  [metadata editing](#metadata-editing), [transfer queue](#transfer-queue),
  [custom commands](#custom-commands), [symbolic links](#symbolic-links),
  [listing export](#listing-export), [checksums](#checksum-files),
  [split and combine](#split-and-combine-files)
- **Comparison and analysis:** [Filtered folder comparison](#filtered-folder-comparison),
  [folder synchronization](#folder-synchronization),
  [file comparison](#file-comparison), [folder content chart](#folder-content-chart),
  [pane comparison](#pane-folder-comparison), [treemap](#cushion-treemap),
  [duplicate finder](#duplicate-finder)
- **Other:** [Embedded terminal](#embedded-terminal),
  [linked navigation](#linked-pane-navigation),
  [file-list minimap](#file-list-minimap), [settings backups](#settings-backups),
  [build and installation](#build-installation-and-references),
  [installing optional tools](#installing-optional-alternatives)

## Jalali dates

With a Persian regional locale or an explicit Jalali calendar override,
Opus's date displays use the Jalali calendar. With the automatic calendar,
the ISO 8601 listing format remains Gregorian; an explicit calendar override
takes precedence. KDE controls dates in its own dialogs and tooltips, and the
additional file information shown under icons in icon and compact views uses
KDE's `KFileItemDelegate`; these dates may remain Gregorian.
([#436](https://github.com/mikepadiernos/opus-file-manager/issues/436))

- **Recommendation:** Use Details view or the Opus Information panel for
  Jalali dates. Select Jalali explicitly, or use a Persian regional locale
  with the automatic calendar and a Locale listing format. Verify the calendar
  before interpreting KDE-provided dates.
  - Comment: [#436.7041290164](https://github.com/mikepadiernos/opus-file-manager/issues/436#issuecomment-7041290164)

## Translation format overrides

The regional locale and custom digit shapes do not control every display.
Opus applies digit shapes to its file dates, listing sizes, preview line
numbers and selected gallery and chart fields. Other text and numbers,
editable Qt controls, KDE-provided details and dialogs, and separately
embedded applications can use their own locale or desktop settings. With a
C/POSIX message locale, an explicitly selected non-English interface language
also requires an installed non-C UTF-8 message locale; Opus warns and follows
the system language if none can be used.
([#440](https://github.com/mikepadiernos/opus-file-manager/issues/440))

- **Recommendation:** Use Details view or the Information panel for
  overridden file dates and verify other displays against the desktop locale.
  Configure desktop regional settings for consistency elsewhere. If the
  selected language is unavailable under C/POSIX, install a non-C UTF-8
  system locale and restart Opus.
  - Comment: [#440.7045227580](https://github.com/mikepadiernos/opus-file-manager/issues/440#issuecomment-7045227580)

## Archives

- **Local files:** Archive creation and integrity testing require local files.
  When compressing a mixed selection, Opus skips items without local paths.
  Password-protected extraction also requires a local archive and destination.
  ([#265][issue-265])

  - **Recommendation:** Copy remote inputs to a local folder before creating,
    testing or extracting an archive; copy the result back afterward. For
    supported remote services, `rclone` is an alternative for transferring files.
    - Comment: [#265.7021749894][comment-265]
  - **Alternative:** Stage remote files locally with Dolphin.
    - Comment: [#265.7029669109](https://github.com/mikepadiernos/opus-file-manager/issues/265#issuecomment-7029669109)
  - **Planned:** [M23 remote connection profiles](ROADMAP.md#m23-integrations)
    are not a commitment to support remote archive creation or testing.
    - Comment: [#265.7021749894][comment-265]
- **Password-protected archives:** Creation supports AES-encrypted ZIP only
  and rejects symbolic links and other unsupported file types. ZIP entry names
  are not encrypted. Extraction rejects links, unsafe paths and unsupported
  entry types. If creation or extraction fails, a partial archive or extraction
  folder may remain. ([#255][issue-255], [#256][issue-256])

  - **Recommendation:** Avoid sensitive ZIP entry names, and check for partial
    output after failures before retrying. Use another trusted archive tool if
    the required format or input type is unsupported, such as 7-Zip for formats
    it supports. For encrypted filenames, 7-Zip can encrypt 7z headers; ZIP
    still exposes entry names. Check its handling of links and paths before use.
    - Comment: [#255.7021741358][comment-255]
    - Comment: [#256.7021742119][comment-256]
  - **Alternatives:**
    - Encrypt an entire archive separately with GnuPG.
      - Comment: [#255.7029665897](https://github.com/mikepadiernos/opus-file-manager/issues/255#issuecomment-7029665897)
    - Extract into a fresh staging directory first.
      - Comment: [#256.7029666231](https://github.com/mikepadiernos/opus-file-manager/issues/256#issuecomment-7029666231)
- **Encrypted 7z:** Extraction uses KArchive and can fail even for valid
  archives. Integrity testing uses the `7z` executable when available and
  otherwise falls back to KArchive. Passwords containing a newline, carriage
  return or NUL cannot be passed to `7z`.
  ([#36][issue-36], [#255][issue-255])

  - **Recommendation:** If extraction fails, try a trusted archive tool that
    supports encrypted 7z. For archives you create and intend to test with `7z`,
    choose a password without those characters. 7-Zip is one external option;
    enter passwords at its prompt rather than putting them in command arguments.
    - Comment: [#36.7021740511][comment-36]
    - Comment: [#255.7021741358][comment-255]
  - **Alternative:** Try standalone Ark with its 7z backend.
    - Comment: [#36.7029665511](https://github.com/mikepadiernos/opus-file-manager/issues/36#issuecomment-7029665511)
  - **Planned:** [M23 Ark integration](ROADMAP.md#m23-integrations) does not
    guarantee encrypted 7z support or safe password handling.
    - Comment: [#36.7021740511][comment-36]

### Possible Ark integration

Opus does not currently integrate with Ark. An optional handoff could improve
encrypted 7z compatibility through Ark's dedicated `7z` backend, but would
require Ark and `7z` at runtime as well as error, destination and overwrite
handling. Ark's
[7z plugin](https://github.com/KDE/ark/blob/master/plugins/cli7zplugin/cliplugin.cpp)
defines a `-p$Password` command-line switch. Verify the installed version and
operation before assuming passwords are not exposed in process arguments.
([#313][issue-313])

- **Recommendation:** Investigate an optional Ark handoff with explicit dependency
  checks and safe error, destination, overwrite and password handling. Until then,
  open archives with 7-Zip or another trusted external tool when needed.
  - Comment: [#313.7021757012][comment-313]
- **Alternative proposal:** Consider a user-controlled external archiver handoff.
  - Comment: [#313.7029672463](https://github.com/mikepadiernos/opus-file-manager/issues/313#issuecomment-7029672463)
- **Planned:** [M23](ROADMAP.md#m23-integrations) includes Ark integration;
  its precise backend and safeguards still need to be established.
  - Comment: [#313.7021757012][comment-313]

## Folder metadata and listings

- **Metadata compatibility:** Opus supports a subset of Max File Manager's
  local `.mfm.json` metadata. Writing sanitizes the file and may discard
  unsupported fields or values, so back up existing metadata before editing.
  The sanitizer retains the upstream hide-from-parent flag, but Opus does not
  apply it to listings. ([#257][issue-257], [#258][issue-258])

  - **Recommendation:** Back up `.mfm.json` before editing it in Opus and do not
    rely on hide-from-parent to hide entries in listings. A text editor can
    inspect a backup, but cannot make Opus apply unsupported metadata.
    - Comment: [#257.7021742864][comment-257]
    - Comment: [#258.7021743599][comment-258]
  - **Alternatives:**
    - Keep a versioned copy of metadata outside Opus.
      - Comment: [#257.7029666537](https://github.com/mikepadiernos/opus-file-manager/issues/257#issuecomment-7029666537)
    - Use ordinary hidden filenames where appropriate.
      - Comment: [#258.7029666857](https://github.com/mikepadiernos/opus-file-manager/issues/258#issuecomment-7029666857)
  - **Planned:** [M17 native tagging](ROADMAP.md#m17-native-tagging) is a
    separate library and does not make `.mfm.json` writes lossless.
    - Comment: [#257.7021742864][comment-257]
- **Metadata limits:** Metadata files are limited to 4,096 bytes, notes to
  500 characters and pins to 200 per folder. Folder-format rules are stored
  in local metadata; remote locations use defaults unless the pane's format
  is locked. ([#263][issue-263])

  - **Recommendation:** Keep metadata within these limits; lock the pane format
    when the same format is needed while browsing remote locations.
    - Comment: [#263.7021748197][comment-263]
  - **Alternative:** Keep longer notes in a separate file.
    - Comment: [#263.7029668469](https://github.com/mikepadiernos/opus-file-manager/issues/263#issuecomment-7029668469)
  - **Planned:** [M17 library text fields](ROADMAP.md#m17-native-tagging)
    are separate from folder notes and do not remove these metadata limits.
    - Comment: [#263.7021748197][comment-263]
- **Flat view:** The `filenamesearch:/` KIO worker is required and is normally
  supplied by `kio-extras`. Create New, New Folder and Paste are disabled,
  and drops are refused while flat view is active. ([#261][issue-261])

  - **Recommendation:** Install the worker if flat view is unavailable, and
    leave flat view before creating, pasting or dropping files. For local
    filename searches, `fd` or GNU `find` is an alternative; neither replaces KIO
    flat view.
    - Comment: [#261.7021745774][comment-261]
  - **Alternative:** Use live search and reveal a result in its folder.
    - Comment: [#261.7029667831](https://github.com/mikepadiernos/opus-file-manager/issues/261#issuecomment-7029667831)
- **Folder sizes:** Background calculations skip symbolic links to
  directories. Calculated sizes are snapshots and can become stale when
  changes occur outside watched directories. View settings offer optional
  periodic refresh for visible panes; it is off by default. Automatic
  calculation and periodic refresh can be costly, especially for remote
  locations. ([#262][issue-262])

  - **Recommendation:** Enable periodic refresh only when its cost is acceptable;
    GNU `du -L` can follow directory symlinks for local paths. Be mindful of
    cycles and repeated targets.
    - Comment: [#262.7021746538][comment-262]
  - **Alternative:** Take an on-demand size snapshot with GNU `du`.
    - Comment: [#262.7029668154](https://github.com/mikepadiernos/opus-file-manager/issues/262#issuecomment-7029668154)

## Planned password-gated folders

The [proposed Opus password gate](ROADMAP.md#m23-integrations) is not
implemented. It would be an Opus-only prompt, not encryption or filesystem
access control. Other file managers and processes with the same user permissions
could access the underlying folder without the password. Standard filesystem
permissions and ACLs distinguish users and groups, not applications running
with the same credentials. ([#406](https://github.com/mikepadiernos/opus-file-manager/issues/406);
feature [#402](https://github.com/mikepadiernos/opus-file-manager/issues/402))

- **Recommendation:** For confidential files, use a separate encrypted vault
  such as [KDE Plasma Vault](https://github.com/KDE/plasma-vault). Applications
  with access can use files while the vault is unlocked and mounted; when it is
  locked, the decrypted contents are unavailable through the mount. This uses
  encryption, unlike the requested gate. Back up the data and protect the
  passphrase; handle any original unencrypted copies separately. Backend
  availability and behaviour vary.
  - Comment: [#406.7040077820](https://github.com/mikepadiernos/opus-file-manager/issues/406#issuecomment-7040077820)
- **Alternatives:**
  - **Cooperative prompts:** Explore a shared gate only with file managers that
    explicitly support it. Unsupported apps and direct access can bypass it;
    cross-manager support is not established and this is not security.
    - Comment: [#406.7040078439](https://github.com/mikepadiernos/opus-file-manager/issues/406#issuecomment-7040078439)
  - **Separate identities:** Use ownership and ACLs to restrict access to
    selected user or service identities. Apps with the same credentials retain
    the same ordinary access; this does not add a folder-password prompt.
    Privileged users can override controls, and administration requires care.
    - Comment: [#406.7040079139](https://github.com/mikepadiernos/opus-file-manager/issues/406#issuecomment-7040079139)
  - **Application sandboxing:** Restrict a controlled set of sandboxed apps and
    grant chosen apps access through supported sandbox or portal mechanisms.
    Unsandboxed same-user processes can still access the folder; support and
    permissions vary, so this is not universal password enforcement.
    - Comment: [#406.7040079777](https://github.com/mikepadiernos/opus-file-manager/issues/406#issuecomment-7040079777)

## Previews and gallery

- **Preview dependencies:** Document previews require a suitable installed
  KPart, media playback requires a Qt Multimedia backend, and available
  metadata depends on KDE extraction plugins. ([#259][issue-259])

  - **Recommendation:** Install the relevant KPart, multimedia backend or
    extraction plugin for the file types you need. For supported media, FFmpeg's
    `ffplay`, when included in the installed build, is an external playback
    alternative.
    - Comment: [#259.7021744368][comment-259]
  - **Alternative:** Open the file with a separate application.
    - Comment: [#259.7029667136](https://github.com/mikepadiernos/opus-file-manager/issues/259#issuecomment-7029667136)
- **Preview limits:** Images are limited to 8 MiB and 40 megapixels. Text
  previews use UTF-8 and default to the first 1 MiB; lines longer than 16,384
  characters are shown without syntax highlighting. Remote previews download
  to a temporary file and use the same limits, or 128 MiB for other file
  types. ([#259][issue-259], [#264][issue-264])

  - **Recommendation:** Open files in a suitable external application when
    previews are incomplete, exceed these limits or require another encoding.
    For local text files, `bat` or Neovim may be useful.
    - Comment: [#259.7021744368][comment-259]
    - Comment: [#264.7021749088][comment-264]
  - **Alternative:** Use Kate or KWrite for long-line text.
    - Comment: [#264.7029668772](https://github.com/mikepadiernos/opus-file-manager/issues/264#issuecomment-7029668772)
  - **Planned:** [M23 editor integrations](ROADMAP.md#m23-integrations) would
    launch separate editors; they do not increase Opus preview limits.
    - Comment: [#264.7021749088][comment-264]
- **Rendered previews:** HTML and Markdown previews disable links and
  external resources. ([#267][issue-267])

  - **Recommendation:** Open trusted documents in an appropriate external
    application if links or external resources are needed.
    - Comment: [#267.7021751490][comment-267]
  - **Alternative:** Use a separate browser for trusted rendered documents.
    - Comment: [#267.7029669771](https://github.com/mikepadiernos/opus-file-manager/issues/267#issuecomment-7029669771)
- **Gallery:** The gallery supports local files only. Gallery images and batch
  conversion inputs are limited to 128 MiB and 40 megapixels. Saving edits or
  converting images does not preserve the original image metadata.
  ([#266][issue-266], [#260][issue-260])

  - **Recommendation:** Work on local copies within the limits and keep the
    originals when their metadata must be preserved. ExifTool can inspect and
    copy supported metadata separately; verify the result for the target format.
    ImageMagick can process supported local images outside Opus's gallery,
    subject to its own resource and format limits.
    - Comment: [#266.7021750778][comment-266]
    - Comment: [#260.7021745029][comment-260]
  - **Alternatives:**
    - Keep the metadata-bearing original alongside an edited copy.
      - Comment: [#260.7029667492](https://github.com/mikepadiernos/opus-file-manager/issues/260#issuecomment-7029667492)
    - Try a separate image viewer for a local copy.
      - Comment: [#266.7029669439](https://github.com/mikepadiernos/opus-file-manager/issues/266#issuecomment-7029669439)
- **Batch conversion:** Conversion reads files on disk, not unsaved gallery
  edits, and turns animated images into still images. The conversion dialog
  warns about both limitations. ([#303][issue-303])

  - **Recommendation:** Save gallery edits before conversion; use an
    animation-capable tool such as FFmpeg or ImageMagick when animation must be
    retained, and verify that the chosen output format supports it.
    - Comment: [#303.7021753023][comment-303]
  - **Alternative proposal:** Consider a future animation-aware conversion backend.
    - Comment: [#303.7029670421](https://github.com/mikepadiernos/opus-file-manager/issues/303#issuecomment-7029670421)
- **Conversion output:** The destination filesystem must support atomic
  no-replace renames, which prevent overwriting files, including those
  created after preview. Unsupported filesystems cause conversion failures.
  Converted files have owner-only permissions; change permissions if needed
  before sharing them. ([#302][issue-302], [#304][issue-304])

  - **Recommendation:** Convert to a filesystem that supports atomic no-replace
    renames, then copy the output if needed. Review permissions before sharing;
    for local files, `chmod` can grant only the access needed.
    - Comment: [#302.7021752278][comment-302]
    - Comment: [#304.7021753785][comment-304]
  - **Alternatives:**
    - Use a separate conversion tool with a fresh destination.
      - Comment: [#302.7029670102](https://github.com/mikepadiernos/opus-file-manager/issues/302#issuecomment-7029670102)
    - Use a targeted ACL when sharing converted output.
      - Comment: [#304.7029670734](https://github.com/mikepadiernos/opus-file-manager/issues/304#issuecomment-7029670734)

## Editor

- **Draft storage:** Drafts are limited to 8 MiB of UTF-8 text and are stored
  in the application's private data directory, not in the original file. If
  draft storage fails, save the document before switching files or closing.
  ([#307][issue-307])

  - **Recommendation:** Save work to the original file regularly and use an
    external editor such as Neovim for documents that exceed the draft limit.
    - Comment: [#307.7021754467][comment-307]
  - **Alternative:** Use a standalone editor and an explicit backup copy.
    - Comment: [#307.7029671086](https://github.com/mikepadiernos/opus-file-manager/issues/307#issuecomment-7029671086)
  - **Planned:** [M23 editor integrations](ROADMAP.md#m23-integrations) are
    separate applications and do not expand Opus draft storage.
    - Comment: [#307.7021754467][comment-307]
- **Draft retention:** Drafts are retained indefinitely by default. An
  optional age limit removes expired drafts unless they are locked by an
  active editor. Legacy drafts from versions without locking remain until
  explicitly discarded. Restored drafts remain until discarded or expired,
  protecting another editor's changes during recovery. ([#307][issue-307])

  - **Recommendation:** Set an age limit if appropriate and explicitly discard
    unneeded legacy or restored drafts after checking their contents.
    - Comment: [#307.7021754467][comment-307]
- **Markdown resources:** Markdown preview, visual editing and PDF export do
  not load local or external resources. Links are also disabled in the
  preview. ([#267][issue-267])

  - **Recommendation:** Use a trusted external renderer when a document or PDF
    needs embedded resources or working links. Pandoc can render local resources;
    PDF output requires an appropriate PDF engine.
    - Comment: [#267.7021751490][comment-267]
- **Visual Markdown editing:** Qt's Markdown subset can normalize the source
  or remove unsupported markup and HTML. Visual editing requires confirmation
  and preserves a recovery snapshot when drafts are enabled. ([#309][issue-309])

  - **Recommendation:** Keep a backup and use source editing when preserving
    unsupported markup is important; Neovim is one external source editor.
    - Comment: [#309.7021755164][comment-309]
  - **Alternative:** Compare a saved copy against the original before adopting edits.
    - Comment: [#309.7029671476](https://github.com/mikepadiernos/opus-file-manager/issues/309#issuecomment-7029671476)
  - **Planned:** [M23 editor integrations](ROADMAP.md#m23-integrations) do
    not change the built-in visual editor's Markdown limitations.
    - Comment: [#309.7021755164][comment-309]

## Search

Tracked in [#362](https://github.com/mikepadiernos/opus-file-manager/issues/362).

- **Live search:** Scans local folders only and skips symbolic links. Content
  search covers UTF-8 files up to 2 MiB without NUL bytes. Search stops after
  5,000 matches or 200,000 entries, but lists each directory before applying
  the entry limit.
- **Indexed search:** Requires Baloo at build and runtime and searches its
  configured locations. It matches indexed terms rather than literal
  substrings, does not accept double quotes, and may omit files or return stale
  results. With path filters, it examines at most 200,000 Baloo matches and
  returns at most 5,000 filtered matches. A depth limit restricts results to
  the current local folder.
- **Index status:** Coverage notices reflect configuration, not proof of
  indexing; exclusions and pending indexing can affect results. Baloo's query
  API does not report index-read failures, and a running query cannot be
  interrupted until it returns.
- **Selection and exclusions:** Path-selection filters apply only to local
  top-level listings or the visible flat list, not expanded child rows.
  Excluded folders are entered one per line, so paths containing newlines
  cannot be entered; exclusions are lexical paths, not resolved links.
- **Results and collections:** Results can be revealed in the active pane, but
  hidden or filtered items may not be visible and flat view must be turned off.
  Directly opening results is not yet available. The shared collection stores
  local URLs, not file contents; references can become stale.

- **Recommendation:** Check Baloo's indexing configuration and status; use
  live search for a current local folder, search smaller folders, inspect
  errors and repeat searches after files change. Use flat view when selecting
  nested local items by path, and verify the resulting selection. Use GNU
  `find` for other local filename searches or ripgrep (`rg`) for content
  searches when Opus limits are unsuitable; check each tool's hidden-file,
  ignore, encoding and symbolic-link behavior. If a revealed result is hidden
  or filtered, adjust the pane's visibility settings. Re-run saved searches
  and check collection references after files change.
  - Comment: [#362.7023269364][comment-362]
- **Alternative:** Use a separately maintained locate database for filenames.
  - Comment: [#362.7029675758](https://github.com/mikepadiernos/opus-file-manager/issues/362#issuecomment-7029675758)
- **Planned:** [M15 advanced search](ROADMAP.md#m15-advanced-search) reuses
  M09 search; its new interface and criteria do not by themselves remove
  current backend limits. [M23 Baloo integration](ROADMAP.md#m23-integrations)
  plans better status, metadata queries, result ranking and an explicit live
  fallback, not guaranteed index completeness. [M21 workspaces](ROADMAP.md#m21-workspaces)
  plan search scoping, not a security boundary.
  - Comment: [#362.7023269364][comment-362]

## Batch rename

Tracked in [#372](https://github.com/mikepadiernos/opus-file-manager/issues/372).

- **Inputs and destinations:** Only local items are supported. The destination
  filesystem must support Linux atomic no-replace renames. Preview rejects
  existing destinations, rename chains, swaps, and selections containing both
  a folder and its descendants.
- **Patterns and metadata:** Metadata tokens depend on installed KDE extractors
  and embedded fields; missing metadata and invalid names prevent application.
  Regular expressions are case-sensitive and apply to base names only,
  preserving extensions. Expensive expressions can delay a preview and cannot
  be canceled.
- **Changes and recovery:** The preview is a snapshot. Source identity is
  checked before each rename, but changes can race those checks. Errors or
  cancellation stop the batch and can leave partial results. Renames are not
  registered with KIO undo. Per-folder Opus labels, colors, ratings, notes and
  pins remain keyed by old names.

- **Recommendation:** Review the preview and back up important items first.
  Use valid patterns and metadata fields present on every selected file; install
  KDE metadata extractors if needed. Use `(?i)` for case-insensitive regex
  matching and avoid expensive expressions. Use a filesystem that supports atomic
  no-replace renames. After an error or cancellation, inspect the displayed
  results and filesystem before retrying; manually rename completed items back
  if needed. Reapply per-folder annotations to new names and remove stale
  entries as appropriate.
  - Comment: [#372.7024683530][comment-372]
- **Alternative:** Try a separate batch renamer on copies.
  - Comment: [#372.7029676077](https://github.com/mikepadiernos/opus-file-manager/issues/372#issuecomment-7029676077)
- **Planned:** [M17](ROADMAP.md#m17-native-tagging) plans to track
  Opus-initiated moves and renames in its library; it does not promise to
  migrate existing per-folder annotations.
  - Comment: [#372.7024683530][comment-372]

## Filtered folder comparison

Tracked in [#376](https://github.com/mikepadiernos/opus-file-manager/issues/376).

- **Scope:** Comparison requires two separate local folders. Patterns match
  relative paths case-sensitively; excluded folders are not traversed. Links
  can be skipped or compared by target, but are never followed. When links are
  skipped, the matching subtree on the other side is also skipped.
- **Results:** Comparison examines at most 100,000 entries per folder, and
  filenames must round-trip through Qt's local filename encoding. Without
  content comparison, matching size and modification time do not prove equal
  contents. Results are snapshots that concurrent changes can invalidate, and
  can produce a large list.

- **Recommendation:** Use separate local folders and choose relative-path
  patterns carefully; enable content comparison when matching bytes matter.
  Compare smaller trees if the entry limit is reached, and rerun after changes.
  Use an external tool for remote locations or unsupported filenames.
  - Comment: [#376.7026216329][comment-376]
- **Alternative:** Use a separate graphical directory comparison.
  - Comment: [#376.7029677475](https://github.com/mikepadiernos/opus-file-manager/issues/376#issuecomment-7029677475)
- **Planned:** The [M23 rsync backend](ROADMAP.md#m23-integrations) is for
  folder synchronization; it does not promise remote support in this
  standalone comparison.
  - Comment: [#376.7026216329][comment-376]

## Folder synchronization

Tracked in [#377](https://github.com/mikepadiernos/opus-file-manager/issues/377).

- **Supported operations:** Synchronization is local-only. It copies,
  overwrites and permanently deletes regular files and creates missing folders.
  Links, directory deletion and type replacement are unsupported. Profiles
  save filters and comparison options, not item actions.
- **Metadata and filesystem:** Copies preserve basic permissions and timestamps,
  but not necessarily ownership, ACLs, extended attributes, sparse layout or
  other metadata. Linux `renameat2` with `RENAME_NOREPLACE` support is required.
- **Recovery and races:** Operations are not registered with KIO undo or
  transactional. Errors or cancellation can leave partial results, and the
  current operation may already have changed files. Private `.opus-sync-*`
  staging folders or backups may remain after errors or crashes. Snapshot and
  identity checks reduce but cannot eliminate races with other processes
  changing or moving items.

- **Recommendation:** Back up important data first, stop other writers and avoid
  moving the folders during synchronization. Review every planned overwrite and
  permanent deletion, inspect the completed-operation report and any reported
  `.opus-sync-*` folders after errors, and compare again before retrying. Use
  another tool for remote locations, links, directory deletion, type replacement
  or metadata that must be preserved.
  - Comment: [#377.7026633904][comment-377]
- **Alternative:** Use rsync separately with a reviewed dry run.
  - Comment: [#377.7029677824](https://github.com/mikepadiernos/opus-file-manager/issues/377#issuecomment-7029677824)
- **Planned:** [M23](ROADMAP.md#m23-integrations) proposes an optional rsync
  backend for local and SSH locations, with a reviewed dry run. It is not
  available yet and would still require confirmation and recovery precautions.
  - Comment: [#377.7026633904][comment-377]

## Metadata editing

Tracked in [#374](https://github.com/mikepadiernos/opus-file-manager/issues/374).

- **Formats and dependencies:** Edit one local JPEG or TIFF (description,
  artist and copyright) or MP3 (title, artist and album) at a time. ExifTool is
  required to read all supported formats and write images; MP3 writes require
  the KDE FileMetaData writer plugin. Unsupported formats, linked files and
  fields containing line breaks cannot be edited.
- **Preview and writes:** The preview covers only displayed values, not
  duplicate or unrelated metadata or media contents. File identity and
  timestamps are checked before writing but can race external changes. Writes
  are not undoable, and a failure or timeout may leave changes. The KDE writer
  does not report write errors: Opus re-reads displayed fields but cannot prove
  preservation of other data.

- **Recommendation:** Back up the original and review the preview. Install
  ExifTool and the KDE metadata writer plugin and ensure ExifTool runs in your
  environment. Use a regular local file without symbolic or hard links. Reopen
  and inspect media and metadata after writing or an error; use a dedicated
  metadata editor for other formats, fields, multiple values or stronger
  preservation guarantees.
  - Comment: [#374.7025134952][comment-374]
- **Alternative:** Try a dedicated tag editor for other audio fields.
  - Comment: [#374.7029676829](https://github.com/mikepadiernos/opus-file-manager/issues/374#issuecomment-7029676829)
- **Planned:** [M17](ROADMAP.md#m17-native-tagging) plans to read and map
  selected embedded metadata but reuses M09 for supported writes; it does not
  promise additional writable formats or preservation guarantees.
  - Comment: [#374.7025134952][comment-374]

## Transfer queue

Tracked in [#375](https://github.com/mikepadiernos/opus-file-manager/issues/375).

- **Coverage:** The queue handles pane, context-menu and gallery copy and move
  actions. Clipboard paste and drag-and-drop use KIO directly; they do not
  appear in the queue or use its conflict policy.
- **Scheduling and conflicts:** One job runs at a time by default; concurrency
  is configurable. Disabling queue scheduling starts jobs without a
  concurrency limit. Automatic overwrite replaces files and merges folders
  without asking.
- **Controls and lifetime:** Progress, speed and controls depend on the KIO
  job and worker. Cancellation may leave partial results. The queue exists only
  in memory: waiting jobs are discarded on application exit and active jobs may
  be canceled. Opus warns before closing a window while transfers are pending.
  External KIO progress interfaces may control a job independently of the queue
  display. KIO records undo when a queued copy or move begins, not while waiting.

- **Recommendation:** Use queued pane, context-menu or gallery copy and move
  actions and choose concurrency appropriate for the destination. Keep the
  Ask conflict policy when confirmation is needed; review the destination
  before using automatic overwrite. Use the normal KIO progress interface for
  paste and drag-and-drop. Check partial results after cancellation or errors
  and keep Opus open until queued work finishes. Resume externally paused jobs
  from the external interface. Inspect the KIO undo stack after a transfer
  starts.
  - Comment: [#375.7025250130][comment-375]
- **Alternative proposal:** Consider adding paste and drop jobs to the queue.
  - Comment: [#375.7029677171](https://github.com/mikepadiernos/opus-file-manager/issues/375#issuecomment-7029677171)
- **Planned:** [M23](ROADMAP.md#m23-integrations) proposes remote-profile
  transfers through the queue where available and Opus-owned job notifications;
  neither promises to route paste or drag-and-drop through the queue.
  - Comment: [#375.7025250130][comment-375]

## Custom commands

Tracked in [#373](https://github.com/mikepadiernos/opus-file-manager/issues/373).

- **Inputs:** Commands require a local working folder and local selections.
  Placeholders occupy whole arguments; a single empty literal argument and
  line breaks in templates or defaults cannot be represented in the editor.
  Preset defaults are stored as plaintext.
- **Execution:** Commands launch directly without an implicit shell, but
  chosen programs may interpret arguments as code. They run with the user's
  permissions and are not registered with KIO undo.
- **Output and process lifetime:** Displayed output is limited to 256 KiB.
  Stop terminates the launched process but may leave descendants running;
  closing the application can terminate the child.

- **Recommendation:** Copy remote inputs locally. Review the executable and
  every argument, run only trusted programs, and avoid passing untrusted paths
  to code-evaluating arguments or saving credentials in presets. Redirect large
  output to a file if needed, and inspect or stop remaining descendants after
  cancellation. Back up files before destructive commands and use external tools
  for unsupported argument templates or undo.
  - Comment: [#373.7024944292][comment-373]
- **Alternative:** Use a reviewed script instead of a complex inline template.
  - Comment: [#373.7029676403](https://github.com/mikepadiernos/opus-file-manager/issues/373#issuecomment-7029676403)

## File comparison

File comparison requires the `diff` executable and two local regular files.
Unified output is decoded as UTF-8, and comparison stops if output exceeds
4 MiB. Binary differences are reported without a visual binary diff.
([#311][issue-311])

- **Recommendation:** Copy remote files locally and use another comparison tool
  for non-UTF-8 text, large output or a visual binary diff. Difftastic can
  compare supported text files; `cmp` can check whether binary files differ,
  but does not provide a visual diff.
  - Comment: [#311.7021755780][comment-311]
- **Alternative:** Use a separate graphical comparison tool.
  - Comment: [#311.7029671783](https://github.com/mikepadiernos/opus-file-manager/issues/311#issuecomment-7029671783)

## Symbolic links

Symbolic-link creation supports existing local files and folders only. The new
link is created beside its target; another destination cannot be chosen.
([#312][issue-312])

- **Recommendation:** Use a suitable external tool when a different link
  destination or remote symbolic-link support is needed. For local paths,
  GNU `ln -s` can create a link at a chosen destination; remote support depends
  on the server and access method.
  - Comment: [#312.7021756400][comment-312]
- **Alternative:** Use an SFTP client with explicit symbolic-link support.
  - Comment: [#312.7029672139](https://github.com/mikepadiernos/opus-file-manager/issues/312#issuecomment-7029672139)

## Listing export

Listing export uses the current filtered rows and displayed values, even in
icon or compact view. By default it uses the visible details columns;
export-specific columns can be selected in Preferences, but are limited to
the supported listing fields. Output is local only, and wide PDF tables may
wrap or clip. CSV prefixes cells that could be interpreted as spreadsheet
formulas with an apostrophe. ([#314](https://github.com/mikepadiernos/opus-file-manager/issues/314))

- **Recommendation:** Check the filter and selected export columns before
  exporting, and reduce columns for wide PDF tables. Keep CSV formula protection
  when opening untrusted data in a spreadsheet; copy local output elsewhere if
  needed. GNU `find -print0` can produce a local, NUL-delimited filename list
  for compatible tools, but does not reproduce Opus filters, columns or CSV output.
  - Comment: [#314.7021757620][comment-314]
- **Alternative:** Produce a custom-field export with a separate script.
  - Comment: [#314.7029672795](https://github.com/mikepadiernos/opus-file-manager/issues/314#issuecomment-7029672795)

## Folder content chart

The chart shows measured logical sizes by immediate entry, not kind or
extension breakdowns or a largest-file drilldown. It includes hidden entries,
skips top-level symbolic links and entries without a known size, and relies on
KIO workers for recursive traversal. Scans can be canceled but have no entry
or time limit and can be costly; results are snapshots.
([#315](https://github.com/mikepadiernos/opus-file-manager/issues/315))

- **Recommendation:** Scan smaller folders when possible, cancel expensive
  scans and rescan when current results are needed. Use another tool for
  extension breakdowns, largest-file analysis or linked-directory sizes.
  `ncdu` can explore local directory usage; GNU `du -L` can follow directory
  symlinks, subject to cycles and repeated targets.
  - Comment: [#315.7021758275][comment-315]
- **Alternative:** Inspect local disk usage with QDirStat.
  - Comment: [#315.7029673162](https://github.com/mikepadiernos/opus-file-manager/issues/315#issuecomment-7029673162)

## Checksum files

Checksum files use UTF-8 GNU-style SHA-256 manifests and local regular files
in the same folder. Other algorithms, remote files, symbolic links, path
entries and non-UTF-8 names are unsupported. Manifests are limited to 10,000
entries. Files can change during hashing; cancellation near completion can
race with publication of the checksum file.
([#316](https://github.com/mikepadiernos/opus-file-manager/issues/316))

- **Recommendation:** Use local regular files in the same folder and keep
  manifests within 10,000 entries. Avoid changing files while hashing, and
  check whether an output file was published after cancellation. Use a trusted
  external checksum tool for unsupported algorithms or inputs. GNU coreutils
  provides `sha256sum` and other checksum commands for local files.
  - Comment: [#316.7021845379][comment-316]
- **Alternative:** Use a separate checksum tool for other algorithms.
  - Comment: [#316.7029673498](https://github.com/mikepadiernos/opus-file-manager/issues/316#issuecomment-7029673498)

## Pane folder comparison

Comparison covers immediate entries only. Files are compared by size and
modification time, not contents; subfolders are not compared. Results are
snapshots, and hidden or filtered entries may not be selectable. Large or
remote listings may be costly.
([#317](https://github.com/mikepadiernos/opus-file-manager/issues/317))

- **Recommendation:** Use file comparison for individual files or filtered
  folder comparison for local recursive comparisons, enabling content
  comparison when needed. For other cases, use a trusted recursive tool; GNU
  `diff -r` compares local directory trees. Show hidden files and clear pane
  filters before selecting differences; compare again after changes.
  - Comment: [#317.7021904155][comment-317]
- **Alternative:** Inspect a folder-sync comparison without applying changes.
  - Comment: [#317.7029673828](https://github.com/mikepadiernos/opus-file-manager/issues/317#issuecomment-7029673828)

## Split and combine files

Split and combine supports local regular files. Parts are raw numbered files;
combining uses the displayed order and cannot detect missing, altered or
incorrectly ordered parts. Inputs can change during processing. Failures or
cancellation can leave incomplete output, and many small parts can be
expensive. Existing outputs are never overwritten and new files are
owner-only. ([#318](https://github.com/mikepadiernos/opus-file-manager/issues/318))

- **Recommendation:** Check the complete part list and order before combining,
  keep originals and verify the reconstructed file with a checksum. Avoid
  changing inputs while processing. Inspect and remove unwanted partial outputs
  after errors or cancellation, choose a sensible part size, and adjust output
  permissions if sharing is needed. GNU `split` and `cat` are external
  alternatives; verify part order and handle output safely to avoid overwriting
  existing files. GNU `sha256sum` can verify reconstructed files against a
  trusted checksum.
  - Comment: [#318.7022199214][comment-318]
- **Alternative:** Create separate recovery information with PAR2.
  - Comment: [#318.7029674177](https://github.com/mikepadiernos/opus-file-manager/issues/318#issuecomment-7029674177)

## Embedded terminal

The embedded terminal requires the Konsole KPart. By default, new sessions
start in the active local folder, or in the home folder with a visible notice
for remote locations, and pane navigation does not change their directory.
Optional folder following instead switches between retained sessions or
creates new ones; it does not change a running shell's directory. Remote or
missing folders are reported without switching sessions. Sessions accumulate
until their tabs are closed. Hiding the panel keeps them running; closing the
window ends them and can affect running programs.
([#321](https://github.com/mikepadiernos/opus-file-manager/issues/321))

- **Recommendation:** Install the Konsole terminal component for embedded
  sessions; on Arch Linux it is supplied by the `konsole` package. For remote
  locations, connect using a suitable terminal tool or use a separate terminal
  configured for that service. Check the terminal working directory before
  running commands, close unneeded session tabs, and finish or safely stop
  programs before closing a tab or the window.
  - Comment: [#321.7022392302][comment-321]
- **Alternative:** Use a terminal multiplexer for independently retained sessions.
  - Comment: [#321.7029674455](https://github.com/mikepadiernos/opus-file-manager/issues/321#issuecomment-7029674455)
- **Planned:** [M23](ROADMAP.md#m23-integrations) proposes additional terminal
  providers and separate SSH sessions for SSH profiles; it does not promise
  embedded remote working directories or persistent sessions after exit.
  - Comment: [#321.7022392302][comment-321]

## Linked pane navigation

Linked navigation supports existing local folder trees rooted at the two
starting folders. It unlinks when navigation leaves either tree, a matching
folder is missing, a symbolic link resolves outside its tree, or a pane is
hidden. Tab and history changes also participate and may unlink. Filesystem
changes can invalidate folders between checks and navigation.
([#322](https://github.com/mikepadiernos/opus-file-manager/issues/322))

- **Recommendation:** Start from corresponding local folders with matching
  subfolder names. Check both panes after navigating, especially when
  directories may change; relink at a new pair of roots when needed. Navigate
  remote folders independently.
  - Comment: [#322.7022490696][comment-322]
- **Alternative:** Navigate the two locations independently.
  - Comment: [#322.7029674758](https://github.com/mikepadiernos/opus-file-manager/issues/322#issuecomment-7029674758)

## Cushion treemap

The treemap uses the folder content chart measurements: tiles represent
measured immediate entries and their relative logical sizes. Nested folders
are aggregated rather than shown simultaneously; activate a folder in the
list to rescan it. Empty, unmeasured and skipped entries have no visible
tile, and very small shares may be difficult to see. Measurements are
snapshots and the [folder content chart](#folder-content-chart) scan limits
also apply. ([#335](https://github.com/mikepadiernos/opus-file-manager/issues/335))

- **Recommendation:** Activate folders in the list to inspect their contents,
  and use the size table for empty, skipped or very small entries. Scan smaller
  folders and rescan when current results are needed. QDirStat is an external
  option for simultaneously visualizing nested disk usage.
  - Comment: [#335.7022647720][comment-335]
- **Alternative:** Use Filelight for a separate nested radial view.
  - Comment: [#335.7029675089](https://github.com/mikepadiernos/opus-file-manager/issues/335#issuecomment-7029675089)

## Duplicate finder

The duplicate finder recursively scans local folders for known regular files,
groups equal sizes and SHA-256 hashes, and allows checked file URLs to be
copied for manual review. Candidates can be selected by modification time or
folder and moved to Trash after confirmation. It rechecks affected groups
before asking, but files can change afterward. Symbolic links are skipped;
hard links may appear as separate paths, and unknown entry types are skipped.
Errors are shown, but scans are unbounded and matching hashes are not a
guarantee of byte identity.
([#336](https://github.com/mikepadiernos/opus-file-manager/issues/336))

- **Recommendation:** Review groups and errors, verify current file contents
  and paths independently, and keep at least one confirmed copy and backups
  before moving files to Trash. Scan smaller local folders if a scan is
  expensive. `jdupes` is an external option for comparing local files; its
  default listing is non-destructive, but deletion and other modifying options
  require care.
  - Comment: [#336.7022793474][comment-336]
- **Alternative:** Compare candidate files byte-for-byte before deletion.
  - Comment: [#336.7029675367](https://github.com/mikepadiernos/opus-file-manager/issues/336#issuecomment-7029675367)

## File-list minimap

The minimap samples at most 512 top-level listing rows rather than showing every
item or expanded child row. It tracks only the vertical scrollbar, so it
cannot navigate horizontally scrolling columns in compact view.
([#434](https://github.com/mikepadiernos/opus-file-manager/issues/434))

- **Recommendation:** Use the ordinary scrollbar or keyboard navigation for
  compact view and expanded folders, and treat the minimap as a sampled
  overview rather than a complete list.
  - Comment: [#434.7040730100](https://github.com/mikepadiernos/opus-file-manager/issues/434#issuecomment-7040730100)

## Settings backups

Backups are unencrypted archives. Credential filtering recognizes certain key
names and value patterns, but cannot guarantee that all sensitive data is
removed; saved searches and other settings may still contain private text or
paths. A matching value can instead exclude an entire settings entry. Current
session tabs and expanded folder locations, saved custom commands and shared
KDE settings are excluded; restoring preserves the corresponding current
state rather than recovering it from the backup.
([#433](https://github.com/mikepadiernos/opus-file-manager/issues/433))

- **Recommendation:** Review backup contents privately before sharing or
  restoring, protect the archive with restrictive access and encryption, and
  avoid putting secrets in settings. Keep separate protected backups of
  excluded commands and other needed data; after restoration, verify
  important settings and recreate excluded items as needed.
  - Comment: [#433.7040727966](https://github.com/mikepadiernos/opus-file-manager/issues/433#issuecomment-7040727966)

## Build, installation and references

For build requirements and installer behaviour, see
[Build and run](../README.md#build-and-run). For the scope of the upstream
inventories, see [Behaviour references](README.md#behaviour-references).

## Installing optional alternatives

These tools can be used independently for the workarounds above. Some are also
required by particular Opus features, as noted in their caveats; none is a
dependency of every feature. Install only what you need and check each tool's
format, platform and security limitations. For example, `difftastic` provides
`difft` for text comparison and `ncdu` provides `ncdu` for browsing directory
usage.

On Arch Linux, install an official package with `sudo pacman -S <package>`;
`paru -S <package>` can also install repository packages. With Homebrew, use
`brew install <formula>`. For the listed mise registry names, `mise use -g
<name>@latest` installs the tool and adds it to your global mise configuration;
ensure mise is activated in your shell. A dash means no mise option is listed
here, not that installation through another backend is impossible.

| Tool | Arch package | Homebrew formula | mise name |
| --- | --- | --- | --- |
| 7-Zip | `7zip` | `sevenzip` | `7zip` |
| rclone | `rclone` | `rclone` | `rclone` |
| fd | `fd` | `fd` | `fd` |
| bat | `bat` | `bat` | `bat` |
| Neovim | `neovim` | `neovim` | `neovim` |
| Difftastic | `difftastic` | `difftastic` | `difftastic` |
| Pandoc | `pandoc-cli` | `pandoc` | `pandoc` |
| ImageMagick | `imagemagick` | `imagemagick` | `imagemagick` |
| FFmpeg | `ffmpeg` | `ffmpeg` | - |
| ExifTool | `perl-image-exiftool` | `exiftool` | `exiftool` |
| ncdu | `ncdu` | `ncdu` | - |
| GNU coreutils | `coreutils` | `coreutils` | `coreutils` |
| GNU diffutils | `diffutils` | `diffutils` | - |
| GNU findutils | `findutils` | `findutils` | - |
| ripgrep | `ripgrep` | `ripgrep` | `ripgrep` |
| jdupes | `jdupes` | `jdupes` | - |
| QDirStat | `qdirstat` | - | - |

Homebrew's GNU coreutils commands are normally prefixed with `g`, for example
`gdu` and `gsha256sum`; Homebrew's GNU diffutils commands likewise use names
such as `gdiff` and `gcmp`, and GNU findutils uses `gfind`. The mise
`coreutils` entry installs uutils coreutils, not GNU coreutils; check
compatibility before relying on GNU-specific behaviour. KDE components
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
[comment-317]: https://github.com/mikepadiernos/opus-file-manager/issues/317#issuecomment-7021904155
[comment-318]: https://github.com/mikepadiernos/opus-file-manager/issues/318#issuecomment-7022199214
[comment-321]: https://github.com/mikepadiernos/opus-file-manager/issues/321#issuecomment-7022392302
[comment-322]: https://github.com/mikepadiernos/opus-file-manager/issues/322#issuecomment-7022490696
[comment-335]: https://github.com/mikepadiernos/opus-file-manager/issues/335#issuecomment-7022647720
[comment-336]: https://github.com/mikepadiernos/opus-file-manager/issues/336#issuecomment-7022793474
[comment-362]: https://github.com/mikepadiernos/opus-file-manager/issues/362#issuecomment-7023269364
[comment-372]: https://github.com/mikepadiernos/opus-file-manager/issues/372#issuecomment-7024683530
[comment-373]: https://github.com/mikepadiernos/opus-file-manager/issues/373#issuecomment-7024944292
[comment-374]: https://github.com/mikepadiernos/opus-file-manager/issues/374#issuecomment-7025134952
[comment-375]: https://github.com/mikepadiernos/opus-file-manager/issues/375#issuecomment-7025250130
[comment-376]: https://github.com/mikepadiernos/opus-file-manager/issues/376#issuecomment-7026216329
[comment-377]: https://github.com/mikepadiernos/opus-file-manager/issues/377#issuecomment-7026633904
