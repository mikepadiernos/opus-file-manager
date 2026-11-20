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
| [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446) | Metadata editing fails with an unusable ExifTool installation | Yes | Merged |

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
- Partial fix: [#452](https://github.com/mikepadiernos/opus-file-manager/pull/452)
  preserves the default expanding toolbar spacer when saving a layout.
- Follow-up: See [pending tasks](TODO.md#clarify-separators-and-expanded-space-444)
  for the reproduction details needed before a fix can be verified.

## Completed reports

### Metadata editing with unusable ExifTool

- Severity: not assessed
- Status: Merged; [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446)
- Summary: Metadata editing failed when ExifTool could not load its Perl module.
- Evidence: The audit environment selected a system ExifTool with an
  incompatible Homebrew Perl. The reporter installed ExifTool 13.64 via mise;
  the shell resolves it successfully and the metadata editor and dialog suites
  pass in an isolated environment using that installation.
- Resolution: [#451](https://github.com/mikepadiernos/opus-file-manager/pull/451)
  improved diagnostics to identify the executable and report silent failures.
  Desktop-launched Opus must also have the working executable on its PATH.

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
