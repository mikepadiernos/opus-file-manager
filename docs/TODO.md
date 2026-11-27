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

| Handoff                                                                     | Milestone                                                                      | Issues         | Board status                                                                                                                                            |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [mpv integration](#mpv-integration-563)                                     | [M27](ROADMAP.md#m27-integrations)                                             | [#563], [#578] | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square) ![Backlog](https://img.shields.io/badge/Backlog-6e7781?style=flat-square) |
| [Markdown editor and backlinks](#markdown-editor-and-backlinks-564)         | [M27](ROADMAP.md#m27-integrations)                                             | [#564]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Workspace vault editing](#workspace-vault-editing-565)                     | [M27](ROADMAP.md#m27-integrations), [M28](ROADMAP.md#m28-integration-settings) | [#565], [#566] | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square) ![Backlog](https://img.shields.io/badge/Backlog-6e7781?style=flat-square) |
| [System file chooser on Plasma](#system-file-chooser-on-plasma-286)         | [M27](ROADMAP.md#m27-integrations)                                             | [#286]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Mount manager on Plasma](#mount-manager-on-plasma-703)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#703]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [KWin script on Plasma](#kwin-script-on-plasma-707-708)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#707], [#708] | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Administrator mode on Plasma](#administrator-mode-on-plasma-288)           | [M27](ROADMAP.md#m27-integrations)                                             | [#288]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Plasma Vault on Plasma](#plasma-vault-on-plasma-654)                       | [M27](ROADMAP.md#m27-integrations)                                             | [#654]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [File encryption on Plasma](#file-encryption-on-plasma-655)                 | [M27](ROADMAP.md#m27-integrations)                                             | [#655]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Send by Email on Plasma](#send-by-email-on-plasma-656)                     | [M27](ROADMAP.md#m27-integrations)                                             | [#656]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Mail attachments on Plasma](#mail-attachments-on-plasma-657)               | [M27](ROADMAP.md#m27-integrations)                                             | [#657]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Mail file preview on Plasma](#mail-file-preview-on-plasma-658)             | [M27](ROADMAP.md#m27-integrations)                                             | [#658]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |
| [Shared Activity context on Plasma](#shared-activity-context-on-plasma-659) | [M27](ROADMAP.md#m27-integrations)                                             | [#659]         | ![Completed](https://img.shields.io/badge/Completed-2ea44f?style=flat-square)                                                                           |

## mpv integration (#563)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#563]
  (Completed); its M28 setting is [#578] (Backlog).
- **Resolved blocker:** Fedora 44 packages `mpv-devel`, and embedded playback
  uses libmpv's OpenGL render API in a `QOpenGLWidget` rather than KDE's
  MpvQt, so there is no Qt Quick and no process-wide numeric locale change
  unless libmpv refuses to start without it. Initialization failures return
  errors instead of terminating, and builds without libmpv or Qt OpenGL
  Widgets keep Qt Multimedia only.
- **Confirmed state:** `feature/M27-integrations` adds `src/core/mediabackend.cpp`
  and `src/core/mpvplayer.cpp` for backend choice and libmpv playback,
  `src/app/mpvvideowidget.cpp` for drawing video, the backend button in the
  preview and gallery playback controls, and **Audio and video playback**
  under **Preferences › Interface › Previews**. When mpv can't start or draw,
  Qt Multimedia plays the file with a notice. `mediabackendtest`,
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

## Markdown editor and backlinks (#564)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#564]
  (Completed).
- **Resolved blocker:** The source stays untouched in KTextEditor, so no
  source-preserving rewriter is needed. The preview keeps Qt's md4c-based
  Markdown renderer with its **CommonMark** and **GitHub Markdown** dialects,
  and wiki links come from a code-aware scanner in `src/core/wikilinks.cpp`
  that skips code blocks, code spans, HTML comments and escapes.
- **Confirmed state:** `feature/M27-integrations` adds the editor's **Vault**
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
  (Completed); its settings belong to
  [M28](ROADMAP.md#m28-integration-settings), [#566] (Backlog).
- **Resolved blocker:** The built-in Markdown and backlinks feature, [#564],
  is Completed, and workspace profiles and location assignments ([#200] and
  [#201]) are merged.
- **Confirmed state:** `feature/M27-integrations` stores an optional editor
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

**Steps to resume:**

1. On Plasma, assign a real Obsidian vault to a workspace, choose **Use as
   Editor Vault**, and check that its `.obsidian` folder and notes are
   unchanged.
2. Switch workspaces with unsaved edits open and confirm the drafts are kept
   and the editor's own vault returns.
3. Rename the vault folder while the workspace is active and confirm the
   editor reports the unreadable vault, then unassign the location and
   confirm the editor's own vault returns.
4. For [#566], provide the workspace vault options in M28 and verify
   workspace-scoped settings persistence and recovery when vault roots or
   editor features are unavailable.

## System file chooser on Plasma (#286)

- **Milestone and issue:** [M27](ROADMAP.md#m27-integrations), [#286]
  (Completed).
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
  (Completed).
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
  (Completed) and [#708] (Completed).
- **Blocker:** The development container has no KWin, no KPackage script
  structure registered with a compositor and no KGlobalAccel daemon, so the
  script cannot be installed, loaded or driven by real windows there.
- **Confirmed state:** The script package, the **Configure KWin Integration**
  dialog, the workspace placements and the drop-down window are implemented and
  pass their tests in the container, against a fake backend and a simulated
  KWin JavaScript API. This verification does not block the features; it is
  recorded as a caveat under
  [Desktop integration](CAVEATS.md#desktop-integration).

**Steps to resume:**

1. On a Plasma 6 session, open **Settings › Configure KWin Integration…**,
   install and enable the script, and confirm the dialog reports it as
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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
  (Completed).
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

[#200]: https://github.com/mikepadiernos/plasma-opus/issues/200
[#201]: https://github.com/mikepadiernos/plasma-opus/issues/201
[#286]: https://github.com/mikepadiernos/plasma-opus/issues/286
[#288]: https://github.com/mikepadiernos/plasma-opus/issues/288
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
[#703]: https://github.com/mikepadiernos/plasma-opus/issues/703
[#707]: https://github.com/mikepadiernos/plasma-opus/issues/707
[#708]: https://github.com/mikepadiernos/plasma-opus/issues/708
