# Pending tasks

## Finish M12 project synchronization and merge

- Retry Project 3 updates after the GitHub GraphQL rate-limit reset. Read the
  board first to account for any partial updates; the previous `gh project
  item-edit` loop stopped at the rate limit. GitHub reported a reset at
  2026-11-20 12:30:21 UTC.
- Current confirmed state: issues #190-#199 are Completed and closed; #328 is
  in Review; #329-#333 remain to be synchronized. Parent #23 is in Development.
  The parent checklist is checked through M12-11. The roadmap strikes M12-01
  through M12-10; leave M12-11 unstruck while #328 is in Review.
- Start #329 in Development with its actual Start date. Move it to Review with
  its Target date and check M12-12 in #23. Then move #328 to
  Completed, close it as completed, and strike M12-11.
- Continue in order: #330 (splitter sizes, M12-13), #331 (preview and gallery,
  M12-14), #332 (information panel, M12-15), and #333 (file-list minimap,
  M12-16). Start each in Development with its actual Start date, then move it
  to Review with its Target date and check its item in #23. When the next issue
  reaches Review, move the previous issue to Completed, close it as completed,
  and strike its roadmap item. After #333 reaches Review, move #332 and then
  #333 to Completed, close them, and strike their roadmap items.
- Set the parent Target date when all features are struck. Review and merge
  PR #384; after merge, move the parent and closed feature issues to Merged.
  Remove the M12 pending note from the roadmap and this section when done.

## Complete the two open bug reports after M12 merges

- Check whether #378 and #383 are on Project 3; add missing items in Backlog
  unless either has already advanced. Start each only after M12 is merged.
- **#378 — Konsole profile background blur:** honor the active profile's blur
  setting when supported; do not force blur when disabled. Cover both embedded
  terminal placements and profile changes when creating a session.
- **#383 — Editor settings icon:** use a notepad-style symbolic icon from the
  active theme in the Settings sidebar.
- Work each bug on its own branch. Verify the behavior, update its project
  status and dates, close it as completed after review, then push and merge.
