# 🖥️ KDE Plasma integration

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Plasma Impresario integration](plasma-impresario.md)** ·
**[Nextcloud integration](nextcloud.md)**

## Status and principles

Plasma Opus is KDE-native. The roadmap is authoritative for implementation
status: the foundations listed below are completed milestones, while the
additional integrations are planned or proposed, not already available.

Use KDE Frameworks and the user's active Plasma configuration instead of
reimplementing shared services. Keep KDE-owned accounts, credentials,
permissions, notifications and desktop preferences under their owner's control.
Optional services must fail visibly when unavailable and must not prevent
ordinary local file management.

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

## Planned integrations

- **File-manager service and portal:** Let other applications reveal files or
  request a chooser through supported desktop interfaces. Preserve user defaults
  and sandbox permissions; the chooser contract still needs definition.
  ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279),
  [#286](https://github.com/mikepadiernos/plasma-opus/issues/286))
- **Baloo and metadata:** Show indexing status and supported metadata search,
  with explicit live-search fallback and honest coverage limits. Never enable
  indexing or change shared KDE settings silently.
  ([#441](https://github.com/mikepadiernos/plasma-opus/issues/441),
  [#442](https://github.com/mikepadiernos/plasma-opus/issues/442))
- **Activities and workspaces:** Offer optional workspace associations and
  per-Activity choices without changing Activities or replacing unsaved state
  automatically. Handle missing services and folders.
  ([#387](https://github.com/mikepadiernos/plasma-opus/issues/387))
- **Notifications and jobs:** Surface Plasma Opus-owned operation progress and
  failures through KDE, reusing KIO reporting rather than duplicating
  notifications. Avoid exposing sensitive paths unnecessarily.
  ([#388](https://github.com/mikepadiernos/plasma-opus/issues/388))
- **KDE Online Accounts:** Discover supported file services and reuse KDE
  credentials and KIO providers. Do not duplicate account management or store
  tokens in URLs or KConfig.
  ([#370](https://github.com/mikepadiernos/plasma-opus/issues/370))
- **KDE Connect:** Share explicitly selected files with a paired device,
  reporting offline devices and transfer failures. Do not pair devices or send
  automatically.
  ([#389](https://github.com/mikepadiernos/plasma-opus/issues/389))
- **KRunner:** Expose only explicitly selected saved workspaces and locations,
  respecting privacy and unavailable targets.
  ([#390](https://github.com/mikepadiernos/plasma-opus/issues/390))
- **Tray and shortcuts:** Offer optional tray behavior and KDE-managed shortcuts
  with conflict reporting; do not replace existing bindings.
  ([#285](https://github.com/mikepadiernos/plasma-opus/issues/285),
  [#230](https://github.com/mikepadiernos/plasma-opus/issues/230))
- **Service-menu discovery:** Reuse KDE's service-menu provider for explicit
  install, update and removal actions; explain that installed services may
  execute commands and be shared with other applications.
  ([#369](https://github.com/mikepadiernos/plasma-opus/issues/369))
- **Privileged locations:** Explore supported KIO `admin:/` operations with
  explicit authorization and clear failure reporting, not a persistent elevated
  application session.
  ([#288](https://github.com/mikepadiernos/plasma-opus/issues/288))
- **Plasma Vault and encryption:** Open, close and reveal Plasma Vault
  containers through its D-Bus interface and reuse GnuPG and Kleopatra for file
  encryption. Leave passwords and vault configuration to those tools.
  ([M27](../ROADMAP.md#m27-integrations))
- **Disk images and network locations:** Reuse Dolphin's Mount ISO action,
  Solid, and KIO's SMB, FTP and `network:/` workers rather than custom mount or
  discovery code. ([M27](../ROADMAP.md#m27-integrations))
- **Mounts and remote drives:** List, mount, unmount and eject file systems
  through Solid and UDisks2, and mount remote locations as local folders through
  KIO FUSE or installed FUSE backends such as sshfs and davfs2. Keep credentials
  in KDE's facilities, ask before changing fstab or backend configuration, and
  unmount only what Plasma Opus mounted.
  ([#702](https://github.com/mikepadiernos/plasma-opus/issues/702),
  [#703](https://github.com/mikepadiernos/plasma-opus/issues/703))
- **Cloud sync and activity:** Keep chosen local folders in two-way sync with
  KIO, KDE Online Accounts or rclone locations, with backup modes and conflict
  choices that never overwrite changes silently, and show transfers, quota,
  pending uploads and cache limits. Keep credentials in KDE's facilities and
  stay clear of folders owned by the Nextcloud desktop client.
  ([#718](https://github.com/mikepadiernos/plasma-opus/issues/718),
  [#719](https://github.com/mikepadiernos/plasma-opus/issues/719))
- **KWin scripts:** Optionally install a Plasma Opus KWin script for window
  placement that Wayland applications cannot perform themselves, and offer a
  drop-down window. See [KWin scripts](#kwin-scripts).
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707),
  [#708](https://github.com/mikepadiernos/plasma-opus/issues/708))
- **Nextcloud:** Reuse KDE accounts, credentials and KIO for optional native
  browsing and asset workflows; leave sync ownership with the desktop client.
  ([M29](../ROADMAP.md#m29-nextcloud-integration),
  [Nextcloud integration](nextcloud.md))

Settings for the general desktop integrations belong in
[M28](../ROADMAP.md#m28-integration-settings); Nextcloud settings belong in
[M30](../ROADMAP.md#m30-nextcloud-settings). Link to shared KDE controls rather
than creating conflicting settings.

## KWin scripts

[KWin scripts](https://develop.kde.org/docs/plasma/kwin/) are JavaScript or QML
packages that run inside KWin. On Wayland, applications cannot choose their own
screen, position, virtual desktop or tile, so an optional Plasma Opus script can
do this for Plasma Opus windows. Plasma Opus would install it per user as a
`KWin/Script` KPackage, enable it through KWin's configuration and talk to it
over D-Bus.

### Planned

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

### Proposed, not yet on the roadmap

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

### Boundaries

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

## Validation

Exercise supported Plasma and Qt/KDE versions, KWin script compatibility,
unavailable services, remote locations, sandbox and portal access, permission
failures and cancellation. Use temporary test configuration and synthetic
credentials. Verify that disabling an optional integration preserves existing
files, layouts, accounts, window rules and desktop settings.
