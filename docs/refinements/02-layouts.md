# M32.02 Layouts

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Refinements index](https://img.shields.io/badge/refinements-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M32 refinements](https://img.shields.io/badge/roadmap-M32%20refinements-8957e5?style=flat-square)](../ROADMAP.md#m32-refinements)

![M32.02 Layouts](https://img.shields.io/badge/M32.02-Layouts-8957e5?style=flat-square)
![Proposal](https://img.shields.io/badge/Proposal-6e7781?style=flat-square)

**Goal:** improve first-run layouts and customization without losing existing
window, panel, toolbar or session choices.

> [!NOTE]
> This plan is a proposal for review. The [roadmap](../ROADMAP.md#m32-refinements)
> defines the scope of M32.02; nothing here is implemented yet.

## Contents

- [Problem](#problem)
- [Proposal](#proposal)
  - [Five-region layout](#five-region-layout)
  - [Customization mode](#customization-mode)
  - [Proposed toolbar layouts](#proposed-toolbar-layouts)
- [Execution order](#execution-order)
- [Open questions](#open-questions)

## Problem

Plasma Opus already restores window geometry, pane layouts, sessions and
customized toolbars. Sidebar panels dock on the left or right, and embedded
tools such as the preview, editor, terminal, search and tag panels open below
or beside the file panes. M11 added drag-and-drop editing of the main and pane
toolbars, including the search and location-bar widgets.

The current main toolbar starts with **Create New**, copy and move to the other
pane, rename and trash, then undo and a new tab, then view controls (view mode,
compact, editor and preview panel). After an expanding space come folder
search, the filter, the dual or stacked layout toggle and the application menu.

M32.02 asks to:

- restore windows to a usable position after monitor, resolution or scale
  changes, and review splitter and panel behaviour after orientation, maximize,
  hide and swap;
- adapt panes, panels, dialogs and menus to small windows, high DPI, long
  translations and right-to-left layouts;
- implement the five-region [interface design](../philosophy/designs.md) and
  review first-run defaults
  ([#583](https://github.com/mikepadiernos/plasma-opus/issues/583));
- extend the toolbar customizer with custom items, per-toolbar revert and reset,
  and a searchable reference of built-in actions;
- harmonize spacing, alignment, icons, labels and visual hierarchy.

## Proposal

### Five-region layout

Implement the five-region [interface structure](../philosophy/designs.md).
Keep file panes in the middle column, sidebar panes in either side column, and
embedded panes configurable across the side columns, middle column and footer.
Support sliding and floating panes outside the fixed regions.

Preserve existing placements and saved layouts, including file-pane state when
an embedded pane occupies the middle column. Bring the existing search (M21) and
tag (M23) panels into the region model, and integrate later panes, such as the
M27 office pane, as they become available without blocking this work. Track this
work in [#583](https://github.com/mikepadiernos/plasma-opus/issues/583).

### Customization mode

Introduce the unified
[customization mode](../philosophy/designs.md#toolbars-and-customization)
separately from application and view modes. While enabled, allow interface parts
and toolbar items to be rearranged by drag and drop without individual toolbar
or panel locks. When disabled, prevent layout and toolbar-content changes while
preserving normal use. Provide keyboard-accessible alternatives and preserve
saved customizations.

Allow toolbars in every region, including above or below each column separately
from file panes. Keep file-pane toolbars customizable, including the movable
search and address-bar widgets that M11 introduced.

### Proposed toolbar layouts

> [!NOTE]
> These editable defaults do not replace saved toolbars. Brackets mark
> widgets and layout items: `[Separator]` is a divider, and `[Expanding space]`
> pushes following items toward the trailing edge.

#### Toolbars

```text
Main toolbar
├── Create New
├── Copy to Other Pane
├── Move to Other Pane
├── Rename
├── Move to Trash
├── [Separator]
├── Undo
├── New Tab
├── [Separator]
├── Pane Layout
├── View Mode
├── Previews
├── Preview Panel
├── Hidden Files
├── [Expanding space]
├── Open Terminal
├── Open Editor
└── Application Menu
```

```text
Pane toolbar
├── Back
├── Forward
├── Up
├── Reload
├── [Separator]
├── View Mode
├── Zoom
├── Sort
├── Previews
├── Hidden Files
├── Filter
├── [Expanding space]
├── [Address Bar]
├── [Search]
└── Toolbar Customization
```

#### Embedded pane toolbars

These are proposed layouts, not a description of current controls. Show
context-specific actions only when applicable. The search and tag panels
already exist; the toolbars below propose how their controls could be arranged.

```text
Preview toolbar
├── Previous Image
├── Next Image
├── Open Gallery
├── Open
├── Render Text
├── Autoplay
├── [Expanding space]
├── Placement
└── Hide Preview
```

```text
Text editor toolbar
├── Open
├── Save
├── Save As
├── [Separator]
├── Source
├── Visual
├── Markdown Preview
├── Export PDF
├── [Expanding space]
├── Placement
└── Hide Editor
```

```text
Metadata editor toolbar
├── Preview Changes
├── Apply Changes
├── [Expanding space]
├── Placement
└── Close Editor
```

```text
Terminal toolbar
├── New Session
├── Close Session
├── [Expanding space]
├── Terminal Panel Options
└── Hide Terminal
```

```text
Advanced search toolbar
├── [Search Query]
├── [Search Scope]
├── Search
├── Cancel Search
├── [Separator]
├── Open Result
├── Reveal Result
├── [Expanding space]
├── Placement
└── Hide Search
```

```text
Tagging toolbar
├── [Tag Search]
├── Create Tag
├── Edit Tag
├── [Separator]
├── Assign Tags
├── Remove Tags
├── [Expanding space]
├── Placement
└── Hide Tagging
```

Keep metadata preview and confirmation safeguards, editor drafts and terminal
sessions intact when changing placement or hiding a pane. Closing a terminal
session remains distinct from hiding the terminal.

The pane toolbar divider and expanding space are proposed for review; verify
their support in the existing customizer before adopting them as defaults.
Preserve existing main and pane toolbar order, custom items and shortcuts. Keep
overflow keyboard accessible and make action target panes clear.

## Execution order

1. Inventory defaults and saved layouts for both pane orientations, panels, tabs
   and toolbars. Agree on revised defaults and apply them only to new
   configurations or explicitly confirmed resets.
   ([#461](https://github.com/mikepadiernos/plasma-opus/issues/461))
2. Establish KDE-native spacing, alignment, icons and visual hierarchy; check
   small windows, high DPI, long translations and right-to-left layouts.
   Preserve theme choices and keep both panes and essential controls usable.
   ([#462](https://github.com/mikepadiernos/plasma-opus/issues/462),
   [#464](https://github.com/mikepadiernos/plasma-opus/issues/464))
3. Review restore behaviour after monitor, resolution and scale changes, and
   splitter or panel changes after orientation, maximize, hide and swap. Provide
   recovery for unusably small panels without discarding saved geometry or
   overriding window-manager placement.
   ([#483](https://github.com/mikepadiernos/plasma-opus/issues/483),
   [#484](https://github.com/mikepadiernos/plasma-opus/issues/484))
4. Implement the [five-region layout](#five-region-layout) and
   [customization mode](#customization-mode), preserving existing placements
   and saved layouts.
   ([#583](https://github.com/mikepadiernos/plasma-opus/issues/583))
5. Extend the existing customizer for custom items on the main and each pane
   toolbar. Preserve per-toolbar order, labels and icons across restarts, and
   report unavailable items without silently deleting saved choices. Coordinate
   pane targeting and keyboard access with [interaction](03-interaction.md).
   ([#492](https://github.com/mikepadiernos/plasma-opus/issues/492),
   [#493](https://github.com/mikepadiernos/plasma-opus/issues/493))
6. Provide a searchable, labelled reference for built-in actions, including
   newly introduced actions, as a sliding or floating pane without resetting
   customized toolbars. Add session-level toolbar edit reversion and confirmed
   individual-toolbar reset without changing other toolbars, menus or saved
   commands.
   ([#539](https://github.com/mikepadiernos/plasma-opus/issues/539),
   [#540](https://github.com/mikepadiernos/plasma-opus/issues/540))
7. Test restored and new layouts, placements and pane state, missing actions,
   customization mode on and off, keyboard access and reset, screen changes,
   large text and both pane orientations. Keep previous user settings intact in
   regression fixtures.

## Open questions

- Which first-run defaults should change, and which should remain as they are?
- What should toolbar edit reversion include after an explicit save?
- How should unavailable custom items be presented without losing them?
- Should the proposed search and tag panel toolbars replace or extend the
  controls those panels already have?

---

[Refinements index](README.md) · ← Previous: [M32.01 Menus](01-menus.md) ·
Next: [M32.03 Interaction](03-interaction.md) →
