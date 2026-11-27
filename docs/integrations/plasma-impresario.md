# Plasma Impresario integration

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Roadmap: M27 integrations](https://img.shields.io/badge/roadmap-M27%20integrations-8957e5?style=flat-square)](../ROADMAP.md#m27-integrations)
[![Roadmap: M29 Nextcloud integration](https://img.shields.io/badge/roadmap-M29%20Nextcloud%20integration-8957e5?style=flat-square)](../ROADMAP.md#m29-nextcloud-integration)
[![Plasma Impresario repository](https://img.shields.io/badge/Plasma%20Impresario-repository-1D99F3?style=flat-square&logo=github)](https://github.com/mikepadiernos/plasma-impresario)

![Milestone M27.09](https://img.shields.io/badge/milestone-M27.09-8957e5?style=flat-square)
![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)

How Plasma Opus plans to hand files to and from
[Plasma Impresario](https://github.com/mikepadiernos/plasma-impresario), the KDE
IMAP mail client formerly named Plasma Mail, and other mail clients.

**Integration plans:** [KDE Plasma](kde-plasma.md) · [Nextcloud](nextcloud.md)
· **Plasma Impresario**

> [!NOTE]
> **Implemented:** **File › Send by Email…**
> ([#656](https://github.com/mikepadiernos/plasma-opus/issues/656)), under
> [Compose with selected files](#compose-with-selected-files); Plasma
> Impresario's composer still opens without the files. Plasma Opus drags
> already provide file URLs. **Planned:** the other workflows under
> [Planned features](#planned-features), in M27.09 Plasma Impresario and
> mail. They are not commitments by Plasma Impresario. Everything under
> [Considered features](#considered-features), including theme sharing, is
> outside the roadmap.

## Contents

- [Goals](#goals)
- [Boundaries](#boundaries)
- [Planned features](#planned-features)
- [Current Plasma Impresario interfaces](#current-plasma-impresario-interfaces)
- [Considered features](#considered-features)
- [Acceptance](#acceptance)

## Goals

Let users move between selecting files and writing mail without copying paths
by hand. Plasma Impresario's
[roadmap](https://github.com/mikepadiernos/plasma-impresario/blob/main/docs/ROADMAP.md)
and
[Plasma Opus integration notes](https://github.com/mikepadiernos/plasma-impresario/blob/main/docs/integrations/plasma-opus.md)
do not yet define a file-handoff API. Prefer standard desktop interfaces so
each workflow also works with other mail clients; use an application-specific
interface only after both projects define and implement it.

## Boundaries

Plasma Impresario owns messages, accounts, drafts, recipients and sending.
Plasma Opus owns file selection and file operations. Neither application should
read the other's private data or change its settings without explicit user
action.

## Planned features

| Workflow                                                                        | Milestone                                                                                               |
| ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| [Compose with selected files](#compose-with-selected-files)                     | M27.09 Send by Email ([M27](../ROADMAP.md#m27-integrations))                                            |
| [Drag and drop into a draft](#drag-and-drop-into-a-draft)                       | M27.09 Mail attachments                                                                                 |
| [Save received attachments into a pane](#save-received-attachments-into-a-pane) | M27.09 Mail attachments                                                                                 |
| [Reveal saved attachments](#reveal-saved-attachments)                           | M27.01 `org.freedesktop.FileManager1` ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279)) |
| [Preview and open mail files](#preview-and-open-mail-files)                     | M27.09 Mail file preview                                                                                |
| [Share links in a draft](#share-links-in-a-draft)                               | [M29](../ROADMAP.md#m29-nextcloud-integration) shares                                                   |
| [Workspaces and Plasma Activities](#workspaces-and-plasma-activities)           | M27.09 Shared context with Plasma Impresario                                                            |

### Compose with selected files

> [!TIP]
> **Implemented** as **File › Send by Email…**, also in the context menu.
> Its limits are recorded under
> [Desktop integration](../CAVEATS.md#desktop-integration).

- Offer an explicit **Send by Email** action for selected files. Use KIO's
  `KEMailClientLauncherJob`, which starts the user's preferred `mailto:`
  handler, and the
  [XDG Email portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Email.html)
  when sandboxed. Respect the configured client; do not hard-code Plasma
  Impresario.
- `KEMailClientLauncherJob` passes attachments to most clients as `attach`
  parameters in a `mailto:` URL. Plasma Impresario currently ignores them, so
  its composer opens without the files; the gap and proposed acceptance
  criteria are
  [recorded on Plasma Impresario #263](https://github.com/mikepadiernos/plasma-impresario/issues/263#issuecomment-7148664548).
  Until it supports them, explain that the files were not attached instead of
  reporting success; do not fall back to its internal D-Bus interface.
- Show the selected filenames and explain unsupported folders or inaccessible
  files before handoff. Preserve multiple-file selection and report a rejected
  request or unavailable mail client. Opening a composer must not send mail.
- For remote files, explicitly offer to prepare private local copies through
  KIO. Show the source and download implications, require consent, report
  failures and cancellation, and keep prepared files available until the
  consumer no longer needs them. Clean up temporary copies safely without
  deleting original files.

### Drag and drop into a draft

> [!TIP]
> **Implemented:** files can be dragged from the **Send by Email** list into
> any composer, including remote files once their private local copies are
> ready. Its limits are recorded under
> [Desktop integration](../CAVEATS.md#desktop-integration).

- Plasma Opus drags already provide file URLs. Plasma Impresario attaches
  dropped local files, so local drags need verification rather than new
  Plasma Opus code.
- For remote items, offer the same private local copy as above instead of
  dragging a URL the composer will ignore. Do not silently download, attach or
  send files, and do not expose credentials embedded in remote URLs.

### Save received attachments into a pane

> [!TIP]
> **Implemented:** a dropped Plasma Impresario attachment reference is
> refused with a status-bar explanation, and dropped file content with a
> suggested filename is saved through KIO.

- Dropping a Plasma Impresario attachment on a Plasma Opus pane currently
  delivers only its private IMAP reference. KIO's drop handling would offer to
  save that reference as a file, so recognize the private format and explain
  that the attachment must be saved from Plasma Impresario instead.
- Accept a real attachment drop once Plasma Impresario provides file content,
  a suggested filename or a completed local file URL. Use the existing KIO drop
  and conflict handling; never contact the mail server from Plasma Opus.

### Reveal saved attachments

- Let Plasma Impresario own attachment selection, saving and confirmation.
  After an explicit save, it may ask the file manager to reveal the saved
  location through `org.freedesktop.FileManager1`, which Plasma Opus provides
  while it is the default folder handler
  ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279)).
- Preserve the selected destination, file identity and conflict handling; show
  errors for unavailable or removed files. Do not access message stores,
  download attachments automatically or infer that a saved file is safe.

### Preview and open mail files

> [!TIP]
> **Implemented:** local `.eml` files show their headers, plain-text body and
> attachments in the preview panel; **Extract All**, **Extract Selected** and
> **Open** act on the attachments only when asked.

- Preview local `.eml` (`message/rfc822`) files in the preview panel as plain
  text: show headers, the plain-text body or a sanitized text rendering, and
  the attachment list. Never load remote content, run scripts or decrypt
  messages. Treat names and content as untrusted.
- Offer explicit actions to extract a listed attachment with the normal
  conflict handling and to open the file in its registered application.
  Plasma Impresario does not currently open mail files, so do not assume it
  is the handler or silently import messages.

### Share links in a draft

- When the planned
  [Nextcloud integration](../ROADMAP.md#m29-nextcloud-integration) provides a
  share link, allow the user to send it by email only after reviewing the link
  and its permissions. Pass it in the `mailto:` body; do not create a share or
  disclose a private link automatically, and keep share management in the
  Nextcloud workflow.

### Workspaces and Plasma Activities

- Plasma Impresario plans to associate mail accounts with workspaces and their
  Plasma Activities. Use the Activity as the only shared context: each
  application follows Activity changes with its own associations. Do not read
  or write the other application's workspace or account configuration, and
  keep unassociated workspaces and accounts unaffected.

## Current Plasma Impresario interfaces

Reviewed against Plasma Impresario `main` at `2c27f2e7`:

| Interface         | Current behaviour                                                                                                                                                               |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Desktop entry     | `io.github.mikepadiernos.plasma_impresario.desktop` registers only `x-scheme-handler/mailto`, with a **New Message** action.                                                    |
| Command line      | `plasma-impresario [mailto:URL]`; `-c` opens the composer and `-p NAME` selects a profile.                                                                                      |
| D-Bus             | The inherited `net.flaska.trojita` service at `/Trojita` exposes `showMainWindow`, `showAddressbookWindow` and `composeMail(url)`. Treat it as internal, not a stable contract. |
| `mailto:` parsing | Reads recipients, subject, body, `In-Reply-To` and `References`; **ignores `attach`**.                                                                                          |
| Composer drops    | Accepts `text/uri-list` drops of **local** files as attachments; ignores remote URLs.                                                                                           |
| Attachment drags  | Provide only a private `application/x-trojita-imap-part` reference, not file content or a URL.                                                                                  |

## Considered features

Plasma Opus is considering these features, but they are **not on the
[roadmap](../ROADMAP.md)**. The calendar, map and kanban views come from the
[alternative library views](../philosophy/designs.md#miscellaneous-ideas) idea,
inspired by TagSpaces'
[Calendar](https://docs.tagspaces.org/perspectives/calendar),
[Mapique](https://docs.tagspaces.org/perspectives/mapique) and
[Kanban](https://docs.tagspaces.org/perspectives/kanban) perspectives. Notes and
journal capture correspond to Plasma Impresario's planned
[mail-to-notes workflow](https://github.com/mikepadiernos/plasma-impresario/blob/main/docs/integrations/plasma-opus.md#notes-and-journal).
Theme sharing builds on the existing M17 theme profiles. Add a feature to the
roadmap, with its issue, before implementing it.

Each view presents existing files and metadata without moving them into a
private store, and makes missing metadata or unavailable providers clear. The
[Nextcloud integration](nextcloud.md#considered-features) describes how these
views could also work with Nextcloud Calendar, Maps, Deck and Notes.

### Calendar

- **Plasma Opus:** Show files from a folder, search or library on a day, week
  or month calendar by a chosen date: modification time, embedded capture date
  from M23's metadata reading, an M23 library date field or a date in the
  folder or file name. Reuse the M13 and M14 calendar, Jalali and date-format
  settings, and identify files without the chosen date.
- **With Plasma Impresario:** Saved attachments appear by their save or embedded
  date. Offer **Send by Email** for files selected from a day or range. Plasma
  Impresario's planned mail-to-calendar workflow owns events; Plasma Opus could
  offer to open a file's linked event only through a supported KDE calendar
  interface, never by reading mail or calendar stores.
- **Boundaries:** A file calendar is not an event calendar. Do not create,
  change or delete events, and do not change file dates when moving items
  between days without an explicit metadata edit.

### Map

- **Plasma Opus:** Show geotagged photos and videos on a map using GPS metadata
  read through KFileMetaData and M23's embedded-metadata reading. Use an opt-in,
  configurable tile server or offline tiles; explain that online tiles disclose
  the viewed area to the server. Identify files without location data and
  never infer locations.
- **With Plasma Impresario:** Send selected files from a map area by email.
  Before handing off images with GPS data, warn that the location will be
  shared and offer to send copies with location metadata removed, without
  changing the originals.
- **Boundaries:** Keep location data local. Do not geocode addresses, upload
  coordinates or write location metadata without explicit action.

### Kanban

- **Plasma Opus:** Present a folder's subfolders as board columns and their
  files as cards, showing thumbnails, labels, ratings, tags and notes. Moving a
  card moves the file to another subfolder through KIO with the normal
  confirmation, conflict handling and undo; card order is Plasma Opus metadata
  that does not rename files.
- **With Plasma Impresario:** Plasma Impresario could save attachments into a
  board's intake column, and Plasma Opus could offer **Send by Email** from a
  card. A card could link to its source message only if Plasma Impresario
  defines a stable message link, such as an
  [RFC 5092](https://www.rfc-editor.org/rfc/rfc5092) IMAP URL, and an interface
  to open it. Store only that link, not message content, and report links that
  no longer resolve.
- **Boundaries:** A board does not change mail state, and moving a card never
  sends, archives or flags a message.

### Notes and journal

- **Plasma Opus:** Extend the planned workspace vault
  ([#565](https://github.com/mikepadiernos/plasma-opus/issues/565)) with an
  optional journal: create or open a dated Markdown note in a configured vault
  folder, using a configurable file-name pattern and template. Notes remain
  ordinary files that other editors can open. Folder notes from M03 remain
  separate short metadata.
- **With Plasma Impresario:** Its planned capture action could create a note in
  the vault, or append to the current journal note, from selected message
  text. Plasma Opus should accept this only through standard handoffs or a
  defined interface that shows the destination and content before saving. A
  captured note could include a message link as described for kanban cards.
  Preserve existing drafts and unsaved edits, and never overwrite a note
  silently.
- **Boundaries:** Plasma Opus never reads Plasma Impresario's mail store, and
  copying message content into a note is always an explicit user choice.

### Share theme settings

- Carry theme settings over in both directions. Plasma Opus already saves,
  imports and exports versioned [theme profiles](../ROADMAP.md#m17-themes) as
  JSON documents (`"format": "plasma-opus-theme"`, version 1). It could import
  a profile exported by Plasma Impresario, once or by following later changes,
  and export its own for Plasma Impresario to import. Plasma Impresario tracks
  its side in
  [plasma-impresario#268](https://github.com/mikepadiernos/plasma-impresario/issues/268).
- Agree on a versioned shared format covering the appearance choice (follow the
  system, a light or dark preset or an installed KDE colour scheme), named
  palette roles and an optional Qt Style Sheet. Map roles that only one
  application uses to the nearest shared role or leave them at their defaults.
- Keep applying a shared theme opt-in, previewable and reversible, and offer
  the choice in [theme settings](../ROADMAP.md#m18-theme-settings). Validate
  imported profiles, ignore unknown roles, keep text, selection and focus
  indicators readable, never execute code from theme files and block external
  resources in stylesheets. Fall back to the active Plasma colour scheme when a
  profile is missing, invalid or unsupported.
- Exchange profiles through export and import files or a documented shared
  location; neither application writes the other's configuration.

### Rejected approaches

- Hard-coding Plasma Impresario instead of respecting the user's configured
  mail client.
- Falling back to Plasma Impresario's internal D-Bus interface when `mailto:`
  attachments are ignored.
- A private API for shared context instead of Plasma Activities.
- Reading either application's private data, mail store or configuration.

## Acceptance

Verify local and remote selections, multiple files, missing clients, sandbox
portal access, cancellation, inaccessible paths and temporary-file lifetime.
Test with isolated files and synthetic messages; never send a real message as
part of an automated test. Standard desktop handoffs should continue to work
with other mail clients.

If theme sharing is added to the roadmap, verify import, export, following
changes, invalid and missing profiles, and use without Plasma Impresario
installed, using isolated configuration.

### Plasma Impresario dependencies

Plasma Impresario changes that would complete these workflows, for its own
roadmap:

- Honour `attach` parameters for local files in `mailto:` URLs, with the user's
  review before sending.
- Provide a file URL or file content with a suggested filename when dragging an
  attachment out of the reading pane.
- Optionally register `message/rfc822` if it should open mail files.
- Define a stable message link and an interface to open it, if notes or kanban
  cards should link back to messages.
- Import and export theme profiles in the agreed shared format
  ([#268](https://github.com/mikepadiernos/plasma-impresario/issues/268)).

Any Plasma Impresario-specific capability remains optional until its interface
and availability are confirmed.
