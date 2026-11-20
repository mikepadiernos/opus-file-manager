# Pending tasks

## Complete the remaining bug report after M12

- **#378 — Konsole profile background blur:** honor the active profile's blur
  setting when supported; do not force blur when disabled. Cover both embedded
  terminal placements and profile changes when creating a session.
- The issue is in Development with start date 2026-11-20; no fix has been
  implemented. KonsolePart exposes `isBlurEnabled` as a Qt slot, and KDE's
  example uses `KWindowEffects::enableBlurBehind`. A fix must also provide a
  translucent top-level surface and keep the blur region aligned with the
  visible session as the panel moves, resizes, hides and switches tabs.
- Implement on `bug/378-terminal-profile-blur`; verify enabled and disabled
  profiles, new sessions, both placements and compositor behavior in an
  isolated environment. Then advance the issue, close it as completed, push
  and merge its branch, and move it to Merged.
