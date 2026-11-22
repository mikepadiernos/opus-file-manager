# M30.5 Performance and startup

**[Refinements](README.md)** · **[Roadmap](../ROADMAP.md#m305-performance-and-startup)** ·
**[Settings](04-settings.md)**

**Goal:** measurably improve startup and settings responsiveness while
preserving behavior and avoiding unproven language rewrites.

## Proposed order

1. Define reproducible cold and warm runs for first interactive window,
   settings opening and first-page readiness. Record machine, build, session,
   input data, sample counts, timing distributions and memory use. Establish
   targets from the observed baseline rather than inventing thresholds.
   ([#477](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/477))
2. Profile the baseline, separate GUI-thread work from I/O and external
   dependencies, and rank bottlenecks by measured impact. Record regressions
   and noise before deciding what to change.
3. Defer nonessential startup work where safe, preserving restored sessions,
   pane readiness, error reporting and user-selected startup behavior.
   Compare results against the same baseline.
   ([#478](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/478))
4. Improve settings opening and page switching with measured lazy creation
   or safe background loading. Preserve search, validation, unsaved edits,
   Apply/Cancel and immediate effects even on unopened pages.
   ([#479](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/479))
5. Prototype Rust for suitable profiled, self-contained core hotspots and
   integrate where benchmarks show a benefit. Use a narrow safe interface;
   account for build, packaging and dependency costs. The M19 pilot can inform
   this work. Keep widgets, settings and KIO operations in Qt/KDE.
   ([#480](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/480))
6. Repeat measurements under matching conditions and add functional regression
   tests for optimized paths, errors and cancellation. Keep ordinary test
   pass/fail criteria independent of hardware-specific timing.
   ([#481](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/481))

## Review decisions

- Which hardware, session sizes and environments are representative?
- What improvement targets are justified by the measured baseline?
- Which Rust candidates deliver enough benefit to justify their costs?
