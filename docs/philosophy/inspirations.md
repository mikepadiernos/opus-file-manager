# Inspirations

The applications that shaped Opus play different roles. Max File Manager is
where the project idea began and remains the main behaviour reference. Dolphin
sets the KDE desktop context, Directory Opus is the power-user benchmark, and
Krusader and Orcinus contribute workflows and feature ideas.

Opus adapts these ideas to its Linux and KDE scope rather than copying another
application's interface or implementation. The
[`roadmap`](../ROADMAP.md) tracks which features are complete or planned.

## Max File Manager — origin and behaviour reference

Opus began with the idea of forking Max File Manager to bring it to Linux and
fill a particular gap: a file manager as familiar and desktop-integrated as
Dolphin, but with the power-user depth associated with Directory Opus. The
project's implementation evolved into a native KDE application rather than a
continuation of Max's Electron architecture.

Max remains Opus's primary behaviour and feature-parity reference. The project
uses its documented user-facing workflows as a guide while implementing them
with Qt and KDE Frameworks. The reference documents cover the
[user interface](../parity/ui.md) and [backend and file operations](../parity/backend.md).

## Dolphin — KDE desktop model

Dolphin is KDE's file manager and the model for how Opus should fit into Plasma.
Opus aims to feel familiar to KDE users and to cooperate with the desktop's
existing settings, icon theme, service menus and file-management services.

Where possible, Opus uses KDE components rather than implementing a parallel
system: KIO for file operations, `KFilePlacesView` for Places,
`KFileItemActions` for service actions, `KPropertiesDialog` for properties,
and KConfig for preferences. Opus offers an alternative dual-pane workflow; it
does not replace, remove or mask Dolphin.

## Directory Opus — power-user benchmark

Directory Opus is the benchmark for the depth of a power-user file manager.
Its combination of dual panes, folder tabs, viewer features, flat browsing,
command automation and extensive file metadata informs the range of workflows
Opus aims to support.

This is an aspiration, not a feature-by-feature parity target. Opus prioritizes
the features in its own roadmap and adapts them to Linux and KDE.

## Krusader — two-pane tools

Krusader is KDE's orthodox two-panel file manager. It demonstrates how keeping
two locations in view can support a broader set of file-management tools.
Ideas adopted into the Opus plan include:

- **Selection and navigation:** select group, occupied space, popular folders
  and a jump-back point.
- **Archives and transfers:** archive integrity testing and password support,
  folder comparison, split and combine, and a transfer queue.
- **Power-user tools:** checksum files, an embedded terminal, command
  placeholders, pane swapping and administrator mode.

These features are assigned to milestones in the roadmap. The planned embedded
terminal is an additional panel alongside both panes, not a replacement for
either pane.

## Orcinus — power-user workflows for Dolphin

Orcinus is a Dolphin fork for power users. Its influence on Opus spans both
preferences and file-management workflows:

- **Preferences:** group related options into pages and tabs; configure startup
  pane count, symbolic-link following, row dragging and executable handling.
  These ideas informed Opus's early settings milestone.
- **Listing and navigation:** per-pane display styles, expandable folders,
  persistent filters and file-name highlighting.
- **Tools and layouts:** linked panes, cushion treemaps and profiles.

Opus's M02 settings milestone adopts several of the preference ideas. The
workflow features are assigned to later milestones and should be considered
planned unless the roadmap marks them complete.

## How to read these influences

Max provides behavioural expectations, while KDE components guide the native
implementation. Directory Opus, Krusader and Orcinus contribute goals and
optional workflows. The [scope](scope.md) defines the product boundaries, and
the [roadmap](../ROADMAP.md) is the source of truth for feature status.
