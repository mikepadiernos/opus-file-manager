# ⏳ Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

These handoffs describe unfinished work and the branch or issue to resume.

> [!IMPORTANT]
> The project board is authoritative. Confirm the current issue and project
> status before acting, and never move an item backward. A handoff is not a
> completion marker.

## Contents

| Handoff                                                                       | Milestone | Board status |
| ----------------------------------------------------------------------------- | --------- | ------------ |
| 🎬 [mpv integration (#563)](#mpv-integration-563)                             | M27       | Backlog      |
| 📝 [Markdown editor and backlinks (#564)](#markdown-editor-and-backlinks-564) | M27       | Backlog      |
| 🗃️ [Workspace vault editing (#565)](#workspace-vault-editing-565)             | M27, M28  | Defined      |
| 🔍 [M22 board updates (#31)](#m22-board-updates-31)                           | M22       | Development  |

## mpv integration (#563)

[#563](https://github.com/mikepadiernos/plasma-opus/issues/563) is in Backlog.
Earlier work exists only on the unmerged `feature/M25-integrations` branch,
which predates the current milestone numbering: commits tagged GH563 add an
external **Open with mpv** action for selected media, with explicit errors for
remote locations, a missing executable or launch failure. Rebase or port that
work onto an M27 branch before resuming; it is not in `main`.

Embedded playback is not implemented. KDE's MpvQt uses Qt Quick's OpenGL
renderer, changes the process numeric locale and can terminate the application
when mpv initialization fails. Before selecting an implementation, verify a safe
libmpv rendering and lifecycle approach for Wayland and X11, including failure
handling and builds without the optional library. Implement backend selection,
audio/video controls, preview and gallery behavior, and cleanup; test autoplay,
seeking, errors, remote temporary-file lifetime, and repeated open/close before
moving the issue to Review.

## Markdown editor and backlinks (#564)

[#564](https://github.com/mikepadiernos/plasma-opus/issues/564) is in Backlog.
The same unmerged `feature/M25-integrations` branch has commits tagged GH564
that add a Qt CommonMark preview dialect alongside the existing GitHub-flavored
dialect; they do not change visual editing or export and are not in `main`.

Backlinks, vault selection and wiki-link navigation are not implemented. Qt's
rich-text renderer supports a subset of Markdown, so verify CommonMark
conformance before claiming full support. Choose and test a source-preserving
parser and renderer, and implement local-vault selection, safe link resolution,
and responsive backlink indexing with visible errors. Test duplicate names,
fragments, aliases, code and escaped markup, inaccessible files, symlinks,
updates, unsaved edits and vault boundaries; preserve existing draft and
external-resource protections. Do not mark the issue complete until the editing
and backlink workflows are implemented and verified.

## Workspace vault editing (#565)

[#565](https://github.com/mikepadiernos/plasma-opus/issues/565) is Defined and
belongs to M27; its settings,
[#566](https://github.com/mikepadiernos/plasma-opus/issues/566), are in Backlog
for M28. Workspace profiles and location assignments
([#200](https://github.com/mikepadiernos/plasma-opus/issues/200) and
[#201](https://github.com/mikepadiernos/plasma-opus/issues/201)) are merged. The
built-in Markdown and backlinks feature
[#564](https://github.com/mikepadiernos/plasma-opus/issues/564) is in Backlog;
its vault and backlink workflows remain unimplemented.

Implement the editor's vault features first. Then associate an assigned local
location with a workspace, integrate vault navigation and editor state, and
provide workspace settings in M28. Test switching with unsaved edits, incomplete
scans, duplicate names, link resolution, unavailable or removed roots, symlink
boundaries and workspace deletion. Preserve the underlying files and existing
Obsidian configuration. For #566, also verify workspace-scoped settings
persistence and recovery when vault roots or editor features are unavailable.

## M22 board updates (#31)

GitHub Projects field updates for project 3 failed on 2026-11-25 with
server errors (HTTP 502 and 504, and GraphQL "Something went wrong") after
repeated retries with backoff. Reads, issue edits and comments still worked. The
M22 implementation is complete and pushed on `feature/M22-advanced-search-settings`;
the full test suite passed in the dev container.

Confirmed state: #31 in Development, start 2026-11-25. Defaults
([#248](https://github.com/mikepadiernos/plasma-opus/issues/248)) is Completed
and checked in the roadmap. Panel
([#245](https://github.com/mikepadiernos/plasma-opus/issues/245)) is in Review
with target date 2026-11-25. Results
([#247](https://github.com/mikepadiernos/plasma-opus/issues/247)) is in Review,
checked in #31, without a target date. Toolbar
([#246](https://github.com/mikepadiernos/plasma-opus/issues/246)) is Defined;
it is implemented in commits `c0ce2d2` to `ec63a27`.

When Projects updates work again, verify each status first, then:

1. Set the #247 target date to 2026-11-25.
2. Move #245 to Completed, close it as completed and check **Panel:** in the
   M22 roadmap section.
3. Move #246 to Development with start date 2026-11-25, then to Review with
   target date 2026-11-25, and check **Toolbar:** in #31.
4. Move #247 to Completed, close it and check **Results:** in the roadmap. Then
   do the same for #246 and **Toolbar:**.
5. Set the #31 target date to 2026-11-25, mark M22 complete in the roadmap
   summary, open and merge the M22 pull request, and move #31, #245, #246, #247
   and #248 to Merged, closed as completed. Leave
   [#362](https://github.com/mikepadiernos/plasma-opus/issues/362) open.
6. Remove this handoff and its roadmap link.
