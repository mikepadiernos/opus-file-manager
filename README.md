# Plasma Opus

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="src/app/branding/plasma-opus-logo-dark.svg">
    <img src="src/app/branding/plasma-opus-logo.svg" alt="Plasma Opus" width="480" height="100">
  </picture>
</p>

<p align="center">
  <strong>A dual-pane file manager for KDE Plasma</strong><br>
  Power-user workflows with native KDE file operations, Places and settings.
</p>

<p align="center">
  <a href="https://github.com/mikepadiernos/plasma-opus/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/mikepadiernos/plasma-opus?style=flat-square&color=3daee9"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white">
  <img alt="Qt 6.8+" src="https://img.shields.io/badge/Qt-6.8%2B-41CD52?style=flat-square&logo=qt&logoColor=white">
  <img alt="KDE Frameworks 6.20+" src="https://img.shields.io/badge/KDE%20Frameworks-6.20%2B-1D99F3?style=flat-square&logo=kde&logoColor=white">
  <img alt="Linux and KDE Plasma" src="https://img.shields.io/badge/platform-Linux%20%C2%B7%20KDE%20Plasma-FCC624?style=flat-square&logo=linux&logoColor=black">
  <a href="https://github.com/users/mikepadiernos/projects/3"><img alt="Project board" src="https://img.shields.io/badge/project-board-8957e5?style=flat-square&logo=github"></a>
</p>

<p align="center">
  <a href="#build-and-run">🚀 Get started</a> ·
  <a href="#features">✨ Features</a> ·
  <a href="#screenshots">📸 Screenshots</a> ·
  <a href="#customize-the-interface">🎛️ Customize</a> ·
  <a href="#safety-and-limitations">🛡️ Safety</a> ·
  <a href="docs/ROADMAP.md">🗺️ Roadmap</a> ·
  <a href="docs/README.md">📚 Docs</a>
</p>

![Plasma Opus with stacked panes and an image preview](preview.png)

> [!NOTE]
> The latest published MVP is
> [v0.2.0](https://github.com/mikepadiernos/plasma-opus/releases/tag/v0.2.0).
> `main` also includes the M15 and M16 workspace milestones, merged after that
> release, the M17 and M18 theme milestones and M19 Branding; see the
> [roadmap](docs/ROADMAP.md#at-a-glance) for status.

## Features

New windows start with stacked panes, visible tab bars and a right-side
preview. Pane navigation starts linked, and activating most files opens a
preview. Both behaviours can be changed in Preferences.

- 🗂️ **Browse:** side-by-side or stacked panes with tabs, history and session
  restoration. Details, icon, compact or flat views, with filtering and
  per-folder formats.
- 📦 **Manage files:** KDE file actions, queued transfers, undo, batch rename,
  duplicate finding and listing export. Browse and manage archives, including
  AES-encrypted ZIP creation and protected extraction, checksums and split
  files.
- 🔀 **Compare and sync:** compare files or folders and preview per-item actions
  before synchronizing local folders.
- 🔎 **Search and automate:** search local folders or a Baloo index, save
  searches and collections, and preview scripts or commands before running
  them.
- 🖼️ **Preview and edit:** preview documents and media, use the image gallery,
  edit text with drafts and edit supported media metadata. Preview, text and
  metadata panels can be placed beside or below the panes.
- 🧭 **Workspaces:** named workspaces with assigned locations, saved layouts,
  explicit pane and tab snapshots, workspace-scoped search, a startup choice,
  recovery options and profile import or export.
- 💻 **Terminal:** embed Konsole below or beside the panes, with optional
  active-folder following. Hiding the panel retains its sessions.
- 🌐 **Languages:** translation catalogues, right-to-left layouts, Jalali dates
  and regional format overrides; see the [translation guide](po/README.md).

## Screenshots

Captured headless in the dev container with synthetic demo files, so KWin
decorations, blur and the Plasma theme are absent. See the
[screenshot notes](docs/screenshots/README.md).

| Dual panes                 | Image preview                  |
| -------------------------- | ------------------------------ |
| [![Dual panes][s1]][s1]    | [![Image preview][s2]][s2]     |
| **Code preview**           | **Embedded terminal**          |
| [![Code preview][s3]][s3]  | [![Embedded terminal][s4]][s4] |
| **Image gallery**          | **Batch rename**               |
| [![Image gallery][s5]][s5] | [![Batch rename][s6]][s6]      |
| **Content chart**          | **Theme settings**             |
| [![Content chart][s7]][s7] | [![Theme settings][s8]][s8]    |

[s1]: docs/screenshots/dual-pane.png
[s2]: docs/screenshots/image-preview.png
[s3]: docs/screenshots/code-preview.png
[s4]: docs/screenshots/embedded-terminal.png
[s5]: docs/screenshots/image-gallery.png
[s6]: docs/screenshots/batch-rename.png
[s7]: docs/screenshots/content-chart.png
[s8]: docs/screenshots/settings-themes.png

## Build and run

**📋 Requirements** · A C++20 compiler, CMake 3.25+, Ninja, Qt 6.8+ with Jalali
calendar support, Extra CMake Modules, libarchive and KDE Frameworks 6.20+. See
`CMakeLists.txt` for the required components; Baloo development files optionally
enable indexed search.

**🔨 Build and test**

```bash
./install.sh --deps  # Print build and optional feature packages; does not install them
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/bin/opus-file-manager
```

**🐳 Dev container** · Without the Linux dependencies, for example on macOS, use
the headless Docker dev container. It extends the `plasma-impresario-dev` image
and is built automatically from a sibling `plasma-impresario` checkout, or from
`IMPRESARIO_DIR`:

```bash
tools/dev-container/run.sh sh -c 'cmake -S . -B build-container -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-container'
tools/dev-container/run.sh ctest --test-dir build-container --output-on-failure -j4
```

**📂 Arguments** · Pass folders or URLs as arguments: the first opens in the
active pane, the second in the other pane, and additional arguments in tabs in
the active pane. Without arguments, the configured startup or restored session
is used.

**📥 Install** · Run `./install.sh` for the current user; use
`./install.sh --help` for options. `./install.sh --default` also changes the
default folder handler. The installer uses `build-plasma` with tests disabled;
set `OPUSFM_BUILD_DIR` to choose another build tree. Uninstall using the same
build tree. The app icon, desktop entry and AppStream metadata are installed
with the application; file and action icons follow the active desktop theme.
See [branding](docs/BRANDING.md) for the logo and icon assets.

> [!TIP]
> If `build` was created before the project folder was renamed, use a fresh
> build directory for the manual build commands above.

**🧩 Optional integrations** · Baloo provides indexed search, Konsole provides
the embedded terminal, and ExifTool supports media metadata editing. See the
[caveats](docs/CAVEATS.md) for requirements and restrictions and the
[translation guide](po/README.md) for language setup.

## Customize the interface

- **🪟 Panes and panels:** Swap current tabs or whole pane sides, dim the
  inactive pane, and save or reset splitter sizes by orientation. Change
  combined panel section order, visibility and titles; embedded panel titles can
  be hidden separately from navigation-panel titles.
- **🧰 Toolbars:** Configure the main and pane toolbars and hamburger menu,
  choose button appearance and lock editing. Toolbar context menus provide
  editing and settings access; the main toolbar also retains KDE's toolbar
  options. Saved scripts and commands can become main-toolbar buttons; running
  them still requires preview and confirmation.
- **🏷️ Tabs and status:** Position tab bars per pane and layout, or keep them
  together between stacked panes. Show or hide close buttons and configure
  status-bar indicators and their order.
- **👁️ Views:** Enable the
  [file-list minimap](docs/CAVEATS.md#file-list-minimap) and set its width.
  Choose Information panel fields and their order, with optional hover
  information. Set a theme, light, dark or checkerboard background for
  transparent images without changing the image.
- **💾 Backup:** Back up settings and window and panel layouts, or preview a
  restore that applies on the next launch. Review the
  [backup limitations](docs/CAVEATS.md#settings-backups) before sharing or
  relying on an archive.

> [!IMPORTANT]
> Path-filter, saved-search, collection, rename-preset and automation management
> changes take effect immediately, even if Preferences is cancelled. KDE File
> Associations changes shared desktop settings.

Preferences also include [tool defaults](docs/ROADMAP.md#m10-tools-settings),
[language and regional formats](docs/ROADMAP.md#m14-translation-settings) and
[workspace settings](docs/ROADMAP.md#m16-workspace-settings). App-local themes,
colours and view styles from [M17](docs/ROADMAP.md#m17-themes) are configured
on the Themes page with the [M18 settings](docs/ROADMAP.md#m18-theme-settings),
and workspaces can save their own appearance.

## Safety and limitations

> [!WARNING]
> Back up existing `.mfm.json` files before editing: unsupported metadata may be
> discarded. Synchronization is not undoable and can leave partial results;
> automatic transfer overwrite can replace files without asking.

See [caveats and limitations](docs/CAVEATS.md) for restrictions and workarounds,
[reported bugs](docs/BUGS.md) for tracked issues, and [edge cases](docs/EDGE.md)
for possible scenarios that are not confirmed bugs.

## Documentation

| 📚                                                                 | Guide                                               |
| ------------------------------------------------------------------ | --------------------------------------------------- |
| [Documentation index](docs/README.md)                              | Design, plans, integrations and upstream references |
| [Roadmap](docs/ROADMAP.md)                                         | Architecture, milestone status and planned features |
| [Pending tasks](docs/TODO.md)                                      | Blocked work and handoffs                           |
| [Caveats](docs/CAVEATS.md)                                         | Limitations, dependencies and workarounds           |
| [Project board](https://github.com/users/mikepadiernos/projects/3) | Issue status from Backlog to Merged                 |
