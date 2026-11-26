# Pending tasks

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap](https://img.shields.io/badge/roadmap-M01%E2%80%93M32-8957e5?style=flat-square)](ROADMAP.md)
[![Caveats](https://img.shields.io/badge/caveats-limitations-bf8700?style=flat-square)](CAVEATS.md)
[![Project board](https://img.shields.io/badge/project-board-6e7781?style=flat-square&logo=github)](https://github.com/users/mikepadiernos/projects/3)

These handoffs describe blocked or unfinished work and where to resume it. Each
entry records the blocker, the affected milestone and issues, the confirmed
state and the steps to resume.

> [!IMPORTANT]
> The project board is authoritative. Confirm the current issue and project
> status before acting, and never move an item backward. A handoff is not a
> completion marker.

## Contents

Board statuses were last confirmed on 2026-11-25.

| Handoff                                                             | Milestone                                                                      | Issues         | Board status                                                                                                                                        |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------ | -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| [mpv integration](#mpv-integration-563)                             | [M27](ROADMAP.md#m27-integrations)                                             | [#563]         | ![Backlog](https://img.shields.io/badge/Backlog-6e7781?style=flat-square)                                                                           |
| [Markdown editor and backlinks](#markdown-editor-and-backlinks-564) | [M27](ROADMAP.md#m27-integrations)                                             | [#564]         | ![Backlog](https://img.shields.io/badge/Backlog-6e7781?style=flat-square)                                                                           |
| [Workspace vault editing](#workspace-vault-editing-565)             | [M27](ROADMAP.md#m27-integrations), [M28](ROADMAP.md#m28-integration-settings) | [#565], [#566] | ![Defined](https://img.shields.io/badge/Defined-6e7781?style=flat-square) ![Backlog](https://img.shields.io/badge/Backlog-6e7781?style=flat-square) |

## mpv integration (#563)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#563]
  (Backlog).
- **Blocker:** Embedded playback has no verified rendering approach. KDE's
  MpvQt uses Qt Quick's OpenGL renderer, changes the process numeric locale and
  can terminate the application when mpv initialization fails.
- **Confirmed state:** `main` has the partial work, merged from
  `feature/M27-integrations`: an external **Open with mpv** context-menu action
  for local audio and video selections, with explicit errors for remote
  locations, a missing executable or launch failure. Embedded playback is not
  implemented.

**Steps to resume:**

1. Continue on `feature/M27-integrations` when M27 starts.
2. Before selecting an implementation, verify a safe libmpv rendering and
   lifecycle approach for Wayland and X11, including failure handling and
   builds without the optional library. If it uses MpvQt, follow the
   [Qt Quick embedding rules](qt-quick.md#embedding-rules): host it in a
   `QQuickWidget`, keep Qt Multimedia as the fallback, and contain the numeric
   locale change.
3. Implement backend selection, audio and video controls, preview and gallery
   behaviour, and cleanup.
4. Test autoplay, seeking, errors, remote temporary-file lifetime, and repeated
   open and close before moving the issue to Review.

## Markdown editor and backlinks (#564)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#564]
  (Backlog).
- **Blocker:** No source-preserving Markdown parser and renderer has been
  chosen. Qt's rich-text renderer supports only a subset of Markdown, so
  CommonMark conformance is unverified.
- **Confirmed state:** `main` has the partial work, merged from
  `feature/M27-integrations`: a **CommonMark** preview dialect alongside the
  existing **GitHub Markdown** dialect, selectable while the editor's Markdown
  preview is shown. It does not change visual editing or PDF export. Backlinks,
  vault selection and wiki-link navigation are not implemented.

**Steps to resume:**

1. Verify CommonMark conformance before claiming full support.
2. Choose and test a source-preserving parser and renderer.
3. Implement local-vault selection, safe link resolution, and responsive
   backlink indexing with visible errors.
4. Test duplicate names, fragments, aliases, code and escaped markup,
   inaccessible files, symlinks, updates, unsaved edits and vault boundaries;
   preserve existing draft and external-resource protections.
5. Do not mark the issue complete until the editing and backlink workflows are
   implemented and verified.

## Workspace vault editing (#565)

- **Milestone and issues:** [M27](ROADMAP.md#m27-integrations), [#565]
  (Defined); its settings belong to
  [M28](ROADMAP.md#m28-integration-settings), [#566] (Backlog).
- **Blocker:** Depends on the built-in Markdown and backlinks feature, [#564]
  (Backlog), whose vault and backlink workflows remain unimplemented.
- **Confirmed state:** Workspace profiles and location assignments ([#200] and
  [#201]) are merged. Vault editing is not implemented.

**Steps to resume:**

1. Implement the editor's vault features in [#564] first.
2. Associate an assigned local location with a workspace, and integrate vault
   navigation and editor state.
3. Provide the workspace settings in M28 ([#566]).
4. Test switching with unsaved edits, incomplete scans, duplicate names, link
   resolution, unavailable or removed roots, symlink boundaries and workspace
   deletion. Preserve the underlying files and existing Obsidian configuration.
5. For [#566], also verify workspace-scoped settings persistence and recovery
   when vault roots or editor features are unavailable.

[#200]: https://github.com/mikepadiernos/plasma-opus/issues/200
[#201]: https://github.com/mikepadiernos/plasma-opus/issues/201
[#563]: https://github.com/mikepadiernos/plasma-opus/issues/563
[#564]: https://github.com/mikepadiernos/plasma-opus/issues/564
[#565]: https://github.com/mikepadiernos/plasma-opus/issues/565
[#566]: https://github.com/mikepadiernos/plasma-opus/issues/566
