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
- Use Plasma's active icon theme as the primary icon source via
  `QIcon::fromTheme()`; do not bundle or hard-code a replacement icon theme.
  Prefer the theme's `-symbolic` variant for icons rendered at 24 logical
  pixels or smaller, and use the regular variant at larger sizes. Route small
  theme icons through `SymbolicIcon::fromTheme()` or `SymbolicIcon::wrap()`.
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
- After completing a milestone feature, strike out its roadmap item as part of
  the same change.
- When updating milestone status, scan the current checkout for implementation
  evidence and strike completed features without changing the existing format;
  leave incomplete or unverified portions unstruck.

## Tracking

Work on each milestone in a corresponding `feature/M##-name` branch. When a
milestone is complete, commit its changes by scope, push the branch, then merge
it into `main`. A milestone is complete only when all its feature items are
struck through in the roadmap.
