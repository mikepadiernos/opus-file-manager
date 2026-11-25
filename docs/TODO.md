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
