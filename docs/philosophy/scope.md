# Scope and objective

## Objective

Opus aims to bring power-user file management to Linux in an application that
feels native to KDE Plasma. Its central idea is the dual-pane workspace: keep
two locations available at once, navigate each side through tabs and history,
and make common operations between them direct and predictable.

The project combines this workflow with KDE desktop integration rather than
recreating a desktop environment inside the file manager. Max File Manager
provides the primary behaviour and feature-parity reference; ideas from other
file managers broaden the workflows described in
[`inspirations.md`](inspirations.md).

## Audience and workflows

Opus is for people who manage files frequently and benefit from seeing source
and destination locations together, as well as users who want a full-featured
file manager that fits their KDE desktop. The intended workflows include:

- Browsing local and remote locations through panes, tabs and navigation
  history.
- Selecting, previewing, organizing and transferring files with familiar
  desktop operations.
- Using views and previews suited to the current task, from detailed listings
  to image browsing.
- Extending basic file management with search, comparison, automation and
  other power-user tools as those roadmap milestones are delivered.

## Product scope

- **Platform:** Linux, with KDE Plasma as the target desktop.
- **Technology:** C++20, Qt 6 Widgets and KDE Frameworks 6.
- **Core:** dual-pane navigation, tabs, file listings and operations, preview
  and gallery workflows, settings, and session restoration.
- **Desktop integration:** use KDE services for file operations, Places,
  properties, settings, icons, service actions and application integration
  wherever they fit.
- **Compatibility:** preserve upstream `.mfm.json` data for concepts without a
  KDE equivalent, and record reference behaviours in `docs/parity/`.
- **Delivery:** track implemented, in-progress and planned work in tested,
  incremental milestones in [`../ROADMAP.md`](../ROADMAP.md).

The roadmap is the authoritative feature inventory. This scope document
describes the product direction; a feature listed in the roadmap is not
necessarily implemented yet.

## Design boundaries

- Opus is Linux-only; a cross-platform port is not a goal.
- Opus complements the KDE desktop and does not replace, remove or mask
  Dolphin.
- Prefer existing KDE facilities over custom equivalents for common desktop
  functions. Implement Opus-specific logic where the workflow has no suitable
  KDE counterpart.
- User data and behaviour should remain explicit and testable. Core logic
  belongs outside widgets when practical, and tests must not modify a real
  user's files, home directory or configuration.

The architecture, KDE-specific implementation choices and milestone status are
described in [`../ROADMAP.md`](../ROADMAP.md).