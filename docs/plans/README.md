# M30 UI and UX polish

This is a **proposed, editable plan for review**, not an implementation or
completion report. Add, remove, reorder or modify steps as needed. The
[roadmap](../ROADMAP.md#m30-ui-and-ux-polish) defines milestone scope, and
the [M30 issue](https://github.com/mikepadiernos/opus-file-manager/issues/28)
and project board track status. When scope changes are accepted, reconcile
the roadmap and issues; plan text alone does not change their status.

Check issue and board state before starting. Follow the
[project workflow](../../CLAUDE.md#project-workflow); this document does not
mark work complete.

## Subsection plans

1. [Menus](01-menus.md)
2. [Layouts](02-layouts.md)
3. [Interaction](03-interaction.md)
4. [Settings](04-settings.md)
5. [Performance and startup](05-performance.md)
6. [Functional and security testing](06-testing.md)

## Proposed execution order

1. **Establish the baseline:** inventory existing workflows, tests,
   customizations and dependencies; measure startup and settings performance.
   Record the environment and known limitations before changing behavior.
   See [testing](06-testing.md) and [performance](05-performance.md).
2. **Agree on interaction and layout rules:** map action ownership, pane
   context, defaults, accessibility and existing customizations. Review these
   choices together before changing menus, layouts or controls.
3. **Implement in vertical slices:** start with action state and menus, then
   layout and toolbar customization, followed by interaction and settings.
   Coordinate shared action, help and customization work across the plans;
   include focused tests in each slice rather than waiting until the end.
4. **Optimize measured bottlenecks:** compare with the baseline, apply startup
   and settings improvements, and evaluate Rust only for suitable measured
   core hotspots. The M15 pilot can inform the decision but is not a reason to
   block unrelated polish if unavailable.
5. **Run cross-feature validation:** execute the functional, accessibility,
   environment and security plan; investigate findings, retest changes and
   document remaining issues. Testing cannot prove the absence of defects.
6. **Close out:** verify each feature, then follow the
   [project workflow](../../CLAUDE.md#project-workflow) and record blockers or
   caveats as required.

The order is a recommendation, not a requirement to serialize independent
work. Cross-cutting design and testing should happen throughout. Review
currently implemented features first; unavailable later features do not block
M30 and should be revisited when they ship.
