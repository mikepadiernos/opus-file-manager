# Opus File Manager

Opus is a Linux-only, dual-pane file manager for KDE Plasma, built with C++20,
Qt 6 Widgets and KDE Frameworks 6.

- `docs/ROADMAP.md` is the authoritative milestone and architecture plan.
- `docs/parity/` records the upstream behaviour being referenced, not
  necessarily Opus's current behaviour.
- `docs/CAVEATS.md` records limitations of implemented features.

## Commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --build build --target clang-format   # format all C++ sources
./build/bin/opus-file-manager [path...]
```

## Implementation conventions

- Put non-widget logic in `src/core/`, with QtTest coverage in `tests/`.
  Put widgets in `src/app/`.
- Prefer KDE Frameworks facilities to custom implementations, including KIO
  jobs, KXmlGui actions, `KFileItemActions` and `KFilePlacesView`.
- Localize user-visible strings with `i18n()` or `i18nc()`.
- Use Plasma's active icon theme as the primary source via
  `QIcon::fromTheme()`; do not bundle or hard-code a replacement icon theme.
  At 24 logical pixels or smaller, prefer the `-symbolic` variant and use
  `SymbolicIcon::fromTheme()` or `SymbolicIcon::wrap()`. Use regular variants
  for larger icons.
- Store settings in KConfig and session state through
  `KSharedConfig::openStateConfig()`.
- Keep tests isolated from the real home folder, trash and configuration. Use
  `QTemporaryDir` and `QStandardPaths::setTestModeEnabled(true)`.

## Roadmap and feature status

- Preserve the roadmap's detailed milestone headings, subheadings and
  individual feature bullets. Do not strike milestone headings, replace
  feature lists with completion paragraphs, or add a "Done so far" summary.
- Strikethrough (`~~...~~`) is the only roadmap completion marker. Strike an
  individual feature only when its issue moves to **Completed** or is already
  **Merged**. Features in **Review** or earlier remain unstruck, even if their
  implementation is finished.
- Before marking a feature complete, verify its implementation in the current
  checkout and check its issue status. Leave unimplemented or unverified
  features unstruck.
- A completed feature may still have caveats. Strike it out, record each
  outstanding caveat as described below, and reference those caveats at the
  end of the milestone.

## Issues and caveats

- Prefix user-reported bug titles with `[bug] ` and apply the `bug` label.
- Prefix reports arising from documented caveats with `[caveat] ` and apply the
  `caveat` label.
- When work on a milestone, bug or caveat begins, move its issue to
  **Development** on the project board, unless it is already further along.

For every new or updated caveat, keep the documentation, issue and
recommendation aligned:

1. Describe the limitation and a practical recommendation in `docs/CAVEATS.md`.
   Distinguish current behaviour from workarounds and proposed improvements.
2. Link the caveat and its `[caveat]` issue to each other, and add the issue
   to the project board.
3. Post the recommendation as an issue comment and link to it from the
   document. If the recommendation changes, update that comment rather than
   leaving conflicting guidance.

## Branches and merging

- Work on each milestone in its `feature/M##-name` branch. Work on bug and
  caveat issues in their own branches.
- A milestone is feature-complete only when all its roadmap feature items are
  struck through. Once complete, commit changes by scope, push the branch and
  merge it into `main`.
- For a fixed bug or caveat, likewise commit changes by scope, push its branch
  and merge it into `main`.

## Unattended milestone work

When the user explicitly requests milestone work without interaction, continue
without asking for routine confirmation: complete and verify every feature in
the current milestone, follow the project workflow, and merge the milestone
into `main`. Then proceed to the next incomplete roadmap milestone in order and
repeat until all milestones are complete and merged or the user stops the work.

Do not treat unfinished or unverified work as complete. If an essential action
is blocked and cannot be resolved without user input or required approval, report
the blocker and pending work rather than claiming completion. Handle blocked
GitHub Projects access as described below.

## Project workflow

Use the [Opus project board](https://github.com/users/mikepadiernos/projects/3)
for milestone parent issues and feature sub-issues. The status sequence is
**Backlog → Defined → Development → Review → Completed → Merged**. Do not move
an item backward.

If GitHub Projects access is blocked, update `docs/ROADMAP.md` with a concise
pending synchronization note identifying the affected issues, intended board
actions and relevant dates. Do not use the note as a completion marker or
strike a feature before its issue is confirmed in Completed or Merged. When
access returns, check the current board and issue states, apply the pending
actions without moving anything backward, and update the roadmap accordingly.
Remove the note once synchronization is complete.

1. **Start a milestone:** Move its parent and not-yet-started feature issues
   to Defined, without moving any more advanced item backward. Record the
   parent's **Start date**.
2. **Start a feature:** Move the feature and its parent to Development, unless
   either is already further along. Record the feature's **Start date**.
3. **Finish implementation:** Move the feature to Review, set its
   **Target date** to the completion date and check it in the parent issue.
   Leave its roadmap item unstruck.
4. **Advance the previous feature:** When the next feature in the same
   milestone moves to Review, move the previous feature from Review to
   Completed. Once the final feature is ready to finish, move it from Review to
   Completed as well.
5. **Complete an issue:** When moving an issue to Completed, close it with the
   completed reason. For a milestone feature, also strike its roadmap item in
   the same change.
6. **Complete a milestone:** When every feature is struck out, set the parent
   issue's **Target date** to the completion date.
7. **Merge:** When an issue's changes are merged into `main`, move the issue to
   Merged and keep it closed with the completed reason. Once a feature-complete
   milestone is merged, close its parent and related feature issues as
   completed and move them to Merged. Keep unresolved bug and caveat issues
   open.
