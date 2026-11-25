# ☁️ Nextcloud integration

**[Documentation](../README.md)** ·
**[Roadmap](../ROADMAP.md#m29-nextcloud-integration)** ·
**[KDE Plasma integration](kde-plasma.md)** ·
**[Plasma Impresario integration](plasma-impresario.md)**

## Status and boundaries

[M29](../ROADMAP.md#m29-nextcloud-integration) plans Nextcloud accounts,
browsing, previews, asset metadata, shares and sync-folder access. This document
describes how Plasma Opus could also work with Nextcloud apps. Only
[Photos](#photos) is on the roadmap; the remaining sections are
[considered features](#considered-features).

- Reuse M27 KDE Online Accounts, KDE credential facilities and KIO WebDAV. Do
  not store passwords or tokens in Plasma Opus configuration, URLs or logs.
- Prefer file-level interfaces: most Nextcloud apps keep their content as
  ordinary files, which Plasma Opus can already browse. Use an app-specific API
  only when files cannot express the workflow.
- Check the server's
  [capabilities](https://docs.nextcloud.com/server/latest/developer_manual/client_apis/OCS/ocs-api-overview.html#capabilities-api)
  and report missing apps, unsupported versions and denied permissions instead
  of assuming every server provides them.
- Each Nextcloud app owns its data. Plasma Opus does not create, change or
  delete app-owned data, such as albums, events, cards or app configuration
  files, without an explicit, confirmed user action.
- Leave synchronization to the Nextcloud desktop client; see M29.

## Nextcloud app interfaces

| App                | Interface                                                                                                                                                                                        |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Files              | WebDAV at `remote.php/dav/files/<user>/`, used by M29.                                                                                                                                           |
| Photos             | WebDAV at `remote.php/dav/photos/<user>/` with `albums`, `sharedalbums` and `places` collections. Server metadata includes `nc:metadata-photos-original_date_time` and `nc:metadata-photos-gps`. |
| Calendar and Tasks | CalDAV. Calendar stores event attachments as ordinary files, in `/Calendar` by default.                                                                                                          |
| Maps               | Custom maps are folders, `/Maps` by default or any folder containing `.index.maps`, holding photos, `.gpx` tracks, contact vCards and `.favorites.json`.                                         |
| Deck               | REST API at `index.php/apps/deck/api/v1.0` for boards, stacks, cards and labels. Since Deck 1.3, card attachments are ordinary files.                                                            |
| Notes              | Markdown files in the folder reported by the `notes_path` capability (Notes 4.12 and later), `Notes` by default, with subfolders as categories.                                                  |
| Text               | Rich workspaces show a folder's `Readme.md` above the Files listing.                                                                                                                             |
| Versions and trash | WebDAV at `remote.php/dav/versions/<user>/` and `remote.php/dav/trashbin/<user>/`.                                                                                                               |

Photos, Calendar, Maps, Deck and Notes were reviewed against their source or
documentation in November 2026; the Text, Versions and trash entries are based
on Nextcloud's documentation and still need verification. App interfaces
change between releases; verify them before implementation.

## Photos

Planned in [M29](../ROADMAP.md#m29-nextcloud-integration):

- Browse albums, shared albums and places as virtual folders in either pane.
  Albums reference files without copying them, so label these folders clearly
  and offer to reveal each photo's real location.
- Sort and group photos by the server's capture date and location metadata
  without downloading them, falling back to file dates and reporting photos
  without metadata.
- Add selected Nextcloud photos to an album, create or rename albums, and
  remove photos from an album, each with confirmation. Removing a photo from
  an album must never delete the file, and the action must say so; deleting a
  file remains a separate, explicit Files action.
- Respect shared-album permissions, and treat places as read-only server
  groupings. Report when the Photos app is unavailable.
- **With Plasma Impresario:** send selected album photos through the M27
  **Send by Email** action, with the location warning described for the
  [map view](plasma-impresario.md#map).

## Considered features

Plasma Opus is considering these features, but they are **not on the
[roadmap](../ROADMAP.md)**. They extend the
[considered views](plasma-impresario.md#considered-features) for calendar, map,
kanban and notes to Nextcloud. Add a feature to the roadmap, with its issue,
before implementing it.

### Calendar

- **Plasma Opus:** Include Nextcloud files in the considered file calendar,
  using server capture dates where available and file dates otherwise, without
  downloading files.
- **Nextcloud Calendar:** Event attachments are ordinary files in the
  attachment folder, so Plasma Opus can browse and reveal them. Opening,
  creating or editing events belongs to calendar clients; on Plasma, offer to
  open an event in the user's KDE calendar application through the CalDAV
  account configured in KDE PIM. Plasma Opus does not implement a CalDAV
  client.
- **Nextcloud Tasks and Deck:** Tasks and Deck cards with due dates appear in
  CalDAV calendars when enabled on the server; treat them like other events.
- **Boundaries:** Never change events, tasks or file dates from the file
  calendar, and never remove an attachment that an event still references
  without warning that the event will lose it.

### Map

- **Nextcloud Photos:** Show Nextcloud photos in the considered map view from
  the server's `nc:metadata-photos-gps` metadata without downloading them, and
  show the server's reverse-geocoded places as browsable groups.
- **Nextcloud Maps:** Recognize custom map folders, show their `.gpx` tracks
  and photos on the map view and offer to open the map in Nextcloud Maps. Read
  `.favorites.json` only as an optional, read-only layer, because its format
  belongs to Maps. Mark a folder as a custom map only on explicit request.
- **Boundaries:** Plasma Opus does not geocode addresses or upload locations.
  Explain that server-side place grouping uses the server's own geocoder.

### Kanban

- **Nextcloud Deck:** Show a Deck board's stacks and cards in the considered
  kanban view, opening card attachments, which are ordinary files, in a pane.
  Offer explicit actions to attach selected Nextcloud files to a card or create
  a card linked to a file.
- **Separate boards:** A folder board moves files between subfolders; a Deck
  board moves cards between stacks and never moves files. Label which kind of
  board is shown, and change a Deck card only through an explicit, confirmed
  action that respects the board's permissions.
- **Boundaries:** Store only board and card references, never Deck content, and
  report cards or boards that are no longer available.

### Notes and journal

- **Nextcloud Notes:** Use the folder reported by `notes_path`, through a local
  sync folder or WebDAV, as a vault or journal folder. Notes remain Markdown
  files that the Notes app shows, with subfolders as categories. Use the Notes
  API only if file access cannot support a workflow.
- **Rich workspaces:** In Nextcloud folders, optionally show a folder's
  `Readme.md` as a read-only description above the listing, separate from M03
  folder notes. Editing it is an explicit action on the file.
- **Boundaries:** Detect concurrent changes from Nextcloud before saving, and
  never overwrite a note silently.

### Other Nextcloud features

- **Versions:** List a Nextcloud file's versions, preview or compare them with
  the current file, and restore one with confirmation.
- **Trash bin:** Browse the Nextcloud trash bin and restore or permanently
  delete items with confirmation, kept separate from the local trash.

## Acceptance

Test against a disposable Nextcloud server with synthetic accounts and data;
never use a real account in automated tests. Verify missing or disabled apps,
older app versions, read-only shares and shared albums, expired credentials,
offline servers, concurrent edits and cancellation. File workflows must keep
working when no optional Nextcloud app is installed.
