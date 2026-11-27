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

| Handoff                                                                     | Milestone                                                                      | Issues         | Board status                                                                                                                                          |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | -------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| [mpv integration](#mpv-integration-563)                                     | [M27](ROADMAP.md#m27-integrations)                                             | [#563], [#578] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square) |
| [Markdown editor and backlinks](#markdown-editor-and-backlinks-564)         | [M27](ROADMAP.md#m27-integrations)                                             | [#564]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Workspace vault editing](#workspace-vault-editing-565)                     | [M27](ROADMAP.md#m27-integrations), [M28](ROADMAP.md#m28-integration-settings) | [#565], [#566] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square) |
| [System file chooser on Plasma](#system-file-chooser-on-plasma-286)         | [M27](ROADMAP.md#m27-integrations)                                             | [#286]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Mount manager on Plasma](#mount-manager-on-plasma-703)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#703]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [KWin script on Plasma](#kwin-script-on-plasma-707-708)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#707], [#708] | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square) ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)       |
| [Administrator mode on Plasma](#administrator-mode-on-plasma-288)           | [M27](ROADMAP.md#m27-integrations)                                             | [#288]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Plasma Vault on Plasma](#plasma-vault-on-plasma-654)                       | [M27](ROADMAP.md#m27-integrations)                                             | [#654]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [File encryption on Plasma](#file-encryption-on-plasma-655)                 | [M27](ROADMAP.md#m27-integrations)                                             | [#655]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Send by Email on Plasma](#send-by-email-on-plasma-656)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#656]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Mail attachments on Plasma](#mail-attachments-on-plasma-657)               | [M27](ROADMAP.md#m27-integrations)                                             | [#657]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Mail file preview on Plasma](#mail-file-preview-on-plasma-658)             | [M27](ROADMAP.md#m27-integrations)                                             | [#658]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Shared Activity context on Plasma](#shared-activity-context-on-plasma-659) | [M27](ROADMAP.md#m27-integrations)                                             | [#659]         | ![Merged](https://img.shields.io/badge/Merged-8957e5?style=flat-square)                                                                               |
| [Default file manager on Plasma](#default-file-manager-on-plasma-293)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#293]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [File chooser settings on Plasma](#file-chooser-settings-on-plasma-292)     | [M28](ROADMAP.md#m28-integration-settings)                                     | [#292]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Chooser start folder on Plasma](#chooser-start-folder-on-plasma-750)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#750]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Operation notifications on Plasma](#operation-notifications-on-plasma-392) | [M28](ROADMAP.md#m28-integration-settings)                                     | [#392]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Start hidden in tray on Plasma](#start-hidden-in-tray-on-plasma-290)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#290]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Global shortcut on Plasma](#global-shortcut-on-plasma-291)                 | [M28](ROADMAP.md#m28-integration-settings)                                     | [#291]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Activity associations on Plasma](#activity-associations-on-plasma-391)     | [M28](ROADMAP.md#m28-integration-settings)                                     | [#391]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [KDE Connect settings on Plasma](#kde-connect-settings-on-plasma-393)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#393]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [KRunner settings on Plasma](#krunner-settings-on-plasma-394)               | [M28](ROADMAP.md#m28-integration-settings)                                     | [#394]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Online account settings on Plasma](#online-account-settings-on-plasma-371) | [M28](ROADMAP.md#m28-integration-settings)                                     | [#371]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [KWin script settings on Plasma](#kwin-script-settings-on-plasma-709)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#709]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Drop-down settings on Plasma](#drop-down-settings-on-plasma-710)           | [M28](ROADMAP.md#m28-integration-settings)                                     | [#710]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Baloo search settings on Plasma](#baloo-search-settings-on-plasma-443)     | [M28](ROADMAP.md#m28-integration-settings)                                     | [#443]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Office settings on Plasma](#office-settings-on-plasma-676)                 | [M28](ROADMAP.md#m28-integration-settings)                                     | [#676]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                         |
| [Mail client settings on Plasma](#mail-client-settings-on-plasma-666)       | [M28](ROADMAP.md#m28-integration-settings)                                     | [#666]         | ![Development](https://img.shields.io/badge/Development-1f6feb?style=flat-square)                                                                     |

## mpv integration (#563)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#563]
  (Merged); its M28 setting is [#578] (Completed).
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
  [M28](ROADMAP.md#m28-integration-settings), [#566] (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Development).
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

[#200]: https://github.com/mikepadiernos/plasma-opus/issues/200
[#201]: https://github.com/mikepadiernos/plasma-opus/issues/201
[#286]: https://github.com/mikepadiernos/plasma-opus/issues/286
[#288]: https://github.com/mikepadiernos/plasma-opus/issues/288
[#290]: https://github.com/mikepadiernos/plasma-opus/issues/290
[#291]: https://github.com/mikepadiernos/plasma-opus/issues/291
[#292]: https://github.com/mikepadiernos/plasma-opus/issues/292
[#293]: https://github.com/mikepadiernos/plasma-opus/issues/293
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
[#654]: https://github.com/mikepadiernos/plasma-opus/issues/654
[#655]: https://github.com/mikepadiernos/plasma-opus/issues/655
[#656]: https://github.com/mikepadiernos/plasma-opus/issues/656
[#657]: https://github.com/mikepadiernos/plasma-opus/issues/657
[#658]: https://github.com/mikepadiernos/plasma-opus/issues/658
[#659]: https://github.com/mikepadiernos/plasma-opus/issues/659
[#666]: https://github.com/mikepadiernos/plasma-opus/issues/666
[#676]: https://github.com/mikepadiernos/plasma-opus/issues/676
[#703]: https://github.com/mikepadiernos/plasma-opus/issues/703
[#707]: https://github.com/mikepadiernos/plasma-opus/issues/707
[#708]: https://github.com/mikepadiernos/plasma-opus/issues/708
[#709]: https://github.com/mikepadiernos/plasma-opus/issues/709
[#710]: https://github.com/mikepadiernos/plasma-opus/issues/710
[#750]: https://github.com/mikepadiernos/plasma-opus/issues/750
