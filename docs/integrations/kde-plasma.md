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
  Application-specific themes are in development in
  [M17](../ROADMAP.md#m17-themes), with their settings planned in
  [M18](../ROADMAP.md#m18-theme-settings).

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
- **Nextcloud:** Reuse KDE accounts, credentials and KIO for optional native
  browsing and asset workflows; leave sync ownership with the desktop client.
  ([M29](../ROADMAP.md#m29-nextcloud-integration),
  [Nextcloud integration](nextcloud.md))

Settings for the general desktop integrations belong in
[M28](../ROADMAP.md#m28-integration-settings); Nextcloud settings belong in
[M30](../ROADMAP.md#m30-nextcloud-settings). Link to shared KDE controls rather
than creating conflicting settings.

## Validation

Exercise supported Plasma and Qt/KDE versions, unavailable services, remote
locations, sandbox and portal access, permission failures and cancellation. Use
temporary test configuration and synthetic credentials. Verify that disabling an
optional integration preserves existing files, layouts, accounts and desktop
settings.
