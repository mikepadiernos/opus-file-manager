# KDE Plasma integration

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Roadmap: M27 integrations](https://img.shields.io/badge/roadmap-M27%20integrations-8957e5?style=flat-square)](../ROADMAP.md#m27-integrations)
[![Roadmap: M28 integration settings](https://img.shields.io/badge/roadmap-M28%20integration%20settings-8957e5?style=flat-square)](../ROADMAP.md#m28-integration-settings)

![Milestones M27–M28](https://img.shields.io/badge/milestones-M27%E2%80%93M28-8957e5?style=flat-square)
![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)

How Plasma Opus builds on KDE Plasma today and which desktop integrations are
planned for [M27](../ROADMAP.md#m27-integrations) and
[M28](../ROADMAP.md#m28-integration-settings).

**Integration plans:** **KDE Plasma** · [Nextcloud](nextcloud.md) ·
[Plasma Impresario](plasma-impresario.md)

> [!NOTE]
> **Implemented:** the KDE foundations under
> [Existing foundations](#existing-foundations), from merged milestones
> M01–M26, and default file-manager registration, the file-manager service,
> Plasma language preferences and Plasma job reporting from M27.01.
> **Planned:** everything under [Planned features](#planned-features) and
> [KWin scripts](#kwin-scripts). The [roadmap](../ROADMAP.md) is authoritative
> for status.

## Contents

- [Goals](#goals)
- [Boundaries](#boundaries)
- [Existing foundations](#existing-foundations)
- [Planned features](#planned-features)
- [KWin scripts](#kwin-scripts)
- [Considered features](#considered-features)
- [Acceptance](#acceptance)

## Goals

Plasma Opus is KDE-native. Use KDE Frameworks and the user's active Plasma
configuration instead of reimplementing shared services, so that Plasma Opus
behaves like the rest of the desktop and follows the user's choices.

## Boundaries

- Keep KDE-owned accounts, credentials, permissions, notifications and desktop
  preferences under their owner's control.
- Optional services must fail visibly when unavailable and must not prevent
  ordinary local file management.
- Settings for the general desktop integrations belong in
  [M28](../ROADMAP.md#m28-integration-settings); Nextcloud settings belong in
  [M30](../ROADMAP.md#m30-nextcloud-settings). Link to shared KDE controls
  rather than creating conflicting settings.

## Existing foundations

- **File operations and locations:** reuse KIO jobs, `KFilePlacesView`, Solid
  and the existing file-operation confirmations for local and supported remote
  locations. See [M01](../ROADMAP.md#m01-core-dual-pane).
- **Applications and actions:** reuse `KFileItemActions`, KDE service menus,
  Open With and the active desktop's file associations. Preserve the user's
  choices and Plasma Opus-specific visibility settings.
- **Appearance and controls:** use the active icon theme, Plasma styling,
  KXmlGui, KDE toolbar and shortcut facilities, and localized strings.
  Optional app-local themes from [M17](../ROADMAP.md#m17-themes), configured
  through [M18](../ROADMAP.md#m18-theme-settings), follow Plasma's appearance
  by default and never change the desktop's global settings.
- **Default file manager:** `install.sh --default` registers Plasma Opus as the
  `inode/directory` handler through `xdg-mime`; uninstalling restores Dolphin
  when Plasma Opus is still the handler.
  ([M27.01](../ROADMAP.md#m27-integrations))
- **File-manager service:** while Plasma Opus is the default folder handler,
  it answers `org.freedesktop.FileManager1` requests, such as **Show in
  folder** in browsers and mail clients. `ShowFolders` opens each folder in a
  new tab, `ShowItems` opens one tab per parent folder with the items
  selected, and `ShowItemProperties` opens KDE's properties dialog. It never
  replaces another owner of the service. `install.sh --default` also installs
  a per-user D-Bus activation file, so a request starts Plasma Opus when no
  file manager is running; uninstalling removes it.
  ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279))
- **Language preferences:** with no interface language selected in
  **Settings › Translation › Language and Layout**, Plasma Opus follows
  Plasma's language list: the session's `LANGUAGE` variable, or the list saved
  in `plasma-localerc` when Plasma Opus starts without it. The page shows
  which language is in effect, falls back to the original English text when
  no Plasma Opus catalogue matches, and opens Plasma's Region & Language
  settings on request. Plasma Opus never changes Plasma's settings itself.
  ([#437](https://github.com/mikepadiernos/plasma-opus/issues/437))
- **Notifications and jobs:** long-running Plasma Opus operations that KIO
  does not already report appear in Plasma's job progress, with a title,
  progress, cancellation and a failure notification: checksum creation and
  verification, folder synchronization, duplicate finding, file splitting,
  batch renaming, batch image conversion and tag-library refreshes. The job
  shows file and folder names, never full paths; error details stay in the
  Plasma Opus dialog. File copies, moves and deletions keep using KIO's own
  reporting.
  ([#388](https://github.com/mikepadiernos/plasma-opus/issues/388))
- **System tray and global shortcut:** while Plasma Opus runs, its icon sits
  in the system tray's hidden section. Clicking it shows or hides the window;
  its menu also opens the home folder or the transfer queue, and quits.
  The **Show or Hide Plasma Opus** global shortcut, Meta+Alt+O by default,
  brings the window to the front, or hides it when it is already in front.
  Change or remove it under **Settings › Configure Keyboard Shortcuts** or in
  System Settings; KDE leaves it unassigned when another application already
  uses the default. The window hides to the tray only while a tray can show
  the icon. While hidden, the icon moves out of the hidden section until the
  window is shown again. Minimizing or closing to the tray is off by default;
  until the M28 settings add the choice, set `MinimizeToTray=true` or
  `CloseToTray=true` in the `[Tray]` group of `~/.config/opus-file-managerrc`.
  Closing to the tray applies only to the window's close button; **File ›
  Quit** and the tray menu's **Quit** still quit.
  ([#285](https://github.com/mikepadiernos/plasma-opus/issues/285); shortcut
  settings in [M28](../ROADMAP.md#m28-integration-settings) and
  [M31](../ROADMAP.md#m31-keyboard-shortcut-settings))

## Planned features

Grouped by the [M27](../ROADMAP.md#m27-integrations) subsection that tracks
them.

### M27.01 KDE Plasma and desktop services

- **System file chooser:** Let applications request a chooser through the XDG
  portal. Preserve user defaults and sandbox permissions.
  ([#286](https://github.com/mikepadiernos/plasma-opus/issues/286))
- **Activities and workspaces:** Offer optional workspace associations and
  per-Activity choices without changing Activities or replacing unsaved state
  automatically. Handle missing services and folders.
  ([#387](https://github.com/mikepadiernos/plasma-opus/issues/387))

- **KDE Connect:** Share explicitly selected files with a paired device,
  reporting offline devices and transfer failures. Do not pair devices or send
  automatically.
  ([#389](https://github.com/mikepadiernos/plasma-opus/issues/389))
- **KRunner:** Expose only explicitly selected saved workspaces and locations,
  respecting privacy and unavailable targets.
  ([#390](https://github.com/mikepadiernos/plasma-opus/issues/390))
- **Service-menu discovery:** Reuse KDE's service-menu provider for explicit
  install, update and removal actions; explain that installed services may
  execute commands and be shared with other applications.
  ([#369](https://github.com/mikepadiernos/plasma-opus/issues/369))
- **Mount manager:** List, mount, unmount and eject file systems through Solid
  and UDisks2. Ask before changing fstab and unmount only what Plasma Opus
  mounted.
  ([#703](https://github.com/mikepadiernos/plasma-opus/issues/703))
- **KWin scripts:** Optionally install a Plasma Opus KWin script for window
  placement that Wayland applications cannot perform themselves, and offer a
  drop-down window. See [KWin scripts](#kwin-scripts).
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707),
  [#708](https://github.com/mikepadiernos/plasma-opus/issues/708))

### M27.02 Archives and application packages

- **Disk images:** Mount and unmount ISO and other disk images through the
  Mount ISO action from Dolphin's plugins when installed, and show mounted
  images in Places through Solid, rather than custom mount code.

### M27.03 Search and indexing

- **Baloo and metadata:** Show indexing status and supported metadata search,
  with explicit live-search fallback and honest coverage limits. Never enable
  indexing or change shared KDE settings silently.
  ([#441](https://github.com/mikepadiernos/plasma-opus/issues/441),
  [#442](https://github.com/mikepadiernos/plasma-opus/issues/442))

### M27.06 Accounts, remote access and synchronization

- **KDE Online Accounts:** Discover supported file services and reuse KDE
  credentials and KIO providers. Do not duplicate account management or store
  tokens in URLs or KConfig.
  ([#370](https://github.com/mikepadiernos/plasma-opus/issues/370))
- **Network locations:** Reuse KIO's SMB, FTP and `network:/` workers rather
  than custom discovery code.
- **Remote drives:** Mount remote locations as local folders through KIO FUSE
  or installed FUSE backends such as sshfs and davfs2. Keep credentials in
  KDE's facilities, ask before changing backend configuration, and unmount only
  what Plasma Opus mounted.
  ([#702](https://github.com/mikepadiernos/plasma-opus/issues/702))
- **Cloud sync and activity:** Keep chosen local folders in two-way sync with
  KIO, KDE Online Accounts or rclone locations, with backup modes and conflict
  choices that never overwrite changes silently, and show transfers, quota,
  pending uploads and cache limits. Keep credentials in KDE's facilities and
  stay clear of folders owned by the Nextcloud desktop client.
  ([#718](https://github.com/mikepadiernos/plasma-opus/issues/718),
  [#719](https://github.com/mikepadiernos/plasma-opus/issues/719))

### M27.07 Access control and automation

- **Privileged locations:** Explore supported KIO `admin:/` operations with
  explicit authorization and clear failure reporting, not a persistent elevated
  application session.
  ([#288](https://github.com/mikepadiernos/plasma-opus/issues/288))
- **Plasma Vault and encryption:** Open, close and reveal Plasma Vault
  containers through its D-Bus interface and reuse GnuPG and Kleopatra for file
  encryption. Leave passwords and vault configuration to those tools.

### M29 Nextcloud

- **Nextcloud:** Reuse KDE accounts, credentials and KIO for optional native
  browsing and asset workflows; leave sync ownership with the desktop client.
  See [M29](../ROADMAP.md#m29-nextcloud-integration) and the
  [Nextcloud integration plan](nextcloud.md).

## KWin scripts

[KWin scripts](https://develop.kde.org/docs/plasma/kwin/) are JavaScript or QML
packages that run inside KWin. On Wayland, applications cannot choose their own
screen, position, virtual desktop or tile, so an optional Plasma Opus script can
do this for Plasma Opus windows. Plasma Opus would install it per user as a
`KWin/Script` KPackage, enable it through KWin's configuration and talk to it
over D-Bus.

### Planned script features

- **Workspace windows:** restore each window's screen, virtual desktop, size,
  tile and keep-above state when applying a workspace or Activity association.
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **Tiled companions:** place terminals, editors and detached previews opened
  from Plasma Opus beside the file manager using KWin's tiles.
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **Virtual desktops:** optionally apply the workspace assigned to the virtual
  desktop that becomes current.
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **Drop-down window:** slide a Plasma Opus window in from a screen edge, like
  [Yakuake](https://apps.kde.org/yakuake/), toggled by a global shortcut or a
  screen or touch-screen edge.
  ([#708](https://github.com/mikepadiernos/plasma-opus/issues/708))

### Script boundaries

- Install or enable nothing without explicit action. Removing the integration
  removes only the Plasma Opus script.
- Act only on Plasma Opus windows, identified by its desktop file name; never
  move, resize or close other applications' windows.
- Use KWindowSystem, XDG activation and KWin's window rules where they suffice.
  Link to KWin's Window Rules and KWin Scripts settings instead of editing
  `kwinrulesrc`, other scripts or global KWin settings.
- Version the D-Bus interface and check the installed script's version. KWin's
  scripting API changes between Plasma releases, so test each supported
  version.
- Keep the script passive while Plasma Opus is not running. When KWin is not
  the compositor or scripting is disabled, report the integration as
  unavailable and keep ordinary window behaviour.

## Considered features

These KWin script ideas are **not on the [roadmap](../ROADMAP.md)**. Add a
feature to the roadmap, with its issue, before implementing it.

- **Dual-window mode:** open the second pane as its own window, tiled beside the
  first or on another screen, keeping source and destination behaviour.
- **Picture-in-picture previews:** keep a detached video or image preview above
  other windows and on all virtual desktops.
- **Workspace window appearance:** apply window opacity or hide the title bar as
  part of a theme attached to a workspace, alongside M17 blur.
- **Raise existing windows:** bring forward the Plasma Opus window already
  showing a requested folder instead of opening a duplicate, where XDG
  activation alone is not enough.
- **Transfer focus:** keep the file manager in view while a long copy or move
  runs, and briefly highlight its window when the operation needs attention,
  without stealing focus.

### Rejected approaches

- Reimplementing shared KDE services, such as accounts, credentials or
  notifications, instead of reusing KDE Frameworks.
- Custom mount or network-discovery code instead of Dolphin's Mount ISO action,
  Solid and KIO workers.
- A persistent elevated application session instead of KIO's `admin:/` worker.
- Storing tokens in URLs or KConfig, or duplicating KDE account management.
- Editing `kwinrulesrc`, other scripts or global KWin settings instead of
  linking to KWin's own settings.

## Acceptance

Exercise supported Plasma and Qt/KDE versions, KWin script compatibility,
unavailable services, remote locations, sandbox and portal access, permission
failures and cancellation. Use temporary test configuration and synthetic
credentials. Verify that disabling an optional integration preserves existing
files, layouts, accounts, window rules and desktop settings.
