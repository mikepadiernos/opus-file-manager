# ⚠️ Caveats and limitations

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap](https://img.shields.io/badge/roadmap-M01%E2%80%93M32-8957e5?style=flat-square)](ROADMAP.md)
[![Reported bugs](https://img.shields.io/badge/reported-bugs-cf222e?style=flat-square)](BUGS.md)
[![Project board](https://img.shields.io/badge/project-board-6e7781?style=flat-square&logo=github)](https://github.com/users/mikepadiernos/projects/3)

Use this guide to find limitations, precautions and practical recommendations
for current features and, where marked, planned features. Tracked caveats link
to their issues and recommendation comments; alternatives supplement the primary
recommendation and external tools are separate applications. Confirmed defects
are in [reported bugs](BUGS.md), and untested scenarios in
[edge cases](EDGE.md).

> [!NOTE]
> Proposed improvements are not implemented features. The
> [roadmap](ROADMAP.md) defines scope and status.

> [!TIP]
> Each caveat opens with a short description and its tracking issue, lists
> the current limitations, then gives a **Recommendation** for what to do
> today with a link to the matching issue comment. Alternatives and external
> tools follow where useful; **Planned** and **Proposed improvements** are not
> yet implemented.

## Find a caveat

- **🗂️ Browsing and viewing:** [Archives](#archives),
  [folder metadata and listings](#folder-metadata-and-listings),
  [planned password-gated folders](#planned-password-gated-folders),
  [previews and gallery](#previews-and-gallery), [editor](#editor),
  [file-list minimap](#file-list-minimap), [Yazi mode](#yazi-mode),
  [Yazi mode settings](#yazi-mode-settings)
- **🔎 Search and operations:** [Search](#search),
  [batch rename](#batch-rename), [metadata editing](#metadata-editing),
  [transfer queue](#transfer-queue), [custom commands](#custom-commands),
  [symbolic links](#symbolic-links), [listing export](#listing-export),
  [checksums](#checksum-files), [split and combine](#split-and-combine-files),
  [native tagging](#native-tagging), [tagging settings](#tagging-settings)
- **🔀 Comparison and analysis:**
  [Filtered folder comparison](#filtered-folder-comparison),
  [folder synchronization](#folder-synchronization),
  [file comparison](#file-comparison),
  [folder content chart](#folder-content-chart),
  [pane comparison](#pane-folder-comparison), [treemap](#cushion-treemap),
  [duplicate finder](#duplicate-finder)
- **🧭 Workspaces and panes:** [Workspace layouts](#workspace-layouts),
  [workspace settings](#workspace-settings),
  [linked navigation](#linked-pane-navigation)
- **🎨 Appearance:** [Themes](#themes), [theme settings](#theme-settings),
  [branding](#branding),
  [native title and header padding](#native-title-and-header-padding)
- **🌐 Languages:** [Jalali dates](#jalali-dates),
  [translation format overrides](#translation-format-overrides)
- **🛠️ Setup and tools:** [Desktop integration](#desktop-integration),
  [embedded terminal](#embedded-terminal),
  [settings backups](#settings-backups),
  [build and installation](#build-installation-and-references),
  [installing optional tools](#installing-optional-alternatives)

## Archives

Archive creation, extraction and integrity testing have these limits, each
tracked in its own issue:

- **Local files:** Archive creation and integrity testing require local files.
  When compressing a mixed selection, Plasma Opus skips items without local
  paths. Password-protected extraction also requires a local archive and
  destination. ([#265][issue-265])

  - **Recommendation:** Copy remote inputs to a local folder before creating,
    testing or extracting an archive; copy the result back afterward. For
    supported remote services, `rclone` is an alternative for transferring
    files.
    - Comment: [#265.7021749894][comment-265]
  - **Alternative:** Stage remote files locally with Dolphin.
    - Comment:
      [#265.7029669109](https://github.com/mikepadiernos/plasma-opus/issues/265#issuecomment-7029669109)
  - **Planned:** [M27 remote connection profiles](ROADMAP.md#m27-integrations)
    are not a commitment to support remote archive creation or testing.
    - Comment: [#265.7021749894][comment-265]

- **Password-protected archives:** Built-in creation supports AES-encrypted ZIP
  only and rejects symbolic links and other unsupported file types. ZIP entry
  names are not encrypted, and 7z archives are created without a password.
  Extraction rejects links, unsafe paths and unsupported entry types. If
  creation or extraction fails, a partial archive or extraction folder may
  remain. ([#255][issue-255], [#256][issue-256])

  - **Recommendation:** Avoid sensitive ZIP entry names, and check for partial
    output after failures before retrying. Use another trusted archive tool if
    the required format or input type is unsupported, such as 7-Zip for formats
    it supports. For encrypted filenames, 7-Zip can encrypt 7z headers; ZIP
    still exposes entry names. Check its handling of links and paths before use.
    - Comment: [#255.7021741358][comment-255]
    - Comment: [#256.7021742119][comment-256]
  - **Alternatives:**
    - Encrypt an entire archive separately with GnuPG.
      - Comment:
        [#255.7029665897](https://github.com/mikepadiernos/plasma-opus/issues/255#issuecomment-7029665897)
    - Extract into a fresh staging directory first.
      - Comment:
        [#256.7029666231](https://github.com/mikepadiernos/plasma-opus/issues/256#issuecomment-7029666231)
    - For encrypted 7z archives with encrypted file names, choose **More
      Options in Ark…** in the Compress dialog; see
      [Ark integration](#ark-integration).

- **Split archives:** **Split into volumes** writes the whole archive, then cuts
  it into raw numbered parts of at most the chosen size, such as
  `photos.7z.001`, as 7-Zip does. They are not ZIP spanned or RAR multi-volume
  archives, so only 7-Zip opens the first part directly; KIO archive browsing
  and most other tools need the parts joined first. Splitting needs free space
  for the whole archive and its parts, and creates the parts with owner-only
  permissions. An archive smaller than the volume size stays a single file. If
  splitting fails, the parts written so far are removed and the whole archive
  is kept. ([#732][issue-732])

  - **Recommendation:** Keep the parts together and open the first with 7-Zip,
    or join them with **File › Combine Files**, naming the result after the
    original archive. Check free space before splitting, and adjust the parts'
    permissions before sharing them with other local users.
    - Comment: [#732.7200937321][comment-732]
  - **Alternative:** For RAR multi-volume archives, choose **More Options in
    Ark…**; Ark offers them only when RAR's proprietary `rar` program is
    installed.
    - Comment: [#732.7200937321][comment-732]

- **Encrypted 7z:** Extraction uses KArchive and can fail even for valid
  archives. Integrity testing uses the `7z` executable when available and
  otherwise falls back to KArchive. Passwords containing a newline, carriage
  return or NUL cannot be passed to `7z`. ([#36][issue-36], [#255][issue-255])

  - **Recommendation:** If extraction fails, try a trusted archive tool that
    supports encrypted 7z. For archives you create and intend to test with `7z`,
    choose a password without those characters. 7-Zip is one external option;
    enter passwords at its prompt rather than putting them in command arguments.
    - Comment: [#36.7021740511][comment-36]
    - Comment: [#255.7021741358][comment-255]
  - **Alternative:** Try standalone Ark with its 7z backend, or **File › Open
    in Ark**.
    - Comment:
      [#36.7029665511](https://github.com/mikepadiernos/plasma-opus/issues/36#issuecomment-7029665511)
  - **Ark:** **More Options in Ark…** creates encrypted 7z archives through
    Ark's dialog. It does not change built-in extraction or guarantee safe
    password handling; see [Ark integration](#ark-integration).
    - Comment: [#36.7021740511][comment-36]

### Ark integration

Plasma Opus hands archives to Ark, KDE's archiving tool, when it is installed
([#313][issue-313], [#280](https://github.com/mikepadiernos/plasma-opus/issues/280),
[#647](https://github.com/mikepadiernos/plasma-opus/issues/647)):

- **Current behaviour:** **File › Open in Ark** opens one local archive in
  Ark's own window, where entries can be added, renamed or removed and
  encrypted archives opened with Ark's password prompt. **Extract to Other
  Pane** uses Ark only for local formats that no KIO archive worker can
  browse, such as RAR. It creates a new, uniquely named folder in the other
  pane's local folder and runs Ark in batch mode there, so nothing existing is
  overwritten. **More Options in Ark…** in the Compress dialog opens Ark's own
  Compress dialog for the selection, with encrypted 7z, encrypted file names,
  compression levels and multi-volume archives where the installed backends
  support them. **File › Add to Archive…** adds the selection to an archive
  Ark can write, after a confirmation. Plasma Opus reads which formats Ark can
  open and write from Ark's installed backend plugins and the programs they
  need, and refuses formats Ark can only extract, such as RAR without `rar`.
- **Limits:** Ark reports its own progress, password prompts and errors;
  Plasma Opus reports only a missing Ark or a failed start. If Ark fails or is
  cancelled, the new folder stays, possibly empty. **Add to Archive** changes
  the archive in place, and Ark replaces an entry with the same name; entries
  are named relative to the first selected item's folder. Ark chooses where its
  Compress dialog saves, so the non-overwriting name and pane choices of the
  built-in Compress dialog do not apply. Remote archives and remote
  destinations are not handed to Ark. Formats KIO can browse, including 7z
  and ZIP, still use the built-in extraction, and **Extract
  Password-Protected Archive** still uses KArchive.
- **Password exposure:** Ark's
  [7z plugin](https://github.com/KDE/ark/blob/master/plugins/cli7zplugin/cliplugin.cpp)
  defines a `-p$Password` command-line switch. Plasma Opus never passes a
  password to Ark, but a password typed into Ark's prompt may appear in the
  `7z` process arguments, which other local users can read on some systems.

- **Recommendation:** For encrypted 7z archives that the built-in extraction
  cannot open, use **Open in Ark** with its 7z backend installed. On shared
  systems, avoid typing sensitive passwords into Ark, or use 7-Zip at its own
  prompt instead. Remove an empty folder left by a cancelled Ark extraction.
  Before **Add to Archive**, keep a copy of an archive you cannot recreate, and
  check that no entry with the same name should be kept.
  - Comment: [#313.7021757012][comment-313]
- **Alternative proposal:** Consider a user-controlled external archiver
  handoff.
  - Comment:
    [#313.7029672463](https://github.com/mikepadiernos/plasma-opus/issues/313#issuecomment-7029672463)

## Folder metadata and listings

Folder metadata, flat view and folder sizes have these limits, each tracked in
its own issue:

- **Metadata compatibility:** Plasma Opus supports a subset of Max File
  Manager's local `.mfm.json` metadata. Writing sanitizes the file and may
  discard unsupported fields or values, so back up existing metadata before
  editing. The sanitizer retains the upstream hide-from-parent flag, but Plasma
  Opus does not apply it to listings. ([#257][issue-257], [#258][issue-258])

  - **Recommendation:** Back up `.mfm.json` before editing it in Plasma Opus and
    do not rely on hide-from-parent to hide entries in listings. A text editor
    can inspect a backup, but cannot make Plasma Opus apply unsupported
    metadata.
    - Comment: [#257.7021742864][comment-257]
    - Comment: [#258.7021743599][comment-258]
  - **Alternatives:**
    - Keep a versioned copy of metadata outside Plasma Opus.
      - Comment:
        [#257.7029666537](https://github.com/mikepadiernos/plasma-opus/issues/257#issuecomment-7029666537)
    - Use ordinary hidden filenames where appropriate.
      - Comment:
        [#258.7029666857](https://github.com/mikepadiernos/plasma-opus/issues/258#issuecomment-7029666857)
  - **Native tagging:** [M23 libraries](#native-tagging) are separate from
    `.mfm.json` and do not make its writes lossless.
    - Comment: [#257.7021742864][comment-257]

- **Metadata limits:** Metadata files are limited to 4,096 bytes, notes to 500
  characters and pins to 200 per folder. Folder-format rules are stored in local
  metadata; remote locations use defaults unless the pane's format is locked.
  ([#263][issue-263])

  - **Recommendation:** Keep metadata within these limits; lock the pane format
    when the same format is needed while browsing remote locations.
    - Comment: [#263.7021748197][comment-263]
  - **Alternative:** Keep longer notes in a separate file.
    - Comment:
      [#263.7029668469](https://github.com/mikepadiernos/plasma-opus/issues/263#issuecomment-7029668469)
  - **Native tagging:** [M23 library fields](#native-tagging) are separate
    from folder notes and do not remove these metadata limits.
    - Comment: [#263.7021748197][comment-263]

- **Flat view:** The `filenamesearch:/` KIO worker is required and is normally
  supplied by `kio-extras`. Create New, New Folder and Paste are disabled, and
  drops are refused while flat view is active. ([#261][issue-261])

  - **Recommendation:** Install the worker if flat view is unavailable, and
    leave flat view before creating, pasting or dropping files. For local
    filename searches, `fd` or GNU `find` is an alternative; neither replaces
    KIO flat view.
    - Comment: [#261.7021745774][comment-261]
  - **Alternative:** Use live search and reveal a result in its folder.
    - Comment:
      [#261.7029667831](https://github.com/mikepadiernos/plasma-opus/issues/261#issuecomment-7029667831)

- **Folder sizes:** Background calculations skip symbolic links to directories.
  Calculated sizes are snapshots and can become stale when changes occur outside
  watched directories. View settings offer optional periodic refresh for visible
  panes; it is off by default. Automatic calculation and periodic refresh can be
  costly, especially for remote locations. ([#262][issue-262])

  - **Recommendation:** Enable periodic refresh only when its cost is
    acceptable; GNU `du -L` can follow directory symlinks for local paths. Be
    mindful of cycles and repeated targets.
    - Comment: [#262.7021746538][comment-262]
  - **Alternative:** Take an on-demand size snapshot with GNU `du`.
    - Comment:
      [#262.7029668154](https://github.com/mikepadiernos/plasma-opus/issues/262#issuecomment-7029668154)

## Planned password-gated folders

The [proposed Plasma Opus password gate](ROADMAP.md#m27-integrations) is not
implemented ([#406](https://github.com/mikepadiernos/plasma-opus/issues/406);
feature [#402](https://github.com/mikepadiernos/plasma-opus/issues/402)). As
proposed, it has these limits:

- **Not security:** It would be a Plasma Opus-only prompt, not encryption or
  filesystem access control.
- **Bypass:** Other file managers and processes with the same user permissions
  could access the underlying folder without the password.
- **Filesystem permissions:** Standard filesystem permissions and ACLs
  distinguish users and groups, not applications running with the same
  credentials.

- **Recommendation:** For confidential files, use a separate encrypted vault
  such as [KDE Plasma Vault](https://github.com/KDE/plasma-vault). Applications
  with access can use files while the vault is unlocked and mounted; when it is
  locked, the decrypted contents are unavailable through the mount. This uses
  encryption, unlike the requested gate. Back up the data and protect the
  passphrase; handle any original unencrypted copies separately. Backend
  availability and behaviour vary.
  - Comment:
    [#406.7040077820](https://github.com/mikepadiernos/plasma-opus/issues/406#issuecomment-7040077820)
- **Alternatives:**
  - **Cooperative prompts:** Explore a shared gate only with file managers that
    explicitly support it. Unsupported apps and direct access can bypass it;
    cross-manager support is not established and this is not security.
    - Comment:
      [#406.7040078439](https://github.com/mikepadiernos/plasma-opus/issues/406#issuecomment-7040078439)
  - **Separate identities:** Use ownership and ACLs to restrict access to
    selected user or service identities. Apps with the same credentials retain
    the same ordinary access; this does not add a folder-password prompt.
    Privileged users can override controls, and administration requires care.
    - Comment:
      [#406.7040079139](https://github.com/mikepadiernos/plasma-opus/issues/406#issuecomment-7040079139)
  - **Application sandboxing:** Restrict a controlled set of sandboxed apps and
    grant chosen apps access through supported sandbox or portal mechanisms.
    Unsandboxed same-user processes can still access the folder; support and
    permissions vary, so this is not universal password enforcement.
    - Comment:
      [#406.7040079777](https://github.com/mikepadiernos/plasma-opus/issues/406#issuecomment-7040079777)

## Previews and gallery

Previews, the gallery and batch image conversion have these limits, each
tracked in its own issue:

- **Preview dependencies:** Document previews require a suitable installed
  KPart, media playback requires a Qt Multimedia backend, and available metadata
  depends on KDE extraction plugins. ([#259][issue-259])

  - **Recommendation:** Install the relevant KPart, multimedia backend or
    extraction plugin for the file types you need. For supported media, FFmpeg's
    `ffplay`, when included in the installed build, is an external playback
    alternative.
    - Comment: [#259.7021744368][comment-259]
  - **Alternative:** Open the file with a separate application.
    - Comment:
      [#259.7029667136](https://github.com/mikepadiernos/plasma-opus/issues/259#issuecomment-7029667136)

- **Preview limits:** Images are limited to 8 MiB and 40 megapixels. Text
  previews use UTF-8 and default to the first 1 MiB; lines longer than 16,384
  characters are shown without syntax highlighting. Remote previews download to
  a temporary file and use the same limits, or 128 MiB for other file types.
  ([#259][issue-259], [#264][issue-264])

  - **Recommendation:** Open files in a suitable external application when
    previews are incomplete, exceed these limits or require another encoding.
    For local text files, `bat` or Neovim may be useful.
    - Comment: [#259.7021744368][comment-259]
    - Comment: [#264.7021749088][comment-264]
  - **Alternative:** Use Kate or KWrite for long-line text.
    - Comment:
      [#264.7029668772](https://github.com/mikepadiernos/plasma-opus/issues/264#issuecomment-7029668772)
  - **Planned:** [M27 editor integrations](ROADMAP.md#m27-integrations)
    launch separate editors; they do not increase Plasma Opus preview limits.
    - Comment: [#264.7021749088][comment-264]

- **Rendered previews:** HTML and Markdown previews disable links and external
  resources. ([#267][issue-267])

  - **Recommendation:** Open trusted documents in an appropriate external
    application if links or external resources are needed.
    - Comment: [#267.7021751490][comment-267]
  - **Alternative:** Use a separate browser for trusted rendered documents.
    - Comment:
      [#267.7029669771](https://github.com/mikepadiernos/plasma-opus/issues/267#issuecomment-7029669771)

- **Gallery:** The gallery supports local files only. Gallery images and batch
  conversion inputs are limited to 128 MiB and 40 megapixels. Saving edits or
  converting images does not preserve the original image metadata.
  ([#266][issue-266], [#260][issue-260])

  - **Recommendation:** Work on local copies within the limits and keep the
    originals when their metadata must be preserved. ExifTool can inspect and
    copy supported metadata separately; verify the result for the target format.
    ImageMagick can process supported local images outside Plasma Opus's
    gallery, subject to its own resource and format limits.
    - Comment: [#266.7021750778][comment-266]
    - Comment: [#260.7021745029][comment-260]
  - **Alternatives:**
    - Keep the metadata-bearing original alongside an edited copy.
      - Comment:
        [#260.7029667492](https://github.com/mikepadiernos/plasma-opus/issues/260#issuecomment-7029667492)
    - Try a separate image viewer for a local copy.
      - Comment:
        [#266.7029669439](https://github.com/mikepadiernos/plasma-opus/issues/266#issuecomment-7029669439)

- **Batch conversion:** Conversion reads files on disk, not unsaved gallery
  edits, and turns animated images into still images. The conversion dialog
  warns about both limitations. ([#303][issue-303])

  - **Recommendation:** Save gallery edits before conversion; use an
    animation-capable tool such as FFmpeg or ImageMagick when animation must be
    retained, and verify that the chosen output format supports it.
    - Comment: [#303.7021753023][comment-303]
  - **Alternative proposal:** Consider a future animation-aware conversion
    backend.
    - Comment:
      [#303.7029670421](https://github.com/mikepadiernos/plasma-opus/issues/303#issuecomment-7029670421)

- **Conversion output:** The destination filesystem must support atomic
  no-replace renames, which prevent overwriting files, including those created
  after preview. Unsupported filesystems cause conversion failures. Converted
  files have owner-only permissions; change permissions if needed before sharing
  them. ([#302][issue-302], [#304][issue-304])

  - **Recommendation:** Convert to a filesystem that supports atomic no-replace
    renames, then copy the output if needed. Review permissions before sharing;
    for local files, `chmod` can grant only the access needed.
    - Comment: [#302.7021752278][comment-302]
    - Comment: [#304.7021753785][comment-304]
  - **Alternatives:**
    - Use a separate conversion tool with a fresh destination.
      - Comment:
        [#302.7029670102](https://github.com/mikepadiernos/plasma-opus/issues/302#issuecomment-7029670102)
    - Use a targeted ACL when sharing converted output.
      - Comment:
        [#304.7029670734](https://github.com/mikepadiernos/plasma-opus/issues/304#issuecomment-7029670734)

## Editor

The text editor's drafts, Markdown handling and editing in the preview have
these limits, each tracked in its own issue:

- **Draft storage:** Drafts are limited to 8 MiB of UTF-8 text and are stored in
  the application's private data directory, not in the original file. If draft
  storage fails, save the document before switching files or closing.
  ([#307][issue-307])

  - **Recommendation:** Save work to the original file regularly and use an
    external editor such as Neovim for documents that exceed the draft limit.
    - Comment: [#307.7021754467][comment-307]
  - **Alternative:** Use a standalone editor and an explicit backup copy.
    - Comment:
      [#307.7029671086](https://github.com/mikepadiernos/plasma-opus/issues/307#issuecomment-7029671086)
  - **Planned:** [M27 editor integrations](ROADMAP.md#m27-integrations) are
    separate applications and do not expand Plasma Opus draft storage.
    - Comment: [#307.7021754467][comment-307]

- **Draft retention:** Drafts are retained indefinitely by default. An optional
  age limit removes expired drafts unless they are locked by an active editor.
  Legacy drafts from versions without locking remain until explicitly discarded.
  Restored drafts remain until discarded or expired, protecting another editor's
  changes during recovery. ([#307][issue-307])

  - **Recommendation:** Set an age limit if appropriate and explicitly discard
    unneeded legacy or restored drafts after checking their contents.
    - Comment: [#307.7021754467][comment-307]

- **Markdown resources:** Markdown preview, visual editing and PDF export do not
  load local or external resources. Links are also disabled in the preview.
  ([#267][issue-267])

  - **Recommendation:** Use a trusted external renderer when a document or PDF
    needs embedded resources or working links. Pandoc can render local
    resources; PDF output requires an appropriate PDF engine.
    - Comment: [#267.7021751490][comment-267]

- **Visual Markdown editing:** Qt's Markdown subset can normalize the source or
  remove unsupported markup and HTML. Visual editing requires confirmation and
  preserves a recovery snapshot when drafts are enabled. ([#309][issue-309])

  - **Recommendation:** Keep a backup and use source editing when preserving
    unsupported markup is important; Neovim is one external source editor.
    - Comment: [#309.7021755164][comment-309]
  - **Alternative:** Compare a saved copy against the original before adopting
    edits.
    - Comment:
      [#309.7029671476](https://github.com/mikepadiernos/plasma-opus/issues/309#issuecomment-7029671476)
  - **Planned:** [M27 editor integrations](ROADMAP.md#m27-integrations) do not
    change the built-in visual editor's Markdown limitations.
    - Comment: [#309.7021755164][comment-309]

- **Editing in the preview:** The preview's **Edit** action edits only local,
  writable text files up to 10 MiB. It refuses binary content and files that
  KTextEditor opens read-only, such as unsupported encodings or very long
  lines, and remote text previews report that they can't be edited there.
  Moving a session to the editor panel keeps the text and its unsaved state,
  but not the undo history, cursor position or visual Markdown mode. A file
  open in both the preview and the editor panel is two independent documents.
  ([#755][issue-755])

  - **Recommendation:** Use the preview for quick edits to local text files,
    and the editor panel or an external editor such as Kate or Neovim for
    remote, large, binary or unusually encoded files. Move a session to the
    editor panel before you need its undo history, and edit a file in only
    one place at a time.
    - Comment: [#755.7207460156][comment-755]

## File-list minimap

The file-list minimap has these limits
([#434](https://github.com/mikepadiernos/plasma-opus/issues/434)):

- **Sampling:** The minimap samples at most 512 top-level listing rows rather
  than showing every item or expanded child row.
- **Scrolling:** It tracks only the vertical scrollbar, so it cannot navigate
  horizontally scrolling columns in compact view.

- **Recommendation:** Use the ordinary scrollbar or keyboard navigation for
  compact view and expanded folders, and treat the minimap as a sampled overview
  rather than a complete list.
  - Comment:
    [#434.7040730100](https://github.com/mikepadiernos/plasma-opus/issues/434#issuecomment-7040730100)

## Yazi mode

The M25 [Yazi mode](yazi-mode.md) has these limits
([#723](https://github.com/mikepadiernos/plasma-opus/issues/723)):

- **Scope:** Yazi mode is a native Qt/KDE browsing mode inspired by Yazi. It
  doesn't run Yazi or read its configuration, keymaps, themes or plugins.
- **Locations:** Search results, recent items, tags, `remote:/` and
  application lists aren't supported; the pane falls back to the normal view
  there. Remote folders, archives, `trash:/` and `desktop:/` are limited: the
  parent column and preview may load slowly, and listings stop after 10
  seconds. Archives are recognised by file extension.
- **Preview:** A folder preview lists at most 500 entries. Remote files are
  downloaded for preview up to the existing size caps; there is no setting
  that stops remote preview downloads. Oversized text is truncated rather
  than refused. Hiding the preview stops media playback, and media may render
  poorly in a narrow column.
- **Keys:** With Vim-style keys, bound letters and digits no longer start
  type-ahead in the columns, <kbd>Space</kbd> no longer opens the gallery, and
  <kbd>Ctrl</kbd>+<kbd>D</kbd> (Add to Favorites) is overridden there.
  <kbd>Ctrl</kbd>+<kbd>C</kbd> doesn't cancel a search because it is Copy.
  With Standard keys, <kbd>Left</kbd> and <kbd>Right</kbd> mean Parent and
  Open in icon and card views. <kbd>Caps Lock</kbd> changes letter bindings.
- **File actions:** <kbd>c</kbd> and <kbd>m</kbd> copy or move straight to
  the other pane like <kbd>F5</kbd> and <kbd>F6</kbd>, so they follow the
  transfer setting **Existing destination**; with **Overwrite automatically**
  they overwrite without asking. <kbd>u</kbd> undoing a copy deletes the
  copies after KIO asks. Window shortcuts such as <kbd>Delete</kbd> don't fall
  back to the focused item when nothing is selected. Paste
  and trash don't appear in the Yazi transfer indicator.
- **Selection:** Marked items stay selected while moving, unlike the normal
  view. In grid views, range selection follows item order.
- **Search:** The status row only tracks searches started with <kbd>s</kbd>
  or <kbd>S</kbd> from that pane, and those keys change the Search panel's
  scope settings.
- **Persistence:** A pane follows the default column ratios until they are
  dragged; other panes pick up a new default only when settings are applied
  again. Existing workspaces tie Yazi state to the views option until their
  Yazi checkbox is changed. When startup opens the home folder or fixed
  folders, panes start with their default mode from **Preferences › Yazi
  Mode › Activation**, which is off unless changed.
- **Performance:** The first listing of very large folders (10,000+ items)
  is slow with or without Yazi mode.

- **Recommendation:**
  - **Scope:** Use Yazi itself, for example in the embedded terminal, when
    you need its plugins or configuration.
  - **Locations:** Browse unsupported locations in the normal view; the mode
    resumes at the next supported folder. Hide the preview with **View › Yazi
    Preview** in slow remote folders.
  - **Preview:** Switch the column to file information (<kbd>i</kbd> with
    Vim-style keys) on slow connections, and open large files with **Open
    With**.
  - **Keys:** Keep the Standard preset if you rely on type-ahead, the gallery
    on <kbd>Space</kbd> or <kbd>Ctrl</kbd>+<kbd>D</kbd>. Cancel searches with
    <kbd>Escape</kbd> or **Cancel**.
  - **File actions:** Keep **Existing destination** at **Ask** (the default)
    before using <kbd>c</kbd> and <kbd>m</kbd>. Select items explicitly before
    using window shortcuts.
  - **Selection:** Press <kbd>Escape</kbd> to clear marks before ordinary
    selection.
  - **Search:** Check the Search panel's scope after using <kbd>s</kbd> or
    <kbd>S</kbd>.
  - **Persistence:** Drag the columns once in each pane to fix its ratios,
    or use **Restore Yazi Mode Defaults** in Preferences to make every pane
    follow the defaults again. Turn on a workspace's **Yazi mode** option to
    restore Yazi state independently of views.
  - **Performance:** Filter or split very large folders.
  - Comment:
    [#723.7180859654](https://github.com/mikepadiernos/plasma-opus/issues/723#issuecomment-7180859654)
- **Proposed improvements:**
  - A remote preview download policy shared with the preview panel.
  - Track paste and trash jobs in the Yazi transfer indicator.
  - Faster first listings of very large folders.

## Yazi mode settings

The M26 [Yazi mode settings](yazi-mode.md#settings) under **Preferences ›
Yazi Mode** have these limits
([#725](https://github.com/mikepadiernos/plasma-opus/issues/725)):

- **Activation:** Each pane's default mode and restore option apply only
  when a window opens; open panes keep their current mode. The restore option
  matters only when **On startup** restores the last session. Sessions saved
  before M25 restore the mode as off. "First pane" is the left pane, or the
  top pane when the panes are stacked.
- **Columns:** Panes with their own widths, from dragging, their session or
  a workspace, keep them when the default widths change, so Apply can seem to
  do nothing. After a drag, the Columns tab shows the widths in pixels, such
  as 180, 540 and 360. The narrow-pane limits are fixed: the preview column
  hides below about 480 pixels and the parent column below about 320.
- **Keyboard:** Preset keys can't be edited one by one. The conflict review
  checks the window that opened Preferences, leaves out shortcuts that only
  work while their own widget has focus and sees shortcut edits only after
  they are saved. **View › Vim-Style Keys in Yazi Mode** switches presets
  without reviewing conflicts.
- **Preview:** There is no Yazi-specific size, provider or remote-download
  setting. Remote preview downloads stop at fixed caps (8 MiB for images,
  128 MiB for other files) and ignore **Show previews in remote folders**;
  **Skip files larger than** and **Show previews for** only affect
  thumbnails. The file-information fields are fixed. Changed presentation
  settings, such as line numbers, apply from the next previewed item.
- **Workspace layouts:** A pane that keeps its own Yazi state differs from
  the saved layout, so the workspace shows the modified marker (`*`) after
  switching. Restoring only the columns also changes panes that aren't in
  Yazi mode; the columns appear when the mode is turned on. The default for
  new workspaces stays under **Workspaces › Behaviors**.
- **Reset:** **Restore Yazi Mode Defaults** makes open panes drop their own
  column and preview choices on Apply, even if values are edited again before
  Apply, and can mark the active workspace as modified. It never turns a
  pane's Yazi mode on or off. The dialog's own **Defaults** button resets
  every page but keeps the panes' own choices.

- **Recommendation:**
  - **Activation:** Turn a pane's mode on or off with **View › Yazi Mode**
    after changing its default; the setting takes over at the next start.
  - **Columns:** Use **Restore Yazi Mode Defaults** when open panes should
    follow new default widths. Use **Restore Default Widths** on the Columns
    tab to return to 1 : 3 : 2 after a drag.
  - **Keyboard:** Change presets from the Keyboard tab rather than the View
    menu so conflicts are reviewed, and save edits in **Configure Keyboard
    Shortcuts…** before reviewing again.
  - **Preview:** Switch the preview column to file information on slow
    connections, and use **Maximum text in preview panel** under **Interface
    › Previews** to limit remote text downloads.
  - **Workspace layouts:** Save the workspace layout again when the modified
    marker should clear, or turn both Yazi options back on to restore the
    saved state.
  - **Reset:** Use the Yazi Mode button instead of **Defaults** when only
    Yazi settings should change.
  - Comment:
    [#725.7182507952](https://github.com/mikepadiernos/plasma-opus/issues/725#issuecomment-7182507952)
- **Proposed improvements:**
  - Editable Yazi key bindings, built on KDE shortcut configuration.
  - Conflict review for the View menu's key preset toggle.
  - A remote preview download policy shared with the preview panel.
  - Settings for the narrow-pane limits.
  - Apply activation defaults to open panes on request.

## Search

Live search, indexed search, the search panel and the toolbar search field
have these limits
([#362](https://github.com/mikepadiernos/plasma-opus/issues/362)):

- **Live search:** Scans local folders only and skips symbolic links. Content
  search covers UTF-8 files up to 2 MiB without NUL bytes. Search stops after
  5,000 matches or 200,000 entries, but lists each directory before applying the
  entry limit.
- **Indexed search:** Requires Baloo at build and runtime and searches its
  configured locations. It matches indexed terms rather than literal substrings,
  does not accept double quotes, and may omit files or return stale results.
  Baloo applies required content terms and alternatives, folder, image, audio
  and video types and modification dates itself; NOT, name, path, size,
  extension, tag and metadata criteria are still checked against the files it
  returns. Types and dates use the MIME type and modification time Baloo
  recorded, so files changed since indexing or with misleading names may be
  missed. Plasma Opus examines at most 200,000 Baloo matches, says so when it
  stops there, and returns at most 5,000 matches. A depth limit or workspace
  search asks Baloo about each location; a location with no indexed matches,
  such as an unindexed folder, makes the search ask the whole index instead,
  which is slower.
  ([#733](https://github.com/mikepadiernos/plasma-opus/issues/733))
- **Index status:** Coverage notices reflect configuration, not proof of
  indexing; exclusions and pending indexing can affect results. Baloo's query
  API does not report index-read failures, and a running query cannot be
  interrupted until it returns. **Indexing Status** reproduces Baloo's
  folder, hidden-item, pattern and file-type rules from KDE File Search
  settings; the indexer may still skip files it can't read, very large
  files or files it failed to extract. **Check Index Coverage** looks up only
  the folder's first 200 files, not subfolders, and a file in the index may
  still have stale contents. Missing extractors are detected only for
  document, text, image, audio, video and ebook types among those files.
  The indexer's state is read over D-Bus when the status is opened or the
  Baloo scope is chosen, then updated when the indexer reports a change; its
  remaining-time estimate is Baloo's own. Suspend, resume and checking for
  unindexed files ask the shared indexer, which may ignore them, for example
  on battery power. This has been verified in a headless container with a
  simulated indexer only, not yet against a running Baloo on a Plasma
  session.
  ([#441](https://github.com/mikepadiernos/plasma-opus/issues/441))
- **Indexed metadata and ranking:** A `meta:` term selects Baloo candidates
  only when the query has no name or content term. Baloo then matches whole
  indexed words in any indexed text and in the enabled text properties, not
  substrings, so `meta:Can` may not find Canon; every candidate is confirmed by
  reading its metadata, which is slower on large result sets. Numbers, dates
  and properties Baloo does not index, or files whose extractor is missing,
  can't be found this way, and Baloo limited to file names finds no metadata;
  add a name term or search live instead. Ranking compares file names only,
  not paths, contents or metadata, and only reorders the results found: it
  can't add files missing from the index or beyond the 5,000-result limit,
  and the order is by score, not relevance as Baloo computes it. **Search
  Current Folder Live** appears only after an indexed search that was not
  cancelled, for a local current folder or workspace locations, and switches
  the scope to the live search before running it, so later searches stay
  live until the scope is changed back.
  ([#442](https://github.com/mikepadiernos/plasma-opus/issues/442))
- **Shared searches:** A saved search shared with KDE Places is a Baloo
  search, so Dolphin and file dialogs list only indexed files, match whole
  words rather than parts of names or text, and use the MIME types and
  dates Baloo recorded. Top-level criteria Baloo can't express are dropped
  rather than approximated, so the place may list more files than the saved
  search: sizes, extensions, paths, tags, metadata numbers, NOT terms, name
  wildcards and exact names, today and yesterday, depth limits, path filters
  and exclusions. A search of several search folders covers every indexed
  location, since Baloo limits a search to one folder, and remote folders
  can't be shared. The place doesn't follow later changes to the saved
  search, and renaming or deleting the saved search leaves it in Places.
  ([#734](https://github.com/mikepadiernos/plasma-opus/issues/734))
- **Selection and exclusions:** Path-selection filters apply only to local
  top-level listings or the visible flat list, not expanded child rows. Excluded
  folders are entered one per line, so paths containing newlines cannot be
  entered; exclusions are lexical paths, not resolved links.
- **Results and collections:** Results can be opened or revealed in the active
  pane, but hidden or filtered items may not be visible and flat view must be
  turned off. A folder result opens in the active pane; a file opens in its
  application. The shared collection stores local URLs, not file contents;
  references can become stale.
- **Search panel:** The panel searches the folder it was opened for and does
  not follow later navigation; search again from a pane to change it. While a
  search runs, the folder can't change until the search finishes or is
  cancelled. By default the panel follows the embedded terminal's placement,
  below or beside the panes. Preferences' Search page can instead place it
  below or right of the panes; when that direction differs from the
  terminal's, the panel spans the panes and terminal together and its size is
  remembered separately. The panel is hidden at startup unless Preferences
  enables reopening it. Each new window then reopens it, without taking focus,
  if it was shown when last toggled in any window; this state is not kept per
  window or workspace. Workspace layouts don't record the search panel's
  placement or visibility. Hiding the panel does not cancel a running search.
- **Toolbar search field:** The optional Search Field toolbar entry is not on
  any toolbar by default; add it from Preferences' Search page, which places
  it before the Search button, or with Customize Main Toolbar. Both change the
  same main toolbar entries, so restoring default toolbars removes it, and
  the Preferences option is applied even when toolbars are locked. Pressing
  <kbd>Enter</kbd> searches the active pane's current folder with the text
  typed, but uses the other criteria, such as scope and filters, currently set
  in the search panel. While a search runs, <kbd>Enter</kbd> does not start
  another search. The field's Saved Searches menu runs a saved search with all
  its saved criteria, including its folder, and replaces the panel's criteria.
  Searching from the field shows the search panel unless Preferences turn this
  off; the status bar then reports how the search ended for five seconds, or
  why it could not start, but errors and skipped files are listed only in the
  panel. Empty text still shows the panel. The field can be placed on the main
  toolbar only, not on pane toolbars. It suggests recent queries while typing,
  and <kbd>Alt</kbd>+<kbd>Down</kbd> lists them.
- **Result presentation and history:** Results and collection references are
  listed by full path or by name with their folder; the choice next to the
  result tabs applies to the current panel and Preferences' Search page sets
  its default. Copying, opening and revealing always use the full path. Recent
  queries, up to 50, are shared by every window and kept as plain text in
  Plasma Opus's state file, not in a protected store. A query is remembered
  when its search starts, including searches started from the toolbar field or
  a saved search. Turning off Remember recent search queries forgets stored
  queries the next time a window applies Preferences, and Clear Search History
  forgets them at once; neither removes queries already shown in an open
  completion list or saved searches.
- **Search keyboard navigation:** <kbd>Down</kbd> in the toolbar field or the
  panel's query moves to the visible result list, or from the toolbar field to
  the query when there are no results. With the panel hidden, <kbd>Down</kbd>
  in the field shows it only when it holds results. <kbd>Escape</kbd> cancels
  a running search; otherwise it returns focus to the active pane and leaves
  the panel open. <kbd>Tab</kbd> moves through the panel in layout order, but
  between the toolbar, panel and panes it follows the window's widget order.
  There are no dedicated shortcuts for focusing the toolbar field or the
  panel.
- **Advanced queries:** AND, OR and NOT are operators only in upper case;
  quote a value that contains a colon, starts with `-` or is one of those
  words. Words without a field match file names, and unquoted name and path
  values treat `*` and `?` as wildcards. Size criteria match files only, so
  NOT size criteria also match folders. Sizes use binary units: 1K is 1,024
  bytes, and `size:10M` matches exactly 10 MiB. Dates use local time. An
  extension such as `type:pdf` compares names only; a MIME type is detected
  from the name and, if needed, the contents, which can slow live searches and
  miss misnamed files. Content criteria use live search's text limits;
  unreadable, binary and oversized files never contain a term, so NOT content
  criteria match them. Indexed queries need a name, content or meta term, or
  a group of content alternatives, that every result must match, which
  selects Baloo candidates. Content terms use Baloo's word matching rather
  than substrings. Baloo also applies required folder, image, audio and video
  types and modification dates; other criteria, and any criterion under NOT
  or in an OR group with other fields, are checked against the files found.
  The normalized query shown while typing uses English field names.
- **Search folders and exclusions:** The search folders and excluded folders
  set on Preferences' Search page apply only to Plasma Opus. They do not
  change what Baloo indexes, and an excluded folder is still searched by other
  applications. Search folders are searched live, so they share live search's
  local-only, symbolic-link and result limits. Exclusions are lexical paths:
  a symbolic link elsewhere does not inherit its target's exclusion.
- **Search defaults:** The default scope, search field and path filter on
  Preferences' Search page set the panel's initial criteria, which the toolbar
  field also uses. When they change, an open panel updates only criteria still
  showing the previous defaults, and waits until a running search finishes.
  The default path filter is stored by name: renaming or deleting the shared
  filter leaves it unavailable, so searches start without a path filter and
  report it until another filter is chosen. Settings from before M22 are
  migrated when Plasma Opus starts; the earlier Boolean keys are removed.
- **Double Shift:** Pressing <kbd>Shift</kbd> twice opens search only while a
  Plasma Opus main window has keyboard focus; it is not a global shortcut and
  can't be assigned in Configure Keyboard Shortcuts. It is turned off while
  Sticky Keys is on in Plasma's accessibility settings, and Plasma Opus can't
  detect input methods or remappings that intercept <kbd>Shift</kbd>.

- **Recommendation:** Check Baloo's indexing configuration and status in
  **File › Indexing Status…**, and use **Check Index Coverage** to confirm
  that a folder's files are indexed, or `balooctl6 status` for details Baloo
  doesn't expose to applications; use live search for a current local folder,
  or **Search Current Folder Live** after an indexed search, search smaller
  folders, inspect errors and repeat searches after files change. Use whole
  words in indexed `meta:` terms, and add a name term or search live for
  numbers, dates or partial words. Turn ranking off to keep the order in which
  results were found. Use flat view when selecting nested local items by path,
  and verify the resulting selection. Use GNU `find` for other local filename
  searches or ripgrep (`rg`) for content searches when Plasma Opus limits are
  unsuitable; check each tool's hidden-file, ignore, encoding and
  symbolic-link behaviour. If a revealed result is hidden or filtered, adjust
  the pane's visibility settings. Re-run saved searches and check collection
  references after files change. Use KDE File Search settings to change what
  Baloo indexes for every application. When pressing <kbd>Shift</kbd> twice is
  unavailable, use the Search shortcut
  (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>F</kbd> by default), which can be
  changed in Configure Keyboard Shortcuts. To search another folder from the
  search panel, use Search from that pane's toolbar or the Search shortcut
  while the pane is active, or type in the toolbar search field. Check the
  panel's criteria before searching from the toolbar field. Press
  <kbd>Down</kbd> to reach the results and <kbd>Escape</kbd> to return to the
  active pane; click the panel or use <kbd>Tab</kbd> when keyboard focus needs
  to move elsewhere. Check the normalized query shown while typing to confirm
  grouping, use parentheses where AND and OR are mixed, and search the current
  folder or search folders when an advanced query has no required name,
  content or meta term. Search live when recently changed or misnamed files
  must match an indexed type or date, and add a name or content term when an
  indexed search reaches the 200,000-match limit. Read the criteria listed
  before sharing a search with KDE Places, share it again after changing it,
  and remove stale places from Places in any KDE application. After renaming
  or deleting a shared path filter, choose the default path filter again on
  Preferences' Search page. With several windows open, hide the search panel
  in the last window you use if it should stay closed at the next startup.
  Turn off Remember recent search queries before searching for sensitive text,
  or use Clear Search History afterwards. Choose Names and folders when long
  paths make results hard to scan. When toolbar searches leave the panel
  hidden, press <kbd>Down</kbd> in the field after a search to check its
  errors and results.
  - Comment: [#362.7023269364][comment-362]
- **Alternative:** Use a separately maintained locate database for filenames.
  - Comment:
    [#362.7029675758](https://github.com/mikepadiernos/plasma-opus/issues/362#issuecomment-7029675758)
- **Planned:** [M21 advanced search](ROADMAP.md#m21-advanced-search) reuses M09
  search; its new interface and criteria do not by themselves remove current
  backend limits.
  [M31 keyboard shortcut settings](ROADMAP.md#m31-keyboard-shortcut-settings)
  plans configurable shortcuts
  ([#230](https://github.com/mikepadiernos/plasma-opus/issues/230)), which
  could include focusing the search field and panel.
  [M15 workspaces](ROADMAP.md#m15-workspaces)
  plan search scoping, not a security boundary.
  - Comment: [#362.7023269364][comment-362]

## Batch rename

Batch rename has these limits
([#372](https://github.com/mikepadiernos/plasma-opus/issues/372)):

- **Inputs and destinations:** Only local items are supported. The destination
  filesystem must support Linux atomic no-replace renames. Preview rejects
  existing destinations, rename chains, swaps, and selections containing both a
  folder and its descendants.
- **Patterns and metadata:** Metadata tokens depend on installed KDE extractors
  and embedded fields; missing metadata and invalid names prevent application.
  Regular expressions are case-sensitive and apply to base names only,
  preserving extensions. Expensive expressions can delay a preview and cannot be
  cancelled.
- **Changes and recovery:** The preview is a snapshot. Source identity is
  checked before each rename, but changes can race those checks. Errors or
  cancellation stop the batch and can leave partial results. Renames are not
  registered with KIO undo. Per-folder Plasma Opus labels, colours, ratings,
  notes and pins remain keyed by old names.

- **Recommendation:** Review the preview and back up important items first. Use
  valid patterns and metadata fields present on every selected file; install KDE
  metadata extractors if needed. Use `(?i)` for case-insensitive regex matching
  and avoid expensive expressions. Use a filesystem that supports atomic
  no-replace renames. After an error or cancellation, inspect the displayed
  results and filesystem before retrying; manually rename completed items back
  if needed. Reapply per-folder annotations to new names and remove stale
  entries as appropriate.
  - Comment: [#372.7024683530][comment-372]
- **Alternative:** Try a separate batch renamer on copies.
  - Comment:
    [#372.7029676077](https://github.com/mikepadiernos/plasma-opus/issues/372#issuecomment-7029676077)
- **Native tagging:** [M23 libraries](#native-tagging) follow batch renames in
  Plasma Opus, keeping library tags and fields; existing per-folder annotations
  are not migrated.
  - Comment: [#372.7024683530][comment-372]

## Metadata editing

Embedded metadata editing has these limits
([#374](https://github.com/mikepadiernos/plasma-opus/issues/374)):

- **Formats and dependencies:** Edit one local JPEG or TIFF (description, artist
  and copyright) or MP3 (title, artist and album) at a time. ExifTool is
  required to read all supported formats and write images; MP3 writes require
  the KDE FileMetaData writer plugin. Unsupported formats, linked files and
  fields containing line breaks cannot be edited.
- **Preview and writes:** The preview covers only displayed values, not
  duplicate or unrelated metadata or media contents. File identity and
  timestamps are checked before writing but can race external changes. Writes
  are not undoable, and a failure or timeout may leave changes. The KDE writer
  does not report write errors: Plasma Opus re-reads displayed fields but cannot
  prove preservation of other data.

- **Recommendation:** Back up the original and review the preview. Install
  ExifTool and the KDE metadata writer plugin and ensure ExifTool runs in your
  environment. Use a regular local file without symbolic or hard links. Reopen
  and inspect media and metadata after writing or an error; use a dedicated
  metadata editor for other formats, fields, multiple values or stronger
  preservation guarantees.
  - Comment: [#374.7025134952][comment-374]
- **Alternative:** Try a dedicated tag editor for other audio fields.
  - Comment:
    [#374.7029676829](https://github.com/mikepadiernos/plasma-opus/issues/374#issuecomment-7029676829)
- **Native tagging:** [M23](#native-tagging) reads and maps selected embedded
  metadata into libraries and reuses this editor for supported writes; it adds
  no writable formats or preservation guarantees.
  - Comment: [#374.7025134952][comment-374]

## Transfer queue

The transfer queue has these limits
([#375](https://github.com/mikepadiernos/plasma-opus/issues/375)):

- **Coverage:** The queue handles pane, context-menu and gallery copy and move
  actions. Clipboard paste and drag-and-drop use KIO directly; they do not
  appear in the queue or use its conflict policy.
- **Scheduling and conflicts:** One job runs at a time by default; concurrency
  is configurable. Disabling queue scheduling starts jobs without a concurrency
  limit. Automatic overwrite replaces files and merges folders without asking.
- **Controls and lifetime:** Progress, speed and controls depend on the KIO job
  and worker. Cancellation may leave partial results. The queue exists only in
  memory: waiting jobs are discarded on application exit and active jobs may be
  cancelled. Plasma Opus warns before closing a window while transfers are
  pending. External KIO progress interfaces may control a job independently of
  the queue display. KIO records undo when a queued copy or move begins, not
  while waiting.

- **Recommendation:** Use queued pane, context-menu or gallery copy and move
  actions and choose concurrency appropriate for the destination. Keep the Ask
  conflict policy when confirmation is needed; review the destination before
  using automatic overwrite. Use the normal KIO progress interface for paste and
  drag-and-drop. Check partial results after cancellation or errors and keep
  Plasma Opus open until queued work finishes. Resume externally paused jobs
  from the external interface. Inspect the KIO undo stack after a transfer
  starts.
  - Comment: [#375.7025250130][comment-375]
- **Alternative proposal:** Consider adding paste and drop jobs to the queue.
  - Comment:
    [#375.7029677171](https://github.com/mikepadiernos/plasma-opus/issues/375#issuecomment-7029677171)
- **Planned:** [M27](ROADMAP.md#m27-integrations) proposes remote-profile
  transfers through the queue where available and Plasma Opus-owned job
  notifications; neither promises to route paste or drag-and-drop through the
  queue.
  - Comment: [#375.7025250130][comment-375]

## Custom commands

Custom commands have these limits
([#373](https://github.com/mikepadiernos/plasma-opus/issues/373)):

- **Inputs:** Commands require a local working folder and local selections.
  Placeholders occupy whole arguments; a single empty literal argument and line
  breaks in templates or defaults cannot be represented in the editor. Preset
  defaults are stored as plaintext.
- **Execution:** Commands launch directly without an implicit shell, but chosen
  programs may interpret arguments as code. They run with the user's permissions
  and are not registered with KIO undo.
- **Output and process lifetime:** Displayed output is limited to 256 KiB. Stop
  terminates the launched process but may leave descendants running; closing the
  application can terminate the child.

- **Recommendation:** Copy remote inputs locally. Review the executable and
  every argument, run only trusted programs, and avoid passing untrusted paths
  to code-evaluating arguments or saving credentials in presets. Redirect large
  output to a file if needed, and inspect or stop remaining descendants after
  cancellation. Back up files before destructive commands and use external tools
  for unsupported argument templates or undo.
  - Comment: [#373.7024944292][comment-373]
- **Alternative:** Use a reviewed script instead of a complex inline template.
  - Comment:
    [#373.7029676403](https://github.com/mikepadiernos/plasma-opus/issues/373#issuecomment-7029676403)

## Symbolic links

Symbolic-link creation has these limits ([#312][issue-312]):

- **Targets:** Only existing local files and folders are supported.
- **Destination:** The new link is created beside its target; another
  destination cannot be chosen.

- **Recommendation:** Use a suitable external tool when a different link
  destination or remote symbolic-link support is needed. For local paths, GNU
  `ln -s` can create a link at a chosen destination; remote support depends on
  the server and access method.
  - Comment: [#312.7021756400][comment-312]
- **Alternative:** Use an SFTP client with explicit symbolic-link support.
  - Comment:
    [#312.7029672139](https://github.com/mikepadiernos/plasma-opus/issues/312#issuecomment-7029672139)

## Listing export

Listing export has these limits
([#314](https://github.com/mikepadiernos/plasma-opus/issues/314)):

- **Rows and values:** Export uses the current filtered rows and displayed
  values, even in icon or compact view.
- **Columns:** By default it uses the visible details columns; export-specific
  columns can be selected in Preferences, but are limited to the supported
  listing fields.
- **Output:** Output is local only, and wide PDF tables may wrap or clip.
- **CSV formulas:** CSV prefixes cells that could be interpreted as
  spreadsheet formulas with an apostrophe.

- **Recommendation:** Check the filter and selected export columns before
  exporting, and reduce columns for wide PDF tables. Keep CSV formula protection
  when opening untrusted data in a spreadsheet; copy local output elsewhere if
  needed. GNU `find -print0` can produce a local, NUL-delimited filename list
  for compatible tools, but does not reproduce Plasma Opus filters, columns or
  CSV output.
  - Comment: [#314.7021757620][comment-314]
- **Alternative:** Produce a custom-field export with a separate script.
  - Comment:
    [#314.7029672795](https://github.com/mikepadiernos/plasma-opus/issues/314#issuecomment-7029672795)

## Checksum files

Checksum files have these limits
([#316](https://github.com/mikepadiernos/plasma-opus/issues/316)):

- **Format and inputs:** Checksum files use UTF-8 GNU-style SHA-256 manifests
  and local regular files in the same folder. Other algorithms, remote files,
  symbolic links, path entries and non-UTF-8 names are unsupported.
- **Size:** Manifests are limited to 10,000 entries.
- **Races:** Files can change during hashing; cancellation near completion can
  race with publication of the checksum file.

- **Recommendation:** Use local regular files in the same folder and keep
  manifests within 10,000 entries. Avoid changing files while hashing, and check
  whether an output file was published after cancellation. Use a trusted
  external checksum tool for unsupported algorithms or inputs. GNU coreutils
  provides `sha256sum` and other checksum commands for local files.
  - Comment: [#316.7021845379][comment-316]
- **Alternative:** Use a separate checksum tool for other algorithms.
  - Comment:
    [#316.7029673498](https://github.com/mikepadiernos/plasma-opus/issues/316#issuecomment-7029673498)

## Split and combine files

Split and combine supports local regular files and has these limits
([#318](https://github.com/mikepadiernos/plasma-opus/issues/318)):

- **Parts:** Parts are raw numbered files; combining uses the displayed order
  and cannot detect missing, altered or incorrectly ordered parts.
- **Interruptions:** Inputs can change during processing. Failures or
  cancellation can leave incomplete output, and many small parts can be
  expensive.
- **Output:** Existing outputs are never overwritten and new files are
  owner-only.

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
  - Comment:
    [#318.7029674177](https://github.com/mikepadiernos/plasma-opus/issues/318#issuecomment-7029674177)

## Native tagging

The M23 [native tagging](tagging.md) libraries have these limits
([#714](https://github.com/mikepadiernos/plasma-opus/issues/714)):

- **Libraries and indexing:** Libraries are local folders only. Symbolic links
  aren't indexed or followed, and names that aren't valid in the system
  encoding are skipped. A library is a single JSON document that is read and
  rewritten whole on each edit, so very large libraries make edits and
  searches slower. Saves detect changes from other windows and retry, but
  there is no file lock, so two processes saving at the same moment can still
  lose one edit.
- **Moves and renames:** Only moves and renames started in Plasma Opus are
  followed. Moves by other programs, copies and KIO undo aren't, and the next
  refresh marks the old paths missing. A folder moved into a library is
  indexed at its top level only until the next refresh. Tags aren't copied
  between libraries. When the rename dialog reports an error, the renames it
  completed aren't applied to the library. Moving or renaming a library's root
  folder leaves its registration pointing at the old path.
- **Showing tags:** Library tags appear in the tag panel and the Information
  panel only. Views have no Tags column or emblem; the cards view's tags field
  shows Baloo file tags, not library tags. The gallery window can't assign
  tags. The tag panel doesn't notice changes made by other windows or
  programs until the selection or folder changes.
- **Tag panel:** Creating a tag from the search field and assigning it are
  separate saves, so if the assignment fails the new tag is kept unassigned.
- **Tag groups:** Groups are reordered with Move Up and Move Down only, not by
  drag and drop. Groups are library data, independent of workspaces and
  locations, as recommended in
  [#252](https://github.com/mikepadiernos/plasma-opus/issues/252).
- **Fields:** Library fields can't be searched with query criteria. A field's
  type can't change while any item has a value for it.
- **Searching:** Tag criteria match only items inside registered libraries;
  `tagged:no` matches items outside them only when
  [tagging settings](#tagging-settings) allow it. A search uses a snapshot of
  each library taken when it starts. Indexed search still needs a name or
  content term. Select by Query can't use `content:`, and its `meta:` criteria
  read every listed file on the interface thread, so large folders can make
  the window unresponsive until it finishes.
- **Embedded metadata:** Reading uses KDE's extractors, so the available
  properties depend on the installed extractor plugins. `meta:` criteria read
  each candidate file and are slow without cheaper criteria. Writing embedded
  metadata is limited to single JPEG, TIFF and MP3 files through
  [metadata editing](#metadata-editing). The import dialog's field list isn't
  refreshed if fields are changed elsewhere while it's open.
- **Export and restore:** Restoring replaces the whole library document; it
  doesn't merge. Items are matched by path only. An export reflects the
  library as loaded, including repairs made when it was read. Backups made
  before the backup limit was added aren't pruned. Exports have no size limit.

- **Recommendation:**
  - **Libraries and indexing:** Keep libraries on local folders, and rename
    files with unusual encodings before indexing them. Split very large
    collections into several libraries, and avoid editing the same library
    from several windows or programs at once.
  - **Moves and renames:** Move and rename tagged items in Plasma Opus. After
    moving items with other programs, undoing a move or moving a folder into a
    library, use **Tags › Refresh Library** and **Review Missing Entries…** to
    relink. To move a library's root folder, remove the library, move the
    folder, then create the library again from the new location; its document
    is adopted with its tags.
  - **Showing tags:** Select items to see their tags in the tag panel or the
    Information panel. Change the selection to refresh the tag panel after
    editing in another window.
  - **Tag panel:** If creating and assigning a tag reports an error, check the
    new tag again to assign it.
  - **Fields:** To find items by a field, import or copy the value as a tag. To
    change a field's type, create a new field and copy the values to it.
  - **Searching:** Turn on **Include items outside libraries in tagged:no
    searches** in **Preferences › Tagging › Search**, or use `NOT tagged:yes`,
    to include items outside libraries, and repeat a search after editing
    tags. Use Search rather than Select by Query for content, and combine
    `meta:` with name or type criteria, as in `type:image/* meta:canon`.
  - **Embedded metadata:** Install KDE's metadata extractors, and ExifTool for
    writing images. Use a dedicated tag editor for other formats or several
    files. Reopen the import dialog after changing fields.
  - **Export and restore:** Export before restoring and review the preview.
    Set **Keep at most … backups per library** in **Preferences › Tagging ›
    Privacy**, and remove older backups without the library's hash in their
    name from the backup folder by hand.
  - Comment:
    [#714.7171889508](https://github.com/mikepadiernos/plasma-opus/issues/714#issuecomment-7171889508)
- **Proposed improvements:**
  - Show library tags in a Tags column or emblem, and allow tagging in the
    gallery.
  - Refresh the tag panel when the library document changes.
  - Index the contents of folders moved into a library, and follow renames of
    the library root.
  - Search library fields with query criteria.
  - Merge restores.

## Tagging settings

The M24 [tagging settings](tagging.md#settings) have these limits
([#716](https://github.com/mikepadiernos/plasma-opus/issues/716)):

- **Private storage:** A private library document is keyed by the library
  folder's path. Moving or renaming the folder leaves the document behind,
  and the library must be created again and restored.
- **Backup folder:** The setting accepts a URL, but only local folders are
  used. A remote location silently falls back to the default
  `library-backups` folder.
- **Preferences:** The Libraries and Fields tabs read every library document
  on the interface thread, so many or very large libraries make Preferences
  slower to open. **Add Library…** uses the storage choice and new-library
  fields as currently shown, even before Apply. The Fields tab's library
  picker doesn't show whether a library is read-only or unreadable.
- **Applying settings:** Apply updates the window Preferences was opened
  from. Other windows move their tag panel when they are opened again, and
  show colour and Information panel changes when those panels next refresh.
  Automatic refresh reports its results in the active window only.
- **Library scope:** Exclusions, hidden items, depth and item limits apply to
  refreshes only; entries already indexed keep their tags and fields.
- **Search:** With the whole-library scope, path filters and the depth limit
  count from the library folder, and the notice that the search widened
  appears only when it finishes. Searches already running keep the settings
  they started with. When `tagged:no` matches items outside libraries, a
  search outside any library matches everything it lists.
- **Fields:** New libraries get their own field identifiers, so fields with
  the same name are distinct between libraries. Invalid stored field
  templates are dropped silently.
- **Embedded metadata:** Disabled properties are hidden in the preview panel
  and gallery but still extracted there. Mappings are stored by field name
  and apply only in libraries with a field of that name and a compatible
  type. When Preferences and the import dialog are both open, whichever saves
  mappings last wins. Importing on refresh has no preview, covers only items
  that refresh added and reads every new file, so the first refresh of a
  large library is slow.

- **Recommendation:**
  - **Private storage:** To move a privately stored library's folder, remove
    the library, move the folder, create the library again at the new path
    and use **Tags › Restore Library Metadata…** with an export or the old
    private document. Alternatively, switch the library to **Keep Metadata in
    Library Folder** before moving it.
  - **Backup folder:** Choose a local folder for backups.
  - **Preferences:** Keep the number of registered libraries modest, and split
    very large collections. Apply the Privacy and Fields tabs before adding
    libraries if you want the choices saved.
  - **Applying settings:** Reopen other windows, or change their selection,
    after changing tagging settings. Keep the window you care about active
    while an automatic refresh runs, or refresh manually with **Tags ›
    Refresh Library**.
  - **Library scope:** Remove tags and field values from items before
    excluding them if you no longer want that metadata.
  - **Search:** With the whole-library scope, write path filters relative to
    the library folder. Start a new search after changing search settings.
    Leave `tagged:no` outside libraries off unless you search limited
    folders, or combine it with name or type criteria.
  - **Fields:** Set up fields for new libraries before creating them, and use
    the same names across libraries so saved mappings apply.
  - **Embedded metadata:** Disable unwanted properties to keep search fast.
    Create the mapped fields in each library, with a date type for date
    properties. Close the import dialog before changing mappings in
    Preferences. Leave importing on refresh off for the first refresh of a
    large library and run **Import Embedded Metadata…** with a preview
    instead.
  - Comment:
    [#716.7175386572](https://github.com/mikepadiernos/plasma-opus/issues/716#issuecomment-7175386572)
- **Proposed improvements:**
  - Key private documents by a stable library identifier and follow folder
    moves.
  - Read library state in Preferences in the background.
  - Apply tagging settings to every open window.
  - Offer to remove entries that new exclusions or limits leave out.
  - Share field identifiers between libraries, or map by identifier.

## Filtered folder comparison

Filtered folder comparison has these limits
([#376](https://github.com/mikepadiernos/plasma-opus/issues/376)):

- **Scope:** The built-in comparison requires two separate local folders; with
  a folder on an SSH host, the [rsync backend](#desktop-integration) compares
  instead. Patterns match
  relative paths case-sensitively; excluded folders are not traversed. Links can
  be skipped or compared by target, but are never followed. When links are
  skipped, the matching subtree on the other side is also skipped.
- **Results:** Comparison examines at most 100,000 entries per folder, and
  filenames must round-trip through Qt's local filename encoding. Without
  content comparison, matching size and modification time do not prove equal
  contents. Results are snapshots that concurrent changes can invalidate, and
  can produce a large list.

- **Recommendation:** Use separate local folders and choose relative-path
  patterns carefully; enable content comparison when matching bytes matter.
  Compare smaller trees if the entry limit is reached, and rerun after changes.
  Choose rsync for folders on SSH hosts, and use an external tool for other
  remote locations or unsupported filenames.
  - Comment: [#376.7026216329][comment-376]
- **Alternative:** Use a separate graphical directory comparison.
  - Comment:
    [#376.7029677475](https://github.com/mikepadiernos/plasma-opus/issues/376#issuecomment-7029677475)

## Folder synchronization

Folder synchronization has these limits
([#377](https://github.com/mikepadiernos/plasma-opus/issues/377)):

- **Supported operations:** The built-in backend is local-only; the optional
  [rsync backend](#desktop-integration) also reaches folders on SSH hosts, with
  its own limits. Synchronization copies, overwrites
  and permanently deletes regular files and creates missing folders. Links,
  directory deletion and type replacement are unsupported. Profiles save filters
  and comparison options, not item actions.
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
  `.opus-sync-*` folders after errors, and compare again before retrying.
  Choose rsync for folders on SSH hosts, and use another tool for links,
  directory deletion, type replacement or metadata that must be preserved.
  - Comment: [#377.7026633904][comment-377]
- **Alternative:** Run rsync yourself with a reviewed dry run for options
  Plasma Opus doesn't offer.
  - Comment:
    [#377.7029677824](https://github.com/mikepadiernos/plasma-opus/issues/377#issuecomment-7029677824)

## File comparison

File comparison has these limits ([#311][issue-311]):

- **Requirements:** Comparison requires the `diff` executable and two local
  regular files.
- **Output:** Unified output is decoded as UTF-8, and comparison stops if
  output exceeds 4 MiB.
- **Binary files:** Binary differences are reported without a visual binary
  diff.

- **Recommendation:** Copy remote files locally and use another comparison tool
  for non-UTF-8 text, large output or a visual binary diff. Difftastic can
  compare supported text files; `cmp` can check whether binary files differ, but
  does not provide a visual diff.
  - Comment: [#311.7021755780][comment-311]
- **Alternative:** Use a separate graphical comparison tool.
  - Comment:
    [#311.7029671783](https://github.com/mikepadiernos/plasma-opus/issues/311#issuecomment-7029671783)

## Folder content chart

The folder content chart has these limits
([#315](https://github.com/mikepadiernos/plasma-opus/issues/315)):

- **Breakdown:** The chart shows measured logical sizes by immediate entry,
  not kind or extension breakdowns or a largest-file drilldown.
- **Entries:** It includes hidden entries, skips top-level symbolic links and
  entries without a known size, and relies on KIO workers for recursive
  traversal.
- **Scans:** Scans can be cancelled but have no entry or time limit and can be
  costly; results are snapshots.

- **Recommendation:** Scan smaller folders when possible, cancel expensive scans
  and rescan when current results are needed. Use another tool for extension
  breakdowns, largest-file analysis or linked-directory sizes. `ncdu` can
  explore local directory usage; GNU `du -L` can follow directory symlinks,
  subject to cycles and repeated targets.
  - Comment: [#315.7021758275][comment-315]
- **Alternative:** Inspect local disk usage with QDirStat.
  - Comment:
    [#315.7029673162](https://github.com/mikepadiernos/plasma-opus/issues/315#issuecomment-7029673162)

## Pane folder comparison

Pane folder comparison has these limits
([#317](https://github.com/mikepadiernos/plasma-opus/issues/317)):

- **Scope:** Comparison covers immediate entries only. Files are compared by
  size and modification time, not contents; subfolders are not compared.
- **Results:** Results are snapshots, and hidden or filtered entries may not
  be selectable.
- **Cost:** Large or remote listings may be costly.

- **Recommendation:** Use file comparison for individual files or filtered
  folder comparison for local recursive comparisons, enabling content comparison
  when needed. For other cases, use a trusted recursive tool; GNU `diff -r`
  compares local directory trees. Show hidden files and clear pane filters
  before selecting differences; compare again after changes.
  - Comment: [#317.7021904155][comment-317]
- **Alternative:** Inspect a folder-sync comparison without applying changes.
  - Comment:
    [#317.7029673828](https://github.com/mikepadiernos/plasma-opus/issues/317#issuecomment-7029673828)

## Cushion treemap

The treemap uses the folder content chart measurements: tiles represent
measured immediate entries and their relative logical sizes. It has these
limits ([#335](https://github.com/mikepadiernos/plasma-opus/issues/335)):

- **Nesting:** Nested folders are aggregated rather than shown simultaneously;
  activate a folder in the list to rescan it.
- **Visibility:** Empty, unmeasured and skipped entries have no visible tile,
  and very small shares may be difficult to see.
- **Measurements:** Measurements are snapshots and the
  [folder content chart](#folder-content-chart) scan limits also apply.

- **Recommendation:** Activate folders in the list to inspect their contents,
  and use the size table for empty, skipped or very small entries. Scan smaller
  folders and rescan when current results are needed. QDirStat is an external
  option for simultaneously visualizing nested disk usage.
  - Comment: [#335.7022647720][comment-335]
- **Alternative:** Use Filelight for a separate nested radial view.
  - Comment:
    [#335.7029675089](https://github.com/mikepadiernos/plasma-opus/issues/335#issuecomment-7029675089)

## Duplicate finder

The duplicate finder recursively scans local folders for known regular files,
groups equal sizes and SHA-256 hashes, and allows checked file URLs to be
copied for manual review. Candidates can be selected by modification time or
folder and moved to Trash after confirmation. It has these limits
([#336](https://github.com/mikepadiernos/plasma-opus/issues/336)):

- **Changes:** It rechecks affected groups before asking, but files can change
  afterward.
- **Entry types:** Symbolic links are skipped; hard links may appear as
  separate paths, and unknown entry types are skipped.
- **Scans and hashes:** Errors are shown, but scans are unbounded and matching
  hashes are not a guarantee of byte identity.

- **Recommendation:** Review groups and errors, verify current file contents and
  paths independently, and keep at least one confirmed copy and backups before
  moving files to Trash. Scan smaller local folders if a scan is expensive.
  `jdupes` is an external option for comparing local files; its default listing
  is non-destructive, but deletion and other modifying options require care.
  - Comment: [#336.7022793474][comment-336]
- **Alternative:** Compare candidate files byte-for-byte before deletion.
  - Comment:
    [#336.7029675367](https://github.com/mikepadiernos/plasma-opus/issues/336#issuecomment-7029675367)

## Workspace layouts

Workspace layouts, merged in M15, save pane and tab locations, active tabs,
view and location-bar modes, toolbar action lists, splitter sizes, terminal
orientation, detached-pane geometry, and supported panel placement and
visibility. They have these limits
([#700](https://github.com/mikepadiernos/plasma-opus/issues/700)):

- **Drafts, sessions and jobs:** Layouts do not persist editor drafts or
  terminal sessions. Running jobs remain alive when switching in the current
  process but are not reconstructed from a saved layout.
- **Startup:** The selected workspace is marked current on startup, while its
  saved layout remains unapplied until the user switches to it.
- **Prerequisites:** A visible terminal requires an existing session; metadata
  and toolbar editors must be closed before saving or switching. Incompatible
  linked panes must be unlinked first.
- **Older snapshots:** Older snapshots may require replacement before they can
  be applied.

- **Recommendation:** Save editor drafts separately and start a terminal session
  before switching to a layout that shows it. Finish metadata and toolbar edits,
  and unlink panes when prompted. Replace older snapshots with **Save Current
  Layout**. Explicitly switch to apply a saved layout after startup.
  - Comment:
    [#700.7163113449](https://github.com/mikepadiernos/plasma-opus/issues/700#issuecomment-7163113449)

## Workspace settings

The M16 settings have these limits
([#609](https://github.com/mikepadiernos/plasma-opus/issues/609)):

- **Panels:** The Layouts panels option restores the saved window state as one
  unit, including main toolbar positions and dock placement. The toolbars
  option only controls toolbar entries.
- **Remote locations:** Recovery checks only that a remote location's KIO
  protocol is installed. It never contacts the server, so a known but
  unreachable remote location is not listed and fails when its tab loads.
- **Unsaved state:** The unsaved marker and the **When unsaved** switch
  confirmation compare the whole captured layout. This includes parts that
  the workspace's behaviors or the restore options would not restore.
- **Import conflicts:** You choose which profiles to import one by one, but
  the replace or keep choice applies to all selected conflicts.
- **Default behaviors:** These apply only to workspaces created in the
  workspace manager. Duplicated and imported workspaces keep their own.

- **Recommendation:**
  - **Panels:** To keep the current docks and toolbar positions while
    restoring toolbar entries, clear the panels option.
  - **Remote locations:** Connect to them before switching, then reload any
    remote tab that was unreachable.
  - **Unsaved state:** Use **Save Current Layout** after deliberate changes,
    or set the switch confirmation to **Never** or **Always** if it asks too
    often.
  - **Import conflicts:** Import conflicting profiles in separate passes when
    some should replace existing workspaces and others should be kept.
  - **Behaviors:** Review the behaviors of duplicated and imported workspaces
    in the workspace manager.
  - Comment:
    [#609.7144292691](https://github.com/mikepadiernos/plasma-opus/issues/609#issuecomment-7144292691)
- **Proposed improvements:**
  - Split window state so that toolbar positions follow the toolbars option.
  - Optionally probe remote locations with a bounded, cancellable KIO stat.
  - Compare only the restorable parts of a layout when marking unsaved state.
  - Choose to replace or keep each conflicting profile separately.

## Linked pane navigation

Linked navigation supports existing local folder trees rooted at the two
starting folders and has these limits
([#322](https://github.com/mikepadiernos/plasma-opus/issues/322)):

- **Unlinking:** Panes unlink when navigation leaves either tree, a matching
  folder is missing, a symbolic link resolves outside its tree, or a pane is
  hidden. Tab and history changes also participate and may unlink.
- **Races:** Filesystem changes can invalidate folders between checks and
  navigation.

- **Recommendation:** Start from corresponding local folders with matching
  subfolder names. Check both panes after navigating, especially when
  directories may change; relink at a new pair of roots when needed. Navigate
  remote folders independently.
  - Comment: [#322.7022490696][comment-322]
- **Alternative:** Navigate the two locations independently.
  - Comment:
    [#322.7029674758](https://github.com/mikepadiernos/plasma-opus/issues/322#issuecomment-7029674758)

## Themes

The M17 theme options have these limits
([#680](https://github.com/mikepadiernos/plasma-opus/issues/680)):

- **Blur and translucency:** Background blur uses KWin's blur effect through
  `KWindowEffects`. Without KWin compositing it is reported as unavailable and
  the background stays opaque. Blur and translucency have been tested only
  headlessly, without a Plasma session.
- **Stylesheets:** A custom Qt stylesheet is limited to 64 KiB and 2,000 rules.
  `@import`, `qproperty-` declarations and `url()` references other than Qt
  resources (`:/` or `qrc:/`) are refused, so local and remote images cannot
  be used. Qt's parser is lenient, so some unsupported selectors or properties
  are ignored without an error. The stylesheet is applied to each main window
  and the dialogs it owns.
- **Theme profiles:** Invalid saved profiles are skipped when loading, with a
  count but without naming them. Profiles leave out folder colour rules and the
  high-contrast safeguard. Unavailable colour schemes and fonts in a profile
  fall back to Plasma's until they are installed.
- **Settings dialog and workspaces:** The settings dialog always edits the
  global appearance. While the active workspace overrides an option, the dialog
  shows the global value without a notice, and its changes appear only where
  the workspace does not override them.
- **Other windows:** Changes to a theme profile attached to a workspace reach
  other open windows only when they next apply settings or switch workspaces.
- **View mode:** A workspace's view mode override sets the panes' view mode
  when switching to the workspace, not at startup, where restored panes keep
  their saved view modes.
- **Folder colour rules:** A workspace keeps its own copy of folder colour
  rules, taken from the global rules rather than edited per workspace.
- **App-wide:** The active workspace is shared by all windows, so its
  appearance applies to every window.

- **Recommendation:**
  - **Blur and translucency:** Enable compositing and KWin's blur effect in
    System Settings. If blur is unavailable, lower opacity only as far as text
    stays readable, or keep backgrounds opaque.
  - **Stylesheets:** Keep stylesheets small and appearance-only, using colours,
    borders and Qt resource images. Check the preview before applying, and
    export the stylesheet before large edits so that you can revert.
  - **Theme profiles:** Export profiles you rely on as backups and re-import
    them if the profile count drops. Keep folder colour rules in the global
    settings or attach them to a workspace.
  - **Settings dialog and workspaces:** Change overridden options in
    **Workspaces… › Workspace Appearance**, or clear the override there first.
    Start with `--no-workspace-appearance` to see only the global appearance.
  - **Other windows:** After editing an attached profile, apply settings or
    switch workspaces in the other windows.
  - **View mode:** Switch to the workspace again, or choose the view mode for
    each pane.
  - **Folder colour rules:** Edit the global rules, then choose **Copy of the
    global rules** in the workspace's appearance and save it.
  - **App-wide:** Use separate workspaces for different appearances; M27 can
    apply separate choices per Activity.
  - Comment:
    [#680.7148716042](https://github.com/mikepadiernos/plasma-opus/issues/680#issuecomment-7148716042)
- **Proposed improvements:**
  - Mark options that the active workspace overrides in the settings dialog.
  - Notify other windows when theme profiles or workspace appearance change.
  - Name skipped theme profiles and offer to export or remove them.
  - Edit folder colour rules per workspace.

## Theme settings

The M18 theme settings have these limits
([#687](https://github.com/mikepadiernos/plasma-opus/issues/687)):

- **Corner radius:** A corner radius other than **Follow widget style** rounds
  item highlights, cards, text fields, tabs and tooltips. Buttons, combo boxes
  and panel frames keep the widget style's corners.
- **Plasma accent:** Colours that follow the Plasma accent are updated live
  only when Plasma announces a change to `kdeglobals`. An accent change written
  without a change notification appears after the next settings change or
  restart.
- **Workspace preview:**
  - Previewing a workspace's appearance doesn't switch the view mode of open
    panes.
  - Applying settings while a preview is shown replaces the preview.
  - The preview is shown in the window that opened the workspace manager,
    although the workspace appearance layer is shared by all windows.
- **All options:** When a workspace saves all appearance options, options that
  have no row in the workspace appearance editor take the workspace's saved
  values, or the current global values when they are first saved. They can't
  be edited individually there.
- **Stylesheet:** A workspace's own options never include the custom
  stylesheet. Only a theme profile attached to the workspace can supply one.
- **Blur and translucency:** These are reported per workspace only when the
  workspace saves those options; otherwise the global values apply.
- **Plasma session:** The theme settings have been tested only headlessly,
  without a Plasma session.

- **Recommendation:**
  - **Corner radius:** Choose the radius closest to the widget style's own
    corners so that rounded and unrounded controls look consistent.
  - **Plasma accent:** If the accent changes without updating the window,
    apply settings again or restart Plasma Opus.
  - **Workspace preview:** Preview before applying other settings, then choose
    **Revert** or save. To check view modes, switch to the workspace after
    saving it.
  - **All options:** Set those options in the settings dialog first, then save
    the workspace with **All options**, or use **Selected options** to save only
    the options you edit.
  - **Stylesheet:** Save a theme profile with the stylesheet and attach it to
    the workspaces that need it.
  - Comment:
    [#687.7149624993](https://github.com/mikepadiernos/plasma-opus/issues/687#issuecomment-7149624993)
- **Proposed improvements:**
  - Round buttons, combo boxes and panel frames with the chosen radius.
  - Preview in every window and keep the preview after applying settings.
  - Show every saved option in the workspace appearance editor.
  - Allow a per-workspace stylesheet.

## Branding

The M19 branding and its M20 settings have these limits
([#689](https://github.com/mikepadiernos/plasma-opus/issues/689)):

- **High contrast:** The startup presentation uses the colour scheme's window
  and text colours with a solid border, but the folder and the word "Opus"
  keep the accent colour, which can contrast less with the card than the text
  does. The version and description are always drawn in the text colour.
- **Screen readers:** The presentation has an accessible name (name and
  version) and description, but it is not announced when it appears, so it
  doesn't interrupt reading.
- **Duration:** The presentation stays for two seconds; the duration can't be
  changed.
- **Icon themes and other desktops:** The small and symbolic icons install to
  `hicolor`, so an icon theme that ships its own `org.kde.opusfilemanager` icon
  takes precedence. The folder follows the Plasma accent and the symbolic icon
  follows the text colour only where KDE's icon loader applies the colour
  scheme; elsewhere they use their built-in Breeze colours.
- **Release notes:** The v0.1.0 and v0.2.0 release notes predate the
  [release notes template](releases/TEMPLATE.md) and were not rewritten.
- **Plasma session:** The presentation, its settings preview and the icons
  have been tested only headlessly, without a Plasma session.

- **Recommendation:**
  - **High contrast:** Read the version and description, which use the text
    colour. If the accent contrasts poorly with the card, choose a stronger
    accent in Plasma's System Settings, or leave the startup presentation off
    (the default) in **Preferences › Branding › Presentation**.
  - **Screen readers:** Use **Help › About Plasma Opus** for the name, version
    and description.
  - **Duration:** Press any key, click or scroll to dismiss the presentation
    early. Input still reaches the window. If it distracts, turn it off in
    **Preferences › Branding › Presentation**.
  - **Icon themes and other desktops:** To see the Plasma Opus icon under a
    theme that overrides it, remove the theme's `org.kde.opusfilemanager` icon
    or choose another icon theme. Outside Plasma, expect the Breeze blue folder.
  - **Release notes:** Start new releases from the template.
  - **Plasma session:** Check the presentation's appearance on Plasma,
    including fractional scaling and Wayland, before relying on it in
    screenshots or release materials.
  - Comment:
    [#689.7150293713](https://github.com/mikepadiernos/plasma-opus/issues/689#issuecomment-7150293713)
- **Proposed improvements:**
  - Draw the folder and "Opus" in the text colour under high contrast.
  - Offer an option to announce the presentation to screen readers.
  - Make the duration configurable.

## Native title and header padding

Native Qt/KDE dock titles and column headers follow the active widget style
([#560](https://github.com/mikepadiernos/plasma-opus/issues/560)):

- **Padding:** Their horizontal padding can be less than eight logical
  pixels. For example, Breeze's column-label rectangle starts six pixels from
  the section edge.

- **Recommendation:** Keep native rendering and choose a widget style whose
  spacing is comfortable for your needs. Check the active style after changing
  desktop appearance; Plasma Opus does not guarantee an eight-pixel minimum for
  these controls.
  - Comment:
    [#560.7071231166](https://github.com/mikepadiernos/plasma-opus/issues/560#issuecomment-7071231166)

## Jalali dates

With a Persian regional locale or an explicit Jalali calendar override, Plasma
Opus's date displays use the Jalali calendar. This has these limits
([#436](https://github.com/mikepadiernos/plasma-opus/issues/436)):

- **Listing format:** With the automatic calendar, the ISO 8601 listing format
  remains Gregorian; an explicit calendar override takes precedence.
- **KDE-provided dates:** KDE controls dates in its own dialogs and tooltips,
  and the additional file information shown under icons in icon and compact
  views uses KDE's `KFileItemDelegate`; these dates may remain Gregorian.

- **Recommendation:** Use Details view or the Plasma Opus Information panel for
  Jalali dates. Select Jalali explicitly, or use a Persian regional locale with
  the automatic calendar and a Locale listing format. Verify the calendar before
  interpreting KDE-provided dates.
  - Comment:
    [#436.7041290164](https://github.com/mikepadiernos/plasma-opus/issues/436#issuecomment-7041290164)

## Translation format overrides

The regional locale and custom digit shapes do not control every display
([#440](https://github.com/mikepadiernos/plasma-opus/issues/440)):

- **Digit shapes:** Plasma Opus applies digit shapes to its file dates,
  listing sizes, preview line numbers and selected gallery and chart fields.
- **Other displays:** Other text and numbers, editable Qt controls,
  KDE-provided details and dialogs, and separately embedded applications can
  use their own locale or desktop settings.
- **C/POSIX message locale:** An explicitly selected non-English interface
  language also requires an installed non-C UTF-8 message locale; Plasma Opus
  warns and follows the system language if none can be used.

- **Recommendation:** Use Details view or the Information panel for overridden
  file dates and verify other displays against the desktop locale. Configure
  desktop regional settings for consistency elsewhere. If the selected language
  is unavailable under C/POSIX, install a non-C UTF-8 system locale and restart
  Plasma Opus.
  - Comment:
    [#440.7045227580](https://github.com/mikepadiernos/plasma-opus/issues/440#issuecomment-7045227580)

## Desktop integration

The M27 desktop integrations have these limits
([#728](https://github.com/mikepadiernos/plasma-opus/issues/728)):

- **🔗 FileManager1 service:** Plasma Opus claims
  `org.freedesktop.FileManager1` only when it starts while it is the default
  folder handler, and never replaces another owner, such as a running Dolphin.
  Changing the default handler while it runs takes effect at the next start.
  The per-user activation file from `install.sh --default` stays when another
  default is chosen in System Settings, so requests made while no file
  manager runs still start Plasma Opus. Requests open tabs in the active pane
  of the existing window; missing items are skipped and reported, and flat
  view opens the folder without a selection.
  ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279))
- **🌐 Language preferences:** Plasma Opus reads Plasma's language list
  only at start. Changes in Plasma's Region & Language settings reach running
  applications through `LANGUAGE` only after logging in again, and Plasma Opus
  falls back to `plasma-localerc` only when `LANGUAGE` is unset. Listed
  languages without a Plasma Opus catalogue are skipped; when none has one,
  the original English text is shown.
  ([#437](https://github.com/mikepadiernos/plasma-opus/issues/437))
- **🔔 Notifications and jobs:** only the checksum, folder synchronization,
  duplicate finder, file split, batch rename, batch image conversion and
  tag-library refresh dialogs report to Plasma's job progress. Automatic
  library refreshes, previews, gallery loads, searches and archive tools do
  not. Plasma's job entry cannot bring the dialog back to the front; it offers
  only its own actions, such as opening the destination folder. Outside a
  Plasma session, with no job-view service running, nothing is reported and
  the dialogs remain the only progress display. There is no Plasma Opus
  setting to turn the reporting off yet.
  ([#388](https://github.com/mikepadiernos/plasma-opus/issues/388))
- **📌 System tray and global shortcut:** the tray icon is on by default;
  minimizing and closing to the tray are off until turned on under
  **Settings › General › System Tray**. On Wayland, KWin
  does not tell applications when they are minimized, so minimizing to the
  tray works only on X11. KWin's focus-stealing prevention may keep a window
  shown from the tray or the global shortcut behind the active window and
  highlight it in the task manager instead. When another application already
  uses Meta+Alt+O, the shortcut stays unassigned without a notice. A window
  hidden in the tray keeps its tabs and running operations, but there is no
  option to start hidden in the tray.
  ([#285](https://github.com/mikepadiernos/plasma-opus/issues/285))
- **🧭 Plasma Activities:** associations are off by default and are only
  offered, in a message above the panes, when the current Activity changes;
  the Activity that is current when Plasma Opus starts is not offered. An
  association whose workspace is already active offers only its default
  folders, and its own behaviours apply only when its workspace is switched
  to. Behaviours chosen by an association last until another workspace is
  chosen, and they are not saved with the session. Default folders always
  open in new tabs, so tabs accumulate with each switch. Only local folders
  are checked before they are offered; a missing remote folder fails when
  its tab opens. Associations are kept for removed Activities and missing
  workspaces until they are cleared under **View › Activity Associations**.
  Without the Activities service, nothing is offered and removed Activities
  cannot be told apart from existing ones.
  ([#387](https://github.com/mikepadiernos/plasma-opus/issues/387))
- **📱 KDE Connect sharing:** **File › Send to Device…** sends only local
  regular files; folders, remote KIO locations and special files are listed
  as unsupported and skipped. A successful reply from KDE Connect means it
  accepted the files, not that they arrived: the transfer's progress, result
  and any failure appear only in KDE Connect's notifications, not in Plasma
  Opus. Device states are read when the dialog opens or KDE Connect reports a
  change, with short blocking D-Bus calls. The action is not in the file
  context menu, where KDE Connect's own **Send to** service menu may already
  appear.
  ([#389](https://github.com/mikepadiernos/plasma-opus/issues/389))
- **🔎 KRunner:** the plugin works only from an installed build: KRunner
  finds it through the installed plugin file and starts the helper through
  D-Bus activation, so a build run from its tree offers nothing. Only saved
  workspaces and Favorites are offered, matched by name; locations inside a
  workspace, history and other places are not. Remote Favorites are offered
  without checking that they can be reached, and fail when their tab opens.
  Choosing a workspace always asks before switching. The plugin is enabled
  by default once installed. The helper keeps running after KRunner first
  starts it, reads the settings again whenever a search session ends, and
  is not stopped when Plasma Opus quits. KRunner may need restarting before
  a newly installed plugin appears.
  ([#390](https://github.com/mikepadiernos/plasma-opus/issues/390))
- **⬇️ Service-menu downloads:** downloading reuses Dolphin's
  `servicemenu.knsrc` provider and its `servicemenuinstaller`, so it needs
  Dolphin installed, and what it installs, updates or removes is shared with
  Dolphin's own download window. Plasma Opus does not review downloaded
  service menus; they come from the KDE Store and can run any command.
  Installation errors are shown in the download window, and the tab names
  only the entries that failed. Service menus installed outside the download
  window, or by another application while the settings are open, appear only
  after the window is used again or the settings are reopened.
  ([#369](https://github.com/mikepadiernos/plasma-opus/issues/369))
- **🗂️ System file chooser:** setup is per user and only through
  `--install-portal` or `./install.sh --file-chooser`; the planned
  [M28 settings row](ROADMAP.md#m28-integration-settings) does not exist yet.
  It writes only `kde-portals.conf`, so it applies in Plasma sessions and not
  in other desktops. xdg-desktop-portal reads per-user portal descriptions
  only from version 1.21.0; older versions ignore Plasma Opus and fall back
  to the next chooser in the preference, normally KDE's. Applications that
  open their own dialog without the portal keep it, including most native
  Qt and KDE applications. The chooser has no tabs,
  previews, side panels or indexed search, only a name filter over the
  current folder, which also lists matching files when choosing a folder.
  Only local files are accepted; remote locations can be browsed but not
  returned. A modal request keeps the dialog over its window, but the
  application's own window is not blocked. Setup and uninstalling restart
  xdg-desktop-portal, which closes file dialogs open in other applications.
  This has been verified in a headless container only, not yet on a Plasma
  session ([pending task](TODO.md#system-file-chooser-on-plasma-286)).
  ([#286](https://github.com/mikepadiernos/plasma-opus/issues/286))
- **💽 Mount manager:** the dialog checks for mounts made by other programs
  every three seconds while it is open, and sizes and free space are read
  only when it opens, on **Refresh** and after a mount. Eject works only for
  optical drives; USB drives can be unmounted but not powered off. File
  systems that Solid does not manage can be mounted only when their fstab
  entry allows users, with `user`, `users`, `owner` or `group`, and unmounted
  only when it allows users or they are FUSE mounts; `/` can never be
  unmounted. autofs and systemd automount points show as not mounted until
  they are first accessed. Pseudo, snap, container and portal file systems
  are hidden, with no option to show them. **Show only removable devices** is
  off by default until the planned
  [M28 setting](ROADMAP.md#m28-integration-settings). Plasma's and UDisks2's
  authorization prompts have been verified in a headless container only, not
  yet on a Plasma session.
  ([#703](https://github.com/mikepadiernos/plasma-opus/issues/703))
- **💽 Disk images:** mounting needs the Mount ISO plugin from Dolphin's
  plugins, usually packaged as `dolphin-plugins`; without it the actions
  explain what is missing and do nothing. The plugin blocks the window while
  UDisks2 attaches and mounts the image, for up to about 20 seconds, and
  reports no errors, so the reason shown after a failure is inferred from
  what Solid reports afterwards and may be inaccurate. Mounting and
  unmounting may ask for authorization through polkit. A refused mount can
  leave the image attached as a loop device without a mount point; choosing
  **Mount Disk Image** again detaches it and retries. Solid hides loop
  devices whose image is outside the home folder, so those images do not
  appear in Places even when mounted. Only images Dolphin's plugin
  recognizes are offered, and only one at a time. This has been verified in
  a headless container with a simulated plugin only, not yet on a Plasma
  session.
  ([#648](https://github.com/mikepadiernos/plasma-opus/issues/648))
- **📦 Application packages:** packages are recognized by MIME type and by
  the standard installation folders, `~/.local/share/flatpak`,
  `/var/lib/flatpak` and `/snap`; custom Flatpak installations are not.
  A Flatpak bundle's application ID is not read, so its installed state and
  details are unknown until it is installed and opened from its folder.
  Sandbox permissions are shown only after installing, since Flatpak
  reports them only for installed applications; installing grants what the
  application requests. Local snap files are installed with `--dangerous`,
  without a Store signature check or automatic updates, and snaps that need
  classic confinement fail to install. Tools run without a terminal, so
  Flatpak and snapd may ask for authorization through polkit, but any other
  prompt fails the change, and progress is not shown beyond a busy
  indicator. **Add to Application Menu** runs a type 2 AppImage's runtime
  to extract its desktop file and icon; type 1 AppImages, and others whose
  files cannot be read, get their file name and a generic icon. Menu entries
  point to the AppImage's current path and break when it is moved or
  deleted. AppImage updates need AppImageUpdate's `appimageupdatetool`.
  This has been verified in a headless container with simulated package
  tools only, not yet with real `flatpak`, `snap` or Discover on a Plasma
  session.
  ([#281](https://github.com/mikepadiernos/plasma-opus/issues/281))
- **🪟 KWin script:** the script has been tested only against a simulated
  KWin in the headless container, not yet on a Plasma session
  ([pending task](TODO.md#kwin-script-on-plasma-707-708)). Its use of KWin's
  tiling API, assigning a tile or `frameGeometry`, and its X11 and Wayland
  behaviour are unverified, and KWin's scripting API can change between
  Plasma releases. Requests reach the script through its **Plasma Opus:
  Process Window Requests** shortcut, which appears, unassigned, in KWin's
  shortcut list. Only the main window's placement is kept, one per workspace,
  without its maximized or full-screen state; a placement is kept only once
  the window has been moved, resized or retiled with that workspace current.
  Tiled companions are only Plasma Opus's own detached windows, such as the
  gallery and the popped-out second pane; terminals, editors and other
  applications it opens are separate processes and are not placed.
  Windows are matched by desktop file name and process id, so a sandbox that
  hides the real process id, such as Flatpak, prevents matching. Virtual
  desktops can be assigned only after the script has reported them. The
  options are in **Settings › Configure KWin Integration…** until the
  [M28 KWin scripts settings](ROADMAP.md#m28-integration-settings).
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **⬇️ Drop-down window:** it is off by default, and its global shortcut
  is unassigned until one is chosen. Only the main window drops down, and
  while it is on, its placement is not kept for workspaces. Without the
  script it opens as a regular window, wherever KWin places it, and stays in
  the taskbar. Hiding on focus loss waits 200 ms and does not hide while a
  dialog, menu or another Plasma Opus window is active; closing the window
  still quits or hides it to the tray as usual instead of rolling it up. The
  slide animation needs KWin's **Sliding popups** effect. The screen and
  touch-screen edges are shared with KWin's own edge actions, and only one
  can claim an edge.
  ([#708](https://github.com/mikepadiernos/plasma-opus/issues/708))
- **🗂️ File-type categories:** Baloo classifies indexed files by their
  detected content, while the live fallback matches MIME types guessed from
  names and file extensions, so the two can list different files, such as a
  renamed file or an ebook saved without its extension. **Ebooks** lists
  only EPUB, Mobipocket, FictionBook, comic book archives and DjVu, and
  **Application Packages** only Flatpak, AppImage, Debian, RPM and Snap
  packages. **Large Files** is fixed at 100 MiB and is always a live search,
  since Baloo doesn't index file sizes. The entries are saved in Plasma's
  shared `user-places.xbel`, marked for Plasma Opus only, with their titles
  in the language in use when they were added. They also show in the KDE
  file dialogs Plasma Opus opens, where they can't be opened. KDE's older
  **Search For** entries, where a distribution still adds them, appear beside
  them. A removed entry isn't added again.
  ([#649](https://github.com/mikepadiernos/plasma-opus/issues/649))
- **💻 Terminal providers:** **Open Terminal With** only lists the
  default terminal, Konsole, Yakuake and Kwi, and finds them through `PATH`,
  so a terminal installed only as a Flatpak shows as not installed. It opens
  the active pane's folder, also from a selected folder's context menu.
  Yakuake has no option to start in a folder, so Plasma Opus adds a session
  and types `cd` into it; a shell that is slow to start or a profile that
  runs another program can miss or misread it, and the command can appear in
  shell history unless leading spaces are ignored. Plasma Opus can't tell
  whether Yakuake's window is shown, so it doesn't open or close it, and
  waits 15 seconds for a Yakuake it started. A running Kwi decides how a
  second request opens. The choice isn't remembered between uses.
  ([#289](https://github.com/mikepadiernos/plasma-opus/issues/289))

- **📝 Editor integrations:** **Edit With** only lists KWrite, Kate, Neovim,
  Rune, micro and nano, and finds them through `PATH`, so an editor
  installed only as a Flatpak shows as not installed. Rune is looked up as
  `rune` and hasn't been tested. Neovim, Rune, micro and nano open in a new
  embedded terminal tab rather than a separate terminal window until the M28
  editor settings choose a session type, so they need the Konsole or Kwi
  component, and they only open local files; KWrite and Kate also open
  remote files through KIO. Folders can't be opened. The editor's tab starts
  in the first file's folder, but a terminal profile with its own start
  folder takes precedence, and the tab closes when the editor exits. The
  editors start with their own configuration, and the choice isn't
  remembered between uses.
  ([#319](https://github.com/mikepadiernos/plasma-opus/issues/319))

- **📄 Office documents:** Plasma Opus doesn't embed an office editor. The
  preview panel shows office documents read-only, and only when a document
  viewer component for the format, such as Calligra's, is installed; otherwise
  it shows the file's details. **Open in Office Application** lists only
  LibreOffice, Calligra and ONLYOFFICE, and finds them through `PATH`, so an
  application installed only as a Flatpak or AppImage shows as not
  installed. It covers OpenDocument and Microsoft Office documents,
  spreadsheets and presentations; RTF, CSV and OpenDocument drawings aren't
  included. LibreOffice and ONLYOFFICE only open local files, and Calligra
  needs a separate Words, Sheets or Stage program for each kind. The
  application handles saving and format conversion, and Plasma Opus doesn't
  track whether a document is open or has unsaved changes.
  ([#674](https://github.com/mikepadiernos/plasma-opus/issues/674))

- **✏️ Edit action:** **Edit** chooses a tool by file type rather than by
  the file associations in System Settings, and always takes the first
  installed tool in a fixed order, so it can't be pointed at a preferred
  editor. It covers text, office documents, SVG drawings, raster images and
  audio; videos, PDF documents, archives and other types can be previewed
  but not edited. Separate tools are found through `PATH`, so Flatpak and
  AppImage installations show as not installed, and only the built-in
  editor, KolourPaint and Calligra open remote files. It edits one file at a
  time. The chosen tool handles saving: some image and audio editors save to
  their own format or export, and an image editor may not support every
  format the preview shows. Plasma Opus doesn't track whether a separate
  tool has unsaved changes.
  ([#675](https://github.com/mikepadiernos/plasma-opus/issues/675))

- **📚 Ebook and font previews:** Ebooks, comics, DjVu documents and help
  files need Okular's viewer component, and the preview only offers what the
  component shows in a narrow panel, without its toolbars or reading
  position. Okular's support for each format depends on its installed
  backends. Fonts are previewed with Qt rather than Plasma's font viewer
  component, without a character map or detailed font information. A font
  whose family name matches an installed font may show the installed one,
  font collections show only their first face, and fonts over 64 MiB aren't
  previewed. Web fonts can be previewed but not installed, and
  **Install Font…** needs Plasma's `kfontinst` in `PATH`; the installer
  decides whether the font is installed for the user or system-wide.
  ([#650](https://github.com/mikepadiernos/plasma-opus/issues/650))

- **🔐 Remote connections:** A profile's private key is passed only to SSH terminals: KIO's SFTP worker
  uses the SSH agent and its own key lookup, so browsing may still ask for a
  password. SSH terminals need `ssh` in `PATH` and the Konsole component, and
  start in the remote home folder rather than the profile's folder.
  Connections aren't tested when saved, and errors appear only once a pane
  opens them. Removing a connection doesn't ask for confirmation and leaves
  any password saved in KWallet. Profiles don't appear in Places.
  ([#282](https://github.com/mikepadiernos/plasma-opus/issues/282))

- **🌍 Network shares and FTP:** KDE ships no FTPS worker, so FTPS profiles
  open only when a third-party worker is installed. Plain FTP profiles can't
  require encryption, and KIO's FTP worker doesn't upgrade them to TLS.
  **Go › Network** shows what KIO's `network:/` worker or Dolphin's `remote:/`
  folder discovers, which depends on Avahi and, for Windows shares, WS-Discovery
  being installed and allowed through the firewall. Plasma Opus doesn't add
  discovered services to saved connections or Places. SMB sign-in, including
  domains and Kerberos, follows KIO's SMB worker.
  ([#651](https://github.com/mikepadiernos/plasma-opus/issues/651))

- **☁️ rclone cloud storage:** remotes open only by mounting, which needs FUSE
  and `fusermount3`; serving a remote over WebDAV isn't offered. Mounts last
  only while Plasma Opus runs, including hidden in the tray, so applications
  editing files on a mount lose access when it quits, and a busy mount may
  stay behind after rclone stops. A mount left by an earlier session opens
  but can't be unmounted from the menu. Remotes with an encrypted rclone
  configuration fail unless `RCLONE_CONFIG_PASS` is set in the environment
  Plasma Opus starts with. Mounting waits up to 20 seconds, and rclone's
  cache settings, other than caching writes, are its defaults. Mounts don't
  appear in Places, and the list refreshes each time the menu opens.
  ([#652](https://github.com/mikepadiernos/plasma-opus/issues/652))

- **💽 Remote drives:** KIO FUSE can't mount read-only or in a chosen folder,
  and can't unmount a single connection: **Unmount** only removes it from
  Plasma Opus and Places, and its folder stays available until KIO FUSE
  stops. sshfs needs `ksshaskpass` to ask for passwords and new host keys;
  without it, only the SSH agent and keys sign in, and unknown hosts are
  refused. davfs2 and CIFS mount only through a user-mountable fstab entry
  that an administrator adds, usually with the user in the `davfs2` group
  and credentials in the backend's own secrets file, and read-only comes
  from that entry. S3-compatible storage and KDE Online Accounts locations
  aren't offered; S3 works through **Cloud Storage** with an rclone remote.
  Mounting at login means mounting when Plasma Opus starts, so it needs
  Plasma Opus in autostart, and mounting when a workspace is activated isn't
  offered. Changed mount options apply after unmounting. All mounts end
  when Plasma Opus quits, and a connection mounted outside Plasma Opus opens
  but can't be unmounted from it. Media streams only where the backend reads
  ranges, which sshfs, CIFS and KIO FUSE's SFTP and SMB workers do.
  ([#702](https://github.com/mikepadiernos/plasma-opus/issues/702))
- **📡 Temporary LAN sharing:** Anyone who sees a link can use it until
  sharing stops: there is no password, and a single link can't be revoked.
  Without HTTPS, others on the network can read links and files in transit.
  The HTTPS certificate is self-signed and made for each share, so browsers
  warn that it isn't trusted; HTTPS needs `openssl`, and QR codes need KDE's
  Prison library when Plasma Opus is built. Firewalls such as firewalld and
  ufw may block the port, which Plasma Opus doesn't open. WebDAV covers
  browsing, downloading and uploading files only: folders can't be created,
  renamed or deleted, nothing is locked, and uploads must declare their size,
  so clients that need locking, such as Windows' WebDAV redirector, may
  connect read-only or not at all. Uploads go only to the upload folder and
  its existing subfolders, and a leading dot is removed from their names.
  Special files aren't served. Each response closes its connection, so
  folders with many small files load more slowly, and at most 32 connections
  are served at once. Shares aren't announced on the network, the address
  list isn't refreshed when the network changes, and sharing ends when the
  dialog closes.
  ([#653](https://github.com/mikepadiernos/plasma-opus/issues/653))
- **👤 KDE Online Accounts:** Only Google Drive, through kio-gdrive, and
  Nextcloud and ownCloud storage, through WebDAV, open as folders; other
  services, such as contacts and YouTube, and providers without a KDE file
  service aren't listed as folders. Plasma Opus doesn't pass account
  credentials to KIO, so WebDAV storage asks for the password the first
  time unless Plasma's Online Accounts integration has already given it to
  KDE's password server. Expired sign-ins and denied access appear as KIO
  errors in the pane, not in the menu. Accounts aren't shown in Places or
  offered as remote drives, and the menu reads accounts only when it opens.
  The menu needs Plasma Opus built with libaccounts-qt6, and **Manage Online
  Accounts…** needs System Settings with its Online Accounts page.
  ([#370](https://github.com/mikepadiernos/plasma-opus/issues/370))
- **🔁 rsync backend:** Each run copies or deletes in one direction, so plans
  that work both ways are refused, and it deletes only regular files missing
  from the source; folders, links and type changes aren't synchronized. It
  works between a local folder and a folder on an SSH host that has rsync
  installed, not between two hosts or with other KIO locations. Every
  comparison, dry run and transfer opens new SSH connections, so passwords
  may be asked for several times, and without ksshaskpass a host that needs
  a password or an unknown host key fails. Remote comparisons use rsync's
  listings: times have one-second precision, symbolic links appear only when
  link targets are compared, and filter patterns with `?`, `\` or a leading
  or trailing `/` are refused. Copies keep modification times but not
  permissions, ownership, ACLs or extended attributes. The dry run is a
  snapshot, runs aren't added to KIO's undo history, and canceling or an
  error leaves the files already transferred changed.
  ([#363](https://github.com/mikepadiernos/plasma-opus/issues/363))
- **🔄 Cloud sync:** Pairs synchronize only while Plasma Opus runs;
  **Synchronize in the background after login** starts it in the system
  tray. Each run of a network-folder pair lists both folders in full and
  compares sizes and modification times, not contents, so an edit that
  keeps the size and time is missed, and a remote that changes modification
  times, or an interrupted upload, can cause conflicts on the next run.
  Renamed files are detected, but a renamed folder is copied anew and the old
  one deleted. **Keep Both** saves the remote version beside the local file,
  and the next run uploads that copy. A file and a folder with the same name
  are left for you to rename, and a remote subfolder that can't be read is
  skipped for that run. rclone pairs use `rclone bisync`: the first run and
  any filter change need `--resync`, which copies newer files both ways
  without deleting anything; the review shows a dry run, so changes made
  after it are synchronized too; and the report gives no counts. Excluding a
  nested pair whose folder name contains `*`, `?` or `[` fails for rclone
  pairs. Only Nextcloud, ownCloud, Syncthing and Dropbox folders are
  recognized as owned by another client. There are no desktop
  notifications: runs that need review or found conflicts are shown in the
  tray and the status bar. Pausing on metered connections relies on Qt's
  network information, usually from NetworkManager.
  ([#718](https://github.com/mikepadiernos/plasma-opus/issues/718))
- **📊 Cloud activity:** Speeds and remaining times are shown only for
  transfers in the transfer queue; rclone mounts and sync runs show their
  status, not their speed, and **Pause All** doesn't stop mounts from
  uploading or sync runs you start yourself. The bandwidth, parallel-transfer
  and cache limits apply only to rclone, not to pairs that synchronize network
  folders through KIO, and take effect at a pair's next run and a remote's
  next mount. Files waiting to upload are found in rclone's cache metadata in
  Plasma Opus's cache folder, so mounts started outside Plasma Opus aren't
  counted, and a change to rclone's metadata format would hide them. The
  offline cache keeps files already opened, but listing a folder or opening an
  uncached file still needs the remote. Unmounting, closing and quitting wait
  for uploads, but logging out waits only when the session lets applications
  ask; otherwise uploads stop and resume at the next mount. Space is shown
  only for mounted remotes whose provider reports it. Exported definitions
  contain no passwords, and imported sync pairs start paused.
  ([#719](https://github.com/mikepadiernos/plasma-opus/issues/719))
- **🛡️ Administrator mode:** **Act as Administrator** needs the separate
  `kio-admin` package and is reported as missing without it. Items in the
  `admin:` view are not local files, so **Move to Trash**, terminals, custom
  commands, checksums, batch rename, symbolic links, metadata editing, split
  and combine and editing in the preview are unavailable there, and deleted
  items skip the trash. Copied or dragged items carry `admin:` URLs that only
  KIO-based applications open. How often the password is asked and how long
  an authorization lasts are decided by `kio-admin` and Polkit; stopping
  administrator mode does not revoke an authorization already granted.
  Entering it in a pane with linked navigation unlinks the panes. The
  container used for development has no `kio-admin` or Polkit agent, so the
  prompt and elevated operations await
  [verification on Plasma](TODO.md#administrator-mode-on-plasma-288).
  ([#288](https://github.com/mikepadiernos/plasma-opus/issues/288))
- **🤖 MCP server:** `opus-file-manager-mcp` speaks only the stdio
  transport, so a client must run on the same computer and start the server
  itself; there is no HTTP transport or access token. Until the M28 settings
  page, access, folders and the audit log are changed only in
  `opus-file-managerrc`, and a running server keeps the settings it started
  with. It reads only UTF-8 text, at most 1 MiB per call, and refuses binary
  files and other encodings. Listings and searches return at most 1,000
  items, and a search stops after visiting 200,000 entries. Moves work only
  within one file system, and a failed folder copy leaves a partial copy.
  Items outside the allowed folders, including the targets of symbolic links,
  are refused, but links copied inside a folder keep pointing where they did.
  Tools work on local files only, not KIO locations, and changes are made
  directly rather than as Plasma Opus jobs, so they can't be undone in Plasma
  Opus. The audit log records paths but no contents, and grows until it is
  deleted.
  ([#287](https://github.com/mikepadiernos/plasma-opus/issues/287))

- **Recommendation:**
  - **FileManager1 service:** Run `./install.sh --default`, quit Dolphin if
    it is running and restart Plasma Opus so it claims the service. To hand
    requests back, run `./install.sh --uninstall` or delete
    `~/.local/share/dbus-1/services/org.kde.opusfilemanager.FileManager1.service`
    as well as changing the default in System Settings.
  - **Language preferences:** After changing Plasma's languages, log out and
    back in, or select the language under **Settings › Translation ›
    Language and Layout** and restart Plasma Opus. The status line on that page
    shows which language is in effect.
  - **Notifications and jobs:** Keep the Plasma Opus dialog open, or switch
    to it from the task manager, to read error details or change options.
    To hide the progress, turn off application progress in notifications
    under **System Settings › Notifications**; this affects every
    application.
  - **System tray and global shortcut:** To hide the window instead of
    closing it, check **Hide the window in the system tray when it is
    closed** under **Settings › General › System Tray**; on Wayland, prefer
    this to hiding when minimized. Clear **Show an icon in the system tray**
    there to remove the icon. If the shortcut does nothing, assign one to
    **Show or Hide Plasma Opus** under **Settings › Configure Keyboard Shortcuts**, and keep the icon
    always visible through the tray's **Configure System Tray…** entries.
  - **Plasma Activities:** Turn associations on under **Settings ›
    Workspaces › Activities**, then choose each Activity's workspace,
    behaviours and folders under **View › Activity Associations**. To apply
    the current Activity's association at any time, including at start or
    after choosing **Not Now**, use **View › Apply Activity Association**.
    Close default-folder tabs you no longer need, and clear associations of
    removed Activities once they are not coming back.
  - **KDE Connect sharing:** To send a folder, compress it first with
    **Compress…** and send the archive; copy remote files to a local folder
    before sending. Watch KDE Connect's notification to confirm delivery. If a
    paired device shows as offline, open KDE Connect on both devices and press
    **Refresh**; if it shows file sharing as turned off, enable the **Share
    and receive** plugin for it in KDE Connect's settings.
  - **KRunner:** Install with `./install.sh`, then run `kquitapp6 krunner`
    or log out and back in if **Plasma Opus** is missing from **System
    Settings › Search › Plasma Search**. Turn off what should not be offered
    under **Settings › General › Plasma Search**, or the plugin itself in
    System Settings. Add a folder to Favorites to make it searchable.
  - **Service-menu downloads:** Install Dolphin to enable **Download New
    Services…**; Plasma Opus does not need to be the default file manager.
    Read a service menu's description and comments before installing it, and
    hide any unwanted action under **Settings › Context Menu › Service
    Menus** instead of removing it if Dolphin should keep it. Reopen the
    settings to see service menus installed by other means.
  - **System file chooser:** Run `opus-file-manager --install-portal` from
    an installed build in a Plasma session with xdg-desktop-portal 1.21.0 or
    later, after closing open file dialogs. If applications still show KDE's
    chooser, check `xdg-desktop-portal --version` and log out and back in.
    Copy remote files to a local folder before choosing them. To go back,
    run `opus-file-manager --uninstall-portal`, or
    `./install.sh --uninstall`, which restores the previous
    `kde-portals.conf`.
  - **Mount manager:** Press **Refresh** for current sizes. To power off a
    USB drive, use Plasma's **Disks & Devices** applet. To let Plasma Opus
    mount an fstab entry without root, add `user` or `users` to its options
    in KDE Partition Manager, or let UDisks2 manage the device instead. Open
    an automount point's folder once to mount it.
  - **Disk images:** Install `dolphin-plugins` from Discover or your
    distribution, and keep images inside the home folder so they appear in
    Places once mounted. Wait for the window to respond after choosing
    **Mount Disk Image**. If an image stays attached without a mount point,
    choose **Unmount Disk Image**, or **Mount Disk Image** again to retry.
  - **Application packages:** Install the `flatpak` or `snap` packages, and
    Discover for **Open in Discover**. Install a Flatpak bundle, then open
    its folder under `~/.local/share/flatpak/app` to review its permissions,
    and adjust them with Plasma's Flatpak permission settings in System
    Settings or with Flatseal. Prefer installing snaps from the Store
    in Discover so they are verified and updated; for a snap that needs
    classic confinement, run `snap install --classic` in a terminal. Remove
    an AppImage from the application menu before moving or deleting it, and
    add it again from its new location.
  - **KWin script:** Install it from **Settings › Configure KWin
    Integration…** in a Plasma session, then move or resize the window once
    with each workspace current so its placement is kept. If the status says
    the script has not contacted Plasma Opus, press **Refresh**, or disable
    and enable it under **KWin Scripts Settings…**. Run Plasma Opus outside
    Flatpak for window management. Leave the processing shortcut unassigned.
    Use KWin's **Window Rules…** for placements the script does not restore,
    such as maximized state.
  - **Drop-down window:** Turn it on and choose a global shortcut under
    **Settings › Configure KWin Integration…**, and install the KWin script
    so it keeps above other windows and out of the taskbar. Enable **Sliding
    popups** under **System Settings › Window Management › Desktop Effects**
    for the animation. If an edge does nothing, free it under **System
    Settings › Mouse & Touchpad › Screen Edges** or **Touch Screen**.
  - **File-type categories:** Open a category from the main window's Places,
    not from a file dialog. If indexed results look incomplete, open
    **File › Indexing Status…** to check the indexer and the folder's
    coverage, or choose **Search a Folder Live…** to read the files directly.
    Hide a category you don't use with Places' **Hide** action instead of
    removing it, so it can be shown again. For other sizes or formats, search
    with `size>` or `type:` in the search panel.
  - **Terminal providers:** Install the terminal from your distribution so
    its command is in `PATH`, or keep using **Open Terminal Here**, which
    follows the terminal chosen under **System Settings › Default
    Applications**. After opening a Yakuake session, press Yakuake's shortcut,
    F12 by default, if its window is hidden, and check the folder before
    running commands. To keep the `cd` out of shell history, set
    `HISTCONTROL=ignorespace` in Bash, or `setopt HIST_IGNORE_SPACE` in Zsh.
  - **Editor integrations:** Install the editor from your distribution so
    its command is in `PATH`, and install the Konsole component for terminal
    editors. To edit remote files in a terminal editor, copy them to a local
    folder first, or use KWrite or Kate. Save your work before closing the
    editor's terminal tab.
  - **Office documents:** Install LibreOffice, Calligra or ONLYOFFICE from
    your distribution so its command is in `PATH`, and install Calligra for
    previews. To open remote documents in LibreOffice or
    ONLYOFFICE, copy them to a local folder first, or use Calligra. Open RTF
    and CSV files with **Open With**.
  - **Edit action:** Install a listed tool from your distribution, use
    **Edit With** or **Open in Office Application** to pick a specific
    editor, or **Open With** for other applications. Copy remote files to a
    local folder before editing them in a local-only tool, and check the
    format when saving from an image or audio editor.
  - **Ebook and font previews:** Install Okular for ebook, comic, DjVu and
    help-file previews, and open long documents in Okular to read them. To
    compare a font with an installed one of the same name, or inspect its
    characters, open it in Plasma's font viewer, KFontView.
  - **Remote connections:** Install `kio-extras` for SFTP and OpenSSH for
    SSH terminals. Load keys into the SSH agent, with `ssh-add`, so browsing
    and terminals sign in without a password. Prefer WebDAV over HTTPS, and
    remove saved passwords you no longer need in KWalletManager. After an
    SSH terminal connects, `cd` to the folder you need.
  - **Network shares and FTP:** Prefer SFTP or WebDAV over HTTPS to plain
    FTP. For FTPS, install a third-party KIO worker or use `lftp` in the
    terminal. If **Network** stays empty, install `kio-extras` with Avahi and
    `kdsoap-ws-discovery-client`, and allow mDNS and WS-Discovery through
    the firewall, or save a connection to the host directly.
  - **rclone cloud storage:** Install rclone and FUSE 3 from your
    distribution and add remotes with `rclone config`. Close files on a
    mount before unmounting it or quitting Plasma Opus. If a mount stays
    behind, run `fusermount3 -u` on its folder under
    `$XDG_RUNTIME_DIR/opus-file-manager/rclone`. For mounts that outlive
    Plasma Opus or start at login, use rclone's own systemd service.
  - **Remote drives:** Keep the default KIO FUSE for occasional access; it
    reuses passwords saved in KWallet. Choose sshfs and install `ksshaskpass`
    for SSH servers that need read-only or a fixed folder. For WebDAV or
    Windows shares with a fixed folder, ask an administrator to add the line
    **Mount Options…** shows to `/etc/fstab`. Close files before unmounting.
    If a folder is reported as stale, run `fusermount3 -u` on it. For mounts
    that outlive Plasma Opus, use a systemd mount unit or fstab entry with
    `auto`.
  - **Temporary LAN sharing:** Keep HTTPS on and, before accepting the
    certificate, check that the fingerprint the browser shows matches the
    dialog's. Share only on trusted networks, with the shortest practical
    time limit, and pass the link through the QR code or a private channel.
    If clients can't connect, choose a fixed port and allow it, for example
    with `sudo firewall-cmd --add-port=PORT/tcp` until the next restart or
    `sudo ufw allow PORT/tcp`, removing the ufw rule afterwards. For paired
    phones, KDE Connect is simpler; for lasting shares, use Samba sharing in
    the folder's **Properties** dialog.
  - **KDE Online Accounts:** Install `kio-gdrive` for Google Drive and
    `kaccounts-providers` for Nextcloud and ownCloud, then add accounts in
    **System Settings › Online Accounts**. When KIO reports an expired
    sign-in, sign in again on that page. For WebDAV storage, let KWallet
    remember the password when KIO asks. For providers without a KDE file
    service, use **Go › Cloud Storage** with rclone.
  - **rsync backend:** Install rsync on both computers. Use SSH keys with an
    agent, or install `ksshaskpass`, and connect to the host once in a
    terminal to accept its host key. Plan one direction per run, read rsync's
    changes in the confirmation's details, and compare again after each run;
    for changes in both directions, run two plans. Back up data whose
    permissions or ownership matter.
  - **Cloud sync:** Add pairs under **File › Cloud Sync…** and check
    **Synchronize in the background after login** so they run on their
    intervals. Read the first run's review and any large deletion before
    choosing **Synchronize**. Choose a backup mode when only one side should
    change, keep pairs out of folders another client synchronizes, and
    resolve conflicts in the review rather than leaving them skipped. Merge
    and delete conflict copies once you have compared them. Rename one side
    of a file-and-folder conflict, and move whole folders with the remote's
    own tools to avoid copying them again. Check the tray icon or the dialog
    for pairs that need attention.
  - **Cloud activity:** Open **File › Cloud Activity…**, or **Cloud
    Activity…** in the tray menu, to follow transfers and errors. Set a
    bandwidth limit when uploads slow other traffic and an offline cache large
    enough for the files you reopen, then unmount and mount remotes again for
    the limits to apply. Let the upload wait finish before unmounting or
    logging out; if you continue without waiting, mount the remote again later
    to finish uploading. Before importing definitions on another computer,
    configure the same rclone remotes there, then sign in to each connection
    and resume the imported pairs under **File › Cloud Sync…**.
  - **Administrator mode:** Install `kio-admin` from Discover or your
    distribution. Act as an administrator only for the change you need,
    copy a file to a backup name in the same folder before editing it, then
    press **Stop Acting as Administrator**. For commands, use `sudo` in a
    normal terminal instead. Lock the screen when leaving the computer,
    because Polkit may keep the authorization for a short time.
  - **MCP server:** Keep the default read-only access, and allow only the
    folders a client needs, such as a single project folder. Turn on write
    access only while a task needs it, start other clients with
    `--read-only`, and restart the client after changing the settings.
    Check `~/.local/state/opus-file-manager/mcp-audit.log` to see what a
    client did, restore items from the trash, and move large files between
    drives yourself.
  - Comment:
    [#728.7196797397](https://github.com/mikepadiernos/plasma-opus/issues/728#issuecomment-7196797397)
- **Proposed improvements:**
  - Claim or release the service when the default handler changes while
    Plasma Opus runs.
  - An M28 **File manager** settings row that installs or removes the
    activation file explicitly.
  - Watch `plasma-localerc` and offer a restart when Plasma's language list
    changes.
  - An M28 setting to choose which operations report to Plasma, and to
    report automatic library refreshes.
  - A job action that raises the dialog that started the operation.
  - An M28 **System tray** setting for starting hidden, with a notice when
    the global shortcut could not be assigned.
  - Reuse an open tab showing an Activity's default folder instead of
    opening another, and offer the current Activity's association at start
    as an option.
  - Keep association behaviours with the session, and an M28
    **Plasma Activities** page that manages associations in the settings.
  - Offer to compress a selected folder and send the archive, and report
    KDE Connect's transfer result in the dialog when its share plugin exposes
    one.
  - Let KRunner match saved locations assigned to a workspace, check remote
    Favorites before offering them, and stop the helper after a period
    without searches.
  - Ship a Plasma Opus provider and installer so downloads work without
    Dolphin, and watch the service-menu folders to refresh the list while
    the settings are open.
  - The M28 **System file chooser** settings row, a version check that
    warns before setup on xdg-desktop-portal older than 1.21.0, previews and
    indexed search in the chooser, and returning remote files through a
    local copy.
  - Watch mount changes instead of polling, refresh sizes periodically,
    power off USB drives after unmounting, and the M28 **Mount manager**
    display settings, including an option to show system file systems.
  - Mount disk images through UDisks2 directly, without blocking the window
    and with its actual error messages, and show images outside the home
    folder in Places once mounted.
  - Read a Flatpak bundle's ID and permissions before installing, show tool
    progress, update or remove AppImage menu entries when the file moves,
    and recognize custom Flatpak installations.
  - Verify the script on X11 and Wayland with each supported Plasma release,
    keep maximized and full-screen state, and record a placement when a
    workspace is saved as well as when the window moves.
  - Ask KWin to place other applications started from Plasma Opus, such as
    terminals and editors, through XDG activation tokens, and the M28 **KWin
    scripts** settings page.
  - Roll the drop-down window up instead of closing it, offer a default
    shortcut when it is free, and let a detached window drop down instead
    of the main window.
  - The M28 **Categories** settings to choose which categories appear, set
    the large-file threshold and restore removed entries; retranslate titles
    when the language changes; hide the entries from file dialogs; and
    confirm live results by content as Baloo does.
  - The M28 terminal **Provider** settings to choose the provider for **Open
    Terminal Here**, find Flatpak terminals, show Yakuake's window through a
    dedicated D-Bus call when it offers one, and open a selected folder
    rather than the pane's folder from its context menu.
  - The M28 editor settings to choose a default editor, open terminal
    editors in a separate terminal, find Flatpak editors and add other
    editors.
  - Find Flatpak office applications, remember the chosen application and
    offer RTF and CSV files to it.
  - An M28 setting to choose the tool for each file type, and use the file
    associations' editors where available.
  - Use Plasma's font viewer component when available, preview every face
    of a font collection, and remember the reading position in ebook
    previews.
  - Show saved connections in Places, test a connection before saving it,
    start SSH terminals in the profile's folder, confirm removals and offer
    to forget the saved password, and import hosts from `~/.ssh/config`.
  - Save a discovered share as a connection from **Network**, and hide FTPS
    when no worker handles it.
  - Serve remotes over WebDAV on the loopback interface when FUSE is
    missing, keep mounts running in the tray, show mounts in Places, and
    choose rclone's cache mode in the M28 settings.
  - Mount KDE Online Accounts and S3 locations, mount when a workspace is
    activated, keep chosen mounts after quitting, remount with changed
    options, and pass KWallet passwords to davfs2 and CIFS with consent.
  - Keep sharing from the system tray after the dialog closes, announce
    shares over DNS-SD, add an optional password and revocable links,
    keep connections alive, create folders over WebDAV, refresh addresses
    when the network changes, and the M28 **LAN sharing** defaults.
  - Show online account folders in Places, mount them as remote drives,
    refresh the menu when accounts change, hand Nextcloud app passwords to
    KIO with consent, and the M28 **Online Accounts** settings tab.
  - Share one SSH connection between rsync runs, offer to keep permissions
    and links, delete folders missing from the source, and the M28
    **rsync** settings page.
  - Detect folder renames, compare checksums where the remote offers them,
    watch local folders instead of waiting for the interval, send desktop
    notifications, show transfers and quota per pair, and the M28 **Cloud
    sync** settings page.
  - Show the speed of rclone mounts and sync runs through rclone's remote
    control, limit the bandwidth of KIO pairs, delay logout while uploads are
    pending, mark folders to keep available offline, and the M28 **Cloud
    activity** settings.
  - Mark administrator tabs in the tab bar, offer **Open as Administrator**
    for a selected folder in a new tab, and allow trash, checksums and other
    local-file actions through the `admin:` worker where KIO supports them.
  - The M28 **MCP** settings page, with folder choices and an access token;
    an optional Streamable HTTP transport on the loopback interface; reading
    other text encodings and images; moving between file systems through KIO
    jobs that can be undone; and a size limit for the audit log.

## Embedded terminal

The embedded terminal requires the Konsole or Kwi terminal component and has
these limits
([#321](https://github.com/mikepadiernos/plasma-opus/issues/321)):

- **Terminal component:** **Terminal Component** in the panel's menu applies
  to new sessions; open sessions keep the component they started with. When
  the chosen component isn't installed, the panel says so instead of loading
  the other. Kwi's component doesn't report its blur setting, so the panel
  isn't blurred behind it.
- **Atuin history search:** The search is typed into the current session's
  shell, so it needs the shell to be idle and to accept `atuin search
--interactive`; a session running another program is refused, but a
  prompt with half-typed text gets the search appended to it. The chosen
  command is printed, not placed at the prompt, unless the shell's own Atuin
  key binding is used. Plasma Opus only checks that the history database
  file exists, honouring `ATUIN_DB_PATH`, `ATUIN_DATA_DIR`,
  `ATUIN_CONFIG_DIR` and a top-level `db_path` in `config.toml`; Atuin can
  still fail to open it. The typed line can appear in shell history unless
  leading spaces are ignored. Kwi's component may not report a running
  program, so a busy Kwi session can receive the search.
  ([#364](https://github.com/mikepadiernos/plasma-opus/issues/364))

- **Working directory:** By default, new sessions start in the active local
  folder, or in the home folder with a visible notice for remote locations,
  and pane navigation does not change their directory.
- **Folder following:** Optional folder following instead switches between
  retained sessions or creates new ones; it does not change a running shell's
  directory. Remote or missing folders are reported without switching
  sessions.
- **Session lifetime:** Sessions accumulate until their tabs are closed.
  Hiding the panel keeps them running; closing the window ends them and can
  affect running programs.

- **Recommendation:** Install the Konsole terminal component for embedded
  sessions; on Arch Linux it is supplied by the `konsole` package. To use Kwi
  instead, install it and choose **Kwi** under the panel menu's **Terminal
  Component**, then open a new session. For remote
  locations, connect using a suitable terminal tool or use a separate terminal
  configured for that service. Check the terminal working directory before
  running commands, close unneeded session tabs, and finish or safely stop
  programs before closing a tab or the window. Before searching history with
  Atuin, clear the prompt and finish running programs; to edit a chosen
  command at the prompt instead, use the key binding from `atuin init`, such
  as Ctrl+R, in the shell.
  - Comment: [#321.7022392302][comment-321]
- **Alternative:** Use a terminal multiplexer for independently retained
  sessions.
  - Comment:
    [#321.7029674455](https://github.com/mikepadiernos/plasma-opus/issues/321#issuecomment-7029674455)
- **Planned:** [M27](ROADMAP.md#m27-integrations) proposes separate SSH
  sessions for SSH profiles; it does not promise embedded remote working
  directories or persistent sessions after exit.
  - Comment: [#321.7022392302][comment-321]

## Settings backups

Settings backups have these limits
([#433](https://github.com/mikepadiernos/plasma-opus/issues/433)):

- **Encryption:** Backups are unencrypted archives.
- **Credential filtering:** Filtering recognizes certain key names and value
  patterns, but cannot guarantee that all sensitive data is removed; saved
  searches and other settings may still contain private text or paths. A
  matching value can instead exclude an entire settings entry.
- **Excluded data:** Current session tabs and expanded folder locations, saved
  custom commands and shared KDE settings are excluded; restoring preserves
  the corresponding current state rather than recovering it from the backup.

- **Recommendation:** Review backup contents privately before sharing or
  restoring, protect the archive with restrictive access and encryption, and
  avoid putting secrets in settings. Keep separate protected backups of excluded
  commands and other needed data; after restoration, verify important settings
  and recreate excluded items as needed.
  - Comment:
    [#433.7040727966](https://github.com/mikepadiernos/plasma-opus/issues/433#issuecomment-7040727966)

## Build, installation and references

For build requirements and installer behaviour, see
[Build and run](../README.md#build-and-run). For the scope of the upstream
inventories, see [Behaviour references](README.md#behaviour-references).

## Installing optional alternatives

These tools can be used independently for the workarounds above. Some are also
required by particular Plasma Opus features, as noted in their caveats; none is
a dependency of every feature. Install only what you need and check each tool's
format, platform and security limitations. For example, `difftastic` provides
`difft` for text comparison and `ncdu` provides `ncdu` for browsing directory
usage.

On Arch Linux, install an official package with `sudo pacman -S <package>`;
`paru -S <package>` can also install repository packages. With Homebrew, use
`brew install <formula>`. For the listed mise registry names,
`mise use -g <name>@latest` installs the tool and adds it to your global mise
configuration; ensure mise is activated in your shell. A dash means no mise
option is listed here, not that installation through another backend is
impossible.

| Tool          | Arch package          | Homebrew formula | mise name     |
| ------------- | --------------------- | ---------------- | ------------- |
| 7-Zip         | `7zip`                | `sevenzip`       | `7zip`        |
| rclone        | `rclone`              | `rclone`         | `rclone`      |
| fd            | `fd`                  | `fd`             | `fd`          |
| bat           | `bat`                 | `bat`            | `bat`         |
| Neovim        | `neovim`              | `neovim`         | `neovim`      |
| Difftastic    | `difftastic`          | `difftastic`     | `difftastic`  |
| Pandoc        | `pandoc-cli`          | `pandoc`         | `pandoc`      |
| ImageMagick   | `imagemagick`         | `imagemagick`    | `imagemagick` |
| FFmpeg        | `ffmpeg`              | `ffmpeg`         | -             |
| ExifTool      | `perl-image-exiftool` | `exiftool`       | `exiftool`    |
| ncdu          | `ncdu`                | `ncdu`           | -             |
| GNU coreutils | `coreutils`           | `coreutils`      | `coreutils`   |
| GNU diffutils | `diffutils`           | `diffutils`      | -             |
| GNU findutils | `findutils`           | `findutils`      | -             |
| ripgrep       | `ripgrep`             | `ripgrep`        | `ripgrep`     |
| jdupes        | `jdupes`              | `jdupes`         | -             |
| QDirStat      | `qdirstat`            | -                | -             |

Homebrew's GNU coreutils commands are normally prefixed with `g`, for example
`gdu` and `gsha256sum`; Homebrew's GNU diffutils commands likewise use names
such as `gdiff` and `gcmp`, and GNU findutils uses `gfind`. The mise `coreutils`
entry installs uutils coreutils, not GNU coreutils; check compatibility before
relying on GNU-specific behaviour. KDE components such as Ark, KParts and KIO
workers should be installed through the system's KDE packages, not treated as
standalone CLI replacements. The PDF engine used by Pandoc is a separate
dependency and varies with the chosen output method.

[issue-36]: https://github.com/mikepadiernos/plasma-opus/issues/36
[issue-255]: https://github.com/mikepadiernos/plasma-opus/issues/255
[issue-256]: https://github.com/mikepadiernos/plasma-opus/issues/256
[issue-257]: https://github.com/mikepadiernos/plasma-opus/issues/257
[issue-258]: https://github.com/mikepadiernos/plasma-opus/issues/258
[issue-259]: https://github.com/mikepadiernos/plasma-opus/issues/259
[issue-260]: https://github.com/mikepadiernos/plasma-opus/issues/260
[issue-261]: https://github.com/mikepadiernos/plasma-opus/issues/261
[issue-262]: https://github.com/mikepadiernos/plasma-opus/issues/262
[issue-263]: https://github.com/mikepadiernos/plasma-opus/issues/263
[issue-264]: https://github.com/mikepadiernos/plasma-opus/issues/264
[issue-265]: https://github.com/mikepadiernos/plasma-opus/issues/265
[issue-266]: https://github.com/mikepadiernos/plasma-opus/issues/266
[issue-267]: https://github.com/mikepadiernos/plasma-opus/issues/267
[issue-302]: https://github.com/mikepadiernos/plasma-opus/issues/302
[issue-303]: https://github.com/mikepadiernos/plasma-opus/issues/303
[issue-304]: https://github.com/mikepadiernos/plasma-opus/issues/304
[issue-307]: https://github.com/mikepadiernos/plasma-opus/issues/307
[issue-309]: https://github.com/mikepadiernos/plasma-opus/issues/309
[issue-311]: https://github.com/mikepadiernos/plasma-opus/issues/311
[issue-312]: https://github.com/mikepadiernos/plasma-opus/issues/312
[issue-313]: https://github.com/mikepadiernos/plasma-opus/issues/313
[issue-732]: https://github.com/mikepadiernos/plasma-opus/issues/732
[issue-755]: https://github.com/mikepadiernos/plasma-opus/issues/755
[comment-36]: https://github.com/mikepadiernos/plasma-opus/issues/36#issuecomment-7021740511
[comment-255]: https://github.com/mikepadiernos/plasma-opus/issues/255#issuecomment-7021741358
[comment-256]: https://github.com/mikepadiernos/plasma-opus/issues/256#issuecomment-7021742119
[comment-257]: https://github.com/mikepadiernos/plasma-opus/issues/257#issuecomment-7021742864
[comment-258]: https://github.com/mikepadiernos/plasma-opus/issues/258#issuecomment-7021743599
[comment-259]: https://github.com/mikepadiernos/plasma-opus/issues/259#issuecomment-7021744368
[comment-260]: https://github.com/mikepadiernos/plasma-opus/issues/260#issuecomment-7021745029
[comment-261]: https://github.com/mikepadiernos/plasma-opus/issues/261#issuecomment-7021745774
[comment-262]: https://github.com/mikepadiernos/plasma-opus/issues/262#issuecomment-7021746538
[comment-263]: https://github.com/mikepadiernos/plasma-opus/issues/263#issuecomment-7021748197
[comment-264]: https://github.com/mikepadiernos/plasma-opus/issues/264#issuecomment-7021749088
[comment-265]: https://github.com/mikepadiernos/plasma-opus/issues/265#issuecomment-7021749894
[comment-266]: https://github.com/mikepadiernos/plasma-opus/issues/266#issuecomment-7021750778
[comment-267]: https://github.com/mikepadiernos/plasma-opus/issues/267#issuecomment-7021751490
[comment-302]: https://github.com/mikepadiernos/plasma-opus/issues/302#issuecomment-7021752278
[comment-303]: https://github.com/mikepadiernos/plasma-opus/issues/303#issuecomment-7021753023
[comment-304]: https://github.com/mikepadiernos/plasma-opus/issues/304#issuecomment-7021753785
[comment-307]: https://github.com/mikepadiernos/plasma-opus/issues/307#issuecomment-7021754467
[comment-309]: https://github.com/mikepadiernos/plasma-opus/issues/309#issuecomment-7021755164
[comment-311]: https://github.com/mikepadiernos/plasma-opus/issues/311#issuecomment-7021755780
[comment-312]: https://github.com/mikepadiernos/plasma-opus/issues/312#issuecomment-7021756400
[comment-313]: https://github.com/mikepadiernos/plasma-opus/issues/313#issuecomment-7021757012
[comment-314]: https://github.com/mikepadiernos/plasma-opus/issues/314#issuecomment-7021757620
[comment-315]: https://github.com/mikepadiernos/plasma-opus/issues/315#issuecomment-7021758275
[comment-316]: https://github.com/mikepadiernos/plasma-opus/issues/316#issuecomment-7021845379
[comment-317]: https://github.com/mikepadiernos/plasma-opus/issues/317#issuecomment-7021904155
[comment-318]: https://github.com/mikepadiernos/plasma-opus/issues/318#issuecomment-7022199214
[comment-321]: https://github.com/mikepadiernos/plasma-opus/issues/321#issuecomment-7022392302
[comment-322]: https://github.com/mikepadiernos/plasma-opus/issues/322#issuecomment-7022490696
[comment-335]: https://github.com/mikepadiernos/plasma-opus/issues/335#issuecomment-7022647720
[comment-336]: https://github.com/mikepadiernos/plasma-opus/issues/336#issuecomment-7022793474
[comment-362]: https://github.com/mikepadiernos/plasma-opus/issues/362#issuecomment-7023269364
[comment-372]: https://github.com/mikepadiernos/plasma-opus/issues/372#issuecomment-7024683530
[comment-373]: https://github.com/mikepadiernos/plasma-opus/issues/373#issuecomment-7024944292
[comment-374]: https://github.com/mikepadiernos/plasma-opus/issues/374#issuecomment-7025134952
[comment-375]: https://github.com/mikepadiernos/plasma-opus/issues/375#issuecomment-7025250130
[comment-376]: https://github.com/mikepadiernos/plasma-opus/issues/376#issuecomment-7026216329
[comment-377]: https://github.com/mikepadiernos/plasma-opus/issues/377#issuecomment-7026633904
[comment-732]: https://github.com/mikepadiernos/plasma-opus/issues/732#issuecomment-7200937321
[comment-755]: https://github.com/mikepadiernos/plasma-opus/issues/755#issuecomment-7207460156
