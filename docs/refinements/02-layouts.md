# M30.2 Layouts

**[Refinements](README.md)** · **[Roadmap](../ROADMAP.md#m302-layouts)** ·
**[Menus](01-menus.md)**

**Goal:** improve first-run layouts and customization without losing existing
window, panel, toolbar or session choices.

## Proposed toolbar layouts

> [!NOTE]
> These editable defaults do not replace saved toolbars. `[Separator]` marks a
> divider; `[Expanding space]` pushes following items toward the trailing edge.

### Toolbars

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
├── Search
├── [Expanding space]
└── Toolbar Customization
```

The pane toolbar divider and expanding space are proposed for review; verify
their support in the existing customizer before adopting them as defaults.
Preserve existing main and pane toolbar order, custom items and shortcuts.
Keep overflow keyboard accessible and make action target panes clear.

## Proposed order

Implement the five-region [interface structure](../philosophy/designs.md) as
part of these refinements. Keep file panes in the middle column, sidebar panes
in either side column, and embedded panes configurable across the side
columns, middle column and footer. Support sliding and floating panes outside
the fixed regions. Preserve existing placements and saved layouts, including
file-pane state when an embedded pane occupies the middle column. Integrate
planned panes as they become available without blocking existing work.

1. Inventory defaults and saved layouts for both pane orientations, panels,
   tabs and toolbars. Agree on revised defaults and apply them only to new
   configurations or explicitly confirmed resets.
   ([#461](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/461))
2. Establish KDE-native spacing, alignment, icons and visual hierarchy;
   check small windows, high DPI, long translations and right-to-left layouts.
   Preserve theme choices and keep both panes and essential controls usable.
   ([#462](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/462),
   [#464](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/464))
3. Review restore behavior after monitor, resolution and scale changes, and
   splitter or panel changes after orientation, maximize, hide and swap.
   Provide recovery for unusably small panels without discarding saved geometry
   or overriding window-manager placement.
   ([#483](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/483),
   [#484](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/484))
4. Extend the existing customizer for custom items on the main and each pane
   toolbar. Preserve per-toolbar order, labels and icons across restarts, and
   report unavailable items without silently deleting saved choices.
   Coordinate pane targeting and keyboard access with
   [interaction](03-interaction.md).
   ([#492](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/492),
   [#493](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/493))
5. Provide a searchable, labeled reference for built-in actions, including
   newly introduced actions, as a sliding or floating pane without resetting
   customized toolbars.
   Add session-level toolbar edit reversion and confirmed individual-toolbar
   reset without changing other toolbars, menus or saved commands.
   ([#539](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/539),
   [#540](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/540))
6. Test restored and new layouts, placements and pane state, missing actions,
   customization and reset, screen changes, large text and both pane
   orientations. Keep previous user settings intact in regression fixtures.

## Review decisions

- Which first-run defaults should change, and which should remain as they are?
- What should toolbar edit reversion include after an explicit save?
- How should unavailable custom items be presented without losing them?
