# Inspirations

Opus combines the dual-pane workflow and power-user features of several file
managers with KDE's native desktop integration. Each reference contributes
different ideas; none dictates Opus's interface or implementation.

The [roadmap](../ROADMAP.md) is the source of truth for feature status. Ideas
described here are not necessarily implemented.

## Max File Manager — origin and behaviour reference

Opus began with the idea of forking Max File Manager, then evolved into a
native KDE application rather than a continuation of Max's Electron
architecture. Max remains the primary reference for user-facing behaviour.
The [UI](../parity/ui.md) and [backend](../parity/backend.md) inventories
record a reviewed Max checkout, not the current state of Opus.

## Dolphin — KDE desktop model

Dolphin guides how Opus fits into Plasma: using the desktop's icon theme,
settings, service menus and file-management services. Opus prefers existing KDE
components, including KIO for file operations, `KFilePlacesView` for Places,
`KFileItemActions` for service actions, `KPropertiesDialog` for properties
and KConfig for preferences. It offers an alternative workflow without
replacing or masking Dolphin.

## Directory Opus — power-user benchmark

Directory Opus sets the benchmark for power-user depth: dual panes, tabs,
viewers, flat browsing, automation and metadata. Its workflows inform
[folder formats and navigation history](../ROADMAP.md#m03-listing-depth),
[collections and duplicate selection](../ROADMAP.md#m09-tools),
[layout customization](../ROADMAP.md#m11-ui-customizations) and
[shortcut settings](../ROADMAP.md#m18-keyboard-shortcut-settings). These are
adapted to Linux and KDE rather than treated as a feature-by-feature parity
target.

## Krusader — two-pane tools

Krusader shows how a two-panel file manager can support deeper file operations.
Its influence includes group selection, occupied space, popular folders and
archive testing, as well as planned comparison, synchronization, checksums,
split and combine, transfer queues and terminal integration. The planned
embedded terminal sits alongside both panes rather than replacing either one.

## Orcinus — power-user workflows for Dolphin

Earlier project notes attribute several Dolphin-oriented power-user ideas to
Orcinus: grouped settings, per-pane display styles, expandable folders,
persistent filters, linked panes, cushion treemaps and profiles. An
authoritative upstream source has not been verified for these entries.

## TagSpaces — focused workspaces

[TagSpaces workspaces](https://docs.tagspaces.org/workspaces/) group locations
and tags by context and scope global searches to the active workspace. Opus's
planned [workspaces](../ROADMAP.md#m13-workspaces) add saved pane, toolbar and
panel layouts for different workflows. They organize the interface and search
scope; they are not a security boundary.

## TagStudio — native tagging

[TagStudio](https://github.com/TagStudioDev/TagStudio) informs the planned
[native tagging system](../ROADMAP.md#m21-native-tagging): opt-in libraries,
tags with aliases and parent relationships, custom fields and combined
searches. Opus will integrate these workflows without requiring TagStudio or
moving files.

## which-key.nvim — shortcut discovery

[which-key.nvim](https://github.com/folke/which-key.nvim) inspires the planned
native hint popup in
[shortcut settings](../ROADMAP.md#m18-keyboard-shortcut-settings). It will
show available continuations and action descriptions for pending key sequences
in the applicable context.
