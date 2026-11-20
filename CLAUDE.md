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
- Strike out implemented features even when they have known caveats. Record
  each outstanding caveat in a GitHub caveat report that links to its entry in
  `docs/CAVEATS.md`, and put caveat references at the end of the milestone.
  Do not treat unimplemented or unverified features as complete.
- When updating milestone status, scan the current checkout for implementation
  evidence and strike completed features without changing the existing format;
  leave incomplete or unverified portions unstruck.

## Tracking

Prefix user-reported bug titles with `[bug] ` and label them `bug`. Prefix
reports arising from documented caveats with `[caveat] ` and label them
`caveat`. Move a milestone, bug or caveat issue to Development on the GitHub
project when work starts.

For each new or updated caveat, keep `docs/CAVEATS.md` and its project issue
aligned:

- Document the limitation and a practical recommendation, distinguishing
  workarounds and proposed improvements from implemented features.
- Link the caveat and its `[caveat]` issue to each other, and add the issue to
  the project board.
- Post the recommendation in a comment on the issue and link to that comment
  from the document; update the comment when the recommendation changes rather
  than leaving conflicting guidance.

Work on each milestone in a corresponding `feature/M##-name` branch. When a
milestone is complete, commit its changes by scope, push the branch, then merge
it into `main`. A milestone is complete only when all its feature items are
struck through in the roadmap. Work on bug and caveat issues in their own
branches and follow the same scoped commit, push and merge process when fixed.

## Project workflow

Use the [Opus project board](https://github.com/users/mikepadiernos/projects/3)
to track each milestone's parent issue and its feature sub-issues:

- When starting a milestone, set its parent and not-yet-started items to
  **Defined**, and record the parent's **Start date**. Do not move items
  already in Development, Review or Completed backwards.
- When starting an item, move it and its parent to **Development** and record
  the item's **Start date**.
- When an item is completed, move it to **Review**, set its **Target date** to
  the completion date, check it in the parent issue and strike it out in the
  local roadmap as part of the same change. When the next item in the same
  milestone moves to **Review**, move the previous item from **Review** to
  **Completed**.
- When all milestone items are struck out, set the parent milestone's
  **Target date** to the completion date.
- Once a feature-complete milestone has been merged into `main`, close its
  milestone issue and related feature issues and sub-issues as completed, and
  ensure their project status is **Completed**. Keep unresolved bug and caveat
  issues open.
