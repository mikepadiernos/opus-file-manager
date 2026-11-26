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
> M01–M26, and the M27.01 integrations listed there, from default
> file-manager registration to the KWin script and drop-down window.
> **Planned:** everything under [Planned features](#planned-features). The
> [roadmap](../ROADMAP.md) is authoritative for status.

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
  turn either on, or remove the icon, under **Settings › General › System
  Tray**. Closing to the tray applies only to the window's close button; **File ›
  Quit** and the tray menu's **Quit** still quit.
  ([#285](https://github.com/mikepadiernos/plasma-opus/issues/285); shortcut
  settings in [M28](../ROADMAP.md#m28-integration-settings) and
  [M31](../ROADMAP.md#m31-keyboard-shortcut-settings))
- **Plasma Activities:** each Activity can name a workspace and a default
  folder for each pane under **View › Activity Associations**, once
  associations are turned on under **Settings › Workspaces › Activities**.
  An association uses the workspace's own behaviours, or all, selected or
  none of them, independently of other Activities. When the current Activity
  changes, a message above the panes offers to switch to its workspace and
  open its folders in new tabs; nothing is applied until **Apply** is
  chosen, and the switch asks first as any workspace switch does.
  **View › Apply Activity Association** applies it at any other time.
  Associations are kept when their Activity is removed, their workspace is
  deleted or a folder is missing, and the missing parts are reported instead
  of applied. Plasma Opus only reads Activities: it never creates, removes or
  switches them, and leaves recent-file history to KDE.
  ([#387](https://github.com/mikepadiernos/plasma-opus/issues/387))
- **KDE Connect:** **File › Send to Device…** sends the selected local files
  to a paired device through KDE Connect's share plugin. The dialog lists the
  files it will send and the items it cannot, such as folders, remote
  locations and missing items, and each paired device with why it cannot
  receive files when it is offline or has file sharing turned off. Nothing is
  sent until **Send** is pressed, and the last device used is chosen again.
  When KDE Connect is not running or no device is paired, the dialog says so.
  KDE Connect's own notifications show each transfer's progress and result.
  Plasma Opus never pairs devices or changes KDE Connect settings.
  ([#389](https://github.com/mikepadiernos/plasma-opus/issues/389))
- **KRunner:** an installed build adds a **Plasma Opus** plugin to KRunner
  and Plasma Search that offers saved workspaces and Favorites whose name
  matches the query. Choosing a workspace starts Plasma Opus, or uses the
  running window, and switches to it after asking as any workspace switch
  does; choosing a Favorite opens it in a new tab. History, searches and
  recent folders are never offered, and a match carries only an opaque id,
  so no location or credentials pass through KRunner. Workspaces or
  Favorites removed since the match was shown are not opened. Turn either
  kind off under **Settings › General › Plasma Search**, or the whole plugin
  off in **System Settings › Search › Plasma Search**. The
  `opus-file-manager-krunner` helper is started by D-Bus when KRunner first
  asks. ([#390](https://github.com/mikepadiernos/plasma-opus/issues/390))
- **Service-menu downloads:** **Download New Services…** under **Settings ›
  Context Menu › Service Menus** opens KDE's download window for service
  menus, using the provider Dolphin installs, to browse, install, update and
  remove them. Nothing is installed until it is chosen there. The tab warns
  that downloaded service menus can run commands and are shared with Dolphin
  and other KDE applications. When the window closes, the tab summarizes what
  was installed, removed or failed, and lists the installed actions again
  while keeping unsaved changes to the Plasma Opus and Dolphin-shared
  visibility settings. Without Dolphin, the button is disabled and the tab
  explains why.
  ([#369](https://github.com/mikepadiernos/plasma-opus/issues/369))
- **System file chooser:** as an explicit opt-in, Plasma Opus answers the
  Open and Save dialogs that applications request through the XDG file
  chooser portal, such as Flatpak and Snap applications and portal-enabled
  GTK, Electron and browser applications; native Qt and KDE applications keep
  Plasma's own dialog. `opus-file-manager --install-portal`, or
  `./install.sh --file-chooser`, installs a per-user portal description and
  D-Bus activation file, puts Plasma Opus first for the file chooser in
  `~/.config/xdg-desktop-portal/kde-portals.conf` with the previous chooser
  next as the fallback, backs up that file and restarts xdg-desktop-portal.
  `opus-file-manager --uninstall-portal`, or `./install.sh --uninstall`,
  restores the backup, or removes only the Plasma Opus entry when the file
  has changed since. Setup changes nothing when xdg-desktop-portal is not
  running. The chooser shows Places, a breadcrumb, the folder in the Plasma
  Opus list, a name filter and the application's file-type filters and
  choices. It supports opening one or several files or a folder, saving with
  the selected filter's extension after confirming an overwrite, and saving
  several files into a folder. It returns local files only, is placed over
  the requesting window and remembers its size. The
  `opus-file-manager-portal` helper is started by D-Bus and exits after two
  idle minutes.
  ([#286](https://github.com/mikepadiernos/plasma-opus/issues/286))
- **Mount manager:** **File › Mount Manager…** lists mounted and
  fstab-defined file systems with their name, state, device, type, mount
  point, size, free space and a usage bar, optionally only removable devices.
  Pseudo, snap, container and other system file systems are hidden; network
  and FUSE mounts, such as remote drives, are included. Activating an entry
  opens it in the active pane, mounting it first when needed. Mount, unmount
  and eject go through Solid and UDisks2, with Plasma's authorization prompts,
  and fall back to `mount`, `umount` and `fusermount` for fstab entries marked
  `user` or `users` and for FUSE mounts. Busy devices, denied authorization,
  cancelled prompts and failures are reported. **Go › Mount Points**, also on
  the toolbar, lists each file system with a check mark when mounted and opens
  or mounts it. Partitioning, formatting and fstab editing are left to KDE
  Partition Manager, which the dialog opens when installed.
  ([#703](https://github.com/mikepadiernos/plasma-opus/issues/703))
- **KWin script:** **Settings › Configure KWin Integration…** installs,
  updates, enables, disables or removes an optional Plasma Opus KWin script,
  only when asked, and shows whether KWin, its scripting interface and the
  script are available and which version is running. With the script running,
  applying a workspace or Activity association that restores the layout also
  restores the window's screen, virtual desktops, size, tile and keep-above
  state as last seen with that workspace; detached Plasma Opus windows can be
  placed beside the main window; and a workspace can be assigned to each
  virtual desktop and applied when that desktop becomes current. See
  [KWin scripts](#kwin-scripts).
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **Drop-down window:** turned on in the same dialog, the main window slides in
  from a chosen screen edge, like [Yakuake](https://apps.kde.org/yakuake/),
  with a width and height as percentages of the screen, on the active or a
  chosen screen. **View › Show or Hide Drop-Down Window** toggles it; its
  global shortcut is unassigned until one is chosen in the dialog or in
  System Settings, and the script can also register a screen edge or a
  touch-screen edge. While shown, it stays above other windows and out of the
  taskbar, pager and task switcher. It can hide when another application is
  activated, but not while a dialog, menu or another Plasma Opus window is
  active, and it can switch to an assigned workspace when shown. Without the
  script it shows and hides as a regular window and says so once.
  ([#708](https://github.com/mikepadiernos/plasma-opus/issues/708))
- **Ark:** when [Ark](https://apps.kde.org/ark/) is installed, **File › Open
  in Ark** opens a local archive in Ark's window to add, rename or remove
  entries. **Extract to Other Pane** hands local formats that no KIO archive
  worker browses, such as RAR, to Ark in batch mode, extracting into a new,
  uniquely named folder so nothing is overwritten. Plasma Opus reads which
  formats Ark can open and create from Ark's installed backend plugins and the
  programs they need, such as `unrar` or `7z`, and reports a missing Ark.
  Ark's own **Extract** and **Compress** service menus appear in the context
  menu as usual. See [Ark integration](../CAVEATS.md#ark-integration).
  ([#280](https://github.com/mikepadiernos/plasma-opus/issues/280))
- **Archive creation through Ark:** the built-in **Compress** dialog also
  writes 7z archives without a password and can split any archive into
  numbered volumes, such as `photos.7z.001`. **More Options in Ark…** opens
  Ark's Compress dialog instead, for encrypted 7z with encrypted file names,
  compression levels and multi-volume archives where Ark's backends support
  them. **File › Add to Archive…** adds the selection to an existing archive
  Ark can write, after a confirmation, and refuses formats the installed
  backends can only extract, such as RAR without `rar`. See
  [Split archives](../CAVEATS.md#archives).
  ([#647](https://github.com/mikepadiernos/plasma-opus/issues/647))
- **Disk images:** **File › Mount Disk Image** and **Unmount Disk Image**
  mount and unmount the selected ISO or raw disk image through the Mount ISO
  action from Dolphin's plugins, which asks UDisks2 to attach a loop device
  and mount it, rather than custom mount code. The context menu offers
  whichever applies, in place of the plugin's own entries. Mounted images
  appear in Places through Solid. The status bar reports the mount point, or
  what is left attached or mounted when authorization is refused, and a
  dialog explains when the plugin is not installed. See
  [Disk images](../CAVEATS.md#desktop-integration).
  ([#648](https://github.com/mikepadiernos/plasma-opus/issues/648))
- **Application packages:** **File › Application Package…**, also in the
  context menu, identifies a selected AppImage, Flatpak bundle, `.flatpakref`
  or `.flatpakrepo` file, snap file, or a folder inside an installed Flatpak
  application or snap. It shows the package's name, ID, branch, source and
  installed state, and the output of `flatpak info`, including sandbox
  permissions, or `snap info` and `snap connections`. It offers the actions
  that apply:
  - run an AppImage, after asking to mark it executable, or an installed
    application;
  - install, update or uninstall with `flatpak` or `snap`, for your account
    or, for Flatpak, all users;
  - add a `.flatpakrepo` remote;
  - open the package in Discover;
  - update an AppImage with AppImageUpdate;
  - add an AppImage to the application menu, or remove it, with its own
    name and icon, named as libappimage names menu entries.

  Changes are confirmed first and never change sandbox permissions or snap
  confinement. Missing tools disable their actions with an explanation, and
  failures show the tool's output. See
  [Application packages](../CAVEATS.md#desktop-integration).
  ([#281](https://github.com/mikepadiernos/plasma-opus/issues/281))

- **Baloo status:** **File › Indexing Status…**, also offered by the search
  panel's **Indexing Status…** button in the **Indexed locations (Baloo)**
  scope, shows the file indexer's state and progress, whether indexing is on
  and covers contents and metadata, the indexed and excluded folders and the
  exclusion patterns from KDE File Search settings. It explains whether the
  current folder is in the configured scope, and why not: an excluded folder,
  a pattern, a hidden item or a folder outside the indexed ones. **Check Index
  Coverage** looks up the folder's first 200 files in the index, confirming
  how many it contains, and names the file types that no installed
  KFileMetaData extractor handles. The search panel warns in the Baloo scope
  when indexing is off, suspended, busy, not running or limited to basic
  details, or when the current folder is excluded. **Suspend Indexing**,
  **Resume Indexing**, **Check for Unindexed Files…**, after a confirmation,
  and **Open KDE File Search Settings** use KDE's shared controls only when
  chosen; Plasma Opus never turns indexing on or changes its settings. See
  [Index status](../CAVEATS.md#search).
  ([#441](https://github.com/mikepadiernos/plasma-opus/issues/441))

- **Baloo metadata and ranking:** Indexed queries accept `meta:` terms, such
  as `meta:Canon type:jpg`. Baloo finds candidates whose indexed text or
  enabled text properties, such as camera maker, title or artist, contain the
  term; Plasma Opus then reads each candidate's metadata to confirm the whole
  query. Name and content terms are preferred as the starting term when a
  query has them. A query with only number or date metadata, no enabled text
  properties or Baloo limited to file names is refused with a suggestion.
  **Rank results by similarity** in the search panel lists exact names first,
  then prefixes, word starts, other matches, near spellings and scattered
  letters; the status explains that ranking only reorders the matches found.
  After an indexed search, **Search Current Folder Live** or **Search
  Workspace Locations Live** repeats it as a live search, with the usual
  limits, progress, cancellation and errors; the scope never changes on its
  own. See [Indexed metadata and ranking](../CAVEATS.md#search).
  ([#442](https://github.com/mikepadiernos/plasma-opus/issues/442))

- **File-type categories in Places:** Places lists **Documents**, **Images**,
  **Audio**, **Videos**, **Archives**, **Ebooks**, **Application Packages**
  and **Large Files** under **Search For**, beside any entries KDE already
  shows there. Plasma Opus adds them once, only for itself, so a removed or
  hidden entry stays that way. Opening a category shows Baloo's results for
  it through `baloosearch:/`, in the pane or a new tab. When indexing is off
  or unavailable, the category explains why and offers **Search a Folder
  Live…**, and **Open KDE File Search Settings** when indexing is merely
  turned off. When the indexer is not running, suspended, busy or limited to
  basic details, it asks first and offers **Show Indexed Results** or the
  live search. The live search runs in the search panel for the chosen folder
  by MIME type or file extension, with the panel's usual limits, progress and
  cancellation. **Large Files**, over 100 MiB, always searches a chosen
  folder live, since Baloo doesn't index file sizes. See
  [File-type categories](../CAVEATS.md#desktop-integration).
  ([#649](https://github.com/mikepadiernos/plasma-opus/issues/649))

- **Terminal providers:** **Tools › Open Terminal With** and the folder
  context menus open the active local folder in KDE's **Default Terminal**,
  [Konsole](https://apps.kde.org/konsole/),
  [Yakuake](https://apps.kde.org/yakuake/) or Kwi. Konsole and Kwi start
  with `--workdir`; Yakuake receives a new session over D-Bus, starting
  Yakuake first when it isn't running, and the status bar notes that its
  window may need its shortcut. Providers that aren't installed are disabled
  with a tooltip saying so, and remote or missing folders are reported
  without starting anything. **Open Terminal Here** still uses KDE's default
  terminal. The embedded terminal panel's menu gains **Terminal Component**,
  which chooses the Konsole or Kwi component for new sessions; open sessions
  keep theirs, and a missing component is reported instead of replaced. See
  [Terminal providers](../CAVEATS.md#desktop-integration) and
  [Embedded terminal](../CAVEATS.md#embedded-terminal).
  ([#289](https://github.com/mikepadiernos/plasma-opus/issues/289))

- **Atuin command history:** **Tools › Search Command History with Atuin**,
  also in the terminal panel's menu, shows the embedded terminal and types
  [Atuin](https://docs.atuin.sh/)'s interactive search into the current
  session. Atuin shows the chosen command in the terminal instead of running
  it. Plasma Opus first checks that Atuin is installed, in `PATH` or the
  folders its installers use, and that its history exists, but never reads
  the history, changes shell configuration, or installs, initializes or syncs
  Atuin. A session running another program is reported instead of receiving
  the search. See [Embedded terminal](../CAVEATS.md#embedded-terminal).
  ([#364](https://github.com/mikepadiernos/plasma-opus/issues/364))

- **Editor integrations:** **Tools › Edit With**, also in the context menu
  of selected files, opens them in [KWrite](https://apps.kde.org/kwrite/) or
  [Kate](https://apps.kde.org/kate/) as separate applications, or in Neovim,
  Rune, micro or nano in a new embedded terminal tab. Editors that aren't
  installed are disabled with a tooltip saying so. Files are passed as
  absolute paths, without a shell; terminal editors start in the first
  file's folder and their tab closes when they exit. KWrite and Kate also
  open remote files, while terminal editors report them, as they do folders.
  **Open Editor** still opens the built-in editor. See
  [Editor integrations](../CAVEATS.md#desktop-integration).
  ([#319](https://github.com/mikepadiernos/plasma-opus/issues/319))
- **Office documents:** The preview panel shows OpenDocument and Microsoft
  Office documents, spreadsheets and presentations read-only through an
  installed viewer component for the format, such as Calligra's.
  **Tools › Open in Office Application**, also in the context menu when every
  selected file is an office document, opens them in
  [LibreOffice](https://www.libreoffice.org/),
  [Calligra](https://calligra.org/) or
  [ONLYOFFICE](https://www.onlyoffice.com/desktop.aspx). Applications that
  aren't installed are disabled with a tooltip saying so. LibreOffice and
  ONLYOFFICE open a mixed selection together; Calligra opens each kind in
  Calligra Words, Sheets or Stage. Only Calligra opens remote files. The
  application handles editing, saving and format conversion. See
  [Office documents](../CAVEATS.md#desktop-integration).
  ([#674](https://github.com/mikepadiernos/plasma-opus/issues/674))

## Planned features

Grouped by the [M27](../ROADMAP.md#m27-integrations) subsection that tracks
them.

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
screen, position, virtual desktop or tile, so the optional Plasma Opus script
does this for Plasma Opus windows. Plasma Opus bundles it, installs it per user
as a `KWin/Script` KPackage under `~/.local/share/kwin/scripts/opusfilemanager`,
enables it through its own entry in `kwinrc`'s `[Plugins]` group and asks KWin
to load it over the `org.kde.KWin` `/Scripting` interface.

### How it works

- **Communication:** KWin scripts cannot export D-Bus methods, so the script
  calls Plasma Opus's versioned `org.kde.opusfilemanager.KWin1` interface at
  `/KWin`. Plasma Opus asks it to call by invoking the script's **Plasma Opus:
  Process Window Requests** shortcut through KGlobalAccel. The reply carries
  the interface version, the Plasma Opus process id, the options and any
  pending commands; a script that receives another interface version stays
  passive.
- **Versions:** the dialog compares the installed package's version with the
  bundled one and offers **Update**, and reports when KWin still runs an older
  version. Installing or updating reloads the script.
- **Workspace windows:** the script reports the main window's screen, virtual
  desktops, geometry, tile and keep-above state after it is moved, resized,
  retiled or sent to another screen or desktop. Plasma Opus keeps the last
  report for the current workspace in its state configuration and sends it
  back when that workspace is applied. A tile is restored when a tile with the
  same geometry still exists; otherwise the geometry is.
- **Tiled companions:** Plasma Opus's own detached windows, such as the gallery
  and the popped-out second pane, open in the half of the screen the main
  window does not occupy, in a matching KWin tile when there is one.
- **Virtual desktops:** the script reports the virtual desktops, which the
  dialog lists for workspace assignment, and the desktop that becomes current.
- **Drop-down window:** the script styles and places the main window whenever
  it is mapped while the drop-down window is on, restores its previous
  geometry and flags when it is turned off, and registers the chosen screen
  and touch-screen edges.

### Script boundaries

- Install or enable nothing without explicit action. Removing the integration
  removes only the Plasma Opus script and its `kwinrc` entry.
- Act only on windows of the running Plasma Opus process, identified by its
  desktop file name and process id; never move, resize or close other
  applications' windows.
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
