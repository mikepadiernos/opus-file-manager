# Edge cases

**[Documentation](README.md)** · **[Testing plan](refinements/06-testing.md)** ·
**[Reported bugs](BUGS.md)** · **[Caveats](CAVEATS.md)**

This inventory records possible test scenarios from a targeted review of
implemented workflows and representative tests on 2026-11-20. It is not an
exhaustive audit or a claim that existing tests are missing. Each entry links
to an `[edge]` issue; consult the
[project board](https://github.com/users/mikepadiernos/projects/3) for status.

> [!NOTE]
> These scenarios are not confirmed bugs. Existing restrictions and
> workarounds are in [caveats](CAVEATS.md); confirmed reports belong in
> [reported bugs](BUGS.md).

> [!WARNING]
> Use temporary fixtures, synthetic credentials and isolated configuration,
> home and trash locations. Never test destructive operations against real user
> data, a live Baloo index or production remote services. Check existing tests
> before adding coverage.

## Find an edge case

- [Navigation and sessions](#navigation-and-sessions)
- [File operations and transfers](#file-operations-and-transfers)
- [Rename, duplicates and synchronization](#rename-duplicates-and-synchronization)
- [Archives, checksums and split files](#archives-checksums-and-split-files)
- [Search and filters](#search-and-filters)
- [Preview, editing and metadata](#preview-editing-and-metadata)
- [Settings, backups and commands](#settings-backups-and-commands)
- [Interface and environment](#interface-and-environment)

## Navigation and sessions

- **Invalid saved tab before the active tab:** [#494][issue-494]. Restore tabs
  with an invalid URL before a valid active tab. `sanitizePane` removes invalid
  URLs but clamps the original active index against the shortened list, so it
  can select a different surviving tab. Verify the intended selection along with
  pinned, locked and named tabs.
- **Unavailable locations:** [#495][issue-495]. Restore local folders that were
  deleted or unmounted and remote tabs that are offline or require
  authentication. Verify that errors are visible and other tabs and saved
  layouts survive.
- **Linked folder boundaries:** [#496][issue-496]. Navigate through symlinks to
  locations inside and outside the linked roots, or remove a corresponding
  folder after linking. Verify that navigation cannot escape the roots and that
  a missing target does not unexpectedly move the other pane.
- **Changing tabs during loading:** [#497][issue-497]. Switch, close or move
  tabs while a folder listing or history selection is still loading. Verify that
  late results do not change the wrong tab, pane, selection or scroll position.

References: [session](../src/core/session.cpp),
[linked navigation](../src/core/linkednavigation.cpp),
[pane](../src/app/pane.cpp), and
[session tests](../tests/sessiontest.cpp).

## File operations and transfers

- **Conflicts and aliases:** [#498][issue-498]. Copy or move items onto existing
  names, including dangling links, case differences, hard links and a source
  that resolves to the destination. Check each conflict policy and ensure the
  operation reports what actually happened.
- **Partial queued work:** [#499][issue-499]. Cancel a waiting, paused or
  running transfer, or disconnect a test destination mid-copy. Verify the final
  queue state, partial output and subsequent jobs; do not assume cancellation
  rolls back KIO work.
- **Concurrent destination changes:** [#500][issue-500]. Create or replace a
  destination after a transfer is queued but before it starts. Check that the
  selected conflict policy and any confirmation still apply to the actual
  destination.
- **Queue lifecycle:** [#501][issue-501]. Change concurrency while jobs are
  paused or finishing, close the queue dialog, and clear finished jobs. Verify
  that running jobs remain controlled and failures are not displayed as
  successes.

References: [file operations](../src/app/fileoperations.cpp),
[transfer queue](../src/core/transferqueue.cpp),
[queue tests](../tests/transferqueuetest.cpp), and
[transfer caveats](CAVEATS.md#transfer-queue).

## Rename, duplicates and synchronization

- **Rename preview becomes stale:** [#502][issue-502]. Replace a source or
  create a destination after preview, including a dangling symlink or a name
  differing only in case. Verify that existing files are not overwritten and
  completed earlier renames are reported if the operation stops.
- **Name boundaries:** [#503][issue-503]. Preview empty, very long,
  Unicode-normalization, leading-dot and newline-containing names, as well as a
  folder and its child in the same selection. Check validation and the actual
  filesystem's naming rules rather than assuming all filesystems behave alike.
- **Duplicate selection changes:** [#504][issue-504]. Modify, replace or remove
  a candidate after scanning but before removal; include hard links and equal
  timestamps. Verify that revalidation and selection do not remove the last
  intended copy.
- **Stale sync plan:** [#505][issue-505]. Change a source, target or parent
  directory after comparison, including replacing a directory with a symlink.
  Verify that stale actions are rejected and completed actions and partial
  results are accurately reported.
- **Overlapping or aliased roots:** [#506][issue-506]. Compare nested roots,
  symlinked roots and files sharing an inode. Confirm that unsupported
  combinations are refused and that neither root is unintentionally overwritten.

References: [batch rename](../src/core/batchrename.cpp),
[duplicate finder](../src/core/duplicatefinder.cpp),
[folder sync](../src/core/foldersync.cpp),
[rename tests](../tests/batchrenametest.cpp) and
[sync tests](../tests/foldersynctest.cpp).

## Archives, checksums and split files

- **Unsafe archive entries:** [#507][issue-507]. In disposable archives,
  exercise traversal and absolute names, duplicate entries, links, existing
  destination entries and malformed or truncated data. Verify that extraction
  stays within its destination, reports failures and does not silently overwrite
  files.
- **Protected archive failures:** [#508][issue-508]. Try wrong passwords,
  unsupported entry types, missing optional tools and cancellation or write
  failure. Check error messages and whether partial output remains; encrypted
  ZIP entry names are not secret.
- **Checksum manifest boundaries:** [#509][issue-509]. Check escaped newlines
  and backslashes in filenames, invalid UTF-8, malformed hashes, oversized
  lines, excessive entry counts and paths outside the manifest directory. Verify
  that rejected entries cannot cause an unrelated file to be read or
  overwritten.
- **Split and combine interruptions:** [#510][issue-510]. Test empty files,
  exact part boundaries, missing or reordered parts, an existing output, a full
  destination and cancellation during writing. Verify output ordering, error
  reporting and identification of incomplete files.

References: [archives](../src/core/archives.cpp),
[checksum handling](../src/core/checksums.cpp),
[split and combine](../src/core/filesplitter.cpp),
[archive tests](../tests/archivewritertest.cpp) and
[split tests](../tests/filesplittertest.cpp).

## Search and filters

- **Search limits and encodings:** [#511][issue-511]. Exercise results at and
  beyond the 5,000 result and 200,000 scanned-entry limits, and content at and
  beyond the 2 MiB limit. Include UTF-8, invalid UTF-8 and binary files; verify
  that limits, skipped files and incomplete results are visible.
- **Filters and traversal:** [#512][issue-512]. Combine maximum depth, excluded
  subtrees, hidden entries, unreadable directories and symlink loops. Verify
  that exclusions are applied consistently and errors are distinguished from no
  matches.
- **Index availability and staleness:** [#513][issue-513]. Use an isolated Baloo
  fixture with disabled, incomplete or stale indexing and missing or unreadable
  index data. Verify notices and cancellation, and do not assume indexed results
  are complete or silently substitute a different search scope.
- **Changing search state:** [#514][issue-514]. Cancel a search while results
  arrive, start a new search or close the dialog; delete or move a saved
  collection's referenced files. Verify stale results are not attributed to the
  new search and collection operations do not modify referenced files.

References: [live search](../src/core/livesearch.cpp),
[indexed search](../src/core/indexedsearch.cpp),
[search dialog](../src/app/searchdialog.cpp),
[live-search tests](../tests/livesearchtest.cpp) and
[index tests](../tests/indexedsearchtest.cpp).

## Preview, editing and metadata

- **Preview boundaries:** [#515][issue-515]. Test text truncated inside a
  multibyte character, invalid encoding, images at byte and pixel limits,
  corrupt files and remote URLs. Verify that truncation, unsupported content and
  errors remain distinct.
- **Changing or interrupted edits:** [#516][issue-516]. Change a file externally
  while it is open, interrupt saving, or switch documents with unsaved visual
  Markdown edits. Check conflict handling, draft recovery and preservation of
  both the source file and unsaved content.
- **Metadata limits and concurrent edits:** [#517][issue-517]. Exercise
  malformed or oversized `.mfm.json`, unknown fields and simultaneous changes
  from another process. Verify errors and review the existing warning that
  unsupported metadata may be discarded before testing writes.
- **Media and optional tools:** [#518][issue-518]. Test stale metadata previews,
  linked files, unsupported formats and missing, failing or incompatible
  ExifTool and image codecs. Verify failures and any possible partial
  modifications are reported.

References: [preview](../src/core/previewcontent.cpp),
[editor](../src/app/editorpanel.cpp),
[folder metadata](../src/core/foldermetadata.cpp),
[media metadata](../src/core/metadataeditor.cpp),
[preview tests](../tests/previewcontenttest.cpp) and
[metadata tests](../tests/metadataeditortest.cpp).

## Settings, backups and commands

- **Interrupted restore:** [#519][issue-519]. Use an isolated configuration to
  simulate a corrupt backup, an unwritable destination or a failure between
  restoring settings and layout. Verify that errors, rollback and
  pending-restore state are clear.
- **Sensitive data in backups:** [#520][issue-520]. Use synthetic secrets in
  varied key names, values and URLs. Check the actual backup and restored
  configuration, while recognizing that heuristic filtering is not a guarantee
  of secret removal.
- **Command argument boundaries:** [#521][issue-521]. Use disposable helper
  programs and paths containing spaces, quotes, Unicode, leading dashes and line
  breaks. Verify placeholder expansion preserves argument boundaries and remote
  selections or missing required selections are rejected where unsupported.
- **Configuration changed elsewhere:** [#522][issue-522]. Open settings in
  multiple windows and change shared KDE or immediately applied data while
  another dialog has pending edits. Verify that Cancel and save failures do not
  imply changes were reverted when they were not.

References: [settings backup](../src/core/settingsbackup.cpp),
[custom commands](../src/core/customcommands.cpp),
[backup tests](../tests/settingsbackuptest.cpp),
[command tests](../tests/customcommandstest.cpp) and
[backup caveats](CAVEATS.md#settings-backups).

## Interface and environment

- **Responsive and accessible controls:** [#523][issue-523]. Exercise narrow
  windows, large text, high DPI, right-to-left layouts, long translations and
  keyboard-only navigation. Verify focus, labels, both panes and essential
  actions remain usable without discarding customized layouts.
- **Terminal availability and lifecycle:** [#524][issue-524]. Test a missing
  Konsole component, unavailable local folders and remote locations; hide,
  reopen and move the terminal while sessions exist. Check the reported state
  and that hiding the panel does not unexpectedly end sessions.
- **Theme and motion:** [#525][issue-525]. Switch themes and icon sizes,
  including themes without symbolic variants. When planned animations are
  implemented, verify desktop motion preferences and the app's disabled-motion
  behavior without relying on animation to communicate state.
- **Locale boundaries:** [#526][issue-526]. Exercise calendar and digit
  overrides around date boundaries, missing translations and mixed KDE/Plasma Opus
  formatting. Check the documented distinction between Plasma Opus and KDE-controlled
  displays.

References: [terminal panel](../src/app/terminalpanel.cpp),
[terminal tests](../tests/terminalpaneltest.cpp),
[date tests](../tests/localizeddatestest.cpp) and
[translation caveats](CAVEATS.md#translation-format-overrides).

[issue-494]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/494
[issue-495]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/495
[issue-496]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/496
[issue-497]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/497
[issue-498]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/498
[issue-499]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/499
[issue-500]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/500
[issue-501]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/501
[issue-502]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/502
[issue-503]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/503
[issue-504]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/504
[issue-505]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/505
[issue-506]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/506
[issue-507]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/507
[issue-508]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/508
[issue-509]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/509
[issue-510]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/510
[issue-511]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/511
[issue-512]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/512
[issue-513]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/513
[issue-514]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/514
[issue-515]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/515
[issue-516]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/516
[issue-517]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/517
[issue-518]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/518
[issue-519]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/519
[issue-520]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/520
[issue-521]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/521
[issue-522]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/522
[issue-523]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/523
[issue-524]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/524
[issue-525]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/525
[issue-526]: https://github.com/mikepadiernos/plasma-opus-file-manager/issues/526
