# M32.03 Interaction

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Refinements index](https://img.shields.io/badge/refinements-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M32 refinements](https://img.shields.io/badge/roadmap-M32%20refinements-8957e5?style=flat-square)](../ROADMAP.md#m32-refinements)

![M32.03 Interaction](https://img.shields.io/badge/M32.03-Interaction-8957e5?style=flat-square)
![Proposal](https://img.shields.io/badge/Proposal-6e7781?style=flat-square)

**Goal:** make the application's state and consequences understandable,
accessible and responsive across pane and panel workflows.

> [!NOTE]
> This plan is a proposal for review. The [roadmap](../ROADMAP.md#m32-refinements)
> defines the scope of M32.03; nothing here is implemented yet.

## Contents

- [Problem](#problem)
- [Proposal](#proposal)
- [Execution order](#execution-order)
- [Open questions](#open-questions)

## Problem

Plasma Opus runs many asynchronous and multi-pane workflows: two panes with
tabs, embedded panels, KIO operations, searches and previews. Users need to
know which pane an action affects, what happened to each item and why an action
is unavailable, whether they use a mouse, a keyboard or a screen reader.

M32.03 asks to:

- prevent late listing, preview and search results from updating the wrong
  pane, tab or dialog;
- make destructive confirmations, pane source and destination, batch results
  and loading, empty and error states clear;
- review keyboard navigation, focus, screen-reader names, contrast and
  tooltips, reusing M31 shortcuts and KDE accessibility facilities;
- show active filters and their scope, and display long, similar and
  bidirectional filenames clearly;
- improve contextual help and discovery, including keyboard-accessible custom
  toolbar items;
- add subtle, optional animations and focused interaction tests.

## Proposal

Treat clarity of state as the shared theme: every pane, panel and dialog should
show what it is doing, what it will affect and how to undo or cancel it, without
relying on colour, animation or a pointing device alone.

- Reuse KDE facilities (KIO job reporting, KDE confirmation policies, KDE help
  and accessibility) rather than adding parallel mechanisms.
- Keep user customizations, shortcuts and existing settings unchanged; add
  hints and indicators without mandatory onboarding.
- Coordinate pane targeting and toolbar keyboard access with
  [layouts](02-layouts.md), and broader walkthroughs and accessibility coverage
  with [testing](06-testing.md).

## Execution order

1. Audit keyboard navigation, focus restoration, screen-reader names and
   contrast. Provide configurable bindings to reach or activate each
   [interface region and pane](../philosophy/designs.md), including sliding
   and floating panes. Reuse M31 and KDE facilities without intercepting text
   entry or terminal input. Cover both mouse and keyboard workflows.
   ([#463](https://github.com/mikepadiernos/plasma-opus/issues/463))
2. Clarify pane source and destination using labels rather than colour alone.
   Show active filters, their scope and visible or hidden item counts where
   available. Allow pane switching without losing selection; distinguish
   filtering the listing from searching elsewhere and make quick-filter
   clearing distinct from removal of other filters.
   ([#542](https://github.com/mikepadiernos/plasma-opus/issues/542),
   [#543](https://github.com/mikepadiernos/plasma-opus/issues/543))
3. Improve loading, empty, unavailable and error states, including progress,
   cancellation, skipped items, partial completion and disabled actions. Avoid
   duplicate KIO notifications and never imply cancellation rolls back
   completed work.
   ([#465](https://github.com/mikepadiernos/plasma-opus/issues/465),
   [#488](https://github.com/mikepadiernos/plasma-opus/issues/488))
4. Review confirmations for destructive work and the display of long,
   similar and bidirectional filenames, with a way to inspect the full original
   name. Identify affected items and targets, use safe defaults and avoid
   redundant prompts; preserve real names, KDE policies and operation semantics.
   ([#486](https://github.com/mikepadiernos/plasma-opus/issues/486),
   [#487](https://github.com/mikepadiernos/plasma-opus/issues/487))
5. Improve contextual discovery and accessible tooltips, and provide
   context-sensitive local help where available. Explain missing help and
   respect user-configured shortcuts.
   ([#467](https://github.com/mikepadiernos/plasma-opus/issues/467),
   [#485](https://github.com/mikepadiernos/plasma-opus/issues/485),
   [#541](https://github.com/mikepadiernos/plasma-opus/issues/541))
6. Make custom toolbar items accessible by keyboard and drag and drop. Ensure
   pane actions target the intended pane, retain command preview and
   confirmation, and do not delete the underlying saved command.
   ([#493](https://github.com/mikepadiernos/plasma-opus/issues/493))
7. Prevent late listing, preview and search results from updating the wrong
   context. Retain focus and selection where possible after navigation,
   cancellation or dialog closure.
   ([#489](https://github.com/mikepadiernos/plasma-opus/issues/489))
8. Add subtle transitions only where useful. **Preferences › Themes › Motion
   and Accessibility** already offers **Animations: Follow Plasma, Reduced or
   Off**; build on it rather than adding a separate switch, honour desktop motion
   settings and keep state clear without animation.
   ([#469](https://github.com/mikepadiernos/plasma-opus/issues/469))
9. Add focused interaction regressions for menu actions, layouts, navigation,
   responsiveness, errors and preserved customizations. Coordinate broader
   walkthroughs and accessibility coverage with [testing](06-testing.md).
   ([#468](https://github.com/mikepadiernos/plasma-opus/issues/468))

## Open questions

- Which pane, filter and partial-result indicators should be persistent?
- Where should context help live, and what should happen when it is missing?
- Which transitions are useful enough to animate?

---

[Refinements index](README.md) · ← Previous: [M32.02 Layouts](02-layouts.md) ·
Next: [M32.04 Settings](04-settings.md) →
