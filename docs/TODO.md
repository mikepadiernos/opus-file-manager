# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Workspace feature completion (#200)

[#200](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/200)
is in Review; the next M15 feature,
[#201](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/201),
is in Development. Once #201 enters Review, move #200 to Completed, close it as
completed, and strike its roadmap item in the same change.

## Workspace location assignments (#201)

[#201](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/201)
is in Development on `feature/M15-workspaces`. The catalog stores exclusive
location UUID associations with conflict-safe writes; duplication does not
copy them and deletion removes only associations. These are not yet connected
to saved locations or exposed in the interface.

Give saved locations stable, credential-free identifiers and connect
assignment, reassignment and unassignment to the workspace interface.
Integrate active and unassigned location presentation with Favorites and KDE
Places without hiding system places or devices, and handle external changes
and multiple windows. Tag-group assignments await an available group provider.
Verify switching, persistence, inaccessible locations, deletion, duplication
and preservation of files, tabs, drafts and active jobs before Review.

## Workspace layout coverage (#203)

[#203](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/203)
is Defined. On `feature/M15-workspaces`, #200 implements the workspace
selector, management actions and supported layout snapshots. Full layout
coverage remains planned and is documented in the
[workspace layout caveat](CAVEATS.md#workspace-layouts).

Extend snapshots to include view modes, toolbar contents, terminal placement
and detached-window geometry. Preserve pane and tab locations, editor drafts,
terminal sessions and active jobs, and report unavailable components. Verify
rollback and the interactions between application-owned placement fields,
dock visibility and their actions.

## mpv integration (#563)

[#563](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/563)
is in Development on `feature/M25-integrations`. That branch implements an
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
is in Development on `feature/M25-integrations`. That branch adds a Qt
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
is Defined. Workspace profiles are implemented on `feature/M15-workspaces`;
location assignment integration remains in progress. Continue workspace
settings work on `feature/M16-workspace-settings`. The built-in Markdown and
backlinks feature [#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)
is still in Development on `feature/M25-integrations`.

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
