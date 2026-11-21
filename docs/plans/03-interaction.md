# M30.3 Interaction

[M30 plan](README.md) · [Roadmap](../ROADMAP.md#m303-interaction)

**Goal:** make the application's state and consequences understandable,
accessible and responsive across pane and panel workflows.

## Proposed order

1. Audit keyboard navigation, focus restoration, screen-reader names and
   contrast. Reuse M29 and KDE facilities without intercepting text entry or
   terminal input. Cover both mouse and keyboard workflows.
   ([#463](https://github.com/mikepadiernos/opus-file-manager/issues/463))
2. Clarify pane source and destination using labels rather than color alone.
   Show active filters, their scope and visible or hidden item counts where
   available. Allow pane switching without losing selection; distinguish
   filtering the listing from searching elsewhere and make quick-filter
   clearing distinct from removal of other filters.
   ([#542](https://github.com/mikepadiernos/opus-file-manager/issues/542),
   [#543](https://github.com/mikepadiernos/opus-file-manager/issues/543))
3. Improve loading, empty, unavailable and error states, including progress,
   cancellation, skipped items, partial completion and disabled actions. Avoid
   duplicate KIO notifications and never imply cancellation rolls back
   completed work.
   ([#465](https://github.com/mikepadiernos/opus-file-manager/issues/465),
   [#488](https://github.com/mikepadiernos/opus-file-manager/issues/488))
4. Review confirmations for destructive work and the display of long,
   similar and bidirectional filenames, with a way to inspect the full original
   name. Identify affected items and targets, use safe defaults and avoid
   redundant prompts; preserve real names, KDE policies and operation semantics.
   ([#486](https://github.com/mikepadiernos/opus-file-manager/issues/486),
   [#487](https://github.com/mikepadiernos/opus-file-manager/issues/487))
5. Improve contextual discovery and accessible tooltips, and provide
   context-sensitive local help where available. Explain missing help and
   respect user-configured shortcuts.
   ([#467](https://github.com/mikepadiernos/opus-file-manager/issues/467),
   [#485](https://github.com/mikepadiernos/opus-file-manager/issues/485),
   [#541](https://github.com/mikepadiernos/opus-file-manager/issues/541))
6. Make custom toolbar items accessible by keyboard and drag and drop. Ensure
   pane actions target the intended pane, retain command preview and
   confirmation, and do not delete the underlying saved command.
   ([#493](https://github.com/mikepadiernos/opus-file-manager/issues/493))
7. Prevent late listing, preview and search results from updating the wrong
   context. Retain focus and selection where possible after navigation,
   cancellation or dialog closure.
   ([#489](https://github.com/mikepadiernos/opus-file-manager/issues/489))
8. Add subtle transitions only where useful; honor desktop motion settings,
   provide an app-level disable option and keep state clear without animation.
   ([#469](https://github.com/mikepadiernos/opus-file-manager/issues/469))
9. Add focused interaction regressions for menu actions, layouts, navigation,
   responsiveness, errors and preserved customizations. Coordinate broader
   walkthroughs and accessibility coverage with [testing](06-testing.md).
   ([#468](https://github.com/mikepadiernos/opus-file-manager/issues/468))

## Review decisions

- Which pane, filter and partial-result indicators should be persistent?
- Where should context help live, and what should happen when it is missing?
- Which transitions are useful enough to animate?
