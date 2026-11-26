# M32.05 Performance and startup

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Refinements index](https://img.shields.io/badge/refinements-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M32 refinements](https://img.shields.io/badge/roadmap-M32%20refinements-8957e5?style=flat-square)](../ROADMAP.md#m32-refinements)

![M32.05 Performance and startup](https://img.shields.io/badge/M32.05-Performance%20and%20startup-8957e5?style=flat-square)
![Proposal](https://img.shields.io/badge/Proposal-6e7781?style=flat-square)

**Goal:** measurably improve startup and settings responsiveness while
preserving behaviour and avoiding unproven language rewrites.

> [!NOTE]
> This plan is a proposal for review. The [roadmap](../ROADMAP.md#m32-refinements)
> defines the scope of M32.05; nothing here is implemented yet.

## Contents

- [Problem](#problem)
- [Proposal](#proposal)
- [Execution order](#execution-order)
- [Open questions](#open-questions)

## Problem

Plasma Opus restores sessions, panes, panels and workspaces at startup, and its
Preferences dialog builds 16 groups of tabbed pages. No baseline yet records how
long startup, opening settings or switching pages take, or how long builds take,
so there is no evidence of where time goes.

M32.05 asks to:

- profile and benchmark cold and warm startup, settings opening and first-page
  readiness, and set targets against that baseline;
- reduce GUI-thread startup work and speed up settings with measured changes
  such as deferred initialization and lazy page creation;
- measure clean and incremental build times and evaluate improvements;
- prototype Rust for profiled, self-contained core hotspots where benchmarks
  show a benefit;
- add reproducible performance checks and functional regression tests.

## Proposal

Measure first, then change only what the measurements justify, and compare
every change against the same baseline under matching conditions.

- Keep widgets, settings and KIO operations in Qt/KDE. Rust is a candidate only
  for self-contained core logic; the M21 query-parser pilot in `rust/searchquery/`
  (built only with the `OPUSFM_RUST_PROTOTYPE` CMake option) and its
  [evaluation](../rust-prototype.md) inform that work.
- Preserve restored sessions, pane readiness, error reporting, unsaved settings
  edits and user-selected startup behaviour.
- Keep ordinary test pass/fail criteria independent of hardware-specific timing.

## Execution order

1. Define reproducible cold and warm runs for first interactive window,
   settings opening and first-page readiness. Record machine, build, session,
   input data, sample counts, timing distributions and memory use. Establish
   targets from the observed baseline rather than inventing thresholds.
   ([#477](https://github.com/mikepadiernos/plasma-opus/issues/477))
2. Profile the baseline, separate GUI-thread work from I/O and external
   dependencies, and rank bottlenecks by measured impact. Record regressions
   and noise before deciding what to change.
3. Defer nonessential startup work where safe, preserving restored sessions,
   pane readiness, error reporting and user-selected startup behaviour.
   Compare results against the same baseline.
   ([#478](https://github.com/mikepadiernos/plasma-opus/issues/478))
4. Improve settings opening and page switching with measured lazy creation
   or safe background loading. Preserve search, validation, unsaved edits,
   Apply/Cancel and immediate effects even on unopened pages.
   ([#479](https://github.com/mikepadiernos/plasma-opus/issues/479))
5. Measure clean and incremental build times, identify bottlenecks and evaluate
   improvements against the same baseline without compromising build
   correctness or the existing CMake workflow.
6. Prototype Rust for suitable profiled, self-contained core hotspots and
   integrate where benchmarks show a benefit. Use a narrow safe interface;
   account for build, packaging and dependency costs. The M21 pilot can inform
   this work. Keep widgets, settings and KIO operations in Qt/KDE.
   ([#480](https://github.com/mikepadiernos/plasma-opus/issues/480))
7. Repeat measurements under matching conditions and add functional regression
   tests for optimized paths, errors and cancellation. Keep ordinary test
   pass/fail criteria independent of hardware-specific timing.
   ([#481](https://github.com/mikepadiernos/plasma-opus/issues/481))

## Open questions

- Which hardware, session sizes and environments are representative?
- What improvement targets are justified by the measured baseline?
- Which Rust candidates deliver enough benefit to justify their costs?

---

[Refinements index](README.md) · ← Previous: [M32.04 Settings](04-settings.md)
· Next: [M32.06 Functional and security testing](06-testing.md) →
