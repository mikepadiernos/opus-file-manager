# M32.06 Functional and security testing

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Refinements index](https://img.shields.io/badge/refinements-index-3daee9?style=flat-square)](README.md)
[![Roadmap: M32 refinements](https://img.shields.io/badge/roadmap-M32%20refinements-8957e5?style=flat-square)](../ROADMAP.md#m32-refinements)
[![Edge cases](https://img.shields.io/badge/edge-cases-3daee9?style=flat-square)](../EDGE.md)

![M32.06 Functional and security testing](https://img.shields.io/badge/M32.06-Functional%20and%20security%20testing-8957e5?style=flat-square)
![Proposal](https://img.shields.io/badge/Proposal-6e7781?style=flat-square)

**Goal:** exercise every implemented functionality and its security-sensitive
boundaries, record evidence and gaps, and avoid treating test candidates as
confirmed defects.

> [!NOTE]
> This plan is a proposal for review. The [roadmap](../ROADMAP.md#m32-refinements)
> defines the scope of M32.06; nothing here is implemented yet.

## Contents

- [Problem](#problem)
- [Proposal](#proposal)
- [Execution order](#execution-order)
- [Open questions](#open-questions)

## Problem

Plasma Opus has 98 QtTest source files in `tests/`, run with `ctest`, but no
map from implemented features and entry points to the tests or manual checks
that cover them. The repository has no CI workflow, and static analysis and
sanitizer runs are not part of the documented workflow. The
[edge-case inventory](../EDGE.md) lists scenarios to test; they are not
confirmed bugs and may overlap existing tests.

M32.06 asks to:

- inventory every implemented feature and map it to reproducible automated or
  documented manual tests, using coverage reports to find gaps;
- review trust boundaries, adversarial inputs, privacy and secret handling;
- run static analysis, dependency checks and sanitizers, and evaluate CI;
- exercise workflows end to end, across a representative accessibility,
  layout, translation and desktop matrix, and in usability walkthroughs.

## Proposal

Build a test matrix from the implemented features, then fill its gaps in order
of risk, recording evidence for each scenario and skipped environment.

- Inputs: [edge cases](../EDGE.md), [bugs](../BUGS.md) and
  [caveats](../CAVEATS.md).
- Isolation: temporary directories, `QStandardPaths::setTestModeEnabled(true)`
  and synthetic credentials; never real home, trash, user data or production
  remote services.
- Findings: triage reproducible findings with severity and remediation; treat
  candidates as unconfirmed until reproduced.

## Execution order

1. Inventory implemented features and entry points; map each to existing
   automated tests or reproducible manual checks. Use
   [edge cases](../EDGE.md), [bugs](../BUGS.md) and
   [caveats](../CAVEATS.md) as inputs. Use coverage reports to identify gaps
   without treating coverage as proof of correctness. Record missing coverage
   and unavailable features explicitly; extend the matrix when later features
   ship.
   ([#470](https://github.com/mikepadiernos/plasma-opus/issues/470))
2. Establish isolated fixtures and representative test environments. Use
   temporary directories, test-mode configuration and synthetic credentials;
   never use real home, trash, user data or production remote services.
3. Run unit, integration and end-to-end workflows for success, invalid input,
   cancellation, partial failure, recovery, persistence and cross-feature
   interaction. Verify destructive operations and confirmations only against
   disposable fixtures.
   ([#471](https://github.com/mikepadiernos/plasma-opus/issues/471))
4. Exercise keyboard, pointer, assistive technology, animations, layouts,
   translations and representative desktop setups. Cover region and pane
   bindings, sliding and floating panes, placements and restored layouts;
   include workspace profiles, and Activity associations once M27 provides
   them. Include missing optional dependencies and local or remote failures;
   record skipped environments.
   ([#472](https://github.com/mikepadiernos/plasma-opus/issues/472))
5. Conduct documented first-run and experienced-user walkthroughs, including
   keyboard and assistive-technology use. Capture reproducible confusion and
   regressions; resolve or triage findings.
   ([#491](https://github.com/mikepadiernos/plasma-opus/issues/491))
6. Review trust boundaries and test adversarial inputs for file operations,
   archives, paths, links, commands, terminal integration and imported data.
   Check validation, permissions, confirmations and errors without executing
   untrusted code.
   ([#473](https://github.com/mikepadiernos/plasma-opus/issues/473))
7. Check privacy and synthetic secret handling in settings, sessions, backups,
   logs, temporary files and remote connections. Record limitations rather
   than assuming heuristic secret filtering is complete.
   ([#474](https://github.com/mikepadiernos/plasma-opus/issues/474))
8. Run suitable static analysis and dependency checks and document a security
   review. Evaluate clang-tidy and PVS-Studio with their licensing and
   integration costs; apply the C++ Core Guidelines where compatible with
   Qt/KDE ownership and conventions. Run focused AddressSanitizer and
   UndefinedBehaviorSanitizer tests on supported toolchains, and evaluate
   ThreadSanitizer separately for concurrency. Record unsupported environments,
   investigate findings, rerun affected checks after fixes and list residual
   risks.
   ([#475](https://github.com/mikepadiernos/plasma-opus/issues/475))
9. Evaluate running the applicable tests, formatting and analysis in CI using
   documented toolchains and dependencies. Keep sanitizer configurations
   separate where required and report unavailable checks explicitly.

## Open questions

- Which environments and accessibility tools are essential for release?
- What evidence is sufficient for a manual scenario, and who reviews it?
- How should findings and unavailable test environments be triaged?

> [!IMPORTANT]
> Testing reduces risk; it does not prove that the application is secure or
> free of defects.

---

[Refinements index](README.md) · ← Previous:
[M32.05 Performance and startup](05-performance.md)
