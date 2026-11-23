# M32.04 Settings

**[Refinements](README.md)** · **[Roadmap](../ROADMAP.md#m32-refinements)** ·
**[Performance](05-performance.md)**

**Goal:** make preferences searchable, understandable and safe to change,
without promising rollback of changes owned by other components.

## Contents

- [Proposed settings layout](#proposed-settings-layout)
- [Proposed order](#proposed-order)
- [Review decisions](#review-decisions)

## Proposed settings layout

> [!NOTE] This tree is a design proposal, not the current dialog. The current
> dialog has sidebar groups with tabs.

Inspired by the supplied KDE System Settings example, add a second navigation
column while retaining Plasma Opus's existing visual style, theme icons and
sidebar customization. This changes navigation, not the application's colors,
theme or styling.

1. **Sidebar:** select a category, such as File or Panels. Provide a
   preference-search entry point here; General > Favorites remains the place to
   manage favorite settings pages.
2. **Subcolumn:** select a page within the category, such as Views or Operations
   under File, or Terminal under Panels.
3. **Content:** display the selected page's controls, using scoped tabs where
   they help group related settings. Simple pages need no tabs. Keep Apply, OK,
   Cancel, defaults and help accessible across navigation.

For example:

```text
Category sidebar       Subcolumn             Content
─────────────────      ─────────────────     ────────────────────────────
File                   Views                 [General] [Details] [Icons] ...
                       Operations            Default view
                                             Sorting
                                             Hidden files
                                             ...
```

In the tree, `(category)` entries appear in the sidebar, `(subpage)` entries
appear in the subcolumn, and `(tab)` entries appear in the content area.
Unmarked descendants are sections, controls or links, not extra tabs. Existing
tabs are mapped into these destinations; unmarked entries are not necessarily
new features. Names, grouping and ordering can be changed. Subpages start with
general or frequently used controls, followed by related specialized features
and import/export or advanced integrations.

- `[proposed]` identifies M32 navigation or controls not yet established.
- `[future]` reserves a place for roadmap work; the marker also applies to its
  descendants. It does not claim the feature exists or make it a prerequisite
  for M32.

```text
Settings
├── General (category)
│   ├── Favorites (subpage) [proposed]
│   │   └── User-selected settings pages
│   ├── Sidebar (subpage)
│   │   └── Sidebar icon size and title position
│   ├── Language and Dates (subpage)
│   │   ├── Language and Layout (tab)
│   │   │   ├── Interface language
│   │   │   ├── Regional formats
│   │   │   ├── Layout direction
│   │   │   └── Numerals
│   │   └── Dates (tab)
│   │       └── Calendar and date/time formats
│   ├── Branding (subpage) [future: M20]
│   │   ├── Startup Branding (tab)
│   │   │   └── Enable or disable the optional startup branding
│   │   └── Branding Variants (tab)
│   │       └── Choose an available variant without changing app identity
│   └── Data (subpage)
│       ├── Backup and Restore (tab)
│       │   ├── App-owned configuration and layout backup
│       │   ├── Preview
│       │   └── Restore
│       └── Portability (tab) [future: M16]
│           ├── Workspace import
│           ├── Workspace export
│           └── Conflict review
├── Appearance (category)
│   ├── Toolbars (subpage)
│   │   └── Main and pane toolbar configuration and custom items
│   ├── Menus (subpage)
│   │   ├── Context Menu Actions (tab)
│   │   └── Automation (tab)
│   ├── Folders and Tabs (subpage)
│   │   ├── Behavior (tab)
│   │   └── Appearance (tab)
│   ├── Status and Location Bars (subpage)
│   ├── Motion (subpage) [proposed]
│   │   └── Desktop motion preferences and app-level animation disable
│   └── Themes (subpage) [future: M18]
│       ├── Appearance (tab)
│       ├── Colors (tab)
│       ├── Stylesheet (tab)
│       └── Theme Profiles (tab)
├── Panels (category)
│   ├── General (subpage) [proposed]
│   │   ├── Panel appearance
│   │   ├── Title visibility
│   │   └── Layout behavior
│   ├── Explorer (subpage)
│   │   ├── Combined Places and Folders
│   │   ├── Visible sections
│   │   └── Focus behavior
│   ├── Linked Panes (subpage)
│   │   └── Startup linking and local roots
│   ├── Preview (subpage)
│   │   ├── General (tab)
│   │   │   └── Preview limits
│   │   ├── Text and Presentation (tab)
│   │   └── Media (tab)
│   ├── Metadata (subpage)
│   │   ├── Information panel fields
│   │   ├── Field ordering
│   │   └── Hover behavior
│   ├── Text Editor (subpage)
│   │   ├── General (tab)
│   │   │   ├── Opening and placement
│   │   │   └── Text
│   │   ├── Markdown (tab)
│   │   │   └── PDF export
│   │   ├── Drafts and Recovery (tab)
│   │   └── KTextEditor (tab)
│   ├── Terminal (subpage)
│   │   └── Placement and following the active pane's local folder
│   └── Search (subpage) [future: M22]
│       └── Panel placement and restoring its visibility
├── File (category)
│   ├── Views (subpage)
│   │   ├── General (tab)
│   │   ├── Details (tab)
│   │   ├── Icons (tab)
│   │   ├── Compact (tab)
│   │   ├── Listing Selection and Filters (tab)
│   │   ├── Folder Formats (tab)
│   │   └── Listing Previews (tab)
│   └── Operations (subpage)
│       ├── Confirmations (tab)
│       ├── Archives (tab)
│       │   └── Defaults
│       ├── Rules (tab)
│       ├── Metadata (tab)
│       ├── Saved Selection Filters (tab)
│       ├── Transfer Queue (tab)
│       └── Applications (tab)
├── Modes (category)
│   ├── Default (subpage) [proposed]
│   │   ├── Normal dual-pane browsing
│   │   ├── Default view and listing settings (link to File > Views)
│   │   └── Startup folders and session (link to Appearance > Folders and Tabs)
│   ├── Gallery (subpage)
│   │   ├── Opening (tab)
│   │   ├── Information and Display (tab)
│   │   ├── Slideshow (tab)
│   │   └── Image Conversion (tab)
│   └── Yazi (subpage) [future: M26]
│       ├── Activation (tab)
│       ├── Columns and Preview (tab)
│       ├── Keyboard (tab)
│       └── Workspace Behavior (tab)
├── Tools (category)
│   ├── Analysis (subpage)
│   │   ├── Duplicate Finder (tab)
│   │   └── Content Chart (tab)
│   ├── Comparison and Sync (subpage)
│   │   ├── File Diff (tab)
│   │   ├── Pane Comparison (tab)
│   │   └── Folder Sync (tab)
│   └── Utilities (subpage)
│       ├── Batch Rename (tab)
│       ├── Listing Export (tab)
│       ├── File Tool Defaults (tab)
│       └── Metadata Editor (tab)
├── Search (category)
│   ├── Defaults (subpage)
│   │   ├── General (tab)
│   │   │   └── Scope and filename or content search
│   │   ├── Filters (tab)
│   │   │   ├── Paths
│   │   │   ├── Subtrees
│   │   │   └── Recursion depth
│   │   └── Saved Searches and Collections (tab)
│   ├── Advanced Search (subpage) [future: M22]
│   │   ├── Toolbar (tab)
│   │   └── Results (tab)
│   └── Results (subpage) [proposed]
│       ├── Page
│       ├── Matching setting
│       └── Direct navigation
├── Workspaces (category)
│   ├── Management (subpage) [future: M16 or M28]
│   │   ├── Workspaces (tab) [future: M16]
│   │   │   ├── Locations
│   │   │   └── Exclusive location assignments
│   │   ├── Vault (tab) [future: M28]
│   │   │   ├── Assigned local vault root
│   │   │   └── Editor, preview and note navigation
│   │   ├── Startup (tab) [future: M16]
│   │   │   └── Startup workspace or last-used workspace
│   │   ├── Recovery (tab) [future: M16]
│   │   │   └── Unavailable locations and providers
│   │   ├── Features (tab) [future: M16]
│   │   │   ├── Workspace features
│   │   │   ├── Theme application [future: M18]
│   │   │   └── Activity overrides [future: M28]
│   │   └── Password-gated Folders (tab) [future: M28]
│   │       └── Folder access managed inside Plasma Opus
│   ├── Layouts (subpage) [future: M16]
│   │   ├── Layouts (tab)
│   │   │   ├── Restored components
│   │   │   ├── Workspace theme [future: M18]
│   │   │   ├── Pane location bars
│   │   │   └── Overwrite confirmation
│   │   ├── Indicators (tab)
│   │   │   ├── Active workspace
│   │   │   ├── Unsaved layout
│   │   │   └── Selector placement
│   │   └── Snapshots (tab)
│   │       ├── Restored pane state
│   │       ├── Restored tab state
│   │       └── Restored Yazi mode state [future: M26]
│   └── Search (subpage) [future: M16]
│       └── Workspace or all-location default scope
├── Tagging (category) [future: M24]
│   ├── Libraries (subpage)
│   │   ├── Locations (tab)
│   │   │   ├── Roots
│   │   │   └── Exclusions
│   │   ├── Scanning (tab)
│   │   │   ├── Refresh
│   │   │   └── Scanning limits
│   │   └── Privacy (tab)
│   │       ├── Metadata storage
│   │       ├── Backups
│   │       └── Consent for file writes
│   ├── Tags (subpage)
│   │   ├── Groups (tab)
│   │   │   ├── Stable identities
│   │   │   ├── Names and aliases
│   │   │   └── Relationships
│   │   ├── Display (tab)
│   │   │   ├── Colors
│   │   │   └── Display options
│   │   └── Search (tab)
│   │       ├── Search scope
│   │       └── Parent-tag matching
│   └── Metadata (subpage)
│       ├── Fields (tab)
│       │   └── Text and date fields
│       └── Embedded Metadata (tab)
│           ├── EXIF and ID3 mappings
│           ├── Refresh
│           └── Conflicts
├── Shortcuts (category) [future: M31]
│   ├── Bindings (subpage)
│   │   ├── Actions
│   │   ├── Alternative bindings
│   │   ├── Key chords
│   │   ├── Contexts
│   │   └── Conflicts
│   ├── Keyboard Map (subpage)
│   │   └── Searchable bindings and printable reference
│   ├── Vim-style Navigation (subpage)
│   │   ├── Mode
│   │   ├── File-list navigation
│   │   └── Text-input behavior
│   ├── Hints (subpage)
│   │   ├── Sliding or floating key-sequence panel
│   │   ├── Delay
│   │   └── On-demand reference
│   ├── Global Shortcuts (subpage)
│   │   ├── Optional KDE-managed global bindings
│   │   └── Integration shortcut (link to Integrations > KDE Plasma)
│   └── Import and Export (subpage)
│       └── Bindings and conflict preview
└── Integrations (category)
    ├── KDE Plasma (subpage)
    │   ├── Service Menus (tab)
    │   │   └── Installed service actions and plugins
    │   ├── System Tray (tab) [future: M28]
    │   │   └── Minimize and close behavior
    │   ├── Activities (tab) [future: M28]
    │   │   └── Workspace associations and Activity-specific choices
    │   ├── Notifications (tab) [future: M28]
    │   │   └── Plasma Opus operation events and KDE notification settings
    │   ├── Global Shortcuts (tab) [future: M28]
    │   │   └── Integration shortcut and KDE shortcut configuration
    │   ├── Online Accounts (tab) [future: M28]
    │   │   └── Opt-in access and account visibility
    │   ├── Baloo (tab) [future: M28]
    │   │   └── Search integration and shared indexing controls
    │   ├── KDE Connect (tab) [future: M28]
    │   │   └── Sharing and remembered device
    │   └── KRunner (tab) [future: M28]
    │       └── Optional runner and exposed workspaces and locations
    ├── File Manager (subpage) [future: M28]
    │   └── File chooser portal and default file manager
    ├── Terminal (subpage) [future: M28]
    │   └── Embedded and separate terminal providers
    ├── Editor (subpage) [future: M28]
    │   └── Editor provider and terminal session type
    ├── Application Packages (subpage) [future: M28]
    │   ├── AppImage actions
    │   ├── Flatpak actions
    │   └── Snap actions
    ├── Remote Connections (subpage) [future: M28]
    │   └── Connection profiles and KDE credential facilities
    ├── Nextcloud (subpage) [future: M30]
    │   └── Account, asset, sharing and sync integration
    ├── rsync (subpage) [future: M28]
    │   └── Optional backend and sync defaults
    ├── Atuin (subpage) [future: M28]
    │   └── Terminal history action and session
    └── MCP (subpage) [future: M28]
        ├── Access level
        ├── Token
        └── Audit log
```

### Mapping and behavior

- Current **Translation** tabs move to General and **Backup** moves to General
  > Data; workspace portability has a separate tab there and reuses the backup
  > controls. Current **Interface** tabs are split between Appearance, Panels
  > and the File category's Views and Operations destinations. Current **View**
  > tabs move into File > Views.
- Current **Context Menu Actions** and **Automation** settings move to
  Appearance > Menus; **Service Menus** moves to Integrations > KDE Plasma.
  Existing service-menu settings remain available without M28. **Archives** and
  **Files** tabs move to File > Operations. Gallery settings move to tabs under
  Modes > Gallery. The Editor and KTextEditor tabs move to Panels > Text Editor,
  and the embedded terminal moves from Tools to Panels. Search becomes its own
  category below Tools. Linked Panes moves to Panels; remaining tools are
  grouped by scope under Tools, except Automation, Applications and Transfer
  Queue, which move as shown above.
- Favorites move into General. Language and Dates share a subpage, as do Folders
  and Tabs behavior and appearance. Modes groups normal browsing, Gallery and
  future Yazi settings; Default links to the authoritative view and startup
  settings rather than introducing competing values.
- The Metadata page refers to the Information panel, not the Metadata Editor
  tool or file metadata settings. Explorer covers the combined Places/Folders
  panel and any separate panel choices where supported. General panel controls
  include title visibility. Existing embedded-title behavior is described in the
  [resolved bug report](../BUGS.md#hide-embedded-panel-titles); its placement in
  this tree is proposed.
- Keep each setting owned in one place. Listing preview settings belong under
  File > Views; panel-specific preview settings belong under Panels > Preview.
  Shared preview providers and limits should have one authoritative control with
  links from related pages, not independent values. Similarly, keep editor-wide
  settings under Panels > Text Editor and search defaults under Search >
  Defaults; link to the latter from Panels > Search. Make clear which editor
  settings also affect standalone windows. Separate embedded areas must not
  replace either file pane; an embedded pane placed in the middle column must
  preserve the file-pane state it temporarily occupies. Coordinate pane
  placement controls with the [interface design](../philosophy/designs.md).
- Listing filters and saved selection filters serve different purposes and stay
  distinct. Advanced Search links to the shared search defaults instead of
  creating a second copy. The integration shortcut remains available with M28;
  M31 links to it when available rather than adding a conflicting binding.
- Future workspace, tagging, integration and shortcut pages reuse shared
  settings where applicable. Link to the authoritative controls instead of
  creating conflicting values. Workspace profiles can select features, layout
  and theme, with optional Activity associations and per-Activity choices; keep
  KDE-owned accounts, credentials and desktop settings under KDE's control.
- Integrations > KDE Plasma groups desktop-specific controls; provider choices
  shared with non-KDE applications remain separate. Password-gated folders are
  under Workspaces > Management, but are an M28 feature and do not depend on M16
  workspace implementation; the gate applies only inside Plasma Opus, not as
  filesystem security.
- Search and favorites navigate to the same underlying pages, not duplicate
  settings. The sidebar search entry point and Search > Results use the same
  preference-search results, separately from file search. A result opens its
  category, page and tab and focuses the control; list filters preserve hidden
  entries and unsaved edits.
- Preserve selection and pending changes when switching categories, pages or
  tabs; select the last visited page within a category when returning to it. On
  narrow windows, collapse or navigate between columns without making pages
  inaccessible; support keyboard focus, screen readers and right-to-left
  layouts. Keep current icon sizing and title placement choices effective.
- Keep Apply, OK, Cancel, page/all defaults and help available independently of
  navigation. Explain scope, effective-value precedence, external KDE ownership
  and immediately applied actions where relevant; do not imply that Cancel
  reverses changes owned by another component.
- Preserve existing saved values, customizations and direct-entry actions when
  reorganizing pages. Keep stable page identities for favorites and links;
  clarify how old links are mapped and report unavailable pages.

### Layout review questions

- How should the subcolumn collapse or be reached in small windows?
- Are Confirmations, Applications and Transfer Queue easier to find under File >
  Operations, or should they remain closer to their current locations?
- Are the proposed tool scopes intuitive and are individual tools easy to find?
- Which future categories should be visible only once their features ship?

### Further consolidation suggestions

These are options for review, not changes to the tree above:

- **Applications and integrations:** link File > Operations > Applications with
  future Integrations controls; keep desktop-wide associations distinct from
  Plasma Opus-specific preferences.

## Proposed order

1. Inventory existing pages, labels, dependencies, shared KDE settings, restart
   requirements and immediately applied actions. Agree on consistent grouping
   and conventions for existing and future pages. Implement the category
   sidebar, scoped subcolumn and content tabs while preserving the existing
   visual styling, settings and navigation state; test narrow and accessible
   layouts.
   ([#221](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/221),
   [#400](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/400))
2. Add settings search by name and description, direct navigation to a matching
   control, favorite pages and clearable keyboard-accessible filters for
   suitable long lists. Preserve hidden entries and unsaved edits.
   ([#220](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/220),
   [#544](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/544),
   [#545](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/545))
3. Clarify Apply, OK, Cancel and dismissal with pending edits; restore live
   previews when cancellation can actually do so. Explain immediately applied
   actions and external KDE changes separately.
   ([#222](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/222),
   [#396](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/396),
   [#490](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/490))
4. Add actionable inline validation and contextual help. Explain unavailable
   dependencies, setting scope and effective-value precedence without changing
   the underlying rules.
   ([#224](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/224),
   [#227](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/227),
   [#395](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/395))
5. Implement explicit page or all-page defaults and clarify backup, restore and
   reset boundaries, including a preview of affected app-owned settings,
   excluded credentials and shared KDE data. Preserve M12 confirmations and
   restart requirements.
   ([#223](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/223),
   [#398](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/398))
6. Verify persistence, save failures and conflicting external changes across
   windows and restarts. Keep large lists and provider data responsive;
   coordinate lazy loading and measurement with
   [performance](05-performance.md).
   ([#397](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/397),
   [#399](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/399))
7. Check keyboard access, focus, accessible errors, small windows, high DPI,
   long translations and right-to-left layouts. Add focused tests for search,
   validation, cancellation, defaults, persistence and shared effects.
   ([#225](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/225),
   [#226](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/226),
   [#401](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/401))

## Review decisions

- Which pages and lists need favorites, direct links or filtering first?
- What does each reset affect, and which changes cannot be undone?
- How should external changes be reported when local edits are pending?
