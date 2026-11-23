# Interface design

**[Documentation](../README.md)** ·
**[Refinements](../refinements/02-layouts.md)** · **[Roadmap](../ROADMAP.md)**

> [!NOTE] This document describes the target design. Placement and customization
> options are not all implemented; see the linked milestones for their status.

## Contents

- [Interface structure](#interface-structure)
- [Panes](#panes)
- [Toolbars and customization](#toolbars-and-customization)
- [Workspaces](#workspaces)
- [Translation ideas](#translation-ideas)
- [Miscellaneous ideas](#miscellaneous-ideas)

## Interface structure

The target interface has five regions: a header, three columns and a footer. The
middle column holds the file-management workspace; the side columns provide
space for supporting panels.

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
the structure as part of the [layout refinements](../refinements/02-layouts.md),
while keeping layouts and panel placement configurable.

Sliding panes and floating panes are additional parts of the interface. They are
not assigned to any of the five fixed regions: sliding panes can be revealed or
hidden, while floating panes can be positioned independently.

Each area of the application should be accessible or activated with a
configurable keyboard binding, including the header, columns, footer and their
panes, as well as sliding and floating panes. Bindings should respect the active
context and avoid intercepting text entry or terminal input; see the
[shortcut settings milestone](../ROADMAP.md#m31-keyboard-shortcut-settings).

## Panes

- **File panes** are the two primary browsing workspaces in the middle column.
  Each contains folder tabs and a file view; they can be arranged side by side
  or stacked. View modes, including the tree view, are part of a pane rather
  than extra panes.
- **Sidebar panes** are supporting panels positioned only in the left or right
  column:
  - **Explorer:** the combined navigation panel, containing Favorites, KDE
    Places groups and the folder Tree.
  - **Places:** the separate KDE Places panel, including locations and devices.
  - **Folders:** the separate folder-tree navigation panel.
  - **Information:** details for the selected or hovered item.

  Explorer is an alternative to the separate Places and Folders panels; its
  sections are not additional standalone panels.

- **Embedded panes** are panels or panes that provide tools or content within
  the application. The target design allows them in the left column, right
  column, middle column or footer:
  - **Preview:** previews files and media.
  - **Text editor:** edits text and drafts.
  - **Metadata editor:** edits supported media metadata.
  - **Terminal:** hosts Konsole sessions.
  - **Office documents:** planned viewing and editing integration for documents,
    spreadsheets and presentations; embedded editing depends on an available
    provider. See [integrations](../ROADMAP.md#m27-integrations).
  - **Advanced search:** searches and displays results in a planned embedded
    pane; the current search interface is a dialog. See the
    [advanced search plan](../ROADMAP.md#m21-advanced-search).
  - **Tagging:** a planned embedded pane for managing and assigning tags; see
    the [tagging plan](../ROADMAP.md#m23-native-tagging).

  Current placement options vary by tool; Preview and the editors also offer
  other modes.

- **Sliding panes** can be revealed or hidden independently of the fixed
  regions. The planned which-key-inspired shortcut hints and toolbar action
  reference can be shown as sliding panes.
- **Floating panes** can be positioned independently of the fixed regions. The
  shortcut hints and toolbar action reference can also be shown as floating
  panes.

For any file that can be viewed, offer editing when a suitable editing tool is
available. Use a supported embedded provider or open a separate application;
show when editing is unavailable rather than assuming a viewer can edit.

See the [shortcut settings](../ROADMAP.md#m31-keyboard-shortcut-settings) and
[layout refinements](../ROADMAP.md#m32-refinements) milestones for these tools.

## Toolbars and customization

Toolbars can be placed in any of the five regions. Each column can have toolbars
at its top or bottom, separate from the file panes. File-pane toolbars are also
customizable: their items can be rearranged by dragging, including widgets such
as search and the address bar.

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

Workspaces are planned to link reusable task profiles, inspired by
[Krusader profiles](https://docs.kde.org/trunk_kf6/en/krusader/krusader/profiles.html),
including saved searches and folder comparison or synchronization profiles.
Profiles remain separate from workspace layouts and reusable elsewhere.
Switching a workspace does not run tasks or change shared defaults; existing
previews and confirmations still apply. See
[#585](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/585).

An optional local vault can give a workspace a note-oriented view, using the
built-in text editor for Markdown, note links and backlinks. It remains a normal
folder: workspace management does not move or delete its notes or modify
Obsidian configuration.

Plasma Opus provides workspace management through **View → Workspaces**,
location assignments and partial layout snapshots. Richer layouts and Activity
associations are planned. Tag groups belong to the separate native tagging
system; see the [workspace milestone](../ROADMAP.md#m15-workspaces),
[layout caveat](../CAVEATS.md#workspace-layouts) and
[native tagging milestone](../ROADMAP.md#m23-native-tagging) and
[Activity integration](../ROADMAP.md#m27-integrations) milestones.

## Translation ideas

Ideas for language, localization and regional formats belong here. See the
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
  files in place and make missing metadata or unavailable providers clear.
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
