# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Workspace catalog and selector (#200)

[#200](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/200)
is in Development on `feature/M15-workspaces`. The core catalog stores stable
workspace identities, full names, short labels, active identity and basic
window and splitter layout snapshots. It can duplicate a saved layout, but is
not yet connected to the application.

Add a discoverable, keyboard-accessible selector and management actions with
explicit error reporting and persistence. Integrate layout capture and
restoration, extend snapshots to cover the full workspace layout, and ensure
duplication does not copy location or tag-group assignments. Verify switching,
renaming and deletion without losing files or unsaved state. Do not move the
issue to Review until the complete feature is implemented and tested.

The current snapshot does not record whether the second pane is in a tab or
separate window, the combined/separate panel mode, or preview and editor
placement and visibility. `QMainWindow::restoreState()` alone is unsafe:
dock visibility changes can update actions and trigger layout changes, while
these placements are managed separately. Capture the missing state and apply
it through the existing placement transitions, synchronizing actions and
reporting unavailable components. Preserve the current pane and tab
locations, editor drafts and active jobs; test rollback on failed restoration.
Reload and coordinate catalog writes across windows and processes so an
outdated catalog cannot overwrite another window's changes.

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
is in Backlog. Named workspace profiles are not implemented in the current
checkout. Continue workspace work on `feature/M15-workspaces` and settings
work on `feature/M16-workspace-settings`. The built-in Markdown and backlinks
feature [#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)
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
