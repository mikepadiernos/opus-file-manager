# Inspirations

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Previous: Scope](https://img.shields.io/badge/previous-Scope-3daee9?style=flat-square)](scope.md)
[![Next: Interface design](https://img.shields.io/badge/next-Interface%20design-3daee9?style=flat-square)](designs.md)
[![Roadmap](https://img.shields.io/badge/roadmap-M01%E2%80%93M32-8957e5?style=flat-square)](../ROADMAP.md)

The file managers and tools whose ideas shape Plasma Opus, and the milestones
they inform. Each reference contributes ideas; none dictates the application's
interface or implementation.

**Philosophy series:** [1. Scope](scope.md) → **2. Inspirations** →
[3. Interface design](designs.md)

> [!NOTE]
> The [roadmap](../ROADMAP.md) is the source of truth for feature status.
> Ideas described here are not necessarily implemented; the status column
> below shows the state of the related roadmap work.

## Contents

- [At a glance](#at-a-glance)
- [Max File Manager — origin and behaviour reference](#max-file-manager--origin-and-behaviour-reference)
- [Dolphin — KDE desktop model](#dolphin--kde-desktop-model)
- [Directory Opus — power-user benchmark](#directory-opus--power-user-benchmark)
- [Krusader — two-pane tools](#krusader--two-pane-tools)
- [MiXplorer — mobile power tools](#mixplorer--mobile-power-tools)
- [RaiDrive — cloud storage as drives](#raidrive--cloud-storage-as-drives)
- [Syncative, SDrive and DucksDrive — cloud sync and activity](#syncative-sdrive-and-ducksdrive--cloud-sync-and-activity)
- [Strata — system file chooser](#strata--system-file-chooser)
- [Luciole — interaction details](#luciole--interaction-details)
- [Lektra — document viewer and keyboard control](#lektra--document-viewer-and-keyboard-control)
- [Orcinus — power-user workflows for Dolphin](#orcinus--power-user-workflows-for-dolphin)
- [TagSpaces — focused workspaces](#tagspaces--focused-workspaces)
- [TagStudio — native tagging](#tagstudio--native-tagging)
- [Yazi — keyboard-first browsing](#yazi--keyboard-first-browsing)
- [which-key.nvim — shortcut discovery](#which-keynvim--shortcut-discovery)

## At a glance

| Source                                                                                        | Contributes                                                    | Milestones                                        | Related work                                                                                                                                                          |
| --------------------------------------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [Max File Manager](#max-file-manager--origin-and-behaviour-reference)                         | Origin and primary behaviour reference                         | [Parity notes](../README.md#behaviour-references) | ![Reference](https://img.shields.io/badge/Reference-6e7781?style=flat-square)                                                                                         |
| [Dolphin](#dolphin--kde-desktop-model)                                                        | KDE desktop model and shared components                        | M01 onwards                                       | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                                               |
| [Directory Opus](#directory-opus--power-user-benchmark)                                       | Power-user depth, layout, shortcuts and help                   | M03, M09, M11; M31, M32                           | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                     |
| [Krusader](#krusader--two-pane-tools)                                                         | Two-pane tools, transfer queue, folder sync and mount manager  | M03, M09; M27                                     | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                     |
| [MiXplorer](#mixplorer--mobile-power-tools)                                                   | Encryption, LAN servers, cloud, archives, menu customization   | M27, M32                                          | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |
| [RaiDrive](#raidrive--cloud-storage-as-drives)                                                | Remote storage mounted as local folders                        | M27                                               | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |
| [Syncative, SDrive and DucksDrive](#syncative-sdrive-and-ducksdrive--cloud-sync-and-activity) | Two-way cloud sync and cloud activity                          | M27                                               | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |
| [Strata](#strata--system-file-chooser)                                                        | Opt-in system file chooser                                     | M27                                               | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |
| [Luciole](#luciole--interaction-details)                                                      | Interaction details, archive checks, operations and thumbnails | M28, M32                                          | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |
| [Lektra](#lektra--document-viewer-and-keyboard-control)                                       | Command palette, keyboard hints and document comparison        | M31, M32                                          | ![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square) ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)           |
| [Orcinus](#orcinus--power-user-workflows-for-dolphin)                                         | Per-pane styles, expandable folders, filters, linked panes     | M03, M09                                          | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Source unverified](https://img.shields.io/badge/source-unverified-bf8700?style=flat-square) |
| [TagSpaces](#tagspaces--focused-workspaces)                                                   | Workspaces that scope locations and search                     | M15                                               | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                                               |
| [TagStudio](#tagstudio--native-tagging)                                                       | Native tag libraries                                           | M23                                               | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                                               |
| [Yazi](#yazi--keyboard-first-browsing)                                                        | Keyboard-first browsing mode                                   | M25                                               | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                                               |
| [which-key.nvim](#which-keynvim--shortcut-discovery)                                          | Shortcut hint pane                                             | M31                                               | ![Planned](https://img.shields.io/badge/Planned-6e7781?style=flat-square)                                                                                             |

## Max File Manager — origin and behaviour reference

Plasma Opus began with the idea of forking Max File Manager, then evolved into a
native KDE application rather than a continuation of Max's Electron
architecture. Max remains the primary reference for user-facing behaviour. The
[UI](../parity/ui.md) and [backend](../parity/backend.md) inventories record a
reviewed Max checkout, not the current state of Plasma Opus.

## Dolphin — KDE desktop model

Dolphin guides how Plasma Opus fits into Plasma: using the desktop's icon theme,
settings, service menus and file-management services. Plasma Opus prefers
existing KDE components, including KIO for file operations, `KFilePlacesView`
for Places, `KFileItemActions` for service actions, `KPropertiesDialog` for
properties and KConfig for preferences. It offers an alternative workflow
without replacing or masking Dolphin.

## Directory Opus — power-user benchmark

Directory Opus sets the benchmark for power-user depth: dual panes, tabs,
viewers, flat browsing, automation and metadata. Its workflows inform
[folder formats and navigation history](../ROADMAP.md#m03-listing-depth),
[collections and duplicate selection](../ROADMAP.md#m09-tools),
[layout customization](../ROADMAP.md#m11-ui-customizations) and the
[shortcut settings](../ROADMAP.md#m31-keyboard-shortcut-settings), which
summarize customized and conflicting shortcuts around KDE's editor and list
every key in a searchable, printable keyboard map; see
[Keyboard shortcuts](../keyboard.md). Its
[manual](https://docs.dopus.com/doku.php?id=introduction) also informs planned
[interface refinements](../ROADMAP.md#m32-refinements), such as contextual help,
filter visibility and discoverable customization. These are adapted to Linux and
KDE rather than treated as a feature-by-feature parity target.

## Krusader — two-pane tools

Krusader shows how a two-panel file manager can support deeper file operations.
Its influence includes group selection, occupied space, popular folders and
archive testing ([M03](../ROADMAP.md#m03-listing-depth)), and comparison,
checksums, split and combine, and an embedded terminal alongside both panes
([M09](../ROADMAP.md#m09-tools)). Copy and move transfers can be queued; folder
synchronization previews filtered, per-item actions before applying them. Its
[MountMan](https://docs.kde.org/trunk_kf6/en/krusader/krusader/mount-man.html)
informs a planned [mount manager](../ROADMAP.md#m27-integrations) that lists,
mounts and unmounts file systems through Solid.

## MiXplorer — mobile power tools

[MiXplorer](https://mixplorer.com/) packs desktop-class tools into an Android
file manager. Its encrypted containers, file encryption, LAN servers, cloud
providers, archive editing and file-type categories inform planned
[integrations](../ROADMAP.md#m27-integrations); its menu customization informs
the planned context-menu reordering in
[M32.01 Menus](../ROADMAP.md#m32-refinements). Plasma Opus adapts them to KDE
facilities, such as Plasma Vault, KIO workers, Ark and KDE's credential
storage, rather than reimplementing them, and does not copy MiXplorer's mobile
interface.

## RaiDrive — cloud storage as drives

[RaiDrive](https://www.raidrive.com/) mounts cloud, S3-compatible and NAS
storage over WebDAV, SFTP and FTP as local drives, so any application can open
and save files in place. It also offers read-only connections, streamed playback
and file locks. It informs the planned
[remote drives](../ROADMAP.md#m27-integrations): Plasma Opus would mount saved
remote locations as local folders through KDE's KIO FUSE or installed backends
such as sshfs, davfs2 and s3fs-fuse, as alternatives to rclone. Accounts and
credentials stay with KDE's facilities rather than a separate provider list.

## Syncative, SDrive and DucksDrive — cloud sync and activity

[Syncative](https://github.com/Santuzius/syncative) keeps any number of local
folders in two-way sync with Google Drive, Proton Drive or Nextcloud. It offers
one-way cloud and local backup modes, exclusion filters, and per-file conflict
choices that never overwrite a file changed on both sides.
[SDrive](https://github.com/TitanniOpen/SDrive) and
[DucksDrive](https://patopatetico2-lab.github.io/ducksdrive/) map cloud and
remote storage as local drives and show what they are doing: connection state,
quota, pending uploads and transfer speed, with bandwidth and cache limits and a
safe disconnection that waits for uploads to finish. They inform the planned
[two-way cloud sync and cloud activity panel](../ROADMAP.md#m27-integrations),
which reuse KIO, KDE accounts, M09 sync filters and the existing transfer queue,
and stay clear of folders owned by the Nextcloud desktop client.

## Strata — system file chooser

[Strata](https://github.com/lgse/strata) can serve as the desktop's Open and
Save dialog through the XDG file chooser portal, as an explicit opt-in. Its
setup informs the optional
[system file chooser](../ROADMAP.md#m27-integrations) planned for Plasma Opus:
users opt in through settings, the installer or a command-line option, KDE's
chooser remains the fallback, and the previous chooser can be restored the
same ways.

## Luciole — interaction details

[Luciole](https://github.com/noctalia-dev/luciole) is a Wayland file manager
that draws its own interface instead of using Qt or KDE, so Plasma Opus adapts
its ideas rather than its code. Its attention to small workflow details
informs planned
[interaction refinements](../ROADMAP.md#m32-refinements): selecting the next
item after deleting, sorting hidden items last, moving dropped items into a
new folder, dragging a tab into a new window, marking partial folder sizes
and an operations panel with retry and reports. Its archive checks inform
free-space estimates before extraction and browsing single compressed files,
and its thumbnail policy informs treating network FUSE mounts as remote and
clearing the thumbnail cache. Its file picker's remembered start folder
informs the [system file chooser](../ROADMAP.md#m28-integration-settings)
settings.

## Lektra — document viewer and keyboard control

[Lektra](https://github.com/dheerajshenoy/lektra) is a fast, keyboard-driven
document and image viewer built with Qt 6 and MuPDF. Plasma Opus already
previews PDFs, ebooks, comics and DjVu through Okular's viewer component, so
Lektra informs workflows rather than replacing that preview. Its command
palette informs the [command palette](../keyboard.md#command-palette), built on
KDE's `KCommandBar` as in Dolphin and Kate, which finds menu, toolbar and saved
command actions by name and runs them in the active pane. Its link hints inform
planned
[keyboard hints](../ROADMAP.md#m31-keyboard-shortcut-settings), and its
synchronized views and page-aware command line inform side-by-side image
comparison in the gallery and opening documents at the previewed page in an
external viewer, as [interaction refinements](../ROADMAP.md#m32-refinements).

Lektra and MuPDF are licensed under the AGPL, while Plasma Opus is GPL-3.0.
Plasma Opus therefore adapts Lektra's ideas, not its code, and launches it only
as a separate, optional application.

## Orcinus — power-user workflows for Dolphin

Earlier project notes attribute several Dolphin-oriented power-user ideas to
Orcinus: grouped settings, per-pane display styles, expandable folders,
persistent filters, linked panes, cushion treemaps and profiles. The roadmap
credits Orcinus for the per-pane display styles, expandable folders, persistent
filters and file-name highlighting in [M03](../ROADMAP.md#m03-listing-depth)
and for linked panes and the cushion treemap in [M09](../ROADMAP.md#m09-tools).
An authoritative upstream source has not been verified for these entries.

## TagSpaces — focused workspaces

[TagSpaces workspaces](https://docs.tagspaces.org/workspaces/) group locations
by context and scope global searches to the active workspace. Plasma Opus
[workspaces](../ROADMAP.md#m15-workspaces) also save pane, toolbar and panel
layouts for different workflows. Workspaces organize the interface and
search scope; native tag groups remain independent in
[native tagging](../ROADMAP.md#m23-native-tagging). Workspaces are not a
security boundary.

## TagStudio — native tagging

[TagStudio](https://github.com/TagStudioDev/TagStudio) informs the
[native tagging system](../ROADMAP.md#m23-native-tagging): opt-in libraries,
tags with aliases and parent relationships, custom fields and combined searches.
Plasma Opus integrates these workflows without requiring TagStudio or moving
files; see the [tagging guide](../tagging.md).

## Yazi — keyboard-first browsing

[Yazi](https://yazi-rs.github.io/) inspires the native
[Yazi mode](../ROADMAP.md#m25-yazi-mode) with keyboard-first navigation, folder
context and previews. Plasma Opus reuses its own KDE file operations rather
than embedding Yazi or assuming plugin compatibility; see the
[Yazi mode guide](../yazi-mode.md). The optional
[Vim-style navigation](../keyboard.md#vim-style-navigation) of
[M31](../ROADMAP.md#m31-keyboard-shortcut-settings) brings the same key
matching, counts and commands to the panes outside Yazi mode.

## which-key.nvim — shortcut discovery

[which-key.nvim](https://github.com/folke/which-key.nvim) inspires the planned
native sliding or floating hint pane in
[shortcut settings](../ROADMAP.md#m31-keyboard-shortcut-settings). It will show
available continuations and action descriptions for pending key sequences in the
applicable context.

---

← Previous: [Scope and objective](scope.md) · Next:
[Interface design](designs.md) →
