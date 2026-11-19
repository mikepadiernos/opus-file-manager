# Opus File Manager

Dual-pane file manager for KDE Plasma: C++20, Qt 6 Widgets, KDE Frameworks 6.
Linux only. See `docs/ROADMAP.md` for architecture, decisions and milestones,
and `docs/parity/` for the upstream behaviour being matched.

## Commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --build build --target clang-format   # format all C++ sources
./build/bin/opus-file-manager [path...]
```

## Conventions

- Logic without widgets goes in `src/core/` and gets QtTest coverage in
  `tests/`. Widgets go in `src/app/`.
- Prefer an existing KDE Frameworks class over custom code (KIO jobs, KXmlGui
  actions, `KFileItemActions`, `KFilePlacesView`).
- User-visible strings go through `i18n()` / `i18nc()`.
- Settings use KConfig; session state uses `KSharedConfig::openStateConfig()`.
- Tests must never touch the real home folder, trash or config: use
  `QTemporaryDir` and `QStandardPaths::setTestModeEnabled(true)`.

## Roadmap format

- Keep `docs/ROADMAP.md` in its detailed milestone format, with individual
  feature bullets and subheadings.
- Strike through completed features individually with `~~...~~`; do not strike
  through milestone headings or replace feature lists with a consolidated
  completion paragraph. Strikethroughs alone indicate completion; do not add a
  separate "Done so far" summary.
- Leave unfinished features unstruck.

## Tracking

No GitHub remote yet. Milestones in `docs/ROADMAP.md` stand in for issues, so
branches are named `feature/M##-name` until a remote exists.
