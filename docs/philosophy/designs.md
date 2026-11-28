# Interface design

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Previous: Inspirations](https://img.shields.io/badge/previous-Inspirations-3daee9?style=flat-square)](inspirations.md)
[![Refinements: layouts](https://img.shields.io/badge/refinements-layouts-6e7781?style=flat-square)](../refinements/02-layouts.md)
[![Roadmap: M32 refinements](https://img.shields.io/badge/roadmap-M32%20refinements-8957e5?style=flat-square)](../ROADMAP.md#m32-refinements)

The target structure of the Plasma Opus window, its panes, toolbars and
workspaces, and how it relates to what exists today.

**Philosophy series:** [1. Scope](scope.md) →
[2. Inspirations](inspirations.md) → **3. Interface design**

> [!NOTE]
> This document mostly describes the **target** design. Sections mark what is
> **current** (merged into `main`) and what is **target** (planned, mainly in
> [M31](../ROADMAP.md#m31-keyboard-shortcut-settings) and
> [M32](../ROADMAP.md#m32-refinements)). See the linked milestones for status.

## Contents

- [Interface structure](#interface-structure)
- [Panes](#panes)
- [Toolbars and customization](#toolbars-and-customization)
- [Workspaces](#workspaces)
- [Translation ideas](#translation-ideas)
- [Miscellaneous ideas](#miscellaneous-ideas)

## Interface structure

**Target.** The interface has five regions: a header, three columns and a
footer. The middle column holds the file-management workspace; the side columns
provide space for supporting panels.

```text
┌────────────────────────────────────────────────────────────┐
│                           Header                           │
├───────────────┬───────────────────────────┬────────────────┤
│               │                           │                │
│  Left column  │       Middle column       │  Right column  │
│               │         Workspace         │                │
│               │                           │                │
├───────────────┴───────────────────────────┴────────────────┤
│                           Footer                           │
└────────────────────────────────────────────────────────────┘
```

This is a structural illustration, not a fixed arrangement of panels. Implement
the structure as part of the [layout refinements](../refinements/02-layouts.md)
([M32.02](../ROADMAP.md#m32-refinements),
[#583](https://github.com/mikepadiernos/plasma-opus/issues/583)), while keeping
layouts and panel placement configurable.

**Current.** The menu bar and main toolbar sit above the two file panes.
Sidebar panels dock on the left or right, and embedded tools open below or to
the right of the panes, as listed under [Panes](#panes).

Sliding panes and floating panes are additional parts of the target interface.
They are not assigned to any of the five fixed regions: sliding panes can be
revealed or hidden, while floating panes can be positioned independently.

Each area of the application should be accessible or activated with a
configurable keyboard binding, including the header, columns, footer and their
panes, as well as sliding and floating panes. Bindings should respect the active
context and avoid intercepting text entry or terminal input; see the
[shortcut settings milestone](../ROADMAP.md#m31-keyboard-shortcut-settings) and
[#463](https://github.com/mikepadiernos/plasma-opus/issues/463).

## Panes

- **File panes** (current) are the two primary browsing workspaces in the
  middle column. Each contains folder tabs and a file view; they can be
  arranged side by side or stacked. View modes, including the tree view, are
  part of a pane rather than extra panes.
- **Sidebar panes** (current) are supporting panels docked in the left or right
  column:
  - **Explorer:** the combined navigation panel, containing Favorites, KDE
    Places groups and the folder Tree.
  - **Places:** the separate KDE Places panel, including locations and devices.
  - **Folders:** the separate folder-tree navigation panel.
  - **Information:** details for the selected or hovered item.

  Explorer is an alternative to the separate Places and Folders panels; its
  sections are not additional standalone panels. **View › Panels** switches
  between **Combined Panel** and **Separate Places and Folders**.

- **Embedded panes** provide tools or content within the application. The
  target design allows them in the left column, right column, middle column or
  footer. Current placement options vary by tool:

  | Pane             | Purpose                                                   | Current placement                                                                          | Milestone                                    | Status                                                                    |
  | ---------------- | --------------------------------------------------------- | ------------------------------------------------------------------------------------------ | -------------------------------------------- | ------------------------------------------------------------------------- |
  | Preview          | Previews files and media                                  | In a pane, below the panes or right of the panes                                           | [M05](../ROADMAP.md#m05-preview-and-gallery) | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Text editor      | Edits text and drafts                                     | Side panel, a separate embedded area below or to the right, a maximized or separate window | [M07](../ROADMAP.md#m07-editor)              | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Metadata editor  | Edits supported media metadata                            | Separate window, below or right                                                            | [M09](../ROADMAP.md#m09-tools)               | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Terminal         | Hosts Konsole sessions                                    | Automatic (right for stacked panes), below both panes or right of both panes               | [M09](../ROADMAP.md#m09-tools)               | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Search           | Searches and displays results                             | With the embedded terminal, below the panes or right of the panes                          | [M21](../ROADMAP.md#m21-advanced-search)     | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Tag panel        | Assigns tags and filters by them                          | On the right or below the panes                                                            | [M23](../ROADMAP.md#m23-native-tagging)      | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
  | Office documents | Views and edits documents, spreadsheets and presentations | Not available; embedded editing depends on an available provider                           | [M27](../ROADMAP.md#m27-integrations)        | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square) |

  The search panel is toggled with **Show Search Panel**, and the tag panel
  with **Tags › Tag Panel**; tags themselves are managed in
  **Tags › Manage Tags…**. See the [tagging guide](../tagging.md).

- **Sliding panes** (target) can be revealed or hidden independently of the
  fixed regions. The which-key-inspired shortcut hints and the planned toolbar
  action reference can be shown as sliding panes.
- **Floating panes** (target) can be positioned independently of the fixed
  regions. The [shortcut hints](../keyboard.md#shortcut-hints) already float
  over the active pane while a key sequence waits, sliding up into place
  when animations are on.

For any file that can be viewed, offer editing when a suitable editing tool is
available. Use a supported embedded provider or open a separate application;
show when editing is unavailable rather than assuming a viewer can edit.

See the [shortcut settings](../ROADMAP.md#m31-keyboard-shortcut-settings) and
[layout refinements](../ROADMAP.md#m32-refinements) milestones for these tools.

## Toolbars and customization

**Current** ([M11](../ROADMAP.md#m11-ui-customizations)). The main toolbar
and each pane's own toolbar are editable: entries can be dragged on, off and
around them directly, including the pane toolbar's location bar and search, and
between the main toolbar, pane toolbars and hamburger menu. Separators, spacers
and custom buttons for scripts, commands and presets can be added.

**Target** ([M32.02](../ROADMAP.md#m32-refinements)). Toolbars can be placed in
any of the five regions. Each column can have toolbars at its top or bottom,
separate from the file panes. File-pane toolbars keep their drag-and-drop
customization, including widgets such as search and the address bar.

Items and widgets from any toolbar can be mixed and matched in any other
toolbar, including custom toolbars. Pane-specific actions and widgets retain
their intended pane context.

A single customization mode enables drag-and-drop arrangement of every part of
the interface, including panes, toolbars and toolbar items, without locking or
unlocking each toolbar or panel separately. It is separate from application and
view modes and applies only while customizing the interface. When customization
mode is off, the interface layout and toolbar contents cannot be changed; normal
application use remains available.

## Workspaces

A workspace is a named profile for a workflow. The target profile can include
features, layout, theme and other preferences, and optionally be associated with
a KDE Plasma Activity with settings selected per Activity.

| Part                                        | Milestone                                                                            | Status                                                                    |
| ------------------------------------------- | ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------- |
| Workspace management, layouts and snapshots | [M15](../ROADMAP.md#m15-workspaces), [M16](../ROADMAP.md#m16-workspace-settings)     | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
| Theme profiles and appearance overrides     | [M17](../ROADMAP.md#m17-themes), [M18](../ROADMAP.md#m18-theme-settings)             | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)   |
| Activity associations                       | [M27](../ROADMAP.md#m27-integrations), [M28](../ROADMAP.md#m28-integration-settings) | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square) |
| Local note vault                            | [#565](https://github.com/mikepadiernos/plasma-opus/issues/565)                      | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square) |

Plasma Opus provides workspace management through **View › Workspaces…**, with
location assignments, saved layouts, explicit pane and tab snapshots, linked
task profiles, appearance overrides and workspace settings. Tag groups belong to
the separate native tagging system. See the
[workspace](../ROADMAP.md#m15-workspaces),
[workspace settings](../ROADMAP.md#m16-workspace-settings),
[native tagging](../ROADMAP.md#m23-native-tagging) and
[Activity integration](../ROADMAP.md#m27-integrations) milestones, and the
[layout caveat](../CAVEATS.md#workspace-layouts).

A workspace can attach a theme profile and selected appearance options, such as
Card view, folder colour rules, blur, corner radius and divider visibility.
Unselected options follow the global appearance. This is implemented by the
[themes](../ROADMAP.md#m17-themes) and
[theme settings](../ROADMAP.md#m18-theme-settings) milestones.

Workspaces link reusable task profiles, inspired by
[Krusader profiles](https://docs.kde.org/trunk_kf6/en/krusader/krusader/profiles.html),
including saved searches and folder comparison or synchronization profiles.
Profiles remain separate from workspace layouts and reusable elsewhere.
Switching a workspace does not run tasks or change shared defaults; existing
previews and confirmations still apply. See
[#585](https://github.com/mikepadiernos/plasma-opus/issues/585).

An optional local vault is planned to give a workspace a note-oriented view,
using the built-in text editor for Markdown, note links and backlinks. It
remains a normal folder: workspace management does not move or delete its notes
or modify Obsidian configuration. See the
[vault handoff](../TODO.md#workspace-vault-editing-565).

## Translation ideas

Ideas for language, localization and regional formats belong here; they are
proposals, not implemented features. See the
[translation milestones](../ROADMAP.md#m13-translations) for current scope.

- **Language-aware toolbar labels:** Following
  [Directory Opus](https://docs.dopus.com/doku.php?id=preferences:preferences_categories:user_interface:language),
  offer to update unmodified built-in labels when the interface language
  changes, while preserving user-written labels.
- **Action glossary:** Explore a searchable comparison of built-in action
  names in two installed languages, inspired by the same Directory Opus
  language settings. Identify missing translations rather than presenting
  fallback text as a translation.
- **Translator participation:** Provide clear catalogue contribution and review
  guidance, inspired by [TagSpaces](https://docs.tagspaces.org/ui/i18n/) and
  KDE's [Lokalize](https://docs.kde.org/trunk_kf6/en/lokalize/lokalize/).
  Reuse the existing [translation guide](../../po/README.md) and KDE tooling.

## Miscellaneous ideas

Ideas below are proposals, not claims of implemented functionality.

- **Alternative library views:** Explore calendar, map or kanban presentations
  of existing files and metadata, inspired by
  [TagSpaces perspectives](https://docs.tagspaces.org/). Keep the underlying
  files in place and make missing metadata or unavailable providers clear. See
  the [considered features](../integrations/plasma-impresario.md#considered-features)
  for these views and a notes journal, including how they could work with
  Plasma Impresario and
  [Nextcloud apps](../integrations/nextcloud.md#considered-features).
- **Copy path components:** Explore keyboard-accessible actions for copying a
  file's path, parent path, filename or basename, inspired by
  [Yazi](https://yazi-rs.github.io/docs/quick-start/). Reuse existing clipboard
  actions and distinguish local paths from remote URLs.
- **Unified job history:** Explore a discoverable view of running and failed
  tasks, inspired by Yazi's
  [task manager](https://yazi-rs.github.io/docs/quick-start/). Reuse existing
  KIO and transfer-queue reporting without duplicating notifications; retain
  accurate cancellation and partial-result status.
- **Collection context:** Explore optional descriptions and original-location
  columns for collections, inspired by
  [Directory Opus collections](https://docs.dopus.com/doku.php?id=basic_concepts:virtual_file_system:file_collections).
  Make clear that removing a reference does not delete the original file.

---

← Previous: [Inspirations](inspirations.md) · Related:
[M32 refinement plans](../refinements/README.md)
