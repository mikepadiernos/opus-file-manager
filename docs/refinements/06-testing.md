# M32.06 Functional and security testing

**[Refinements](README.md)** ·
**[Roadmap](../ROADMAP.md#m32-refinements)** ·
**[Edge cases](../EDGE.md)**

**Goal:** exercise every implemented functionality and its security-sensitive
boundaries, record evidence and gaps, and avoid treating test candidates as
confirmed defects.

## Contents

- [Proposed order](#proposed-order)
- [Review decisions](#review-decisions)

## Proposed order

1. Inventory implemented features and entry points; map each to existing
   automated tests or reproducible manual checks. Use
   [edge cases](../EDGE.md), [bugs](../BUGS.md) and
   [caveats](../CAVEATS.md) as inputs. Record missing coverage and unavailable
   features explicitly; extend the matrix when later features ship.
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
   include workspace profiles and Activity associations when available.
   Include missing optional dependencies and local or remote failures; record
   skipped environments.
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

## Review decisions

- Which environments and accessibility tools are essential for release?
- What evidence is sufficient for a manual scenario, and who reviews it?
- How should findings and unavailable test environments be triaged?

Testing reduces risk; it does not prove that the application is secure or
free of defects.
