# 🎯 Scope and objective

**[Documentation](../README.md)** · **[Inspirations](inspirations.md)** ·
**[Roadmap](../ROADMAP.md)**

## Contents

- [Objective](#objective)
- [Audience and workflows](#audience-and-workflows)
- [Product scope](#product-scope)
- [Design boundaries](#design-boundaries)

## Objective

Plasma Opus brings power-user file management to Linux in an application
designed for KDE Plasma. Its central idea is the dual-pane workspace: keep two
locations available, navigate each through tabs and history, and make operations
between them direct and predictable.

The project combines this workflow with KDE desktop integration rather than
recreating a desktop environment inside the file manager. Max File Manager
provides the primary behaviour and feature-parity reference; ideas from other
file managers broaden the workflows described in
[`inspirations.md`](inspirations.md).

## Audience and workflows

Plasma Opus is for people who manage files frequently and benefit from seeing
source and destination locations together. It combines browsing, preview and
transfer workflows with power-user tools. See the
[feature overview](../../README.md#features) for current capabilities.

## Product scope

- **Platform:** Linux, with KDE Plasma as the target desktop.
- **Desktop integration:** use KDE services for file operations, Places,
  properties, settings, icons and application integration wherever they fit.
- **Compatibility:** support a subset of upstream `.mfm.json` metadata. See
  [metadata limitations](../CAVEATS.md#folder-metadata-and-listings) and the
  [behaviour references](../README.md#behaviour-references).
- **Delivery:** track tested, incremental milestones in the
  [roadmap](../ROADMAP.md). Planned features are not necessarily implemented.

## Design boundaries

- Plasma Opus is Linux-only; a cross-platform port is not a goal.
- Plasma Opus complements the KDE desktop and does not replace, remove or mask
  Dolphin.
- Prefer existing KDE facilities for common desktop functions. Implement Plasma
  Opus-specific logic where the workflow has no suitable KDE counterpart.
- User data and behaviour should remain explicit and testable. Core logic
  belongs outside widgets when practical, and tests must not modify a real
  user's files, home directory or configuration.
