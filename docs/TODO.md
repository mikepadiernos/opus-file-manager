# Pending tasks

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap](https://img.shields.io/badge/roadmap-M01%E2%80%93M32-8957e5?style=flat-square)](ROADMAP.md)
[![Caveats](https://img.shields.io/badge/caveats-limitations-bf8700?style=flat-square)](CAVEATS.md)
[![Project board](https://img.shields.io/badge/project-board-6e7781?style=flat-square&logo=github)](https://github.com/users/mikepadiernos/projects/3)

These handoffs describe blocked or unfinished work and where to resume it. Each
entry records the blocker, the affected milestone and issues, the confirmed
state and the steps to resume.

> [!IMPORTANT]
> The project board is authoritative. Confirm the current issue and project
> status before acting, and never move an item backward. A handoff is not a
> completion marker.

## Contents

Board statuses were last confirmed on 2026-11-27.

| Handoff                                                                                       | Milestone                                                                      | Issues         | Board status                                                                                                                                    |
| --------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | -------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| [mpv integration](#mpv-integration-563)                                                       | [M27](ROADMAP.md#m27-integrations)                                             | [#563], [#578] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) |
| [Markdown editor and backlinks](#markdown-editor-and-backlinks-564)                           | [M27](ROADMAP.md#m27-integrations)                                             | [#564]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Workspace vault editing](#workspace-vault-editing-565)                                       | [M27](ROADMAP.md#m27-integrations), [M28](ROADMAP.md#m28-integration-settings) | [#565], [#566] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) |
| [System file chooser on Plasma](#system-file-chooser-on-plasma-286)                           | [M27](ROADMAP.md#m27-integrations)                                             | [#286]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Mount manager on Plasma](#mount-manager-on-plasma-703)                                       | [M27](ROADMAP.md#m27-integrations)                                             | [#703]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [KWin script on Plasma](#kwin-script-on-plasma-707-708)                                       | [M27](ROADMAP.md#m27-integrations)                                             | [#707], [#708] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) |
| [Administrator mode on Plasma](#administrator-mode-on-plasma-288)                             | [M27](ROADMAP.md#m27-integrations)                                             | [#288]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Plasma Vault on Plasma](#plasma-vault-on-plasma-654)                                         | [M27](ROADMAP.md#m27-integrations)                                             | [#654]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [File encryption on Plasma](#file-encryption-on-plasma-655)                                   | [M27](ROADMAP.md#m27-integrations)                                             | [#655]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Send by Email on Plasma](#send-by-email-on-plasma-656)                                       | [M27](ROADMAP.md#m27-integrations)                                             | [#656]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Mail attachments on Plasma](#mail-attachments-on-plasma-657)                                 | [M27](ROADMAP.md#m27-integrations)                                             | [#657]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Mail file preview on Plasma](#mail-file-preview-on-plasma-658)                               | [M27](ROADMAP.md#m27-integrations)                                             | [#658]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Shared Activity context on Plasma](#shared-activity-context-on-plasma-659)                   | [M27](ROADMAP.md#m27-integrations)                                             | [#659]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Default file manager on Plasma](#default-file-manager-on-plasma-293)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#293]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [File chooser settings on Plasma](#file-chooser-settings-on-plasma-292)                       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#292]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Chooser start folder on Plasma](#chooser-start-folder-on-plasma-750)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#750]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Operation notifications on Plasma](#operation-notifications-on-plasma-392)                   | [M28](ROADMAP.md#m28-integration-settings)                                     | [#392]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Start hidden in tray on Plasma](#start-hidden-in-tray-on-plasma-290)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#290]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Global shortcut on Plasma](#global-shortcut-on-plasma-291)                                   | [M28](ROADMAP.md#m28-integration-settings)                                     | [#291]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Activity associations on Plasma](#activity-associations-on-plasma-391)                       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#391]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [KDE Connect settings on Plasma](#kde-connect-settings-on-plasma-393)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#393]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [KRunner settings on Plasma](#krunner-settings-on-plasma-394)                                 | [M28](ROADMAP.md#m28-integration-settings)                                     | [#394]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Online account settings on Plasma](#online-account-settings-on-plasma-371)                   | [M28](ROADMAP.md#m28-integration-settings)                                     | [#371]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [KWin script settings on Plasma](#kwin-script-settings-on-plasma-709)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#709]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Drop-down settings on Plasma](#drop-down-settings-on-plasma-710)                             | [M28](ROADMAP.md#m28-integration-settings)                                     | [#710]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Baloo search settings on Plasma](#baloo-search-settings-on-plasma-443)                       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#443]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Office settings on Plasma](#office-settings-on-plasma-676)                                   | [M28](ROADMAP.md#m28-integration-settings)                                     | [#676]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Mail client settings on Plasma](#mail-client-settings-on-plasma-666)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#666]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Remote connection settings on Plasma](#remote-connection-settings-on-plasma-297)             | [M28](ROADMAP.md#m28-integration-settings)                                     | [#297]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [rclone settings on Plasma](#rclone-settings-on-plasma-663)                                   | [M28](ROADMAP.md#m28-integration-settings)                                     | [#663]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Remote drive settings on Plasma](#remote-drive-settings-on-plasma-704)                       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#704]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Cloud sync settings on Plasma](#cloud-sync-settings-on-plasma-720)                           | [M28](ROADMAP.md#m28-integration-settings)                                     | [#720]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [rsync settings on Plasma](#rsync-settings-on-plasma-366)                                     | [M28](ROADMAP.md#m28-integration-settings)                                     | [#366]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Transfer and cache settings on Plasma](#transfer-and-cache-settings-on-plasma-721)           | [M28](ROADMAP.md#m28-integration-settings)                                     | [#721]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [LAN sharing settings on Plasma](#lan-sharing-settings-on-plasma-664)                         | [M28](ROADMAP.md#m28-integration-settings)                                     | [#664]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Plasma Vault settings on Plasma](#plasma-vault-settings-on-plasma-669)                       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#669]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [File encryption settings on Plasma](#file-encryption-settings-on-plasma-670)                 | [M28](ROADMAP.md#m28-integration-settings)                                     | [#670]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud accounts on a real server](#nextcloud-accounts-on-a-real-server-588)               | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#588]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud browsing on a real server](#nextcloud-browsing-on-a-real-server-365)               | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#365]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud previews on a real server](#nextcloud-previews-on-a-real-server-589)               | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#589]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud asset search on a real server](#nextcloud-asset-search-on-a-real-server-590)       | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#590]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud Photos on a real server](#nextcloud-photos-on-a-real-server-671)                   | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#671]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud shares on a real server](#nextcloud-shares-on-a-real-server-591)                   | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#591]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud desktop client on Plasma](#nextcloud-desktop-client-on-plasma-592)                 | [M29](ROADMAP.md#m29-nextcloud-integration)                                    | [#592]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                         |
| [Nextcloud settings on Plasma](#nextcloud-settings-on-plasma-368)                             | [M30](ROADMAP.md#m30-nextcloud-settings)                                       | [#368]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                   |
| [Nextcloud Photos settings on a real server](#nextcloud-photos-settings-on-a-real-server-672) | [M30](ROADMAP.md#m30-nextcloud-settings)                                       | [#672]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                   |
| [Keyboard shortcuts on Plasma](#keyboard-shortcuts-on-plasma-230)                             | [M31](ROADMAP.md#m31-keyboard-shortcut-settings)                               | [#230]         | ![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square)                                                               |
| [Shortcut contexts on Plasma](#shortcut-contexts-on-plasma-233)                               | [M31](ROADMAP.md#m31-keyboard-shortcut-settings)                               | [#233]         | ![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square)                                                               |

## mpv integration (#563)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#563]
  (Merged); its M28 setting is [#578] (Merged).
- **Resolved blocker:** Fedora 44 packages `mpv-devel`, and embedded playback
  uses libmpv's OpenGL render API in a `QOpenGLWidget` rather than KDE's
  MpvQt, so there is no Qt Quick and no process-wide numeric locale change
  unless libmpv refuses to start without it. Initialization failures return
  errors instead of terminating, and builds without libmpv or Qt OpenGL
  Widgets keep Qt Multimedia only.
- **Confirmed state:** `main` includes `src/core/mediabackend.cpp`
  and `src/core/mpvplayer.cpp` for backend choice and libmpv playback,
  `src/app/mpvvideowidget.cpp` for drawing video, the backend button in the
  preview and gallery playback controls, and **Audio and video playback**,
  now under **Preferences › Applications › Media**, which says what plays
  audio and video ([#578]). When mpv can't start or draw, Qt Multimedia
  plays the file with a notice. `mediabackendtest`,
  `mpvplayertest` and `mediaviewtest` cover backend choice, audio playback,
  seeking, end of file, errors and fallback. The development container has
  no OpenGL, so video through mpv is untested there. See
  [Embedded mpv playback](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On Plasma under Wayland and under X11, choose mpv with the backend button
   and play a video in the preview panel and the gallery; check the picture,
   seeking, pause and end of file.
2. Repeat with software OpenGL, for example
   `LIBGL_ALWAYS_SOFTWARE=1 opus-file-manager`, and confirm video plays or
   falls back to Qt Multimedia with a notice.
3. Open and close many videos quickly, switch backends during playback, and
   close the window while a video plays; confirm there are no crashes, stale
   frames or leftover `mpv` threads.
4. Preview remote audio and video and confirm the temporary download is
   removed when the preview changes.
5. Choose mpv under **Preferences › Applications › Media** and check that
   the tab says mpv plays audio and video where mpv draws video, and names
   the Qt Multimedia fallback where it can't.

## Markdown editor and backlinks (#564)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#564]
  (Merged).
- **Resolved blocker:** The source stays untouched in KTextEditor, so no
  source-preserving rewriter is needed. The preview keeps Qt's md4c-based
  Markdown renderer with its **CommonMark** and **GitHub Markdown** dialects,
  and wiki links come from a code-aware scanner in `src/core/wikilinks.cpp`
  that skips code blocks, code spans, HTML comments and escapes.
- **Confirmed state:** `main` includes the editor's **Vault**
  menu, a background index of the chosen folder in `src/core/notevault.cpp`,
  clickable wiki links in the preview of notes inside the vault, and a
  **Backlinks** list with missing, ambiguous, out-of-vault, unreadable and
  incomplete-scan reports. `notevaulttest`, `wikilinkstest` and
  `editorpaneltest` cover duplicate names, headings, aliases, embeds, code
  and escaped markup, invalid UTF-8, symbolic links, vault boundaries,
  rescans and draft-preserving navigation. Raw HTML, footnotes and math stay
  unrendered; see [Markdown vault links](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On Plasma, choose a real Obsidian vault through **Vault › Choose Vault
   Folder…** and check that its `.obsidian` folder and notes are unchanged.
2. Follow links between notes with unsaved changes and confirm the drafts
   are offered again when returning.
3. Open an attachment link and confirm it opens in its registered
   application.

## Workspace vault editing (#565)

- **Milestone and issues:** [M27](ROADMAP.md#m27-integrations), [#565]
  (Merged); its settings belong to
  [M28](ROADMAP.md#m28-integration-settings), [#566] (Merged).
- **Resolved blocker:** The built-in Markdown and backlinks feature, [#564],
  is Completed, and workspace profiles and location assignments ([#200] and
  [#201]) are merged.
- **Confirmed state:** `main` stores an optional editor
  vault per workspace in `src/core/workspacecatalog.cpp`, chosen with **Use
  as Editor Vault** in **Workspaces** from the workspace's assigned local
  locations. While that workspace is active, every editor uses its vault and
  the editor's own vault returns afterwards. The editor's vault panel adds
  **Links** and **Tags** tabs beside **Backlinks**, with tags indexed by
  `src/core/wikilinks.cpp` and `src/core/notevault.cpp`.
  `workspacecatalogtest`, `wikilinkstest`, `notevaulttest`,
  `editorpaneltest`, `workspacedialogtest` and `mainwindowtest` cover vault
  validation, duplication, unassignment and removal, inline and front matter
  tags, nested tags, and switching between the workspace and own vaults. See
  [Workspace vaults and tags](CAVEATS.md#desktop-integration).
- **M28 state:** `feature/M28-integration-settings` adds the **Workspace
  Vault** group under **Preferences › Applications › Editor**
  (`src/app/workspacevaultsettings.cpp`). It stores each workspace's vault
  switch and own Markdown options in the workspace catalog, which is now
  version 9, reports a vault location that can't be found in the group and
  the editor, and counts the vault's notes. `workspacecatalogtest`,
  `editorpaneltest`, `workspacedialogtest`, `mainwindowtest` and
  `integrationsettingstest` cover persistence, turning the vault off, its own
  options and a removed location. See
  [Workspace vault settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, assign a real Obsidian vault to a workspace, choose **Use as
   Editor Vault**, and check that its `.obsidian` folder and notes are
   unchanged.
2. Switch workspaces with unsaved edits open and confirm the drafts are kept
   and the editor's own vault returns.
3. Rename the vault folder while the workspace is active and confirm the
   editor reports the unreadable vault, then unassign the location and
   confirm the editor's own vault returns.
4. For [#566], on Plasma, put a workspace's vault on a removable drive,
   eject it while the workspace is active, and confirm the editor and
   **Preferences › Applications › Editor › Workspace Vault** report it, keep
   open notes and drafts, and read the vault again after the drive is
   mounted and **Vault › Rescan Vault** is chosen.

## System file chooser on Plasma (#286)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#286]
  (Merged).
- **Blocker:** The development container has no Plasma session, no
  xdg-desktop-portal frontend and no KWin, so the portal routing, window
  parenting and the restart of xdg-desktop-portal cannot be tried there.
- **Confirmed state:** The chooser, its D-Bus backend and the
  `--install-portal` and `--uninstall-portal` setup are implemented and pass
  their tests in the container, including a request answered over a private
  session bus. This verification does not block the feature; it is recorded
  as a caveat under [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session with xdg-desktop-portal 1.21.0 or later, install
   with `./install.sh --file-chooser`.
2. Open and save files from a Flatpak application, a GTK application with
   `GTK_USE_PORTAL=1` and a browser set to use the portal file chooser.
   Check filters, choices,
   multiple selection, folder selection, overwrite confirmation and that the
   dialog stays over the requesting window on Wayland and X11.
3. Run `./install.sh --uninstall` and confirm the previous chooser returns
   and `~/.config/xdg-desktop-portal/kde-portals.conf` matches its backup.
4. On an older xdg-desktop-portal, confirm KDE's chooser is used as the
   fallback. Record any differences in [CAVEATS](CAVEATS.md), then remove
   this entry and its roadmap link.

## Mount manager on Plasma (#703)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#703]
  (Merged).
- **Blocker:** The development container has no Plasma session, no UDisks2
  daemon, no polkit agent and no removable or optical drives, so Solid's
  mount, unmount and eject requests and their authorization prompts cannot
  be tried there.
- **Confirmed state:** The mount-table parsing, the **Mount Manager** dialog
  and the **Mount Points** menu are implemented and pass their tests in the
  container against a fake backend. This verification does not block the
  feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, open **File › Mount Manager…** with a USB drive, an
   optical disc, a network or FUSE mount and an fstab entry marked `user`.
2. Mount, unmount and eject each one, cancel and deny an authorization
   prompt, and unmount a drive with a file open in another application.
   Check that each result is reported and that the list updates.
3. Check **Go › Mount Points** on the toolbar opens or mounts each entry.
   Record any differences in [CAVEATS](CAVEATS.md), then remove this entry
   and its roadmap link.

## KWin script on Plasma (#707, #708)

- **Milestone and issues:** [M27](ROADMAP.md#m27-integrations), [#707]
  (Merged) and [#708] (Merged).
- **Blocker:** The development container has no KWin, no KPackage script
  structure registered with a compositor and no KGlobalAccel daemon, so the
  script cannot be installed, loaded or driven by real windows there.
- **Confirmed state:** The script package, the **KDE Plasma › KWin** tab of
  Preferences, the workspace placements and the drop-down window are implemented
  and pass their tests in the container, against a fake backend and a simulated
  KWin JavaScript API. This verification does not block the features; it is
  recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, open **Preferences › KDE Plasma › KWin**,
   install and enable the script, and confirm the tab reports it as
   running. Check `journalctl --user -b | grep -i opus` for script errors.
2. Save and restore window placements across two workspaces on Wayland and
   X11: screen, virtual desktop, size, tile and keep-above state.
3. Detach a preview, an editor and a pane and confirm they are placed beside
   the main window. Assign workspaces to virtual desktops and confirm the
   assigned workspace is applied when each desktop becomes current.
4. Enable the drop-down window, assign a global shortcut, and slide it in and
   out from each screen edge at several widths and heights, on multiple
   screens and with the window focused and unfocused.
5. Remove the script and confirm it is unloaded. Record any differences in
   [CAVEATS](CAVEATS.md), then remove this entry and its roadmap links.

## Administrator mode on Plasma (#288)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#288]
  (Merged).
- **Blocker:** The development container has no `kio-admin` worker, no
  Polkit agent and no Plasma session, so the authorization prompt and
  operations performed as root cannot be tried there.
- **Confirmed state:** **File › Act as Administrator**, the warning, the
  warning bar, the missing-worker report and session saving are implemented
  and pass their tests in the container. The test that cancels the warning
  is skipped there because it needs `kio-admin`. This verification does not
  block the feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session with `kio-admin` installed, run
   `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R adminmodeuitest
--output-on-failure` and confirm the cancel test runs and passes.
2. Open `/etc` in Plasma Opus, choose **File › Act as Administrator**,
   accept the warning and authenticate. Check that the warning bar appears
   and the action is checked.
3. Create, rename, edit through **Open With** and delete a test file in a
   root-owned folder, then deny and cancel the authorization prompt in a new
   window and check each failure is reported.
4. Press **Stop Acting as Administrator**, then quit and restart Plasma Opus
   and save and apply a workspace, confirming no tab reopens as `admin:`.
   Record any differences in [CAVEATS](CAVEATS.md), then remove this entry
   and its roadmap link.

## Plasma Vault on Plasma (#654)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#654]
  (Merged).
- **Blocker:** The development container has no Plasma Vault, no kded6
  module for it and no CryFS, gocryptfs or EncFS, so vaults cannot be
  created, opened or closed there.
- **Confirmed state:** **Go › Plasma Vaults**, the D-Bus client, the
  check for processes using a vault and showing an opened vault are
  implemented and pass their tests in the container, against a fake
  `org.kde.plasmavault` service on a private session bus. This verification
  does not block the feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Vault and gocryptfs or CryFS
   installed, open **Go › Plasma Vaults** and choose **Create Vault…**;
   finish Plasma Vault's wizard and check the vault appears in the menu.
2. Choose **Open Vault…**, enter the password and check the vault opens in
   the active pane. Cancel the prompt once and enter a wrong password once,
   checking each result.
3. Open a file from the vault in another application or `cd` into it in a
   terminal, choose **Close Vault** and check the warning lists the program.
   Close anyway and check Plasma Vault's refusal appears in the status bar.
4. Stop kded6 with `kquitapp6 kded6` and check the menu reports Plasma Vault
   as unavailable. Record any differences in [CAVEATS](CAVEATS.md), then
   remove this entry and its roadmap link.

## File encryption on Plasma (#655)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#655]
  (Merged).
- **Blocker:** The development container has no `aescrypt` tool, no
  Kleopatra and no pinentry program, so AES Crypt files, Kleopatra's dialogs
  and GnuPG's password prompt cannot be tried there.
- **Confirmed state:** **File › Encryption**, the encryption dialog and the
  information panel row are implemented and pass their tests in the
  container. Encrypting, decrypting, signing and verifying run against real
  GnuPG with a key that has no password; AES Crypt and Kleopatra run
  against fake programs that record their arguments. This verification does
  not block the feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session with GnuPG, `pinentry-qt` and Kleopatra
   installed, create a key with a password in Kleopatra. Select a file,
   choose **File › Encryption › Encrypt…**, pick the key and check a `.gpg`
   file appears beside it. Choose **Decrypt** on that file and check
   GnuPG's password prompt appears and the decrypted copy matches.
2. Choose **Sign…** and then **Verify Signature** on the file, and check
   the result names the key. Edit the file and verify again, checking the
   signature is reported as bad.
3. Choose **Sign and Encrypt with Kleopatra…** and **Decrypt and Verify
   with Kleopatra…** and check Kleopatra's own dialogs open for the
   selected files.
4. Install `aescrypt`, encrypt a file with **Encrypt with AES Crypt…**,
   decrypt it once with a wrong password and once with the right one, and
   check the results and that no key file remains under
   `$XDG_RUNTIME_DIR`. Record any differences in [CAVEATS](CAVEATS.md),
   then remove this entry and its roadmap link.

## Send by Email on Plasma (#656)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#656]
  (Merged).
- **Blocker:** The development container has no mail client, no Plasma
  Impresario and no XDG Email portal, so real composers and the sandboxed
  path cannot be tried there.
- **Confirmed state:** **File › Send by Email…** and its dialog are
  implemented and pass their tests in the container. A fake `mailto:`
  handler records the `mailto:` URL it receives, remote files are copied
  from a `tar:` archive through KIO, and a fake handler with Plasma
  Impresario's desktop ID checks the warning. The portal call is covered
  only by a unit test of its options. This verification does not block the
  feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, set KMail as the email client under **System
   Settings › Default Applications**. Select two local files and a folder,
   choose **File › Send by Email…**, and check the folder is listed as
   skipped and KMail's composer opens with both files attached. Close the
   composer without sending.
2. Repeat with Thunderbird as the email client, then with a file on an
   SFTP or SMB location: allow the local copy and check it is attached and
   its folder under `/tmp` is removed after quitting Plasma Opus.
3. Set Plasma Impresario as the email client and check the warning before
   and after its composer opens without the files.
4. Run the Flatpak build, if available, and check the XDG Email portal
   opens the composer with the files attached. Record any differences in
   [CAVEATS](CAVEATS.md), then remove this entry and its roadmap link.

## Mail attachments on Plasma (#657)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#657]
  (Merged).
- **Blocker:** The development container has no Plasma Impresario, no IMAP
  account and no drag-and-drop between applications, so real drags into its
  composer and out of its attachment list cannot be tried there.
- **Confirmed state:** Dragging local files and prepared copies from the
  **Send by Email** list, refusing a Plasma Impresario attachment reference
  and saving dropped content are implemented and pass their tests in the
  container, with simulated drag data. This verification does not block the
  feature; it is recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Impresario as the email client, select
   a local file and a file on an SFTP or SMB location and choose **File ›
   Send by Email…**.
2. Tick the consent box, wait for the copy, then drag both items from the
   list into Plasma Impresario's composer and check both are attached.
   Close the dialog and check the copy is still attached when the message
   is sent to yourself.
3. Drag an attachment from a received message onto a Plasma Opus pane and
   check the drop is refused with the status-bar explanation and no file is
   created.
4. Drag a selection of text with a suggested filename, for example from
   KMail's attachment list, onto a pane and check KIO asks for the file name
   and saves the content. Record any differences in [CAVEATS](CAVEATS.md),
   then remove this entry and its roadmap link.

## Mail file preview on Plasma (#658)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#658]
  (Merged).
- **Blocker:** The development container has no registered applications, no
  mail client and no Plasma session, so opening an attachment through its
  application and the preview's layout in a real window cannot be tried
  there.
- **Confirmed state:** Parsing headers, bodies, encodings, multipart,
  signed, encrypted and forwarded messages, extraction beside the message,
  the changed-message error and read-only private copies pass their tests in
  the container. This verification does not block the feature; it is
  recorded as a caveat under [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, save a message with a PDF and an image attached
   from KMail or Thunderbird as an `.eml` file, and select it with the
   preview panel shown.
2. Check the headers, body and attachment list fit the panel, and that an
   HTML-only newsletter shows as plain text without loading images.
3. Select the PDF and press **Open**; check Okular or the PDF handler opens
   a read-only copy. Save a `.desktop` or shell script attachment and check
   **Open** doesn't run it.
4. Press **Extract All** twice and check KIO asks how to handle the existing
   files. Record any differences in [CAVEATS](CAVEATS.md), then remove this
   entry and its roadmap link.

## Shared Activity context on Plasma (#659)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#659]
  (Merged).
- **Blocker:** The development container has no Activities service, and
  Plasma Impresario's account-to-workspace associations aren't released, so
  reports reaching Plasma's history and both applications following one
  Activity change cannot be tried there.
- **Confirmed state:** Choosing what to report, leaving out temporary files,
  encrypted folders, virtual locations and passwords, reporting pane visits
  and the setting pass their tests in the container with a replaced
  reporter. This verification does not block the feature; it is recorded as
  a caveat under [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, visit a few folders and open a file in Plasma
   Opus, then check they appear under **Recent Locations** and **Recent
   Files** in Kickoff, and in Dolphin's `recentlyused:/` view.
2. Open a Plasma Vault and visit its folder; check it doesn't appear. Turn
   off **Report visited folders and opened files to Plasma Activities** and
   check new visits don't appear either.
3. Once Plasma Impresario associates accounts with workspaces, switch
   Activities and check each application applies only its own association.
   Record any differences in [CAVEATS](CAVEATS.md), then remove this entry
   and its roadmap link.

## Default file manager on Plasma (#293)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#293]
  (Merged).
- **Blocker:** The development container has no Plasma session, no System
  Settings, no running file manager and no dbus-broker, so the default that
  Plasma and other applications see, claiming `org.freedesktop.FileManager1`
  while Dolphin runs and reloading activation files cannot be tried there.
- **Confirmed state:** `src/core/defaultfilemanager.cpp` reads and sets the
  default application for folders, finds an overriding desktop-specific
  list, and installs, updates and removes the activation file;
  **Preferences › KDE Plasma › File Manager** offers them.
  `defaultfilemanagertest`, `integrationsettingstest` and
  `filemanagerservicetest`, which claims the service on a private session
  bus, pass in the container. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh` and
   Dolphin as the default, open **Preferences › KDE Plasma › File Manager**
   and check that the status names Dolphin.
2. Choose **Set Plasma Opus as Default**. Check that System Settings ›
   Default Applications shows Plasma Opus and that a browser's **Show in
   folder** opens Plasma Opus. With Dolphin running, check the warning; quit
   Dolphin, restart Plasma Opus and check that **Show in folder** reaches it.
3. Choose **Install Activation File**, quit Plasma Opus and every file
   manager, and check that **Show in folder** starts Plasma Opus, under
   dbus-daemon and, where available, dbus-broker. Choose **Remove Activation
   File** and check that it no longer does.
4. Add `inode/directory=org.kde.dolphin.desktop;` to
   `~/.config/kde-mimeapps.list` and check that **Set Plasma Opus as
   Default** reports it. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and its
   roadmap link.

## File chooser settings on Plasma (#292)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#292]
  (Merged).
- **Blocker:** The development container has no Plasma session, no session
  bus, no xdg-desktop-portal and no systemd user manager, so whether the
  portal is found, the restart through `systemctl --user` and the chooser
  that applications then show cannot be tried there.
- **Confirmed state:** `PortalSetup::change()` in
  `src/core/portalsetup.cpp` sets up or restores the chooser and restarts
  xdg-desktop-portal for both **Preferences › KDE Plasma › File Manager**
  and `--install-portal`/`--uninstall-portal`. `portalsetuptest` and
  `integrationsettingstest`, which replaces the portal check, restart and
  question, pass in the container. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with xdg-desktop-portal 1.21.0 or later and
   Plasma Opus installed by `./install.sh`, open **Preferences › KDE Plasma ›
   File Manager** and check that the **System File Chooser** status says the
   system's own chooser is used.
2. Open a file dialog in a Flatpak application, choose **Set Up** and
   confirm. Check that the dialog closes, that the next one is the Plasma
   Opus chooser and that the status updates.
3. Edit `~/.config/xdg-desktop-portal/kde-portals.conf` to prefer `kde`,
   reopen the settings and check that **Complete Setup** appears and works.
4. Choose **Restore Previous Chooser** and check that KDE's chooser returns
   and the file matches its backup. Where xdg-desktop-portal does not run
   as a systemd user service, check that the failed restart is reported
   with the advice to log out and back in. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## Chooser start folder on Plasma (#750)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#750]
  (Merged).
- **Blocker:** The development container has no xdg-desktop-portal, so the
  application IDs it passes for sandboxed and unsandboxed applications
  cannot be seen there.
- **Confirmed state:** `FileChooser::startFolder()`, `acceptedFolder()`,
  `lastFolder()` and `rememberFolder()` in `src/core/filechooser.cpp` choose
  and remember the folder; `FileChooserPortal` applies **Start folder** from
  **Preferences › KDE Plasma › File Manager**. `filechoosertest`,
  `chooserdialogtest`, including requests over a private session bus, and
  `integrationsettingstest` pass in the container. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with the Plasma Opus file chooser set up, set
   **Start folder** to **Folder of the last choice** and apply.
2. In a Flatpak application, open a file from a folder other than home,
   then open the dialog again without a suggested folder and check that it
   starts there. Check that another Flatpak application still starts in the
   home folder.
3. Repeat with an unsandboxed portal application, such as a GTK
   application with `GTK_USE_PORTAL=1`, and check which application ID
   `~/.local/state/opus-file-managerstaterc` records under `[FileChooser]`.
4. Choose **Forget Remembered Folders** and check that the dialogs start in
   the home folder again. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## Operation notifications on Plasma (#392)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#392]
  (Merged).
- **Blocker:** The development container has no Plasma session, job-view
  service or System Settings, so Plasma's job reports and its Notifications
  settings cannot be seen there.
- **Confirmed state:** `OperationJob::reports()` in
  `src/core/operationjob.cpp` reads the settings under **Preferences › KDE
  Plasma › Notifications**, and `OperationJob::track()` leaves operations
  whose setting is cleared unreported. **Configure Plasma Notifications…**
  runs `kcmshell6 kcm_notifications --args
--desktop-entry=org.kde.opusfilemanager`, or `systemsettings` with the
  same arguments. `operationjobtest` and `integrationsettingstest` pass in
  the container. See [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh`,
   verify a checksum file and synchronize two folders, and check that both
   appear in Plasma's job progress.
2. Clear **Creating and verifying checksum files** under **Preferences › KDE
   Plasma › Notifications** and apply. Verify the checksum file again and
   check that Plasma shows nothing while the dialog shows the progress and
   result, and that folder synchronization still reports.
3. Choose **Configure Plasma Notifications…** and check that it opens on the
   Plasma Opus entry, with `kcmshell6` and, with it removed from `PATH`,
   with `systemsettings`. Check that the options there change how Plasma
   shows Plasma Opus's reports. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## Start hidden in tray on Plasma (#290)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#290]
  (Merged).
- **Blocker:** The development container has no Plasma session or system
  tray host, so starting hidden with Plasma's tray, at login or otherwise,
  cannot be seen there.
- **Confirmed state:** `TrayPolicy::startHidden()` in
  `src/core/traypolicy.cpp` hides the window at start only when **Start with
  the window hidden in the system tray** is checked under **Preferences ›
  KDE Plasma › System Tray**, no folders or workspace were asked for and a
  tray host can show the icon; `DesktopTray::startHidden()` applies it from
  `main.cpp`. `traypolicytest`, `desktoptraytest` and
  `integrationsettingstest` pass in the container with a simulated tray host.
  See [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh`,
   check **Start with the window hidden in the system tray** under
   **Preferences › KDE Plasma › System Tray** and apply. Quit and start
   Plasma Opus from the launcher; check that only the tray icon appears and
   that choosing it shows the window with its tabs.
2. Quit, then run `opus-file-manager ~/Documents`, and check that the
   window is shown. Start Plasma Opus while it is hidden and check that the
   window is shown.
3. Add Plasma Opus under **System Settings › Autostart**, log out and back
   in, and check whether it starts hidden or shows the window because the
   tray was not ready. Check both on Wayland and X11, and that the tab's
   Wayland note appears only on Wayland. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## Global shortcut on Plasma (#291)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#291]
  (Merged).
- **Blocker:** The development container has no KDE global shortcut
  service (kglobalaccel), so registering, removing and changing the
  shortcut cannot be seen there.
- **Confirmed state:** `DesktopTray::applySettings()` in
  `src/app/desktoptray.cpp` registers **Show or Hide Plasma Opus** with
  KGlobalAccel only while **Show or hide Plasma Opus with a global
  shortcut** is checked under **Preferences › KDE Plasma › Shortcuts**, and
  removes it otherwise, keeping its key in the `[Tray]` group of
  `opus-file-managerstaterc`. The tab's key goes to KGlobalAccel with
  `KGlobalAccel::setShortcut()`. `desktoptraytest`, `traypolicytest` and
  `integrationsettingstest` pass in the container, where KGlobalAccel
  assigns no key. See [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh`, open
   **Preferences › KDE Plasma › Shortcuts** and check that the tab shows
   Meta+Alt+O, or says that no key is assigned when another application
   uses it, and that pressing the key shows and hides the window.
2. Choose another key and apply; check that it works, that Meta+Alt+O no
   longer does, and that **System Settings › Keyboard › Shortcuts** shows
   the new key. Choose a key another application uses and check that the
   tab asks before taking it over. Choose the current key again after
   clearing it and check whether the tab asks to take it from Plasma Opus.
3. Clear **Show or hide Plasma Opus with a global shortcut** and apply;
   check that the key does nothing and that Plasma's Shortcuts settings no
   longer list it. Check it again, apply, and check that the previous key
   works. Change the key in Plasma's Shortcuts settings while Preferences is
   open, and check that the tab's status follows. Choose **Configure Plasma
   Shortcuts…** and check that System Settings opens. Record any
   differences in [CAVEATS](CAVEATS.md#integration-settings), then remove
   this entry and its roadmap link.

## Activity associations on Plasma (#391)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#391]
  (Merged).
- **Blocker:** The development container has no Plasma Activities service
  and no System Settings, so the Activity list and opening Plasma's
  Activities settings cannot be seen there.
- **Confirmed state:** `SettingsDialog::createActivitiesIntegrationPage()`
  in `src/app/integrationsettingspages.cpp` embeds
  `ActivityAssociationsEditor` under **Preferences › KDE Plasma ›
  Activities**, reading Activities through the main window's
  `ActivityController`, and saves the associations with the other settings.
  **Configure Plasma Activities…** opens the `kcm_activities` module through
  `kcmshell6` or System Settings. `integrationsettingstest` and
  `activitycontrollertest` pass in the container with a replaced Activity
  source. See [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with at least two Activities, open **View ›
   Activity Associations** and check that **Preferences › KDE Plasma ›
   Activities** opens with every Activity listed and the current one
   selected.
2. Check **Associate workspaces and default folders with Plasma
   Activities**, choose a workspace, **Selected** behaviours and folders for
   another Activity, and apply. Switch to that Activity and check that the
   offer above the panes applies the chosen workspace, behaviours and
   folders.
3. Create, rename and remove an Activity with Preferences open, and check
   that the list follows. Choose **Configure Plasma Activities…** and check
   that Plasma's Activities settings open. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## KDE Connect settings on Plasma (#393)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#393]
  (Merged).
- **Blocker:** The development container has no KDE Connect and no System
  Settings, so **Configure KDE Connect…** cannot open KDE Connect's settings
  there.
- **Confirmed state:** `SettingsDialog::createKdeConnectIntegrationPage()`
  in `src/app/integrationsettingspages.cpp` opens the `kcm_kdeconnect`
  module through `kcmshell6` or System Settings, looking for it among both
  the QML and the QWidgets System Settings modules. Turning **File › Send to
  Device…** off and forgetting the remembered device pass
  `kdeconnectsharetest`, `kdeconnectsharedialogtest` and
  `integrationsettingstest` in the container with a replaced KDE Connect
  client. See [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with KDE Connect installed, open **Preferences ›
   KDE Plasma › KDE Connect**, choose **Configure KDE Connect…** and check
   that KDE Connect's settings open rather than the tab reporting them as
   not installed.
2. With two paired devices that can receive files, send a file to the
   second one, clear **Choose the device the files were last sent to
   again**, apply, and check that **File › Send to Device…** chooses the
   first device. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## KRunner settings on Plasma (#394)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#394]
  (Merged).
- **Blocker:** The development container has no KRunner, no Plasma session
  and no System Settings, so the installed plugin cannot be searched and
  **Configure Plasma Search…** cannot open Plasma's search settings there.
- **Confirmed state:** `SettingsDialog::createKRunnerIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` stores the switch and the cleared
  workspaces and Favorites, and opens the `kcm_plasmasearch` module through
  `kcmshell6` or System Settings. `OpusRunner` in `src/runner/` reads them
  again when a search session ends. Leaving out what is turned off or
  cleared passes `runnermatchestest`, `opusrunnertest` and
  `integrationsettingstest` in the container, calling the runner directly
  rather than through KRunner. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh`, open
   **Preferences › KDE Plasma › KRunner**, clear one saved workspace and one
   Favorite, apply, close KRunner and search for both; check that neither
   is offered while the others still are.
2. Clear **Offer saved workspaces and Favorites in KRunner and Plasma
   Search**, apply, and check that a new search offers nothing from Plasma
   Opus. Choose **Configure Plasma Search…** and check that Plasma's search
   settings open rather than the tab reporting them as not installed.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings),
   then remove this entry and its roadmap link.

## Online account settings on Plasma (#371)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#371]
  (Merged).
- **Blocker:** The development container has no Plasma session and no KDE
  account providers, so accounts cannot be added through System Settings and
  **Manage Online Accounts…** cannot open its Online Accounts page there.
- **Confirmed state:** `SettingsDialog::createOnlineAccountsIntegrationPage()`
  in `src/app/integrationsettingspages.cpp` stores the opt-in and the cleared
  account ids, and opens the `kcm_kaccounts` module through `kcmshell6` or
  System Settings. `OnlineAccounts::shownAccounts()` in `src/core/` leaves
  out cleared accounts and those without file storage for **Go › Online
  Accounts** and the cloud sync accounts menu. Listing, clearing and hiding
  accounts pass `onlineaccountstest`, `onlineaccountsmenutest` and
  `integrationsettingstest` in the container, with accounts written to a
  temporary libaccounts database. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh` and
   `kio-gdrive` and `kaccounts-providers` installed, add a Google or
   Nextcloud account and an account without file storage in **System
   Settings › Online Accounts**.
2. Check that **Go › Online Accounts** is hidden, then check **Show the
   folders of KDE Online Accounts in Plasma Opus** under **Preferences › KDE
   Plasma › Online Accounts**, and check that the tab lists only the account
   with file storage, names the other one, and that the menu offers it after
   **Apply**.
3. Clear the account, apply, and check that the menu and the cloud sync
   accounts menu leave it out while System Settings still lists it. Choose
   **Manage Online Accounts…** and check that the Online Accounts page opens.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings),
   then remove this entry and its roadmap link.

## KWin script settings on Plasma (#709)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#709]
  (Merged).
- **Blocker:** The development container has no KWin, KGlobalAccel daemon or
  System Settings, so the script cannot be installed, enabled or removed there,
  and **Configure KWin Scripts…** and **Configure Window Rules…** cannot open
  KWin's settings.
- **Confirmed state:** `SettingsDialog::createKWinIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` drives the main window's
  `KWinIntegration` to install, update, enable, disable and remove the script,
  stores the window management options in the `KWinScript` group and the
  virtual desktop assignments in `KWinVirtualDesktops`, and opens the
  `kcm_kwin_scripts` and `kcm_kwinrules` modules through `kcmshell6` or System
  Settings. The actions, their reports and the options sent to the script pass
  `kwinscripttest` and `integrationsettingstest` in the container against a
  fake KWin backend. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with Plasma Opus installed by `./install.sh`, open
   **Preferences › KDE Plasma › KWin**, choose **Install and Enable**, and
   check that the tab reports the script as running and that it appears,
   enabled, under **Configure KWin Scripts…**.
2. Clear **Restore the window's screen, virtual desktop, size, tile and
   keep-above state with each workspace**, apply, and check that applying a
   workspace no longer moves the window; check it again and confirm the
   placement is restored. Check **Place detached previews, editors and panes
   beside the main window** and detach a preview.
3. Check **Apply the workspace assigned to the virtual desktop that becomes
   current**, assign a workspace to a virtual desktop, apply, and switch to
   that desktop. Choose **Disable** and **Enable**, then **Remove…**, and check
   KWin's script settings after each. Choose **Configure Window Rules…** and
   check that KWin's window rules open.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings),
   then remove this entry and its roadmap link.

## Drop-down settings on Plasma (#710)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#710]
  (Merged).
- **Blocker:** The development container has no KWin or KGlobalAccel daemon,
  so the drop-down window cannot be placed by the script or toggled by a real
  global shortcut or screen edge there.
- **Confirmed state:** `SettingsDialog::createDropDownGroup()` in
  `src/app/integrationsettingspages.cpp` stores the drop-down options in the
  `KWinScript` group, which `KWinIntegration` sends to the script at its next
  sync, and assigns the shortcut of the main window's `toggle_drop_down`
  action through KGlobalAccel. **Settings › Configure KWin Integration…**
  opens the tab. The options, the shortcut and the menu action pass
  `kwinscripttest`, `integrationsettingstest` and `kwinintegrationtest` in the
  container against a fake KWin backend and simulated shortcut hooks. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with the KWin script running, open **Preferences ›
   KDE Plasma › KWin**, check **Slide the window in from a screen edge and hide
   it again**, choose a global shortcut and apply. Check that the tab reports
   the shortcut as working and that Plasma's Shortcuts settings list it.
2. Press the shortcut from each **Screen edge** at several widths and heights,
   on a chosen screen and on **Active screen**, with **Slide in and out**
   checked and cleared.
3. Check **Hide when another application is activated**, both triggers and a
   workspace, apply, and check each. Choose **Restore Defaults** and apply,
   and check that the shortcut is removed from Plasma's Shortcuts settings.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings),
   then remove this entry and its roadmap link.

## Baloo search settings on Plasma (#443)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#443]
  (Merged).
- **Blocker:** The development container has no running Baloo indexer or
  System Settings, so the tab's indexer state can't be compared with a real
  index there, and **Configure File Search…** cannot open KDE's File Search
  settings.
- **Confirmed state:** `SettingsDialog::createBalooSearchGroup()` in
  `src/app/integrationsettingspages.cpp` stores `SearchOfferRanking` and
  `SearchOfferLiveFallback` in the `Search` group, which `SearchDefaults::read()`
  reads and `SearchWidget` applies when the settings are applied. The tab reads
  the indexer's state through `IndexStatus::Config::read()` and
  `SystemIndexBackend`, and opens the `kcm_baloofile` module through
  `kcmshell6` or System Settings. The options pass `searchwidgettest` and
  `integrationsettingstest` in the container. See
  [Integration settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On a Plasma 6 session with file indexing on, open **Preferences › KDE
   Plasma › Baloo** and check that the tab reports the indexer's state as
   `balooctl6 status` does. Turn indexing off in KDE File Search settings and
   check that the tab says so the next time it is shown.
2. Choose **Configure File Search…** and check that KDE's File Search settings
   open. Choose **Search Defaults…** and check that **Tools › Search** is
   shown.
3. Clear both options, apply, and run an indexed search in the search panel:
   check that **Rank results by similarity** and **Search Current Folder
   Live** are not shown. Check them again, apply, and check that both return.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings),
   then remove this entry and its roadmap link.

## Office settings on Plasma (#676)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#676]
  (Merged).
- **Blocker:** The development container has no office applications,
  document viewer components or System Settings, so the **Office** tab's list
  of viewers, previews through a real component and **File Associations…**
  can't be checked there.
- **Confirmed state:** `SettingsDialog::createOfficeEditorGroup()` and
  `createOfficePreviewGroup()` in `src/app/integrationsettingspages.cpp`
  store `OfficeEditor` and `OfficePreviews` in the `Office` group.
  `EditTools::choose()` uses only the chosen application through
  `OfficeApps::preferredApp()`, and `PreviewPanel` shows a message instead of
  a viewer when `DocumentFormats::previewEnabled()` is false. The tab lists
  each kind's first viewer from `KParts::PartLoader::partsForMimeType()` and
  opens the `kcm_filetypes` module. `edittoolstest`, `documentformatstest` and
  `integrationsettingstest` pass in the container with fake applications. See
  [Office settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma with Calligra and Okular installed, open **Preferences ›
   Applications › Office** and check that the **Previews** group names
   Calligra's viewer for text documents, spreadsheets and presentations.
2. Choose **File Associations…**, check that KDE's File Associations
   settings open, raise another viewer for ODT files, and check that the tab
   and the preview of an ODT file follow it.
3. Choose LibreOffice, apply, and check that **Edit** opens a local ODT file
   in LibreOffice and reports a document on an SFTP folder. Clear **Preview
   office documents in the preview panel**, apply, and check that local and
   remote office documents show the message without downloading. Record any
   differences in [CAVEATS](CAVEATS.md#integration-settings), then remove
   this entry and its roadmap link.

## Mail client settings on Plasma (#666)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#666]
  (Merged).
- **Blocker:** The development container has no mail client or System
  Settings, so the **Mail** tab's client report can only be checked with a
  fake `mailto:` handler, and **Configure Default Applications…** cannot open
  Plasma's Default Applications settings.
- **Confirmed state:** `SettingsDialog::createMailClientGroup()` in
  `src/app/integrationsettingspages.cpp` names the client from
  `MailHandoff::preferredClient()`, opens the `kcm_componentchooser` module
  through `kcmshell6` or System Settings, and stores `SendByEmail` in the
  `Mail` group. `MainWindow::applySettings()` hides **Send by Email…** and
  `updateActions()` disables it, which also leaves it out of the context
  menus. `integrationsettingstest` passes in the container with fake clients.
  See [Mail client settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma with KMail or Thunderbird installed, open **Preferences ›
   Applications › Mail** and check that the **Client** group names the
   client chosen under **System Settings › Default Applications › Email
   client**. Choose Plasma Impresario there, show the tab again, and check
   that it says Plasma Impresario can't receive attached files.
2. Choose **Configure Default Applications…** and check that Plasma's Default
   Applications settings open.
3. Clear **Offer Send by Email for selected files**, apply, and check that
   **File › Send by Email…** and the file context menu no longer offer it.
   Check it again, apply, and check that both offer it. Record any
   differences in [CAVEATS](CAVEATS.md#integration-settings), then remove
   this entry and its roadmap link.

## Remote connection settings on Plasma (#297)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#297]
  (Merged).
- **Blocker:** The development container has no KDE password server
  (`kpasswdserver6`), KDE Wallet or KDE Wallet Manager, so passwords for
  connections saved under **Preferences › Remote and Cloud › Connections**
  can't be asked for or kept, and **Open KDE Wallet Manager…** can only report
  that the manager is missing.
- **Confirmed state:**
  `SettingsDialog::createRemoteConnectionsIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` embeds `RemoteProfilesEditor`
  (`src/app/remoteprofileseditor.cpp`), the editor that the **Manage Remote
  Connections** window also uses, on the `RemoteProfiles` group, and stores the
  defaults for new connections as `RemoteDefaultProtocol`, `RemoteDefaultUser`
  and `RemoteDefaultAuthentication` in the `RemoteConnections` group. Profiles
  and URLs never contain passwords. `integrationsettingstest`,
  `remoteprofilestest` and `remoteprofilesdialogtest` pass in the container.
  See [Remote connection settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, save an SFTP or WebDAV connection with a password under
   **Preferences › Remote and Cloud › Connections**, open it from **Go ›
   Remote Connections**, and check that KIO asks for the password and offers
   to keep it in KDE Wallet. Check that `opusfilemanagerrc` contains no
   password.
2. Choose **Open KDE Wallet Manager…** and check that KDE Wallet Manager opens
   and lists the saved password, then remove it there.
3. Set **Defaults for New Connections**, apply, and check that **Manage Remote
   Connections…** starts new connections with them. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and its
   roadmap link.

## rclone settings on Plasma (#663)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#663]
  (Merged).
- **Blocker:** The development container has no rclone, KDE password server
  (`kpasswdserver6`) or KDE Wallet, so serving a real remote over WebDAV, and
  KIO signing in with the password that the password server keeps, are tested
  only with a fake rclone and a fake password provider.
- **Confirmed state:** `SettingsDialog::createRcloneIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` stores `RcloneEnabled`,
  `RcloneHiddenRemotes` and `RcloneOpenMode` in the `CloudStorage` group.
  `RcloneMounts::serve()` in `src/core/rclone.cpp` runs `rclone serve webdav`
  on 127.0.0.1 with the password in `RCLONE_PASS`, and `MainWindow` looks the
  password up with `KPasswdServerClient::checkAuthInfo()`, or creates one and
  keeps it with `addAuthInfo()`. `rclonetest` and `integrationsettingstest`
  pass in the container. See [rclone settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with rclone and a configured remote, choose **Serve it over
   WebDAV on this computer only** under **Preferences › Remote and Cloud ›
   rclone**, apply, and open the remote from **Go › Cloud Storage**. Check
   that it opens at `webdav://rclone@127.0.0.1:…/` without asking for a
   password, that KDE Wallet holds the entry, and that the location bar and
   `ps` show no password.
2. Open a file from the served remote in a KIO application, such as Kate, save
   it, and check that the change reaches the remote. Choose the remote under
   **Stop Serving** and check that the panes leave it and rclone stops.
3. Quit and restart Plasma Opus, serve the remote again, and check that the
   kept password is reused. Clear a remote and the integration, and check the
   menu. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and its
   roadmap link.

## Remote drive settings on Plasma (#704)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#704]
  (Merged).
- **Blocker:** The development container has no sshfs, FUSE, `fusermount3`,
  KIO FUSE, davfs2, CIFS client or Plasma session, so mounting with the chosen
  backends, keeping sshfs mounts after quitting, taking them over at startup,
  mounting on workspace activation and starting at login are tested only with
  fake tools, a fake KIO FUSE service and a test-mode autostart folder.
- **Confirmed state:** `SettingsDialog::createRemoteDrivesIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` stores the `RemoteDrives` group's
  `RemoteDriveSftpBackend`, `RemoteDriveWebDavBackend`, `RemoteDriveSmbBackend`,
  `RemoteDriveMountFolder`, `RemoteDriveReadOnly`, `RemoteDriveMountAtLogin`,
  `RemoteDriveMountOnWorkspace` and `RemoteDriveUnmountOnExit`.
  `RemoteMounts::defaultsFromSettings()` in `src/core/remotemounts.cpp` gives
  the options of connections without saved ones. With unmounting on exit off,
  `RemoteMountManager` starts sshfs without `-f`, leaves mounts at quit and
  takes over mounts it finds in place, unmounting them with `fusermount -u`.
  `MainWindow::applySettings()` writes or removes
  `autostart/<desktop file>-remote-drives.desktop`, which starts
  `--background-mount`, and `MainWindow::mountRemoteProfilesForWorkspace()`
  mounts the connections covering a workspace's assigned locations.
  `remotemountstest`, `remotemountdialogtest` and `integrationsettingstest`
  pass in the container. See
  [Remote drive settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with sshfs, FUSE 3 and an SSH server reached with a key, choose
   **sshfs** for SFTP and a mount folder such as `~/Remote` under **Preferences
   › Remote and Cloud › Remote Drives**, apply, and mount a saved SFTP
   connection with **Mount as Folder**. Check that it mounts in that folder,
   read-only when **Mount read-only** is checked, and that a connection with
   saved **Mount Options…** keeps them.
2. Clear **Unmount sshfs, davfs2 and CIFS mounts when Plasma Opus quits**,
   apply, mount the connection, quit Plasma Opus and check with `findmnt` that
   the mount stays and that `sshfs` runs on its own. Start Plasma Opus again,
   check that the mount is in Places and **Go › Remote Connections** offers
   **Unmount**, and that **Unmount** removes it. With the setting checked
   again, check that a kept mount ends at quit.
3. Check **Start Plasma Opus in the system tray at login**, choose **Mount
   when Plasma Opus starts** for a connection, apply, and check that **Open
   Autostart Settings…** lists the entry. Log out and in, and check that Plasma
   Opus starts in the tray without a window and mounts the connection. Clear
   the setting and check that the entry is removed.
4. Assign a Favorite on a saved connection to a workspace, check **Mount the
   connections holding a workspace's locations when it is activated**, and
   switch to the workspace from **Go › Workspaces**, an Activity and KRunner.
   Check that the connection mounts each time, with KIO FUSE and with sshfs.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings), then
   remove this entry and its roadmap link.

## Cloud sync settings on Plasma (#720)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#720]
  (Merged).
- **Blocker:** The development container has no Plasma session, system tray,
  NetworkManager or rclone, so starting at login, pausing on metered
  connections and rclone's `--conflict-resolve` are tested only with a
  test-mode autostart folder, a fake metered check and the generated rclone
  arguments.
- **Confirmed state:** `SettingsDialog::createSyncIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` embeds `CloudSyncPairsWidget` from
  `src/app/cloudsyncdialog.cpp`, the pair list of **File › Cloud Sync…**, and
  stores the `CloudSync` group's `CloudSyncAtLogin`, `CloudSyncPauseOnMetered`
  and `CloudSyncCheckInterval`. `MainWindow::applySettings()` writes or removes
  `autostart/org.kde.opusfilemanager-cloud-sync.desktop`, which starts
  `--background-sync`, and `CloudSync::Scheduler` reads the metered and
  interval keys. Pairs store `DefaultInterval` and `ConflictChoice`;
  `CloudSync::Runner::planRun()` applies the choice to automatic runs and
  `CloudSync::rcloneArguments()` passes it to bisync. `cloudsynctest`,
  `cloudsyncrunnertest`, `cloudsyncdialogtest` and `integrationsettingstest`
  pass in the container. See
  [Cloud sync settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, add a pair under **Preferences › Remote and Cloud › Sync**, check
   that it appears in **File › Cloud Sync…** at once, and that cancelling
   Preferences keeps it.
2. Check **Start Plasma Opus in the system tray at login to synchronize pairs
   in the background**, apply, and check that **Open Autostart Settings…**
   lists the entry. Log out and in, and check that Plasma Opus starts in the
   tray without a window and synchronizes the pair on the default interval.
   With **Start Plasma Opus in the system tray at login** under **Remote
   Drives** also checked, check that a single tray instance starts. Clear the
   setting and check that the entry is removed.
3. With NetworkManager, mark the connection as metered (`nmcli connection
   modify ID connection.metered yes`), check that the tab reports it and that
   background runs wait, then clear **Pause background synchronization on
   metered connections** and check that they run.
4. With rclone 1.66 or later, give an rclone pair **Keep Local Version**,
   change a file on both sides, and check that **Sync Now** keeps the local
   version and bisync keeps the remote one under a conflict name. Record any
   differences in [CAVEATS](CAVEATS.md#integration-settings), then remove this
   entry.

## rsync settings on Plasma (#366)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#366]
  (Merged).
- **Blocker:** The development container has rsync but no SSH host, KIO
  `sftp:/` worker connection or ksshaskpass, so the settings are tested with
  local folders and an unreachable SSH address only.
- **Confirmed state:** `SettingsDialog::createSyncIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` adds the **rsync Backend** group,
  which stores the `Rsync` group's `RsyncEnabled`, `RsyncForLocalFolders`,
  `RsyncProfile`, `RsyncCopyPolicy` and `RsyncAllowDeletions`.
  `FolderSyncDialog` in `src/app/foldersyncdialog.cpp` reads them when it
  opens, hides **Delete** actions for rsync while deletions aren't allowed,
  and refuses such plans through `RsyncSync::deletionsError()`.
  `rsyncsynctest`, `foldersyncprofilestest`, `foldersyncdialogtest` and
  `integrationsettingstest` pass in the container. See
  [rsync settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with rsync and ksshaskpass installed and an SSH host that has
   rsync, save a profile with filters in **File › Compare and Synchronize
   Folders…** and choose it under **Preferences › Remote and Cloud › Sync ›
   rsync Backend**.
2. Show the host in one pane through `sftp:/`, open the dialog, and check
   that it starts with rsync, loads the profile and uses **Only replace older
   files**. Compare, and check that no **Delete** action is offered.
3. Allow deletions, compare again, plan a deletion and check that the dry
   run lists it and the run removes the file on the host.
4. Turn rsync off and check that the dialog reports that SSH folders can't be
   compared. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry.

## Transfer and cache settings on Plasma (#721)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#721]
  (Merged).
- **Blocker:** The development container has no rclone remote, FUSE device or
  system tray, so the settings are tested through the rclone options they
  produce and an offscreen tray menu only.
- **Confirmed state:** `SettingsDialog::createTransfersIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` adds the **Transfers and Cache** tab,
  which stores the `CloudActivity` group's `BandwidthLimitKiB`,
  `ParallelTransfers`, `CacheSizeMiB`, `CacheMaxAgeHours` and `ShowInTray`.
  `CloudActivity::loadLimits()` and `rcloneMountOptions()` in
  `src/core/cloudactivity.cpp` turn them into `--bwlimit`, `--transfers`,
  `--vfs-cache-max-size` and `--vfs-cache-max-age`, which
  `MainWindow::applySettings()` passes to `RcloneMounts` and the cloud sync
  scheduler. `DesktopTray` hides **Cloud Activity…** when `ShowInTray` is off.
  `cloudactivitytest`, `cloudactivitydialogtest`, `desktoptraytest` and
  `integrationsettingstest` pass in the container. See
  [Transfer and cache settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with rclone and a configured remote, set a bandwidth limit,
   parallel transfers, a cache size and a retention time under
   **Preferences › Remote and Cloud › Transfers and Cache**.
2. Mount the remote and check that `ps` shows `--bwlimit`, `--transfers`,
   `--vfs-cache-max-size` and `--vfs-cache-max-age` with those values. Open a
   file, disconnect the network and check that it still opens, then check
   that it leaves the cache after the retention time.
3. Run an rclone cloud sync pair and check that its `rclone bisync` command
   uses the same bandwidth limit and parallel transfers.
4. Turn off **Offer Cloud Activity… in the system tray menu** and check
   that the tray menu hides **Cloud Activity…**, then turn it on again.
5. Change the limits in **File › Cloud Activity…** and check that
   Preferences shows them when it opens again. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry.

## LAN sharing settings on Plasma (#664)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#664]
  (Merged).
- **Blocker:** The development container has only loopback and its own network
  interface, with no Wi-Fi, VPN, firewall or phone client, so interfaces are
  tested with loopback and fake interfaces only.
- **Confirmed state:** `SettingsDialog::createLanSharingIntegrationPage()` in
  `src/app/integrationsettingspages.cpp` adds the **LAN Sharing** tab, which
  stores the `LanShare` group's `Protocol`, `Https`, `ExcludedInterfaces`,
  `Port`, `TimeoutMinutes`, `AllowUploads` and `UploadFolder`.
  `LanShare::defaultsFromSettings()`, `interfaceAddresses()`,
  `networkInterfaces()` and `chosenByDefault()` in `src/core/lanshare.cpp`
  read them, and `LanShareServer::Options::protocol` refuses `PROPFIND` for
  **Browsers only** and folder pages for **File managers only (WebDAV)**.
  `LanShareDialog` starts from the defaults and adds **Serve to:**.
  `lansharetest`, `lansharedialogtest` and `integrationsettingstest` pass in
  the container. See [LAN sharing settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with a VPN or virtual-machine interface up, clear it under
   **Preferences › Remote and Cloud › LAN Sharing** and check that **File ›
   Share on Local Network…** starts with only the other private addresses
   checked.
2. Set a fixed port, open it with `sudo firewall-cmd --add-port=PORT/tcp` or
   `sudo ufw allow PORT/tcp`, and connect from a phone over HTTPS, comparing
   the certificate's fingerprint with the window's.
3. Choose **Browsers only** and check that Dolphin can't open the share's
   `webdavs://` link; choose **File managers only (WebDAV)** and check that a
   browser gets the explanation for folders but still downloads a file link.
4. Allow uploads by default into a chosen folder, upload from the phone and
   check that the file arrives in that folder.
5. Set a short time limit and check that the share stops after it. Record any
   differences in [CAVEATS](CAVEATS.md#integration-settings), then remove this
   entry.

## Plasma Vault settings on Plasma (#669)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#669]
  (Merged).
- **Blocker:** The development container has no Plasma Vault, no kded6
  module for it and no CryFS, gocryptfs or EncFS, so the settings are tested
  with a fake vault client only.
- **Confirmed state:** `SettingsDialog::createPlasmaVaultGroup()` in
  `src/app/integrationsettingspages.cpp` adds the **Plasma Vault** group of
  the **Encryption** tab, which stores the `PlasmaVault` group's
  `PlasmaVaultMenu`, `PlasmaVaultHidden` and `PlasmaVaultShowState`.
  `MainWindow::applySettings()` shows or hides **Go › Plasma Vaults**, and
  `PlasmaVaultMenu::populate()` in `src/app/plasmavaultmenu.cpp` leaves out
  hidden vaults through `PlasmaVaults::shownVaults()` and their state through
  `PlasmaVaults::isLockState()`. `plasmavaultstest`, `plasmavaultmenutest`
  and `integrationsettingstest` pass in the container. See
  [Plasma Vault settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with Plasma Vault and two vaults, open **Preferences ›
   Security › Encryption** and check that both vaults are listed with their
   state, then open one from the system tray applet and check that the list
   follows.
2. Clear one vault, choose **Apply** and check that **Go › Plasma Vaults**
   leaves it out, and that a wrong password entered for it in the applet
   shows no message in the Plasma Opus status bar.
3. Clear **Show whether each vault is open or closed beside its name and in
   its icon** and check the menu's names and icons.
4. Clear **Offer Plasma Vault's encrypted vaults in Go › Plasma Vaults**,
   check that the menu and any shortcut assigned to it are gone, and that
   `dbus-monitor` shows no `availableDevices` call when Preferences opens.
   Record any differences in [CAVEATS](CAVEATS.md#integration-settings), then
   remove this entry and its roadmap link.

## File encryption settings on Plasma (#670)

- **Milestone and issue:** [M28](ROADMAP.md#m28-integration-settings), [#670]
  (Merged).
- **Blocker:** The development container has no Kleopatra, no `aescrypt`
  and no pinentry program, so the settings are tested with GnuPG keys
  without passwords and fake `aescrypt` and `kleopatra` scripts only.
- **Confirmed state:** `SettingsDialog::createFileEncryptionGroup()` in
  `src/app/integrationsettingspages.cpp` adds the **Files** group of the
  **Encryption** tab, which stores the `Encryption` group's `GnuPGActions`,
  `AesCryptActions`, `EncryptionArmor`, `EncryptedSuffix` and
  `DecryptedNames`. `MainWindow::applyEncryptionSettings()` shows or hides
  the actions and **File › Encryption**, `FileEncryptionDialog` reads the
  text format and names, and `FileEncryption::outputName()` in
  `src/core/fileencryption.cpp` names the files. `fileencryptiontest`,
  `fileencryptiondialogtest` and `integrationsettingstest` pass in the
  container. See [File encryption settings](CAVEATS.md#integration-settings).

**Steps to resume:**

1. On Plasma, with GnuPG, `pinentry-qt`, Kleopatra and `aescrypt`
   installed, assign shortcuts to **Encrypt…** and **Encrypt with AES
   Crypt…** and add **Encryption** to the main toolbar.
2. Clear **Offer AES Crypt's encrypt and decrypt actions in File ›
   Encryption**, choose **Apply** and check that only the AES Crypt entries
   leave **File › Encryption** and the context menu, and that their shortcut
   does nothing.
3. Clear **Offer GnuPG's encrypt, decrypt, sign and verify actions, and
   Kleopatra's, in File › Encryption** as well and check that the menu, the
   toolbar button and the shortcuts are gone; select both again and check
   that Kleopatra's entries return.
4. Choose `.pgp`, **Marked as decrypted** and the text format, encrypt a
   file for your key, then decrypt it and an `.aes` file, and check the
   names, that GnuPG's password prompt appears and that Kleopatra opens the
   `.pgp` file. Record any differences in
   [CAVEATS](CAVEATS.md#integration-settings), then remove this entry and
   its roadmap link.

## Nextcloud accounts on a real server (#588)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#588]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, web
  browser, KDE Wallet, Plasma session or KDE account providers, so Login
  Flow, the stored app password and accounts from KDE Online Accounts are
  tested only against a local fake server and an in-memory password store.
- **Confirmed state:** `NextcloudAccounts` in
  `src/core/nextcloudaccount.cpp` saves account references in the
  `NextcloudAccounts` group without credentials and merges those from KDE
  Online Accounts. `NextcloudClient` in `src/core/nextcloudclient.cpp` runs
  Login Flow v2, reads the capabilities, user and Photos folder, revokes app
  passwords, and keeps them through `kpasswdserver` with
  `Nextcloud::PasswordServerStore`. `NextcloudAccountsDialog` and
  `NextcloudMenu` in `src/app/` provide **Go › Nextcloud** and **Manage
  Nextcloud Accounts…**. `nextcloudaccounttest`, `nextcloudclienttest` and
  `nextcloudaccountsdialogtest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled and a disposable Nextcloud server with a synthetic account,
   check that **Go › Nextcloud** shows only **No Nextcloud accounts** and
   **Manage Nextcloud Accounts…**.
2. Enter the server address and choose **Sign In…**; check that the
   browser opens Nextcloud's sign-in page, and that after granting access
   the account is listed and **Check** reports the version, the user and the
   available apps. Check that `~/.config/opus-file-managerrc` holds no
   password and that KDE Wallet Manager shows the app password.
3. Open the account from **Go › Nextcloud** and check that the files list
   without a password prompt. If KIO asks for a password, note the realm in
   the server's `WWW-Authenticate` header and the realm stored for the
   account; the two must match.
4. Revoke the app password in Nextcloud under **Settings › Security**,
   choose **Check** and check that it reports expired access. Try a server
   with a self-signed certificate and check that the dialog reports the
   certificate problem.
5. Add a Nextcloud account in **System Settings › Online Accounts**, opt in
   under **Preferences › KDE Plasma › Online Accounts**, and check that it is
   listed but can't be removed. Choose **Remove…** › **Revoke and Remove**
   for the signed-in account and check that the server and KDE Wallet no
   longer have the app password. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud browsing on a real server (#365)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#365]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, KDE Wallet
  or Plasma session, so browsing and server permissions are tested only
  against a local fake server that answers `PROPFIND` without
  authentication.
- **Confirmed state:** `MainWindow` in `src/app/mainwindow.cpp` reads the
  `oc:permissions` of a Nextcloud folder with
  `NextcloudClient::fetchPermissions` after each listing, and
  `Nextcloud::deniedReason` in `src/core/nextcloudclient.cpp` decides which
  of **Rename…**, **Delete Permanently**, **Cut**, **Paste**, **Create New**,
  **New Folder…**, **Copy to Other Pane** and **Move to Other Pane** to
  disable, with the reason as the tooltip. `nextcloudclienttest` and
  `nextcloudaccountsdialogtest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled and a disposable Nextcloud server with two synthetic users, sign
   in as the first user under **Go › Nextcloud › Manage Nextcloud
   Accounts…**. As the second user, share a folder with the first user
   read-only and another with every permission.
2. Open the account in each pane from **Go › Nextcloud**. Copy, move,
   rename and delete files in the user's own folders, cancel a large
   transfer, and check KIO's confirmations, progress and error reports.
3. Open the read-only share and check that **Paste**, **Create New** and
   **New Folder…** are disabled with the server's reason, that the status
   bar reports it, and that **Rename…**, **Delete Permanently** and **Cut**
   are disabled for its items. Check that **Copy to Other Pane** is disabled
   when the other pane shows it.
4. Drag files into the read-only share and check how KIO reports the
   refusal. Change the share to allow editing, reload the folder and check
   that the actions are enabled again.
5. Repeat with a Nextcloud account from KDE Online Accounts, and note
   whether the permissions can be read. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud previews on a real server (#589)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#589]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, KDE Wallet
  or Plasma session, so server previews and metadata are tested only against
  a local fake server and recorded Nextcloud 26 and 29 responses.
- **Confirmed state:** `NextcloudClient::fetchMetadata` and
  `NextcloudClient::fetchPreview` in `src/core/nextcloudassets.cpp` read an
  item's properties with a `PROPFIND` of depth 0 and its preview from
  `index.php/core/preview`. `MainWindow` in `src/app/mainwindow.cpp` hands the
  preview to `PreviewPanel::showServerPreview` and the metadata to
  `InformationPanel::setServerMetadata`. `nextcloudassetstest` and
  `nextcloudpreviewtest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled and a disposable Nextcloud server with a synthetic user and the
   Photos app, upload photos with EXIF and GPS data, a PDF, a video and a
   text file. Sign in under **Go › Nextcloud › Manage Nextcloud Accounts…**.
2. Open the account from **Go › Nextcloud**, show **View › Preview Panel**
   and select each file. Check that photos and the PDF show **Preview from
   the server** without downloading the file, and that **Download Full
   Preview** downloads it. Check that the video and text file fall back to
   the usual download when the server has no preview.
3. Show **View › Information Panel** and check the **Nextcloud server**
   section: file ID, owner, size, modification time, ETag, favourite, server
   tags, sharing and the photo metadata. Mark a file as a favourite in the
   web interface and check that the panel shows it within a minute.
4. Repeat with a shared file, a file on external storage and an account from
   KDE Online Accounts, and note which properties the server withholds.
   Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud asset search on a real server (#590)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#590]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, KDE Wallet
  or Plasma session, so search, favourites and server tags are tested only
  against a local fake server and recorded Nextcloud responses.
- **Confirmed state:** `NextcloudClient::search`,
  `NextcloudClient::listFavorites`, `NextcloudClient::listTagged`,
  `NextcloudClient::setFavorite` and the system tag requests in
  `src/core/nextcloudorganize.cpp` send `SEARCH`, `REPORT`, `PROPPATCH`,
  `PROPFIND`, `PUT`, `DELETE` and `POST` requests through KIO.
  `NextcloudAssetsDialog` and `NextcloudTagsDialog` in `src/app/` and the
  **Nextcloud** context submenu use them. `nextcloudorganizetest` and
  `nextcloudorganizedialogtest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled and a disposable Nextcloud server with a synthetic user and the
   Collaborative tags app, upload photos, documents and a folder, and sign in
   under **Go › Nextcloud › Manage Nextcloud Accounts…**.
2. Open **Go › Nextcloud › *account* › Search and Organize…**. Search by name
   and by each kind, and check that **Show in Pane** opens the results. Search
   for a common letter and check that more than 500 matches are reported as
   cut short.
3. From a file's context menu, choose **Nextcloud › Add to Nextcloud
   Favourites**, check the star in the web interface and the **Favourites**
   list, then remove it again.
4. Choose **Nextcloud › Nextcloud Tags…** for several files, assign and
   remove tags, create a tag after confirming, and check the changes in the
   web interface and in the **Nextcloud tag (server)** list. Check that a
   restricted tag is disabled for a user who isn't an administrator and that
   no server tag appears in the **Tag Panel**.
5. Repeat with Nextcloud 15, a server with the Collaborative tags app
   disabled, a read-only share and an account from KDE Online Accounts, and
   check the dialog's notices. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud Photos on a real server (#671)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#671]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, Photos app,
  KDE Wallet or Plasma session, so albums, shared albums and places are tested
  only against a local fake server. The `places` or `locations` collection,
  the album entry names, the `nc:metadata-photos-*` properties and the
  permissions of shared albums are unverified against a real server.
- **Confirmed state:** `NextcloudClient::listPhotoCollections`,
  `NextcloudClient::listPhotos`, `NextcloudClient::addToAlbum`,
  `NextcloudClient::removeFromAlbum` and `NextcloudClient::createAlbum` in
  `src/core/nextcloudphotos.cpp` send `PROPFIND`, `COPY`, `DELETE` and `MKCOL`
  requests through KIO, and `Nextcloud::albumEntryUrl` refuses any `DELETE`
  target but a photo directly in an album. `NextcloudPhotosDialog` and
  `NextcloudAddToAlbumDialog` in `src/app/nextcloudphotosdialog.cpp` provide
  **Go › Nextcloud › *account* › Photos…** and **Nextcloud › Add to
  Nextcloud Album…**. `nextcloudphotostest` and `nextcloudphotosdialogtest`
  pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled and a disposable Nextcloud server with the Photos app and two
   synthetic users, upload photos with EXIF dates and GPS data and a photo
   without them as the first user, and sign in under **Go › Nextcloud ›
   Manage Nextcloud Accounts…**.
2. Open **Go › Nextcloud › *account* › Photos…**, create an album with **New
   Album…**, and add photos to it from a pane with **Nextcloud › Add to
   Nextcloud Album…**. Check that the album lists them, sorted and grouped by
   capture date and place, and that the undated photo is listed last.
3. Select a photo and choose **Remove from Album…**. Check that the album no
   longer lists it and that the file is still in its folder, in the web
   interface's Files and in other albums.
4. Show **Places** and check that the server's places are listed and
   read-only. Share an album with the second user, read-only and with
   adding allowed, and sign in as that user to check **Shared albums**, the
   disabled **Remove from Album…** and the albums offered by **Add to
   Nextcloud Album…**.
5. Disable the Photos app and check that the window says Photos is
   unavailable. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud shares on a real server (#591)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#591]
  (Merged).
- **Blocker:** The development container has no Nextcloud server, mail client,
  desktop portal, KDE Wallet or Plasma session, so shares are tested only
  against a local fake server with recorded OCS responses, and **Send Link by
  Email…** only with a fake composer. The permission combinations the server
  accepts for links and user and group shares, its responses to `PUT` and
  `DELETE`, and its error messages are unverified.
- **Confirmed state:** `NextcloudClient::listShares`,
  `NextcloudClient::fetchShareCapabilities`, `NextcloudClient::createShare`,
  `NextcloudClient::updateShare` and `NextcloudClient::deleteShare` in
  `src/core/nextcloudshares.cpp` send OCS requests through KIO and never keep
  share passwords or password hashes. `NextcloudSharesDialog` and
  `NextcloudShareEditor` in `src/app/nextcloudsharesdialog.cpp` provide **Go ›
  Nextcloud › *account* › Shares…** and **Nextcloud › Nextcloud Shares…**,
  confirm creating, broadening and revoking shares, and open a reviewed link
  through `SendByEmailDialog::composeMessage`. `nextcloudsharestest` and
  `nextcloudsharesdialogtest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, KDE Wallet
   enabled, a default mail client and a disposable Nextcloud server with two
   synthetic users and a group, sign in as the first user under **Go ›
   Nextcloud › Manage Nextcloud Accounts…**.
2. In a pane, choose **Nextcloud › Nextcloud Shares…** on a folder. Create a
   public link with a password and an expiry, a file drop link and a share
   with the second user, confirming each, and check them in the web
   interface's sharing sidebar. Check that the password isn't in
   `~/.config/opus-file-managerrc` or `~/.local/state/opus-file-managerstaterc`.
3. Narrow and widen a share with **Change…**, checking that only widening
   asks first, then **Revoke…** a link and check that it stops working.
4. Choose **Send Link by Email…**, review the link and check that the mail
   client opens a message with the link and without the password. Repeat in
   the Flatpak build through the desktop portal.
5. Sign in as the second user and check **Shared with me**. Enforce link
   passwords and expiry on the server, then disable public links and the
   sharing app, and check the editor's rules and the window's notices.
   Record any differences in [CAVEATS](CAVEATS.md#nextcloud-integration),
   then remove this entry and its roadmap link.

## Nextcloud desktop client on Plasma (#592)

- **Milestone and issue:** [M29](ROADMAP.md#m29-nextcloud-integration), [#592]
  (Merged).
- **Blocker:** The development container has no Nextcloud desktop client,
  Nextcloud server or Plasma session, so the desktop client's folders are
  tested only with recorded configuration files and a fake local socket. The
  configuration written by current client releases, the replies of a running
  client, its socket and configuration paths in Flatpak and Snap builds, and
  launching the client from its desktop entry are unverified.
- **Confirmed state:** `NextcloudDesktop::readConfiguration` in
  `src/core/nextclouddesktop.cpp` reads `nextcloud.cfg` read-only with
  `QSettings`, and `NextcloudDesktop::StatusQuery` asks the client's socket for
  each folder's status with `RETRIEVE_FILE_STATUS` and a timeout.
  `CloudSync` reuses the same parser to recognize the client's folders.
  `NextcloudMenu::populateDesktopClient` and
  `MainWindow::populateNextcloudDesktop` provide **Go › Nextcloud › Desktop
  Client**, and `MainWindow::openNextcloudDesktopClient` starts the client
  from its desktop entry. `nextclouddesktoptest`, `nextclouddesktopmenutest`
  and `cloudsynctest` pass in the container. See
  [Nextcloud integration](CAVEATS.md#nextcloud-integration).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, a disposable
   Nextcloud server and the distribution's Nextcloud desktop client, sign the
   client in and add one folder with full copies and one with virtual files.
   Note where it keeps `nextcloud.cfg`.
2. Open **Go › Nextcloud › Desktop Client** and check that the folders show
   **Local** and **Remote** with **Up to date**, then **Synchronizing** while
   a large file uploads, and **Needs attention** or **Error** for a file the
   server rejects. Check **Open**, **Open in Other Pane** and, after adding
   the same account under **Manage Nextcloud Accounts…**, **Open on Server**.
3. Pause a folder in the client and check **Offline**. Quit the client and
   check **Desktop client not running**, then start it again with **Open
   Desktop Client**. Remove a folder's local directory and check
   **Unknown**.
4. Repeat with the Flatpak client and with the Plasma Opus Flatpak build, and
   record where each keeps its configuration and socket.
5. Quit the client, open the menu several times and check that
   `nextcloud.cfg` keeps its modification time and that no
   `nextcloud.cfg.lock` is left behind. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-integration), then remove this entry and
   its roadmap link.

## Nextcloud settings on Plasma (#368)

- **Milestone and issue:** [M30](ROADMAP.md#m30-nextcloud-settings), [#368]
  (Completed).
- **Blocker:** The development container has no Nextcloud server, Plasma
  session, KDE Online Accounts or Nextcloud desktop client, so
  **Preferences › Remote and Cloud › Nextcloud** is tested only with fake
  servers, recorded client configuration and a fake socket. The tab's layout
  and wording in Plasma, and its settings against a real server and client,
  are unverified.
- **Confirmed state:** The `Nextcloud` group in
  `src/core/opusfilemanager.kcfg` holds the settings, and
  `SettingsDialog::createNextcloudIntegrationPage` in
  `src/app/integrationsettingspages.cpp` provides the tab. `MainWindow` reads
  them for previews, metadata, permissions, the context menu, **Go ›
  Nextcloud** and the desktop client; `NextcloudAssetsDialog::setResultLimit`
  and `NextcloudSharesDialog::setDefaultLinkExpiryDays` apply the limit and
  expiry. `settingstest`, `integrationsettingstest` and the `nextcloud` tests
  pass in the container. See
  [Nextcloud settings](CAVEATS.md#nextcloud-settings).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, a disposable
   Nextcloud server with a synthetic account and the Nextcloud desktop
   client, open **Preferences › Remote and Cloud › Nextcloud** and check that
   the tab fits the dialog, scrolls when it is small and reports the saved
   accounts, Online Accounts, thumbnails, mail and desktop client correctly.
2. Choose **Manage Nextcloud Accounts…**, add the account and check that the
   tab's count updates. Check that the two **Go to** buttons open the right
   tabs.
3. Clear and restore each setting, choosing **Apply** each time, and check
   the preview panel, the information panel, actions in a read-only share,
   the context menu, **Go › Nextcloud**, **Search and Organize…** and the
   desktop client's folders against the descriptions under
   [Settings](integrations/nextcloud.md#implemented-features).
4. Set **New public links expire after** to 7 days and create a link, then
   enforce a 3-day maximum on the server and check that new links start with
   3 days. Record any differences in
   [CAVEATS](CAVEATS.md#nextcloud-settings), then remove this entry and its
   roadmap link.

## Nextcloud Photos settings on a real server (#672)

- **Milestone and issue:** [M30](ROADMAP.md#m30-nextcloud-settings), [#672]
  (Completed).
- **Blocker:** The development container has no Nextcloud server with the
  Photos app or Plasma session, so the **Photos** group of **Preferences ›
  Remote and Cloud › Nextcloud** is tested only against a fake server. The
  group's layout in Plasma, and its effect on real albums, shared albums and
  places, are unverified.
- **Confirmed state:** The `PhotosShowAlbums`, `PhotosShowSharedAlbums`,
  `PhotosShowPlaces`, `PhotosGrouping` and `PhotosConfirmChanges` entries of
  the `Nextcloud` group in `src/core/opusfilemanager.kcfg` hold the settings,
  and `SettingsDialog::createNextcloudIntegrationPage` in
  `src/app/integrationsettingspages.cpp` provides the group. `MainWindow`
  passes them to `NextcloudPhotosDialog` and `NextcloudAddToAlbumDialog` as
  `NextcloudPhotos::Options`, and offers **Photos…** and **Add to Nextcloud
  Album…** only when a kind they list is shown. See
  [Nextcloud Photos settings](CAVEATS.md#nextcloud-settings).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh` and a disposable
   Nextcloud server with the Photos app, a synthetic account, an album of
   your own shared with a second synthetic user, an album that user shares
   with you and indexed photos with places, open **Preferences › Remote and
   Cloud › Nextcloud** and check that the **Photos** group fits the tab and
   its status explains what isn't offered.
2. Clear each kind in turn, choosing **Apply**, and check **Show** in
   **Photos…**, **New Album…** in both windows, the albums listed in **Add
   to Nextcloud Album…** and whether **Go › Nextcloud** and the context menu
   offer them.
3. Choose each **Group photos by** option and check the grouping **Photos…**
   opens with, and that **As last chosen in the window** restores the
   previous behaviour.
4. Clear **Ask before adding photos to or removing them from your albums**
   and check that an unshared album of your own changes without a question,
   while your album shared with the second user and the album shared with
   you still ask. Record any differences, in particular whether the server
   reports `nc:collaborators` for your shared album, in
   [CAVEATS](CAVEATS.md#nextcloud-settings), then remove this entry and its
   roadmap link.

## Keyboard shortcuts on Plasma (#230)

- **Milestone and issue:** [M31](ROADMAP.md#m31-keyboard-shortcut-settings),
  [#230] (Development).
- **Blocker:** The development container has no Plasma session, D-Bus session
  services or KDE global shortcut service (KGlobalAccel), and only an
  offscreen platform without real keyboard layouts. **Preferences › Keyboard
  › Shortcuts** is tested with window shortcuts only; its global shortcut
  rows, conflicts between global and window shortcuts, its layout in Plasma
  and key sequences recorded in KDE's editor are unverified.
- **Confirmed state:** `src/core/keybindings.cpp` finds customized shortcuts
  and conflicts, and `SettingsDialog::createKeyboardShortcutsPage` in
  `src/app/keyboardsettingspages.cpp` provides the tab. `keybindingstest`,
  `settingsdialogtest`, `settingstest` and `mainwindowtest` pass in the
  container. See [Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, open
   **Preferences › Keyboard › Shortcuts** and check that the tab fits the
   dialog, says every action has its default shortcuts and lists no
   conflicts, and that **Global Shortcuts** shows the keys of **Show or Hide
   Plasma Opus** and the drop-down window.
2. In **Configure Keyboard Shortcuts…**, give **Copy to Other Pane** the
   sequence <kbd>Ctrl</kbd>+<kbd>K</kbd>, <kbd>Ctrl</kbd>+<kbd>C</kbd> with a
   non-US layout as well as US English, save, and check that the sequence
   runs the action and that the tab lists it as customized.
3. Give a window action the key of the global shortcut and check that the
   tab lists the conflict and which one runs. Stop `kglobalacceld` and check
   that the tab says global shortcuts don't work.
4. Choose **Reset All Shortcuts…** and check that window shortcuts return to
   their defaults while global shortcuts are kept. Record any differences in
   [CAVEATS](CAVEATS.md#keyboard-shortcuts), then remove this entry and its
   roadmap link.

## Shortcut contexts on Plasma (#233)

- **Milestone and issue:** [M31](ROADMAP.md#m31-keyboard-shortcut-settings),
  [#233] (Development).
- **Blocker:** The development container has only an offscreen platform with
  synthetic key events, no real keyboard layouts or input methods, and no
  Konsole session to type into. Whether text fields, lists and the folder tree
  keep their keys with real layouts and input methods is unverified.
- **Confirmed state:** `KeyBindings::isReserved` in
  `src/core/keybindings.cpp` decides which keys each context keeps,
  `ShortcutGuard` in `src/app/shortcutguard.cpp` keeps them for the focused
  widget of the main window, and the **Applies in** column and the reserved
  key conflicts are on **Preferences › Keyboard › Shortcuts**.
  `keybindingstest`, `settingsdialogtest` and `mainwindowtest` pass in the
  container. See [Keyboard shortcuts](CAVEATS.md#keyboard-shortcuts).

**Steps to resume:**

1. On Plasma 6 with Plasma Opus installed by `./install.sh`, turn on
   **Tab switches panes** and check that <kbd>Tab</kbd> in the location bar,
   the filter bar and an inline rename editor moves the focus or completes
   rather than switching panes, and still switches panes from a file view.
2. In **Configure Keyboard Shortcuts…**, give **Copy to Other Pane** the
   alternate shortcut <kbd>Down</kbd>, save, and check that
   **Preferences › Keyboard › Shortcuts** lists three conflicts, that
   <kbd>Down</kbd> still moves in a file view, the folder tree and a text
   field, and that it runs the action with the focus on a toolbar button.
3. Repeat step 1 with a non-Latin layout, such as Russian, and with an input
   method, such as Fcitx 5 or IBus, and check that typing in the location bar
   still works and <kbd>Ctrl</kbd>+<kbd>C</kbd> copies text there.
4. Open the embedded terminal and check that its keys behave as before.
   Restore the shortcuts. Record any differences in
   [CAVEATS](CAVEATS.md#keyboard-shortcuts), then remove this entry and its
   roadmap link.

[#200]: https://github.com/mikepadiernos/plasma-opus/issues/200
[#201]: https://github.com/mikepadiernos/plasma-opus/issues/201
[#230]: https://github.com/mikepadiernos/plasma-opus/issues/230
[#233]: https://github.com/mikepadiernos/plasma-opus/issues/233
[#286]: https://github.com/mikepadiernos/plasma-opus/issues/286
[#288]: https://github.com/mikepadiernos/plasma-opus/issues/288
[#290]: https://github.com/mikepadiernos/plasma-opus/issues/290
[#291]: https://github.com/mikepadiernos/plasma-opus/issues/291
[#292]: https://github.com/mikepadiernos/plasma-opus/issues/292
[#293]: https://github.com/mikepadiernos/plasma-opus/issues/293
[#297]: https://github.com/mikepadiernos/plasma-opus/issues/297
[#365]: https://github.com/mikepadiernos/plasma-opus/issues/365
[#366]: https://github.com/mikepadiernos/plasma-opus/issues/366
[#368]: https://github.com/mikepadiernos/plasma-opus/issues/368
[#371]: https://github.com/mikepadiernos/plasma-opus/issues/371
[#391]: https://github.com/mikepadiernos/plasma-opus/issues/391
[#392]: https://github.com/mikepadiernos/plasma-opus/issues/392
[#393]: https://github.com/mikepadiernos/plasma-opus/issues/393
[#394]: https://github.com/mikepadiernos/plasma-opus/issues/394
[#443]: https://github.com/mikepadiernos/plasma-opus/issues/443
[#563]: https://github.com/mikepadiernos/plasma-opus/issues/563
[#564]: https://github.com/mikepadiernos/plasma-opus/issues/564
[#565]: https://github.com/mikepadiernos/plasma-opus/issues/565
[#566]: https://github.com/mikepadiernos/plasma-opus/issues/566
[#578]: https://github.com/mikepadiernos/plasma-opus/issues/578
[#588]: https://github.com/mikepadiernos/plasma-opus/issues/588
[#589]: https://github.com/mikepadiernos/plasma-opus/issues/589
[#590]: https://github.com/mikepadiernos/plasma-opus/issues/590
[#591]: https://github.com/mikepadiernos/plasma-opus/issues/591
[#592]: https://github.com/mikepadiernos/plasma-opus/issues/592
[#654]: https://github.com/mikepadiernos/plasma-opus/issues/654
[#655]: https://github.com/mikepadiernos/plasma-opus/issues/655
[#656]: https://github.com/mikepadiernos/plasma-opus/issues/656
[#657]: https://github.com/mikepadiernos/plasma-opus/issues/657
[#658]: https://github.com/mikepadiernos/plasma-opus/issues/658
[#659]: https://github.com/mikepadiernos/plasma-opus/issues/659
[#663]: https://github.com/mikepadiernos/plasma-opus/issues/663
[#664]: https://github.com/mikepadiernos/plasma-opus/issues/664
[#666]: https://github.com/mikepadiernos/plasma-opus/issues/666
[#669]: https://github.com/mikepadiernos/plasma-opus/issues/669
[#670]: https://github.com/mikepadiernos/plasma-opus/issues/670
[#671]: https://github.com/mikepadiernos/plasma-opus/issues/671
[#672]: https://github.com/mikepadiernos/plasma-opus/issues/672
[#676]: https://github.com/mikepadiernos/plasma-opus/issues/676
[#703]: https://github.com/mikepadiernos/plasma-opus/issues/703
[#704]: https://github.com/mikepadiernos/plasma-opus/issues/704
[#707]: https://github.com/mikepadiernos/plasma-opus/issues/707
[#708]: https://github.com/mikepadiernos/plasma-opus/issues/708
[#709]: https://github.com/mikepadiernos/plasma-opus/issues/709
[#710]: https://github.com/mikepadiernos/plasma-opus/issues/710
[#720]: https://github.com/mikepadiernos/plasma-opus/issues/720
[#721]: https://github.com/mikepadiernos/plasma-opus/issues/721
[#750]: https://github.com/mikepadiernos/plasma-opus/issues/750
