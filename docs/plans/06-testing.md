# M30.6 Functional and security testing

[M30 plan](README.md) · [Roadmap](../ROADMAP.md#m306-extensive-functional-and-security-testing)

**Goal:** exercise every implemented functionality and its security-sensitive
boundaries, record evidence and gaps, and avoid treating test candidates as
confirmed defects.

## Proposed order

1. Inventory implemented features and entry points; map each to existing
   automated tests or reproducible manual checks. Use
   [edge cases](../EDGE.md), [bugs](../BUGS.md) and
   [caveats](../CAVEATS.md) as inputs. Record missing coverage and unavailable
   features explicitly; extend the matrix when later features ship.
   ([#470](https://github.com/mikepadiernos/opus-file-manager/issues/470))
2. Establish isolated fixtures and representative test environments. Use
   temporary directories, test-mode configuration and synthetic credentials;
   never use real home, trash, user data or production remote services.
3. Run unit, integration and end-to-end workflows for success, invalid input,
   cancellation, partial failure, recovery, persistence and cross-feature
   interaction. Verify destructive operations and confirmations only against
   disposable fixtures.
   ([#471](https://github.com/mikepadiernos/opus-file-manager/issues/471))
4. Exercise keyboard, pointer, assistive technology, animations, layouts,
   translations and representative desktop setups. Include missing optional
   dependencies and local or remote failures; record skipped environments.
   ([#472](https://github.com/mikepadiernos/opus-file-manager/issues/472))
5. Conduct documented first-run and experienced-user walkthroughs, including
   keyboard and assistive-technology use. Capture reproducible confusion and
   regressions; resolve or triage findings.
   ([#491](https://github.com/mikepadiernos/opus-file-manager/issues/491))
6. Review trust boundaries and test adversarial inputs for file operations,
   archives, paths, links, commands, terminal integration and imported data.
   Check validation, permissions, confirmations and errors without executing
   untrusted code.
   ([#473](https://github.com/mikepadiernos/opus-file-manager/issues/473))
7. Check privacy and synthetic secret handling in settings, sessions, backups,
   logs, temporary files and remote connections. Record limitations rather
   than assuming heuristic secret filtering is complete.
   ([#474](https://github.com/mikepadiernos/opus-file-manager/issues/474))
8. Run suitable static analysis and dependency checks and document a security
   review. Track reproducible findings, severity and remediation separately;
   rerun affected checks after fixes and list residual risks.
   ([#475](https://github.com/mikepadiernos/opus-file-manager/issues/475))

## Review decisions

- Which environments and accessibility tools are essential for release?
- What evidence is sufficient for a manual scenario, and who reviews it?
- How should findings and unavailable test environments be triaged?

Testing reduces risk; it does not prove that the application is secure or
free of defects.
