# Reported bugs

This file is the project log for user-visible bugs and reliability issues.
It is meant to stay aligned with the [Opus project board](https://github.com/users/mikepadiernos/projects/3): every bug added here should also have a matching board issue and a link back to it from this file.

A bug is considered complete only when its project status is **Completed** or
**Merged**. **Completed** does not necessarily mean merged into `main`.
Potential scenarios awaiting investigation are tracked separately in
[edge cases](EDGE.md); they are not confirmed bugs.

---

## Current tracked issues

| Issue | Bug | Complete? | Project status |
| --- | --- | --- | --- |
| [#37](https://github.com/mikepadiernos/opus-file-manager/issues/37) | Combined panel header and accordion titles are misaligned | Yes | Merged |
| [#378](https://github.com/mikepadiernos/opus-file-manager/issues/378) | Embedded Konsole terminal does not honor profile background blur | Yes | Merged |
| [#383](https://github.com/mikepadiernos/opus-file-manager/issues/383) | Editor settings should use the notepad symbolic icon | Yes | Merged |
| [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444) | KDE toolbar editor separators and spacers do not match the toolbar | Yes | Merged |
| [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446) | Metadata editing fails with an unusable ExifTool installation | Yes | Merged |
| [#457](https://github.com/mikepadiernos/opus-file-manager/issues/457) | Embedded terminal panel controls and background blur | Yes | Merged |
| [#476](https://github.com/mikepadiernos/opus-file-manager/issues/476) | Toolbar context menus and symbolic icons | Yes | Merged |
| [#532](https://github.com/mikepadiernos/opus-file-manager/issues/532) | Consistent panel placement beside or below the panes | Yes | Merged |
| [#533](https://github.com/mikepadiernos/opus-file-manager/issues/533) | Embedded panel titles cannot be hidden like Explorer | Yes | Merged |

## Status legend

- **Completed**: issue is closed and marked completed, but not necessarily merged.
- **Merged**: issue has been merged into `main`.
- **Review**: implementation is ready for review.
- **Development**: work is in progress.
- **Backlog**: issue has been recorded on the project board but work has not
  started.
- **Untriaged**: issue has been logged locally but has not yet been created or
  assigned on the project board.

---

## Tracking policy

- Add all user-reported bugs to this file.
- Add a matching issue to the project board whenever a bug is recorded here.
- Keep the board issue and this log synchronized whenever status changes.
- Verify the linked issue and board state before marking a bug as resolved.

## Open reports

None.

## Completed reports

### Hide embedded panel titles

- Severity: not assessed
- Status: Merged; [#533](https://github.com/mikepadiernos/opus-file-manager/issues/533)
- Summary: Embedded panel titles should be hideable without losing controls,
  document status, metadata changes or terminal sessions.
- Resolution: Panel Appearance offers a separate, persistent embedded-title
  preference for the terminal, preview, text editor, metadata editor and
  information panel. Controls remain available, document identity and unsaved
  status move out of the title area, and the information panel supplies a
  placement and hide menu when its dock title is hidden. Existing navigation
  title preferences and separate-window decorations remain unchanged.

### Consistent panel placement

- Severity: not assessed
- Status: Merged; [#532](https://github.com/mikepadiernos/opus-file-manager/issues/532)
- Summary: Preview, text editor and metadata editor need placement below or
  right of the panes without losing their existing modes or content.
- Resolution: Preview and metadata editing offer below/right placement alongside
  their existing pane and separate-window modes. Editor placement and dock
  sizes are remembered. Moving metadata editing retains pending fields and
  preview/apply safeguards; active operations prevent closing. Existing
  navigation and information docks already allow right-side placement.

### Toolbar context menus and symbolic icons

- Severity: not assessed
- Status: Merged; [#476](https://github.com/mikepadiernos/opus-file-manager/issues/476)
- Summary: The view mode button should use a symbolic icon consistent with
  other small toolbar controls. The reporter also observes a full-colour
  Open Editor icon and requests the symbolic icon used on the Editor settings
  page. The main and pane toolbars also need context menus with appropriate
  entries and access to toolbar editing settings.
- Expected behaviour: Prefer the active theme's symbolic icon at small sizes,
  with the existing regular-icon fallback for larger sizes or themes without
  a symbolic variant. Use the settings page's notepad icon for Open Editor
  and other applicable editor actions. Provide context menus for the main
  toolbar and each pane toolbar with relevant entries, including an option to
  open Preferences → Interface → Toolbars. Preserve existing action-specific
  context menus, view-mode, editor and toolbar customization behaviour.
- Investigation: The customizer overwrote the pane view and sort buttons'
  symbolic icons from their empty widget-action icons. The separate
  `toggle_view_mode` action requested regular icons directly. `open_editor`
  requested `accessories-text-editor`, whereas the Editor settings page uses
  `view-pim-notes`. Other editor actions were reviewed for the same mismatch.
- Resolution: Preserve widget-action icons, use theme-aware view and editor
  icons, and provide toolbar-specific context menus with settings access and
  the existing KDE toolbar options.

### Embedded terminal panel controls and background blur

- Severity: not assessed
- Status: Merged; [#457](https://github.com/mikepadiernos/opus-file-manager/issues/457)
- Summary: The embedded terminal lacks an accessible panel context menu and
  dismissal controls, while profile blur reportedly disappears after opening.
- Evidence: The reporter's screenshot shows the terminal and persistent
  "Terminal sessions remain open until their tabs are closed." message. The
  reporter observed the background initially blurred, then transparent without
  blur.
- Expected behaviour: Provide panel controls independently of Konsole's
  terminal context menu, allow the informational message to be dismissed,
  and let users hide the panel without ending sessions. Honor the active
  profile's blur setting consistently when supported.
- Actual behaviour: Right-clicking offers the terminal's menu, the message
  cannot be dismissed, and the reporter could not find an in-panel way to
  close the panel without ending a session. Blur reportedly does not persist.
- Resolution: [#459](https://github.com/mikepadiernos/opus-file-manager/pull/459)
  adds panel controls to hide the panel without ending sessions and dismiss
  messages. When the Konsole profile enables blur, Opus reapplies a window-wide
  blur request during repaint so the active style cannot replace it with an
  incomplete region. The previous blur report was
  [#378](https://github.com/mikepadiernos/opus-file-manager/issues/378).

### KDE toolbar editor separators and spacers

- Severity: medium
- Status: Merged; [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444)
- Summary: Separators and expanding spacers in KDE's toolbar editor did not
  match the live or restored toolbar.
- Evidence: The reporter's screenshots identify the KDE toolbar editor and the
  resulting toolbar. KDE replaced the toolbar on Apply, leaving Opus with the
  old toolbar and saved layout.
- Resolution: [#454](https://github.com/mikepadiernos/opus-file-manager/pull/454)
  synchronizes the editor with the current layout and saves the rebuilt toolbar.
  [#452](https://github.com/mikepadiernos/opus-file-manager/pull/452) also
  preserves the default spacer when saving. Previously overridden layouts must
  be configured and applied again.

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
