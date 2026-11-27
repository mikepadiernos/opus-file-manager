# KDE Plasma integration

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Roadmap: M27 integrations](https://img.shields.io/badge/roadmap-M27%20integrations-8957e5?style=flat-square)](../ROADMAP.md#m27-integrations)
[![Roadmap: M28 integration settings](https://img.shields.io/badge/roadmap-M28%20integration%20settings-8957e5?style=flat-square)](../ROADMAP.md#m28-integration-settings)

![Milestones M27–M28](https://img.shields.io/badge/milestones-M27%E2%80%93M28-8957e5?style=flat-square)
![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)

How Plasma Opus builds on KDE Plasma: the desktop integrations added in
[M27](../ROADMAP.md#m27-integrations), and the Preferences that configure them
in [M28](../ROADMAP.md#m28-integration-settings).

**Integration plans:** **KDE Plasma** · [Nextcloud](nextcloud.md) ·
[Plasma Impresario](plasma-impresario.md)

> [!NOTE]
> **Implemented:** the KDE foundations under
> [Existing foundations](#existing-foundations), from merged milestones
> M01–M26, and the M27 integrations listed there, from default file-manager
> registration to cloud sync and cloud activity.
> **Settings:** the M28 settings for these integrations, on the **KDE Plasma**,
> **Applications**, **Remote and Cloud** and **Security** pages of
> Preferences; each foundation names its setting and location.
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
  when Plasma Opus is still the handler. **Preferences › KDE Plasma › File
  Manager** shows the current default file manager; **Set Plasma Opus as
  Default** makes Plasma Opus the user's default for folders, as System
  Settings › Default Applications does, and reports failures, including a
  desktop-specific list such as `kde-mimeapps.list` that overrides it.
  ([M27.01](../ROADMAP.md#m27-integrations),
  [#293](https://github.com/mikepadiernos/plasma-opus/issues/293))
- **File-manager service:** while Plasma Opus is the default folder handler,
  it answers `org.freedesktop.FileManager1` requests, such as **Show in
  folder** in browsers and mail clients. `ShowFolders` opens each folder in a
  new tab, `ShowItems` opens one tab per parent folder with the items
  selected, and `ShowItemProperties` opens KDE's properties dialog. It never
  replaces another owner of the service. `install.sh --default` also installs
  a per-user D-Bus activation file, so a request starts Plasma Opus when no
  file manager is running; uninstalling removes it. Install, update or remove
  the activation file under **Preferences › KDE Plasma › File Manager**,
  which leaves a file that another application wrote unchanged. Setting
  Plasma Opus as the default there also claims the service at once when no
  other application owns it.
  ([#279](https://github.com/mikepadiernos/plasma-opus/issues/279),
  [#293](https://github.com/mikepadiernos/plasma-opus/issues/293))
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
  reporting. Choose which of these operations report to Plasma under
  **Preferences › KDE Plasma › Notifications**; all do by default, and an
  operation that does not report still shows its progress and errors in its
  dialog. **Configure Plasma Notifications…** there opens Plasma's
  notification settings for Plasma Opus, which control how the reports
  appear, and reports when System Settings cannot open them.
  ([#388](https://github.com/mikepadiernos/plasma-opus/issues/388),
  [#392](https://github.com/mikepadiernos/plasma-opus/issues/392))
- **System tray and global shortcut:** while Plasma Opus runs, its icon sits
  in the system tray's hidden section. Clicking it shows or hides the window;
  its menu also opens the home folder or the transfer queue, and quits.
  The **Show or Hide Plasma Opus** global shortcut, Meta+Alt+O by default,
  brings the window to the front, or hides it when it is already in front.
  Turn it off, or choose its key combination, under **Preferences › KDE
  Plasma › Shortcuts**, which also says when no key is assigned or KDE's
  global shortcut service is not running; Plasma's Shortcuts settings can
  change it too while it is on. KDE leaves it unassigned when another
  application already uses the default. The window hides to the tray only
  while a tray can show the icon. While hidden, the icon moves out of the
  hidden section until the window is shown again. Minimizing or closing to
  the tray is off by default; turn either on, or remove the icon, under
  **Preferences › KDE Plasma › System Tray**. Closing to the tray applies
  only to the window's close button; **File › Quit** and the tray menu's
  **Quit** still quit. **Start with the window hidden in the system tray**
  there starts Plasma Opus with only its tray icon when it opens no folders
  or workspace and a tray can already show the icon; otherwise the window is
  shown as usual.
  ([#285](https://github.com/mikepadiernos/plasma-opus/issues/285),
  [#290](https://github.com/mikepadiernos/plasma-opus/issues/290),
  [#291](https://github.com/mikepadiernos/plasma-opus/issues/291); more
  shortcut settings in [M31](../ROADMAP.md#m31-keyboard-shortcut-settings))
- **Plasma Activities:** each Activity can name a workspace and a default folder
  for each pane under **Preferences › KDE Plasma › Activities**, once
  **Associate workspaces and default folders with Plasma Activities** is checked
  there; **View › Activity Associations** opens the tab. An association uses the
  workspace's own behaviours, or all, selected or none of them, independently of
  other Activities. Changes are saved with the other settings, and **Restore
  Defaults** turns associations off without clearing them. **Configure Plasma
  Activities…** there opens Plasma's Activities settings to create, rename or
  remove Activities. When the current Activity changes, a message above the
  panes offers to switch to its workspace and open its folders in new tabs;
  nothing is applied until **Apply** is chosen, and the switch asks first as any
  workspace switch does. **View › Apply Activity Association** applies it at any
  other time. Associations are kept when their Activity is removed, their
  workspace is deleted or a folder is missing, and the missing parts are
  reported instead of applied. Plasma Opus only reads Activities: it never
  creates, removes or switches them, and leaves recent-file history to KDE. It
  reports visited folders and opened files to the current Activity, as Dolphin
  does, so they appear in Plasma's recent files and locations; turn this off
  under **Preferences › KDE Plasma › Activities**. Temporary files, open
  encrypted vaults, searches, the trash and administrator locations are never
  reported, nor are passwords in remote addresses.
  ([#387](https://github.com/mikepadiernos/plasma-opus/issues/387),
  [#391](https://github.com/mikepadiernos/plasma-opus/issues/391),
  [#659](https://github.com/mikepadiernos/plasma-opus/issues/659))
- **KDE Connect:** **File › Send to Device…** sends the selected local files
  to a paired device through KDE Connect's share plugin. The dialog lists the
  files it will send and the items it cannot, such as folders, remote
  locations and missing items, and each paired device with why it cannot
  receive files when it is offline or has file sharing turned off. Nothing is
  sent until **Send** is pressed, and the last device used is chosen again.
  When KDE Connect is not running or no device is paired, the dialog says so.
  KDE Connect's own notifications show each transfer's progress and result.
  Plasma Opus never pairs devices or changes KDE Connect settings. Turn the
  action off, or stop choosing the last device used again, under
  **Preferences › KDE Plasma › KDE Connect**, where **Configure KDE
  Connect…** opens KDE Connect's own settings.
  ([#389](https://github.com/mikepadiernos/plasma-opus/issues/389),
  [#393](https://github.com/mikepadiernos/plasma-opus/issues/393))
- **KRunner:** an installed build adds a **Plasma Opus** plugin to KRunner
  and Plasma Search that offers saved workspaces and Favorites whose name
  matches the query. Choosing a workspace starts Plasma Opus, or uses the
  running window, and switches to it after asking as any workspace switch
  does; choosing a Favorite opens it in a new tab. History, searches and
  recent folders are never offered, and a match carries only an opaque id,
  so no location or credentials pass through KRunner. Workspaces or
  Favorites removed since the match was shown are not opened. Turn the
  results off, or choose which saved workspaces and Favorites are offered,
  under **Preferences › KDE Plasma › KRunner**; workspaces and Favorites
  added later are offered until they are cleared there, and KRunner uses a
  change from its next search session. Turn the whole plugin off in
  **System Settings › Search › Plasma Search**, which **Configure Plasma
  Search…** there opens. The `opus-file-manager-krunner` helper is started
  by D-Bus when KRunner first asks.
  ([#390](https://github.com/mikepadiernos/plasma-opus/issues/390),
  [#394](https://github.com/mikepadiernos/plasma-opus/issues/394))
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
  running. **Preferences › KDE Plasma › File Manager** shows whether Plasma
  Opus serves the file chooser and offers **Set Up**, **Complete Setup** for
  a partial setup and **Restore Previous Chooser**, which ask before
  restarting xdg-desktop-portal and report failures
  ([#292](https://github.com/mikepadiernos/plasma-opus/issues/292)). The
  chooser shows Places, a breadcrumb, the folder in the Plasma
  Opus list, a name filter and the application's file-type filters and
  choices. It supports opening one or several files or a folder, saving with
  the selected filter's extension after confirming an overwrite, and saving
  several files into a folder. It returns local files only, is placed over
  the requesting window and remembers its size. When the application
  suggests no folder, it starts in the home folder or, with **Start folder**
  set to **Folder of the last choice** under **Preferences › KDE Plasma ›
  File Manager**, in the folder of that application's last accepted choice,
  kept in session state; **Forget Remembered Folders** clears them
  ([#750](https://github.com/mikepadiernos/plasma-opus/issues/750)). The
  `opus-file-manager-portal` helper is started by D-Bus and exits after two
  idle minutes.
  ([#286](https://github.com/mikepadiernos/plasma-opus/issues/286))
- **Mount manager:** **File › Mount Manager…** lists mounted and
  fstab-defined file systems with their name, state, device, type, mount
  point, size, free space and a usage bar. Network and FUSE mounts, such as
  remote drives, are included. Activating an entry opens it in the active
  pane, mounting it first when needed. Mount, unmount and eject go through
  Solid and UDisks2, with Plasma's authorization prompts, and fall back to
  `mount`, `umount` and `fusermount` for fstab entries marked `user` or
  `users` and for FUSE mounts. Busy devices, denied authorization,
  cancelled prompts and failures are reported. **Go › Mount Points** lists
  each file system with a check mark when mounted and opens or mounts it.
  Partitioning, formatting and fstab editing are left to KDE Partition
  Manager, which the dialog opens when installed. Under **Preferences › KDE
  Plasma › Mount Manager**, **Show only removable devices**, on by default,
  limits **Go › Mount Points** to removable devices and is how the dialog's
  own checkbox starts each time it opens; **Hide pseudo, snap and other system
  file systems**, also on by default, can be cleared to list them, though
  Plasma Opus never mounts or unmounts them; and **Show the Mount Points menu
  on the main toolbar** adds or removes the toolbar menu
  ([#705](https://github.com/mikepadiernos/plasma-opus/issues/705)).
  ([#703](https://github.com/mikepadiernos/plasma-opus/issues/703))
- **KWin script:** the **KWin Script** group under **Preferences › KDE Plasma ›
  KWin** installs, updates, enables, disables or removes an optional Plasma Opus
  KWin script, only when asked, shows whether KWin, its scripting interface and
  the script are available and which version is running, and opens KWin's script
  settings. With the script running, applying a workspace or Activity
  association that restores the layout also restores the window's screen,
  virtual desktops, size, tile and keep-above state as last seen with that
  workspace; detached Plasma Opus windows can be placed beside the main window;
  and a workspace can be assigned to each virtual desktop and applied when that
  desktop becomes current. The **Window Management** group in the same tab
  chooses each of these behaviours and opens KWin's window rules, which Plasma
  Opus never edits
  ([#709](https://github.com/mikepadiernos/plasma-opus/issues/709)). See
  [KWin scripts](#kwin-scripts).
  ([#707](https://github.com/mikepadiernos/plasma-opus/issues/707))
- **Drop-down window:** turned on in the **Drop-Down Window** group under
  **Preferences › KDE Plasma › KWin**, which **Settings › Configure KWin
  Integration…** also opens, the main window slides in from a chosen screen
  edge, like [Yakuake](https://apps.kde.org/yakuake/), with a width and height
  as percentages of the screen, on the active or a chosen screen. **View › Show
  or Hide Drop-Down Window** toggles it; its global shortcut is unassigned until
  one is chosen in that group or in System Settings, and the script can also
  register a screen edge or a touch-screen edge. While shown, it stays above
  other windows and out of the taskbar, pager and task switcher. It can hide
  when another application is activated, but not while a dialog, menu or another
  Plasma Opus window is active, and it can switch to an assigned workspace when
  shown. Without the script it shows and hides as a regular window and says so
  once. The group also chooses the slide animation and whether it hides on
  focus loss, and says whether the global shortcut works
  ([#710](https://github.com/mikepadiernos/plasma-opus/issues/710)).
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
  backends can only extract, such as RAR without `rar`. Set the default
  format, whether to split into volumes and their size, and whether to offer
  a ZIP password under **Preferences › Archives › Defaults**, which also
  reports whether Ark can write encrypted 7z archives. See
  [Split archives](../CAVEATS.md#archives) and
  [Archive settings](../CAVEATS.md#integration-settings).
  ([#647](https://github.com/mikepadiernos/plasma-opus/issues/647),
  [#665](https://github.com/mikepadiernos/plasma-opus/issues/665))
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
  failures show the tool's output. Turn AppImage, Flatpak and Snap actions
  on or off separately under **Preferences › Applications › Packages**,
  which reports the missing tools; other packages then open like any other
  file. The same tab turns off **Add to Application Menu**, while existing
  entries can still be removed, and can add AppImages without running their
  runtime to read their name and icon. See
  [Application packages](../CAVEATS.md#desktop-integration) and
  [Package settings](../CAVEATS.md#integration-settings).
  ([#281](https://github.com/mikepadiernos/plasma-opus/issues/281),
  [#296](https://github.com/mikepadiernos/plasma-opus/issues/296))

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
  own. Choose whether the search panel offers ranking and the live search
  under **Preferences › KDE Plasma › Baloo**, which also shows the indexer's
  state and opens KDE File Search settings; the search defaults stay under
  **Preferences › Tools › Search**
  ([#443](https://github.com/mikepadiernos/plasma-opus/issues/443)). See
  [Indexed metadata and ranking](../CAVEATS.md#search).
  ([#442](https://github.com/mikepadiernos/plasma-opus/issues/442))

- **Baloo queries:** Indexed searches pass required content terms and
  alternatives, folder, image, audio and video types and modification dates
  to Baloo, so it returns fewer files for Plasma Opus to check. Searches
  limited in depth or to workspace locations ask Baloo about each location
  and fall back to the whole index for a location it hasn't indexed. Baloo
  6.32 and later search metadata through its metadata scope. See
  [Indexed search](../CAVEATS.md#search).
  ([#733](https://github.com/mikepadiernos/plasma-opus/issues/733))

- **Shared searches:** **Share with KDE Places…** in Preferences' **Manage
  Saved Searches and Collections…** adds a saved search to Places as a Baloo
  search, so Dolphin, file dialogs and other KDE applications can open it.
  The confirmation names the folder searched and lists the criteria Baloo
  can't express, such as sizes, tags, NOT terms, depth limits, path filters
  and exclusions, which the shared place ignores. Searches Baloo can't
  express at all are refused. The place is a copy: Plasma Opus never changes
  or removes it, but offers to replace a place of the same name when the
  search is shared again. See [Shared searches](../CAVEATS.md#search).
  ([#734](https://github.com/mikepadiernos/plasma-opus/issues/734))

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
  cancellation. **Large Files**, over 100 MiB by default, always searches a
  chosen folder live, since Baloo doesn't index file sizes. Choose which
  categories Places lists, and the size above which a file is large, under
  **Preferences › KDE Plasma › Baloo**; removing an entry from Places clears
  its category there
  ([#660](https://github.com/mikepadiernos/plasma-opus/issues/660)). See
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
  without starting anything. The embedded terminal panel's menu gains
  **Terminal Component**, which chooses the Konsole or Kwi component for new
  sessions; open sessions keep theirs, and a missing component is reported
  instead of replaced. Choose the terminal that **Open Terminal Here** opens,
  KDE's default terminal unless changed, and the embedded terminal's component
  under **Preferences › Applications › Terminal**, which marks the terminals
  that aren't installed
  ([#295](https://github.com/mikepadiernos/plasma-opus/issues/295)). See
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
  the search. Hide the action, or choose whether the search uses the current
  session, a new one when it is busy or always a new one, under
  **Preferences › Applications › Terminal**
  ([#367](https://github.com/mikepadiernos/plasma-opus/issues/367)). See
  [Embedded terminal](../CAVEATS.md#embedded-terminal).
  ([#364](https://github.com/mikepadiernos/plasma-opus/issues/364))

- **Editor integrations:** **Tools › Edit With**, also in the context menu
  of selected files, opens them in [KWrite](https://apps.kde.org/kwrite/) or
  [Kate](https://apps.kde.org/kate/) as separate applications, or in Neovim,
  Rune, micro or nano in a new embedded terminal tab. Editors that aren't
  installed are disabled with a tooltip saying so. Files are passed as
  absolute paths, without a shell; terminal editors start in the first
  file's folder and their tab closes when they exit. KWrite and Kate also
  open remote files, while terminal editors report them, as they do folders.
  **Open Editor** still opens the built-in editor. Choose whether terminal
  editors run in the embedded terminal or in a separate window of KDE's
  default terminal under **Preferences › Applications › Editor**
  ([#320](https://github.com/mikepadiernos/plasma-opus/issues/320)). See
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
  application handles editing, saving and format conversion. Choose the
  application that **Edit** opens office documents in, and whether the
  preview panel shows them, under **Preferences › Applications › Office**,
  which marks the applications that aren't installed and lists the viewer
  component that previews each kind of document; a chosen application that
  isn't installed is reported rather than replaced. See
  [Office documents](../CAVEATS.md#desktop-integration) and
  [Office settings](../CAVEATS.md#integration-settings).
  ([#674](https://github.com/mikepadiernos/plasma-opus/issues/674),
  [#676](https://github.com/mikepadiernos/plasma-opus/issues/676))
- **Edit action:** **Tools › Edit**, <kbd>F4</kbd>, also in the context
  menu of a single editable file, opens the selected file in a suitable tool:
  text files in the built-in editor; office documents in LibreOffice,
  Calligra or ONLYOFFICE; SVG drawings in [Inkscape](https://inkscape.org/),
  or as source in the built-in editor; images in
  [KolourPaint](https://apps.kde.org/kolourpaint/),
  [Krita](https://krita.org/) or [GIMP](https://www.gimp.org/); and audio in
  [Audacity](https://www.audacityteam.org/) or
  [Tenacity](https://tenacityaudio.org/). It uses the first installed tool
  in that order, and its tooltip names the tool or the tools to install.
  Files that can be viewed but not edited, such as videos and PDF documents,
  leave it disabled. Choose KWrite, Kate, Neovim, Rune, micro or nano for
  text files instead of the built-in editor under
  **Preferences › Applications › Editor**, which marks the editors that
  aren't installed; a chosen editor that isn't installed is reported rather
  than replaced
  ([#320](https://github.com/mikepadiernos/plasma-opus/issues/320)). Choose
  the office application likewise under **Preferences › Applications ›
  Office** ([#676](https://github.com/mikepadiernos/plasma-opus/issues/676)).
  See
  [Edit action](../CAVEATS.md#desktop-integration).
  ([#675](https://github.com/mikepadiernos/plasma-opus/issues/675))
- **Editing in the preview:** When the preview panel shows a text file,
  **Edit** switches it in place to the built-in KTextEditor editor with the
  whole file, not the truncated preview, using the editor's drafts, Markdown
  preview and settings. Saving or **Close Editor** returns to the read-only
  preview. While changes are unsaved, the file stays open when the selection
  or folder changes, and **Preview Selection…**, hiding the panel or quitting
  asks to save, discard or keep editing. **Move to Editor** continues the
  session in the editor panel with its unsaved changes. Read-only, remote,
  binary and very large files are reported rather than opened. Under
  **Preferences › Applications › Editor**, **Preview Editing** chooses
  whether text previews offer **Edit**, whether double-clicking the preview
  starts editing and whether saving returns to the preview; editing in the
  preview always uses the built-in editor. See
  [Editing in the preview](../CAVEATS.md#editor) and
  [Preview editing settings](../CAVEATS.md#integration-settings).
  ([#729](https://github.com/mikepadiernos/plasma-opus/issues/729),
  [#730](https://github.com/mikepadiernos/plasma-opus/issues/730))
- **Markdown vault links:** The built-in editor's **Vault** menu chooses a
  local folder of Markdown notes, such as an Obsidian vault, without
  changing its notes or configuration. In a note inside that folder, the
  Markdown preview turns `[[wiki links]]` into links that open the named
  note in the editor, keeping unsaved changes as a draft, and the
  **Backlinks** list shows the notes that link to the open one. Missing,
  ambiguous and out-of-vault links, and incomplete scans, are reported
  below the list. Choose the vault folder, the preview dialect, whether the
  preview shows when a Markdown file opens, whether wiki links open notes and
  which lists the vault panel shows under **Preferences › Applications ›
  Editor**. See [Markdown vault links](../CAVEATS.md#desktop-integration) and
  [Markdown settings](../CAVEATS.md#integration-settings).
  ([#564](https://github.com/mikepadiernos/plasma-opus/issues/564),
  [#577](https://github.com/mikepadiernos/plasma-opus/issues/577))
- **Workspace vaults and tags:** In **Workspaces**, **Use as Editor Vault**
  makes one of a workspace's assigned local folders the vault of every
  editor while the workspace is active, and the editor's own vault returns
  afterwards. Beside **Backlinks**, the **Links** tab lists the open note's
  wiki links with whether each resolves, and the **Tags** tab lists its
  inline `#tags` and front matter `tags:` with the other notes that share
  them; activating an entry opens the note. Under **Preferences ›
  Applications › Editor**, **Workspace Vault** turns each workspace's vault
  on or off, chooses its location and can give it its own Markdown mode,
  preview dialect, preview, wiki link and vault panel options, kept with the
  workspace; it reports a vault location that can't be found and how many
  notes the vault has. See
  [Workspace vaults and tags](../CAVEATS.md#desktop-integration) and
  [Workspace vault settings](../CAVEATS.md#integration-settings).
  ([#565](https://github.com/mikepadiernos/plasma-opus/issues/565),
  [#566](https://github.com/mikepadiernos/plasma-opus/issues/566))
- **Embedded mpv playback:** When Plasma Opus is built with libmpv, the
  preview panel and gallery can play audio and video through an embedded
  [mpv](https://mpv.io/) player instead of Qt Multimedia. Choose it with the
  backend button beside the playback controls, or with **Audio and video
  playback** under **Preferences › Applications › Media**, which reports
  when this build has no mpv support or mpv can't show video, and whether
  the mpv application is installed; an open preview follows the choice.
  Video needs OpenGL; when mpv is missing or can't draw, Qt Multimedia plays
  the file and a notice says why. The embedded player ignores your
  `mpv.conf` and scripts. **Open with mpv** in the context menu still opens
  local audio and video in the mpv application. See
  [Embedded mpv playback](../CAVEATS.md#desktop-integration) and
  [Media settings](../CAVEATS.md#integration-settings).
  ([#563](https://github.com/mikepadiernos/plasma-opus/issues/563),
  [#578](https://github.com/mikepadiernos/plasma-opus/issues/578))
- **Ebooks, comics, help files and fonts:** The preview panel shows EPUB,
  MOBI, FictionBook, comic book archives such as CBZ and CBR, DjVu and CHM
  help files through [Okular](https://okular.kde.org/)'s viewer component
  when it is installed, and names what to install when it isn't. Remote
  files download first, within the 128 MiB remote preview limit. Fonts show
  their family and style with editable sample text at several sizes.
  **Install Font…** opens Plasma's font installer, `kfontinst`, for
  TrueType, OpenType, Type 1 and bitmap fonts. Turn previews of ebooks and
  DjVu documents, comics, help files and fonts on or off, and set the
  sample text that every font preview shows, under **Preferences ›
  Applications › Ebooks and Fonts**, which lists the viewer component for
  each kind and reports when `kfontinst` is missing; editing the text in a
  preview changes the same setting. See
  [Ebook and font previews](../CAVEATS.md#desktop-integration) and
  [Ebook and font settings](../CAVEATS.md#integration-settings).
  ([#650](https://github.com/mikepadiernos/plasma-opus/issues/650),
  [#661](https://github.com/mikepadiernos/plasma-opus/issues/661))
- **Remote connections:** **Go › Remote Connections** lists saved SSH
  (SFTP), WebDAV and WebDAV over HTTPS connections. Each one opens in the
  active pane or the other pane through KDE's KIO workers, and SSH
  connections can also open an SSH session in the embedded terminal.
  **Manage Remote Connections…** adds, edits and removes them, with a name,
  host, port, user, starting folder and sign-in method: a password that KIO
  asks for when needed, or an SSH key or agent. Passwords are never saved
  with the connection; KIO keeps them in KWallet when you choose to. KIO and
  `ssh` verify SSH host keys and TLS certificates, and connection and
  sign-in errors appear in the pane. Unencrypted WebDAV shows a warning. Manage
  saved connections under **Preferences › Remote and Cloud › Connections** too,
  where changes take effect immediately, choose the protocol, user and SSH
  sign-in method that new connections start with, and open KDE Wallet Manager to
  review saved passwords. See [Remote
  connections](../CAVEATS.md#desktop-integration) and [Remote connection
  settings](../CAVEATS.md#integration-settings).
  ([#282](https://github.com/mikepadiernos/plasma-opus/issues/282),
  [#297](https://github.com/mikepadiernos/plasma-opus/issues/297))
- **Network shares and FTP:** Remote connections also cover Windows shares
  (SMB), FTP and FTP over TLS (FTPS) through KIO. An SMB user can include a
  domain as `DOMAIN;user`. Plain FTP shows a warning that it sends passwords and
  files unencrypted, and the dialog names the package to install when no KIO
  worker handles a protocol. **Go › Network**, also in the Remote Connections
  menu, opens KIO's `network:/` location to browse shares and services
  discovered on the local network, or Dolphin's `remote:/` Network folder when
  `network:/` isn't installed. Show or hide it under **Preferences › Remote and
  Cloud › Connections**. See [Network shares and
  FTP](../CAVEATS.md#desktop-integration) and [Network discovery
  settings](../CAVEATS.md#integration-settings).
  ([#651](https://github.com/mikepadiernos/plasma-opus/issues/651),
  [#662](https://github.com/mikepadiernos/plasma-opus/issues/662))
- **rclone cloud storage:** **Go › Cloud Storage** lists the remotes
  configured in [rclone](https://rclone.org/), such as Dropbox, OneDrive,
  Box, MEGA and pCloud, with their types. Choosing one mounts it with
  `rclone mount` in a private folder under the user's runtime directory and
  opens it in the active pane; applications can open and save its files in
  place, as rclone caches writes. Mounted remotes are checked, and the
  menu's **Unmount** section stops a mount after moving either pane out of
  it. Plasma Opus stops the mounts it started when it quits.
  Plasma Opus only reads remote names and types: it never reads, stores or
  changes rclone's configuration or credentials, and rclone reports an
  error instead of asking for a configuration password. Missing rclone,
  listing and mount errors are reported.
  Under **Preferences › Remote and Cloud › rclone**, turn the integration off to
  hide **Go › Cloud Storage** and its shortcut, clear the remotes the menu
  leaves out, and choose to serve remotes over WebDAV instead of mounting them.
  A served remote runs `rclone serve webdav` on 127.0.0.1, at a port derived
  from its name, and opens through KIO's WebDAV worker; it needs no FUSE. Its
  password is created once, passed to rclone in its environment rather than on
  the command line, and kept by KDE's password server, in KDE Wallet when it's
  enabled, so KIO signs in without asking and the location shows no password.
  Served remotes are checked too, and the menu's **Stop Serving** section stops
  them. See [rclone cloud storage](../CAVEATS.md#desktop-integration) and
  [rclone settings](../CAVEATS.md#integration-settings).
  ([#652](https://github.com/mikepadiernos/plasma-opus/issues/652),
  [#663](https://github.com/mikepadiernos/plasma-opus/issues/663))
- **Remote drives:** each saved connection in **Go › Remote Connections**
  offers **Mount as Folder**, which mounts it as a local folder that
  applications without KIO support can open and save in, opens it in the
  active pane and adds it to Places until it is unmounted. **Mount Options…**
  chooses how: KDE's KIO FUSE by default for every protocol, sshfs for SSH,
  davfs2 for WebDAV or the kernel CIFS client for Windows shares. sshfs,
  davfs2 and CIFS mounts can be read-only and use an empty folder under the
  user's runtime directory or a chosen empty folder; any connection can mount
  when Plasma Opus starts. A mounted connection offers **Open Mounted
  Folder** and **Unmount**, and by default Plasma Opus unmounts the sshfs,
  davfs2 and CIFS mounts it started when it quits. Credentials stay with KDE and
  the backends: KIO FUSE asks KDE's password server, sshfs uses the SSH agent,
  keys or KDE's `ksshaskpass` prompt, and davfs2 and CIFS mount only through a
  user-mountable fstab entry, for which the dialog shows an example line. Plasma
  Opus never edits fstab or backend configuration. Missing tools, missing or
  unsuitable fstab entries, folders that aren't empty, stale mounts, busy
  folders and failed unmounts are reported. Under **Preferences › Remote and
  Cloud › Remote Drives**, choose the backend each protocol mounts with, from
  KIO FUSE and the installed alternatives, the folder that holds sshfs, davfs2
  and CIFS mounts, and whether they mount read-only, for connections without
  **Mount Options…** of their own. The same tab starts Plasma Opus in the system
  tray at login, through an autostart entry, to mount the connections chosen to
  mount at startup; mounts the connections holding the Favorites and Places
  assigned to a workspace when it is activated; and chooses whether sshfs,
  davfs2 and CIFS mounts stay after Plasma Opus quits. sshfs then runs on its
  own, and the next session takes the mounts it finds in place over, so that
  they can be opened and unmounted from the menu. See [Remote
  drives](../CAVEATS.md#desktop-integration) and [Remote drive
  settings](../CAVEATS.md#integration-settings).
  ([#702](https://github.com/mikepadiernos/plasma-opus/issues/702),
  [#704](https://github.com/mikepadiernos/plasma-opus/issues/704))
- **Temporary LAN sharing:** **File › Share on Local Network…** shares the
  selected local items, or the current folder when nothing is selected, with
  browsers and WebDAV clients on the local network until the dialog closes,
  Plasma Opus quits or a time limit, 30 minutes by default, runs out. Each
  link contains a random token, and shared items are read-only. The dialog
  offers the computer's network addresses, checking only private ones, a
  port, HTTPS with a certificate made for the share, and an optional upload
  folder, Downloads by default, where uploads never replace existing files.
  While sharing, it shows each address's link with **Copy Link** and a QR
  code, the certificate's fingerprint, the time left and each client's
  requests and transfers. Browsers get a folder listing, with an upload form
  in the upload folder; WebDAV clients can browse, download and upload. Hidden
  files and symbolic links leading outside the shared items aren't served.
  Plasma Opus never opens firewall ports, warns when the connection isn't
  encrypted and points to KDE Connect and Samba sharing.
  **Preferences › Remote and Cloud › LAN Sharing** sets the dialog's starting
  choices: whether to serve browsers, WebDAV file managers or both, HTTPS, the
  network interfaces whose private addresses start checked, the port, the time
  limit, and whether uploads are allowed and to which folder. The dialog also
  offers **Serve to:**, and choices made in it apply to that share only. See
  [Temporary LAN sharing](../CAVEATS.md#desktop-integration) and [LAN sharing
  settings](../CAVEATS.md#integration-settings).
  ([#653](https://github.com/mikepadiernos/plasma-opus/issues/653),
  [#664](https://github.com/mikepadiernos/plasma-opus/issues/664))
- **KDE Online Accounts:** **Go › Online Accounts** lists the accounts set up
  in System Settings' Online Accounts page, each with the folders it gives
  access to: Google Drive through
  [kio-gdrive](https://apps.kde.org/kio_gdrive/), and Nextcloud and
  ownCloud storage through KIO's WebDAV worker. Each folder opens in the
  active pane or the other pane. Turned-off accounts and services, accounts
  without a server address and folders whose KIO worker isn't installed are
  disabled; the tooltip explains why and names what to install. Accounts
  without file storage aren't listed. **Manage Online Accounts…** opens
  the System Settings page to add, remove or sign in to accounts. The menu
  rereads the accounts each time it opens, so removed accounts disappear.
  Plasma Opus only reads account names, services and server addresses; it
  never reads or stores tokens or passwords, which stay with KDE's providers
  and KIO, and KIO reports denied access and expired sign-ins in the pane.
  Plasma Opus reads the accounts, and offers the menu and the accounts menu
  of cloud sync pairs, only after checking **Show the folders of KDE Online
  Accounts in Plasma Opus** under **Preferences › KDE Plasma › Online
  Accounts**, where you also choose the accounts to show; clearing an
  account only hides it in Plasma Opus, and **Manage Online Accounts…**
  there opens KDE's settings
  ([#371](https://github.com/mikepadiernos/plasma-opus/issues/371)).
  See [KDE Online Accounts](../CAVEATS.md#desktop-integration).
  ([#370](https://github.com/mikepadiernos/plasma-opus/issues/370))
- **rsync backend:** **File › Compare and Synchronize Folders…** offers
  **Synchronize with › rsync** when [rsync](https://rsync.samba.org/) is
  installed, and also opens when one pane shows a folder on an SSH host
  through `sftp:/` or `fish:/`; rsync then compares the folders too, with the
  same filters, profiles and comparison options. Plans start with every item
  skipped. Before anything changes, Plasma Opus checks that rsync can carry
  out the plan exactly: one direction per run, deleting only files missing
  from the source, with every parent folder planned. An rsync dry run then
  itemizes the new files, folders, overwrites and deletions for
  confirmation, and planned items rsync would skip are named. The run copies
  and deletes only the listed items, honours the copy policy and speed limit,
  and reports progress and cancellation to Plasma's job view; the report
  lists what rsync changed and any errors. SSH keeps OpenSSH's host-key
  checks, and KDE's ksshaskpass asks for passwords; passwords never reach the
  command line. Under **Preferences › Remote and Cloud › Sync**, the **rsync
  Backend** group turns rsync on or off, chooses whether local folders start
  with rsync, and sets the comparison profile and copy policy used when the
  dialog starts with rsync. It starts with safe defaults: rsync replaces only
  older files and doesn't delete files until deletions are allowed there, and
  the dry run and confirmation stay required either way. See [rsync
  backend](../CAVEATS.md#desktop-integration) and [rsync
  settings](../CAVEATS.md#integration-settings).
  ([#363](https://github.com/mikepadiernos/plasma-opus/issues/363),
  [#366](https://github.com/mikepadiernos/plasma-opus/issues/366))
- **Cloud sync:** **File › Cloud Sync…** keeps chosen local folders in
  two-way sync with KIO locations, such as Nextcloud, WebDAV, SFTP, SMB or S3
  folders and KDE Online Accounts storage, or with rclone remotes. Each pair
  runs as a two-way sync, a cloud backup or a local backup, optionally
  mirroring deletions, with saved filters, include and exclude patterns and
  an interval. Network-folder pairs keep a snapshot of both sides, so a run
  tells edits, additions, deletions and renames on either side apart; a file
  changed on both sides is a conflict that the review resolves by keeping
  one or both versions, and nothing is overwritten silently. Local deletions
  go to the trash. The first run and large deletions stop for review, and
  **Review…** previews any run. rclone pairs use `rclone bisync`, or `copy`
  and `sync` for backups. Pairs can be paused one at a time or by account,
  background runs pause on metered connections, and folders owned by
  Nextcloud, ownCloud, Syncthing or Dropbox clients are refused. Nested pairs
  can exclude each other. **Synchronize in the background after login**
  starts Plasma Opus in the system tray, which asks for attention when a run
  needs review. Pairs store no passwords. Each pair also chooses how conflicts
  are resolved: **Ask When Reviewing**, **Keep Both**, **Keep Local Version** or
  **Keep Remote Version**; background and **Sync Now** runs apply that choice,
  and reviews start from it. Under **Preferences › Remote and Cloud › Sync**,
  manage the same pairs with the same editor, with changes taking effect
  immediately, and choose whether Plasma Opus starts in the system tray at login
  to synchronize them, whether background runs pause on metered connections, and
  how often pairs that use the default interval are checked, 15 minutes unless
  changed, since remotes don't report changes. See [Cloud
  sync](../CAVEATS.md#desktop-integration) and [Cloud sync
  settings](../CAVEATS.md#integration-settings).
  ([#718](https://github.com/mikepadiernos/plasma-opus/issues/718),
  [#720](https://github.com/mikepadiernos/plasma-opus/issues/720))
- **Cloud activity:** **File › Cloud Activity…** and the tray menu's
  **Cloud Activity…** open one window for cloud sync pairs and rclone mounts.
  It lists each connection with its state, free space and pending uploads,
  and each transfer with its speed and remaining time, above a speed graph
  and a list of errors. **Pause All** pauses queued transfers and background
  sync runs until **Resume All**; mounted remotes keep uploading. rclone
  bandwidth, parallel transfer and cache size limits apply to mounts and
  sync runs. Unmounting a remote or quitting Plasma Opus waits for pending
  uploads, with **Continue Without Waiting** and **Cancel**. Connections can
  be exported and imported without passwords; imported pairs start paused.
  The same limits, and how long files not in use stay in the offline cache, are
  under **Preferences › Remote and Cloud › Transfers and Cache**, which also
  chooses whether the tray menu offers **Cloud Activity…**. They apply only to
  rclone; remote drives mounted with KIO FUSE, sshfs, davfs2 or CIFS, and cloud
  sync pairs with network folders, ignore them, as the tab says. See [Cloud
  activity](../CAVEATS.md#desktop-integration) and [Transfer and cache
  settings](../CAVEATS.md#integration-settings).
  ([#719](https://github.com/mikepadiernos/plasma-opus/issues/719),
  [#721](https://github.com/mikepadiernos/plasma-opus/issues/721))
- **Administrator mode:** **File › Act as Administrator** reopens the active
  pane's local folder through KIO's `admin:` worker from the `kio-admin`
  package, as in Dolphin, after a warning that can be turned off. KDE's
  authorization prompt asks for an administrator password, and the worker
  performs each operation as root. A warning bar above the folder offers
  **Stop Acting as Administrator**; turning the action off or opening a
  location outside the `admin:` view also ends it. Other tabs and the other
  pane are unaffected, and sessions and workspaces save the tab as the
  ordinary folder, so administrator access is never restored. A missing
  `kio-admin` package and remote locations are reported. See
  [Administrator mode](../CAVEATS.md#desktop-integration).
  ([#288](https://github.com/mikepadiernos/plasma-opus/issues/288))
- **MCP server:** `opus-file-manager-mcp` is a Model Context Protocol server
  that MCP clients, such as AI assistants and editors, start when they need
  it. It speaks JSON-RPC 2.0 over standard input and output and offers tools
  to list folders, describe items, read UTF-8 text files, search by name and
  show a folder in Plasma Opus. Access is read-only by default and limited to
  the home folder. With write access, clients can also create folders, save
  text files, copy, move and rename items and move them to the trash; nothing
  is deleted permanently, and an existing file is replaced only when the
  client asks to overwrite it. Every path, and the target of every symbolic
  link followed, must stay inside the allowed folders, and password-protected
  folders are refused as if locked. Each call is recorded,
  without file contents, in `~/.local/state/opus-file-manager/mcp-audit.log`.
  To use it, add the command to the client's MCP servers:

  ```json
  { "mcpServers": { "plasma-opus": { "command": "opus-file-manager-mcp" } } }
  ```

  Choose whether clients may read or also change files, or turn access off,
  the folders they may use and whether calls are logged under
  **Preferences › Security › MCP**, which also shows this configuration and
  the audit log. Running servers follow the settings from the next request.
  No access token is needed, because clients start the server themselves as
  you and no network port is opened. The `--read-only` option keeps one
  client to reading. See
  [MCP server](../CAVEATS.md#desktop-integration) and
  [MCP settings](../CAVEATS.md#integration-settings).
  ([#287](https://github.com/mikepadiernos/plasma-opus/issues/287),
  [#294](https://github.com/mikepadiernos/plasma-opus/issues/294))

- **Password-gated folders:** **File › Password Protection › Protect with
  Password…** asks for a password before Plasma Opus shows a local folder. A
  protected folder starts locked in each session: Plasma Opus shows it with a
  lock icon and an **Unlock…** bar, and does not list, preview, search,
  copy, move, trash or open a terminal in it or anything inside it until it
  is unlocked. **Lock Protected Folders** locks them again, and the same
  submenu changes or removes a password. **Preferences › Security ›
  Password-Gated Folders** lists the protected folders and whether each is
  locked, and protects, unlocks, locks, changes or removes them; its changes
  take effect at once in every window, even if Preferences is cancelled.
  Plasma Opus stores a salted PBKDF2-SHA256 verifier, never the password. The
  gate applies only inside Plasma Opus and is not encryption; other
  applications open the folder freely. See
  [Password-gated folders](../CAVEATS.md#password-gated-folders).
  ([#402](https://github.com/mikepadiernos/plasma-opus/issues/402),
  [#403](https://github.com/mikepadiernos/plasma-opus/issues/403))
- **Plasma Vault:** **Go › Plasma Vaults** lists Plasma Vault's encrypted
  CryFS, gocryptfs and EncFS vaults with their state, read from its
  `org.kde.plasmavault` kded6 module. **Open Vault…** asks Plasma Vault to
  open a vault with its own password prompt, then shows it in the active
  pane; **Show Vault Contents**, **Close Vault**, **Configure Vault…** and
  **Create Vault…** use the same service. Before closing, Plasma Opus lists
  the programs using files in the vault and asks first. Plasma Opus stores no
  vault passwords or vault configuration, and reports a missing Plasma Vault
  in the menu. **Preferences › Security › Encryption** lists the vaults with
  their state, hides the menu and its shortcut, leaves chosen vaults and
  their messages out of it, and can leave out whether each vault is open or
  closed. See [Plasma Vault](../CAVEATS.md#desktop-integration) and
  [Plasma Vault settings](../CAVEATS.md#integration-settings).
  ([#654](https://github.com/mikepadiernos/plasma-opus/issues/654),
  [#669](https://github.com/mikepadiernos/plasma-opus/issues/669))
- **File encryption:** **File › Encryption**, also in the context menu,
  encrypts selected files for OpenPGP keys or with a password, decrypts
  them, saves detached signatures and verifies signed files through GnuPG.
  When Kleopatra is installed, **Sign and Encrypt with Kleopatra…** and
  **Decrypt and Verify with Kleopatra…** open its own file dialogs. AES Crypt
  (`.aes`) files are encrypted and decrypted through an installed `aescrypt`
  tool, which reads the password from a private file rather than the command
  line. Results are new files beside the originals, and the information
  panel names encrypted, signed and signature files. Keys and key passwords
  stay with GnuPG and Kleopatra. **Preferences › Security › Encryption**
  offers or hides the GnuPG and Kleopatra actions and the AES Crypt actions,
  together with their shortcuts, and hides **File › Encryption** when both
  are off. It also chooses whether encrypting and signing start with text
  (`.asc`) output, whether binary encrypted files end in `.gpg` or `.pgp`,
  and whether decrypted copies take back their name before encryption or
  are marked, as in `report (decrypted).pdf`. See
  [File encryption](../CAVEATS.md#desktop-integration) and
  [File encryption settings](../CAVEATS.md#integration-settings).
  ([#655](https://github.com/mikepadiernos/plasma-opus/issues/655),
  [#670](https://github.com/mikepadiernos/plasma-opus/issues/670))

## Planned features

Grouped by the milestone that tracks them.

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
- **Versions:** the **KWin** tab compares the installed package's version with
  the bundled one and offers **Update**, and reports when KWin still runs an
  older version. Installing or updating reloads the script.
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
  **KWin** tab lists for workspace assignment, and the desktop that becomes
  current.
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
- A GraphQL or other server layer in front of Baloo. Plasma Opus is a local
  client and already calls Baloo's library directly; a query server would add
  serialization and latency without new capabilities. Use D-Bus for local
  automation.

## Acceptance

Exercise supported Plasma and Qt/KDE versions, KWin script compatibility,
unavailable services, remote locations, sandbox and portal access, permission
failures and cancellation. Use temporary test configuration and synthetic
credentials. Verify that disabling an optional integration preserves existing
files, layouts, accounts, window rules and desktop settings.
