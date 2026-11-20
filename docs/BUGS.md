# Reported bugs

This file is the project log for user-visible bugs and reliability issues.
It is meant to stay aligned with the [Opus project board](https://github.com/users/mikepadiernos/projects/3): every bug added here should also have a matching board issue and a link back to it from this file.

A bug is considered complete only when its project status is **Completed** or
**Merged**. **Completed** does not necessarily mean merged into `main`.

---

## Current tracked issues

| Issue | Bug | Complete? | Project status |
| --- | --- | --- | --- |
| [#37](https://github.com/mikepadiernos/opus-file-manager/issues/37) | Combined panel header and accordion titles are misaligned | Yes | Merged |
| [#378](https://github.com/mikepadiernos/opus-file-manager/issues/378) | Embedded Konsole terminal does not honor profile background blur | Yes | Merged |
| [#383](https://github.com/mikepadiernos/opus-file-manager/issues/383) | Editor settings should use the notepad symbolic icon | Yes | Merged |
| [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444) | Separators and expanded space are not reflected | No | Development |
| [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446) | Metadata editing fails with an unusable ExifTool installation | No | Backlog |

## Status legend

- **Completed**: issue is closed and marked completed, but not necessarily merged.
- **Merged**: issue has been merged into `main`.
- **Untriaged**: issue has been logged locally but has not yet been created or
  assigned on the project board.

---

## Tracking policy

- Add all user-reported bugs to this file.
- Add a matching issue to the project board whenever a bug is recorded here.
- Keep the board issue and this log synchronized whenever status changes.
- Verify the linked issue and board state before marking a bug as resolved.

## Current audit findings

A project-wide audit completed on 2026-11-20 found the implementation broadly
healthy, but not yet fully release-ready. The build succeeds and the test suite
is mostly green: 68 of 70 tests passed.

### Metadata editing with unusable ExifTool

- Severity: not yet assessed
- Status: Backlog; [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446)
  is awaiting triage
- Summary: Metadata editing fails when the installed ExifTool executable cannot
  load its Perl module. This is specific to affected environments, not a
  confirmed failure with a working ExifTool installation.
- Evidence: On 2026-11-20, `opusfm-metadataeditortest` and
  `opusfm-metadataeditdialogtest` failed in this environment. ExifTool reported
  `Can't locate Image/ExifTool.pm in @INC`.
- Expected behaviour: Supported metadata previews and writes succeed with a
  working ExifTool; broken dependencies are reported clearly.
- Actual behaviour: Metadata preview fails in the affected environment; Opus
  reports the ExifTool error.
- Likely area to inspect: ExifTool's runtime installation and dependency
  diagnostics, including mixed Perl environments.
- Recommended follow-up: Investigate dependency detection and runtime setup,
  verify the toolchain documentation, and add coverage for unusable ExifTool
  installations where appropriate.

## UI report

### Separators and expanded spacing do not reflect the current tree state

- Severity: medium
- Status: Development; [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444)
  remains open pending clarification.
- Report: separators and expanded space are not reflecting in the displayed UI.
- Evidence: the original screenshot shows a main toolbar, Explorer panel and
  details-view folder tree, but does not identify which separator or spacer was
  configured or show the settings used. The earlier tree-specific diagnosis was
  speculative. A synthetic tree height check did not reproduce a failure.
- Follow-up: See [pending tasks](TODO.md#clarify-separators-and-expanded-space-444)
  for the reproduction details needed before a fix can be verified.

## Completed reports

### Combined panel header and accordion alignment

- Severity: not recorded
- Status: Merged; [#37](https://github.com/mikepadiernos/opus-file-manager/issues/37)
- Summary: The combined panel used the wrong header and centred accordion
  titles, with the caret on the left.
- Evidence: The issue includes the original screenshot and reproduction steps.
- Expected behaviour: Show the explorer panel header, left-align section
  titles, place carets on the right, and make the relevant sections expandable.
- Actual behaviour: The header said Places, titles were centred and section
  expansion did not match the requested layout.
- Resolution: Marked Merged on the project board.

### Embedded terminal profile blur

- Severity: not recorded
- Status: Merged; [#378](https://github.com/mikepadiernos/opus-file-manager/issues/378)
- Summary: The embedded terminal did not honor the active Konsole profile's
  background blur.
- Evidence: The issue describes comparing embedded and standalone Konsole
  sessions using a translucent profile with blur enabled.
- Expected behaviour: Honor profile blur when the compositor supports it,
  without forcing blur when disabled.
- Actual behaviour: The embedded background was not blurred.
- Resolution: Marked Merged on the project board.

### Editor settings icon

- Severity: not recorded
- Status: Merged; [#383](https://github.com/mikepadiernos/opus-file-manager/issues/383)
- Summary: The Editor settings page did not use the requested notepad-style
  symbolic icon.
- Evidence: The issue identifies the Editor page in the settings sidebar.
- Expected behaviour: Use a notepad symbolic icon from the active icon theme.
- Actual behaviour: The page requested `accessories-text-editor` instead.
- Resolution: Marked Merged on the project board.
