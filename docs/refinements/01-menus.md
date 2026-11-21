# M30.1 Menus

**[Refinements](README.md)** · **[Roadmap](../ROADMAP.md#m301-menus)** ·
**[Layouts](02-layouts.md)**

**Goal:** make actions easy to find and accurately scoped without replacing
KDE integration or user-customized menus and toolbars.

## Proposed menu layout

> [!NOTE]
> This editable design is not the current menu order. Branches labeled *group*
> organize actions visually and do not necessarily require nested submenus.
> Names and order can change; offer only actions supported by the current
> location, selection and provider. Future features are not required.

### Main menus

```text
Main menus
├── File
│   ├── Create and open (group)
│   │   ├── Create New
│   │   ├── New Folder
│   │   ├── Open
│   │   └── Open With
│   ├── File operations (group)
│   │   ├── Copy to Other Pane
│   │   ├── Move to Other Pane
│   │   ├── Rename
│   │   ├── Batch Rename
│   │   ├── Create Symbolic Link
│   │   ├── Move to Trash
│   │   └── Delete Permanently
│   ├── Archives (group)
│   │   ├── Compress
│   │   ├── Extract to Other Pane
│   │   ├── Extract Protected Archive
│   │   └── Test Archive Integrity
│   ├── Metadata and properties (group)
│   │   ├── Edit Metadata
│   │   ├── Properties
│   │   └── Calculate Occupied Space
│   └── Quit
├── Edit
│   ├── Undo
│   ├── Cut
│   ├── Copy
│   ├── Paste
│   ├── Copy Path
│   ├── Select All
│   ├── Selection actions
│   ├── Selection Mode
│   └── Filter Bar
├── View
│   ├── File display (group)
│   │   ├── View Mode
│   │   ├── Folder Format
│   │   ├── Folder Size Policy
│   │   ├── Selection Markers
│   │   ├── Previews
│   │   ├── Hidden Files
│   │   └── Show Flat
│   ├── Panes and tabs (group)
│   │   ├── Pane Layout and Orientation
│   │   ├── Swap Panes or Tabs
│   │   ├── Move or Restore Second Pane
│   │   ├── Splitter Sizes
│   │   ├── Tab Bar Options
│   │   ├── Dim Inactive Pane
│   │   └── Link Panes
│   ├── Panels (group)
│   │   ├── Explorer and Places
│   │   ├── Preview
│   │   ├── Information
│   │   └── Embedded Terminal
│   ├── Open Gallery
│   └── Reload
├── Go
│   ├── Back
│   ├── Forward
│   ├── Up
│   ├── Home
│   ├── Jump Back
│   ├── Popular Folders
│   ├── Edit Location
│   ├── Add to Favorites
│   ├── Switch Pane
│   └── Tabs (group)
│       ├── New Tab
│       ├── Close Tab
│       ├── Reopen Closed Tab
│       ├── Next Tab
│       └── Previous Tab
├── Tools
│   ├── Search Folder
│   ├── Find Duplicates
│   ├── Content Chart
│   ├── Compare Files
│   ├── Compare Pane Folders
│   ├── Compare Folders with Filters
│   ├── Checksums
│   ├── Split or Combine Files
│   ├── Export Listing
│   ├── Transfer Queue
│   ├── Open Terminal
│   ├── Open Editor
│   └── Custom Commands
├── Settings
│   ├── Configure Plasma Opus
│   ├── Configure Shortcuts
│   └── Customize Main Toolbar and Menu
└── Help
    └── KDE-provided help and About actions
```

### Context menus by area

Each tree is scoped to the surface where the menu opens. Entries are proposed
organization, not a promise that every action is available in every context.

#### File listing: selected items

```text
File listing: selected items
├── Open
├── Open With
├── Open Folder in New Tab
├── Open Folder in Other Pane
├── Cut
├── Copy
├── Paste into Selected Folder
├── Copy To
├── Move To
├── Rename
├── Batch Rename
├── Create Symbolic Link
├── Move to Trash
├── Delete Permanently
├── Open Terminal Here
├── Content Chart
├── Add to Favorites
├── Folder Look
├── Metadata
├── Archive actions
├── Copy Path
├── Properties
└── KDE service and plugin actions
```

#### File listing: empty space

```text
File listing: empty space
├── Create New
├── Paste
├── New Tab
├── Reload
├── Open Terminal Here
├── Calculate Occupied Space
├── Content Chart
├── Folder Look
├── Copy Path
└── Properties
```

#### Listing column header

```text
Listing column header
├── Visible Columns
├── Group Column
├── Fit Column
├── Fit All Columns
└── Group By
```

#### Pane tab

```text
Pane tab
├── New Tab
├── Duplicate Tab
├── Rename Tab
├── Pin or Unpin Tab
├── Lock or Unlock Tab
├── Tab Colour
├── Copy Path
├── Close Tab
├── Close Other Tabs
└── Close Tabs to the Right
```

#### Explorer: folder tree

```text
Explorer: folder tree
├── Open in New Tab
├── Open in Other Pane
├── Add to Favorites
└── Properties
```

#### Explorer: favorite

```text
Explorer: favorite
├── Open in New Tab
├── Rename
└── Remove from Favorites
```

#### Places and devices

```text
Places and devices
└── KDE Places actions appropriate to the item
```

#### Main or pane toolbar

```text
Main or pane toolbar
└── Customization and item appearance actions
```

#### Terminal panel header or session tab

```text
Terminal panel header or session tab
├── Dismiss Message
└── Hide Terminal Panel
```

#### Terminal content

```text
Terminal content
└── Terminal provider's context menu
```

#### Text editor content

```text
Text editor content
└── KTextEditor-provided editing actions
```

### Behavior and boundaries

- See the [layouts plan](02-layouts.md#proposed-toolbar-layouts) for toolbar
  order, customization and overflow.
- Make actions discoverable from the menu bar and hamburger menu without
  duplicating command implementations or overriding their customized
  contents. Keep KDE/KXmlGui actions, Open With, service menus and Places
  device actions under their existing providers.
- File-item context menus target the items for which the menu opened; empty
  space actions target that pane's location. Apply selection, locality,
  capability and user-visibility rules, and preserve KIO error and conflict
  handling, confirmation policies and pinned or locked tab protections.
- Preserve provider-owned editor and terminal menus. Review whether other
  panel headers need context actions without assuming a menu already exists.
  Customization, hiding a panel and destructive actions must remain
  distinguishable and accessible.

## Proposed order

1. Inventory main, pane and context menus, toolbar actions and shortcuts;
   record duplicate labels, confusing groups and actions supplied by KDE.
   Include customized layouts and both pane orientations.
2. Propose consistent names, grouping and placement for existing actions.
   Review menu changes against keyboard access, translations, active-pane
   context and destructive-action confirmations before implementing them.
   ([#458](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/458))
3. Improve toolbar and pane-action discovery and narrow-window overflow.
   Preserve shortcut bindings, current toolbar configuration and an obvious
   indication of which pane an action affects.
   ([#460](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/460))
4. Synchronize enabled, checked and visible action state after selection,
   navigation and tab changes. Keep an open context menu tied to the
   selection for which it was invoked.
   ([#482](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/482))
5. Exercise mouse and keyboard menu use, empty and mixed selections, remote
   locations, pane changes while a menu is open and restored customizations.
   Add focused regression tests alongside the changes.

## Review decisions

- Which current actions are hard to find or named inconsistently?
- Which menu changes should be defaults for new users only, rather than
  modifications to saved layouts?
- Which KDE-provided menus must remain untouched?
