# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Renumbered milestone branches

The workspace, workspace-settings and integrations milestones are now M15,
M16 and M25. Their existing branches retain their original names:
`feature/M21-workspaces`, `feature/M22-workspace-settings` and
`feature/M23-integrations`. Workspace profiles and vault settings remain in
Backlog ([#200](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/200),
[#565](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/565),
[#566](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/566));
mpv and Markdown work remains in Development
([#563](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/563),
[#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)).

Before resuming each branch, reconcile it with `main`, retaining the current
milestone numbers and roadmap plans without losing branch changes. Complete
and verify each feature and follow the project workflow before merging its
milestone. Do not mark the partial integration work complete.

## mpv integration (#563)

[#563](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/563)
is in Development on `feature/M23-integrations`. That branch implements an
external **Open with mpv** action for selected media, with explicit errors for
remote locations, a missing executable or launch failure; the action is not
yet in `main`.

Embedded playback is not implemented. KDE's MpvQt uses Qt Quick's OpenGL
renderer, changes the process numeric locale and can terminate the application
when mpv initialization fails. Before selecting an implementation, verify a
safe libmpv rendering and lifecycle approach for Wayland and X11, including
failure handling and builds without the optional library. Implement backend
selection, audio/video controls, preview and gallery behavior, and cleanup;
test autoplay, seeking, errors, remote temporary-file lifetime, and repeated
open/close before moving the issue to Review.

## Markdown editor and backlinks (#564)

[#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)
is in Development on `feature/M23-integrations`. That branch adds a Qt
CommonMark preview dialect alongside the existing GitHub-flavored dialect;
it does not change visual editing or export and is not yet in `main`.

Backlinks, vault selection and wiki-link navigation are not implemented. Qt's
rich-text renderer supports a subset of Markdown, so verify CommonMark
conformance before claiming full support. Choose and test a source-preserving
parser and renderer, and implement local-vault selection, safe link resolution,
and responsive backlink indexing with visible errors. Test duplicate names,
fragments, aliases, code and escaped markup, inaccessible files, symlinks,
updates, unsaved edits and vault boundaries; preserve existing draft and
external-resource protections. Do not mark the issue complete until the
editing and backlink workflows are implemented and verified.
## Workspace vault editing (#565)

[#565](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/565)
is in Backlog. Named workspace profiles are not implemented in the current
checkout, and the built-in Markdown and backlinks feature
[#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)
is still in Development on `feature/M23-integrations`.

Implement workspace profiles ([#200](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/200))
and the editor's vault features first. Then associate an assigned local
location with a workspace, integrate vault navigation and editor state, and
provide workspace settings ([#566](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/566)).
Test switching with unsaved edits, incomplete scans, duplicate names, link
resolution, unavailable or removed roots, symlink boundaries and workspace
deletion. Preserve the underlying files and existing Obsidian configuration.
For [#566](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/566),
also verify workspace-scoped settings persistence and recovery when vault
roots or editor features are unavailable.
