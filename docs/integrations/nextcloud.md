# Nextcloud integration

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Roadmap: M29 Nextcloud integration](https://img.shields.io/badge/roadmap-M29%20Nextcloud%20integration-8957e5?style=flat-square)](../ROADMAP.md#m29-nextcloud-integration)
[![Roadmap: M30 Nextcloud settings](https://img.shields.io/badge/roadmap-M30%20Nextcloud%20settings-8957e5?style=flat-square)](../ROADMAP.md#m30-nextcloud-settings)

![Milestones M29–M30](https://img.shields.io/badge/milestones-M29%E2%80%93M30-8957e5?style=flat-square)
![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square)

How Plasma Opus works with Nextcloud files and apps, the server interfaces it
uses, its planned settings and ideas considered beyond the roadmap.

**Integration plans:** [KDE Plasma](kde-plasma.md) · **Nextcloud** ·
[Plasma Impresario](plasma-impresario.md)

> [!NOTE]
> **Implemented:** all of
> [M29](../ROADMAP.md#m29-nextcloud-integration): Nextcloud accounts, browsing,
> previews, asset search, Photos albums, shares and the desktop client's
> folders, described under [Implemented features](#implemented-features).
> **Planned:** only the settings in
> [M30](../ROADMAP.md#m30-nextcloud-settings), summarized under
> [Planned features](#planned-features). Everything under
> [Considered features](#considered-features) is outside the roadmap.

## Contents

- [Goals](#goals)
- [Boundaries](#boundaries)
- [Implemented features](#implemented-features)
- [Planned features](#planned-features)
- [Nextcloud app interfaces](#nextcloud-app-interfaces)
- [Photos](#photos)
- [Considered features](#considered-features)
- [Acceptance](#acceptance)

## Goals

Provide optional native Nextcloud navigation, file management and digital asset
management, reusing M27 KDE Online Accounts and KIO WebDAV rather than replacing
the Nextcloud desktop client or its synchronization engine. Beyond files, this
document describes how Plasma Opus could also work with Nextcloud apps.

## Boundaries

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
- Leave synchronization of the desktop client's folders to the Nextcloud
  desktop client: Plasma Opus only reads its configuration and asks it for
  status. Optional
  [two-way cloud sync](../ROADMAP.md#m27-integrations) pairs can target
  other Nextcloud folders but never overlap the client's.
  ([#718](https://github.com/mikepadiernos/plasma-opus/issues/718))

## Implemented features

- **Accounts:** **Go › Nextcloud** lists each Nextcloud account, with **Open**
  and **Open in Other Pane** for its files, and ends with **Manage Nextcloud
  Accounts…**. Without accounts, the menu only says so and offers the dialog;
  nothing else in the window changes. The dialog lists the accounts and adds
  one with [Login Flow
  v2](https://docs.nextcloud.com/server/latest/developer_manual/client_apis/LoginFlow/index.html):
  enter the server address and choose **Sign In…**, then sign in and grant
  access in the web browser that opens, or cancel while Plasma Opus waits.
  - **Credentials:** the app password that Login Flow creates goes to KDE's
    password server, `kpasswdserver`, which keeps it in KDE Wallet when the
    wallet is enabled. The `NextcloudAccounts` group of the Plasma Opus
    settings saves only the server address, user and login name. Signing in
    to an existing account again replaces its app password.
  - **Online Accounts:** Nextcloud and ownCloud accounts from
    [KDE Online Accounts](kde-plasma.md#existing-foundations) are listed too,
    after opting in under **Preferences › KDE Plasma › Online Accounts**.
    They are managed in System Settings and can't be removed here; an account
    added in both places is listed once.
  - **Check:** **Check** reports the server's product name and version, the
    signed-in user, and whether sharing, collaborative tags, Photos and file
    search are available. It also reports expired or revoked access,
    maintenance mode, unreachable servers and certificate problems.
  - **Remove:** **Remove…** asks for confirmation and offers to revoke the app
    password on the server as well; otherwise the dialog explains how to
    revoke it later in Nextcloud.
  - **Security:** Plasma Opus refuses unencrypted `http` addresses, except for
    servers on the same computer, and addresses that contain a user name or
    password. The dialog's requests verify the server's certificate and fail
    with an error rather than offering to accept an invalid one; panes use
    KIO's usual certificate prompt. Login Flow is accepted
    only when the server's polling address is encrypted and on the same
    server.
  - **Server interfaces:** `ocs/v2.php/cloud/capabilities` and
    `ocs/v2.php/cloud/user` with `OCS-APIRequest: true` and `format=json`,
    `index.php/login/v2`, `ocs/v2.php/core/apppassword` to revoke, and a
    `PROPFIND` on `remote.php/dav/photos/<user>/` to detect Photos. Files
    open at `webdavs://<login>@<server>/remote.php/dav/files/<user>/` through
    KIO.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#588](https://github.com/mikepadiernos/plasma-opus/issues/588))

- **Browsing:** **Open** and **Open in Other Pane**, in **Go › Nextcloud** or
  the **Manage Nextcloud Accounts** dialog, show the account's files in either
  pane. Copying, moving, renaming, deleting, creating and pasting use the same
  KIO jobs as other remote folders, with their confirmations, progress,
  cancellation and error reports. When the server refuses an operation, or
  some items of a larger one fail, KIO reports the error for each item.
  - **Permissions:** after listing a folder, Plasma Opus reads the
    `oc:permissions` of the folder and its items. It disables **Rename…**,
    **Delete Permanently**, **Cut** and **Move to Other Pane** for selected
    items that the server doesn't allow renaming, deleting or moving. It
    disables **Paste**, **Create New** and **New Folder…** in a folder that
    doesn't allow adding files or folders, and **Copy to Other Pane** and
    **Move to Other Pane** when the other pane shows such a folder. The tooltip
    of a disabled action gives the reason, and the status bar reports a folder
    that doesn't allow adding items. When the permissions can't be read,
    nothing is disabled and the server's own errors apply.
  - **Server interfaces:** KIO WebDAV on `remote.php/dav/files/<user>/`, and a
    `PROPFIND` of depth 1 for `oc:permissions`.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#365](https://github.com/mikepadiernos/plasma-opus/issues/365))

- **Previews:** with **View › Preview Panel** shown, a file in a Nextcloud
  account shows the server's preview image, sized to the panel, instead of
  downloading the file. The footer says **Preview from the server**, and
  **Download Full Preview** downloads the file within the usual preview
  limits, for example to play a video or read every page. Folders, text
  files, files the server can't preview and previews over 8 MiB are
  downloaded as before, and formats whose previews are turned off aren't
  requested.
  - **Server metadata:** **View › Information Panel** adds a **Nextcloud
    server** section for an item in a Nextcloud account: its file ID, owner,
    size, modification time, ETag, favourite, server tags, share types, mount
    type, whether the server has a preview, and photo metadata such as the
    camera, EXIF values, location and capture date, where the server provides
    them. Server tags are labelled apart from Plasma Opus tags. The section
    lists properties the server withholds and shows errors in place of the
    values. Metadata is read only while the panel is shown, and read again
    after a minute or when the item changes.
  - **Server interfaces:** a `PROPFIND` of depth 0 on the item for its `d:`,
    `oc:` and `nc:` properties, including `nc:has-preview` and
    `nc:metadata-photos-*`, and
    `index.php/core/preview?fileId=<id>&x=<size>&y=<size>&a=1` for previews,
    from 64 to 2,048 pixels.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#589](https://github.com/mikepadiernos/plasma-opus/issues/589))

- **Search and organize:** **Go › Nextcloud › *account* › Search and
  Organize…** searches the account's files on the server by name, kind and
  favourites, lists your favourites, or lists the files with a server tag.
  Results show the folder, modification time, favourite and server tags, newest
  first, up to 500 at a time. **Show in Pane**, or a double-click, opens a
  folder or reveals a file. The context menu of items in one Nextcloud account
  adds a **Nextcloud** submenu with the same dialog.
  - **Favourites:** **Add to Nextcloud Favourites** and **Remove from Nextcloud
    Favourites** in the context menu, or the favourite button in the dialog,
    change the server's favourites, which Nextcloud's web interface and apps
    show. They are separate from the **Places** panel.
  - **Tags:** **Nextcloud Tags…** assigns or removes the server's
    collaborative tags. A tag on only some of the items is partly checked, and
    a tag you can't assign is disabled. **Create Tag…** asks for confirmation
    before creating a tag, because the server creates it at once and shows it
    to every user. Choosing **OK** saves only the changes, and lists any that
    fail.
  - **Separate from local tags:** server tags are labelled **Nextcloud tags
    (server)** and kept apart from [Plasma Opus tags](../tagging.md); they
    aren't stored locally or listed in the **Tag Panel**.
  - **Availability:** search needs WebDAV search, from Nextcloud 15, and tags
    need the Collaborative tags app; the dialog disables a feature the server
    lacks and says why. The status line says when only the first 500 results
    are listed, when the server withheld results or properties, and which
    items couldn't be changed.
  - **Server interfaces:** `SEARCH` with `d:basicsearch` on `remote.php/dav/`
    scoped to `/files/<user>`, `REPORT` with `oc:filter-files` for favourites
    and tags, `PROPPATCH` of `oc:favorite`, `PROPFIND` of
    `remote.php/dav/systemtags/` and
    `remote.php/dav/systemtags-relations/files/<fileid>/`, `PUT` and `DELETE`
    of `…/<fileid>/<tagid>`, and a JSON `POST` to `remote.php/dav/systemtags`
    to create a tag.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#590](https://github.com/mikepadiernos/plasma-opus/issues/590))

- **Photos:** **Go › Nextcloud › *account* › Photos…** opens a window that
  lists the account's Nextcloud Photos **Albums**, **Shared albums** or
  **Places**, chosen under **Show**, as virtual folders; see
  [Photos](#photos). A collection's tooltip gives its photo count, location,
  dates and collaborators. Selecting one lists its photos with the capture
  date and place the server read from them, without downloading the files.
  **Sort** orders them newest or oldest first, by name or by place, and
  **Group by** groups them by the month taken or by place; the window
  remembers both. Photos without a capture date or place come last, and the
  status line counts those without a capture date. Selecting a photo shows the
  server's preview, and **Show in Pane**, or a double-click, reveals the file
  in the active pane when it is in your own files.
  - **Albums:** **New Album…** creates an album. **Remove from Album…** asks
    for confirmation, saying that the files aren't deleted, then deletes only
    the album's entry for each photo; Plasma Opus refuses to send a `DELETE`
    for anything but a photo directly in an album. The context menu of files,
    not folders, in a Nextcloud account adds **Nextcloud › Add to Nextcloud
    Album…**, which lists your albums and the shared albums that accept
    photos, and adds the files after confirmation. The question says that the
    files stay where they are and, for a shared album, that everyone it is
    shared with will see them.
  - **Permissions and availability:** a shared album whose owner doesn't
    allow adding photos is left out of **Add to Nextcloud Album…**, and one
    that doesn't allow removing them disables **Remove from Album…**, with the
    reason as its tooltip. Places are read-only server groupings. When the
    server has no Photos app, the window says so and disables **New Album…**.
  - **Server interfaces:** a `PROPFIND` of depth 1 on
    `remote.php/dav/photos/<user>/albums/`, `sharedalbums/` and `places/`,
    falling back to `locations/`, and on each collection, for `nc:nbItems`,
    `nc:location`, `nc:dateRange`, `nc:collaborators`, `oc:permissions`,
    `nc:file-name`, `nc:realpath` and `nc:metadata-photos-*`; `MKCOL` of
    `albums/<name>`; `COPY` from `remote.php/dav/files/<user>/…` to
    `albums/<name>/<file>` with `Overwrite: F`; and `DELETE` of
    `albums/<name>/<entry>` or `sharedalbums/<name>/<entry>`.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#671](https://github.com/mikepadiernos/plasma-opus/issues/671))

- **Shares:** **Go › Nextcloud › *account* › Shares…** lists the shares you
  made, under **Shared by me**, or those others made with you, under **Shared
  with me**; the window remembers which. For a single file or folder in a
  Nextcloud account, the context menu's **Nextcloud › Nextcloud Shares…**
  starts with that item's shares, including reshares by others. Each share
  shows the item, who it is with, its access, expiry and whether a password
  protects it; **Show in Pane** opens a folder or reveals a file.
  - **Create:** **Create Public Link…** and **Share with User or Group…** ask
    for the access, an expiry and, for links, a password, then ask for
    confirmation with who will get which access and for how long. The editor
    enforces the server's required link password and expiry, its maximum
    expiry and whether links may accept uploads; a folder link with only
    **Upload** is a file drop. User and group IDs must be typed; names aren't
    suggested.
  - **Change and revoke:** **Change…** edits the access, expiry and link
    password. It asks for confirmation only when the change gives more
    access: more permissions, a later or no expiry, or no password. Narrowing
    a share is applied at once. **Revoke…** always asks first. Shares with
    you are read-only, and the server's `can_edit` and `can_delete` disable
    the buttons for the shares it won't let you change. Other kinds of share,
    such as email, federated or Talk shares, are listed and can be revoked,
    but are changed in the web interface.
  - **Passwords:** a share password is sent to the server once and never
    stored, shown or written to the message; the list only says whether a
    share has one. Password hashes in the server's responses are discarded.
  - **Send by Email:** **Copy Link** copies a public link. When **Offer Send
    by Email for selected files** is on, **Send Link by Email…** first shows
    the link, its access and expiry for review, and that a password must be
    sent separately, then opens a new message with the link in your mail
    client, or through the desktop portal in a sandbox, for you to complete.
  - **Availability:** the window reports when the server doesn't offer
    sharing or has it turned off, and disables creating shares, or public
    links alone when the server turns them off.
  - **Server interfaces:** OCS at
    `ocs/v2.php/apps/files_sharing/api/v1/shares` with `OCS-APIRequest: true`
    and `format=json`: `GET` with `path` and `reshares=true` or
    `shared_with_me=true`, form-encoded `POST` to create, `PUT` of
    `shares/<id>` to change and `DELETE` of `shares/<id>` to revoke; and the
    `files_sharing` part of `ocs/v2.php/cloud/capabilities`.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#591](https://github.com/mikepadiernos/plasma-opus/issues/591))

- **Desktop client:** when the Nextcloud desktop client is installed or
  configured, **Go › Nextcloud › Desktop Client** offers **Open Desktop
  Client**, then a submenu for each folder the client synchronizes, titled
  with where its files are and its status, such as *Documents (Local, Up to
  date)*. Each folder offers **Open**, **Open in Other Pane** and, when the
  same user and server are added as a Plasma Opus account, **Open on Server**
  for the server's copy. Its tooltip names the folder on the server, the
  account and what its state means.
  - **States:** **Local** folders hold full copies of their files. **Remote**
    folders use virtual files, whose contents stay on the server until
    opened. **Offline** means the client isn't running or the folder is
    paused, so changes wait until it resumes. **Unknown** means the client
    didn't report a status or the folder is missing. The status is **Up to
    date**, **Synchronizing**, **Needs attention** or **Error**, as the client
    reports it; while the client is asked, the menu shows **Checking
    synchronization status…**, and **Desktop client not running** when it
    can't be reached.
  - **Boundaries:** Plasma Opus reads the client's configuration without
    writing or locking it, never starts a second sync engine and never
    changes the client's folders or settings; pause, resume and add folders
    in the client. Files browsed over WebDAV are never described as available
    offline. Two-way cloud sync still refuses pairs that overlap the client's
    folders.
  - **Interfaces:** the client's `Nextcloud/nextcloud.cfg` under the
    configuration folder, read for the `[Accounts]` entries
    `N\Folders\M\localPath`, `targetPath`, `virtualFilesMode` and `paused`,
    and its local socket `Nextcloud/socket` under `XDG_RUNTIME_DIR`, asked
    with `RETRIEVE_FILE_STATUS:<path>` and answered with
    `STATUS:<status>:<path>`. The client is started from its desktop entry.

  See [Nextcloud integration](../CAVEATS.md#nextcloud-integration).
  ([#592](https://github.com/mikepadiernos/plasma-opus/issues/592))

## Planned features

| Feature         | Plan                                                                                                                                                        | Milestone                                   |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------- |
| Settings        | Account references, browsing and preview choices, asset metadata and search, sharing and desktop-client integration, stored in KConfig without credentials. | [M30](../ROADMAP.md#m30-nextcloud-settings) |
| Photos settings | Show or hide albums, shared albums and places, choose their default grouping and whether to confirm album changes.                                          | [M30](../ROADMAP.md#m30-nextcloud-settings) |

## Nextcloud app interfaces

| App                | Interface                                                                                                                                                                                                                                                                  |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Files              | WebDAV at `remote.php/dav/files/<user>/`, used by M29.                                                                                                                                                                                                                     |
| Photos             | WebDAV at `remote.php/dav/photos/<user>/` with `albums`, `sharedalbums` and `places` collections, or `locations` in some versions. Server metadata includes `nc:metadata-photos-original_date_time`, `nc:metadata-photos-place` and `nc:metadata-photos-gps`. Used by M29. |
| Calendar and Tasks | CalDAV. Calendar stores event attachments as ordinary files, in `/Calendar` by default.                                                                                                                                                                                    |
| Maps               | Custom maps are folders, `/Maps` by default or any folder containing `.index.maps`, holding photos, `.gpx` tracks, contact vCards and `.favorites.json`.                                                                                                                   |
| Deck               | REST API at `index.php/apps/deck/api/v1.0` for boards, stacks, cards and labels. Since Deck 1.3, card attachments are ordinary files.                                                                                                                                      |
| Notes              | Markdown files in the folder reported by the `notes_path` capability (Notes 4.12 and later), `Notes` by default, with subfolders as categories.                                                                                                                            |
| Text               | Rich workspaces show a folder's `Readme.md` above the Files listing.                                                                                                                                                                                                       |
| Versions and trash | WebDAV at `remote.php/dav/versions/<user>/` and `remote.php/dav/trashbin/<user>/`.                                                                                                                                                                                         |

> [!CAUTION]
> Photos, Calendar, Maps, Deck and Notes were reviewed against their source or
> documentation in November 2026; the Text, Versions and trash entries are based
> on Nextcloud's documentation and still need verification. App interfaces
> change between releases; verify them before implementation.

## Photos

Implemented in [M29](../ROADMAP.md#m29-nextcloud-integration) as the
**Photos…** window described under
[Implemented features](#implemented-features), with settings planned in
[M30](../ROADMAP.md#m30-nextcloud-settings):

- Albums, shared albums and places are browsed as virtual folders in their
  own window rather than in a pane, because albums reference files without
  copying them. The window says so, and **Show in Pane** reveals each photo's
  real location.
- Photos are sorted and grouped by the server's capture date and place without
  downloading them. Photos without a capture date or place are listed last and
  counted; the server's modification date is only shown in a tooltip.
- Adding files to an album, creating an album and removing photos from an
  album each need an explicit action, and adding or removing needs
  confirmation. Removing a photo from an album never deletes the file, and
  the question says so; deleting a file remains a separate, explicit Files
  action.
- Shared-album permissions are respected, and places are read-only server
  groupings. The window reports when the Photos app is unavailable.
- **Not yet:** renaming albums, browsing albums in a pane, and, **with Plasma
  Impresario**, sending selected album photos through the M27 **Send by
  Email** action with the location warning described for the
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

### Rejected approaches

- Replacing the Nextcloud desktop client or starting a second sync engine for
  its folders.
- Implementing a CalDAV client instead of handing events to KDE's calendar
  application.
- App-specific APIs where ordinary file access can express the workflow.
- Geocoding addresses or uploading locations from Plasma Opus.

## Acceptance

Test against a disposable Nextcloud server with synthetic accounts and data;
never use a real account in automated tests. Verify missing or disabled apps,
older app versions, read-only shares and shared albums, expired credentials,
offline servers, concurrent edits and cancellation. File workflows must keep
working when no optional Nextcloud app is installed.
