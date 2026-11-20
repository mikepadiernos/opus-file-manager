# Pending tasks

## Complete M13 and M14 translations

- M13 parent #26 is in Development. Catalogue issue #210 is in Review and
  checked in the parent; Persian issue #211, RTL issue #212 and Jalali issue
  #213 are in Development. The M13 branch is
  `feature/M13-translations`.
- The Persian catalogue is incomplete: 144 of 1,884 extracted messages are
  translated. Continue translating and review contexts, plural forms,
  accelerators and placeholders; verify the installed catalogue in the UI.
  Do not mark #211 complete until the translation is ready.
- RTL startup direction, pane navigation, section headers, gallery navigation
  and preview gutters are implemented for #212. Verify the remaining interface
  and installed-app behavior for Persian, Arabic and Hebrew before Review.
  Jalali date formatting and calendar-aware date groups are implemented for
  #213; KDE-controlled date displays are tracked in caveat #436. Advance
  issues in project order, check the parent checklist and strike roadmap
  features only when completed. M14 still needs explicit calendar and format
  settings with precedence over the listing defaults.
- Complete M14 translation settings (#214-#219), including independent locale,
  language, calendar, custom date format, direction and digit choices. Merge
  each completed milestone and update its project status.
- Prepare the MVP release only after M13 and M14 are fully verified and
  merged. No MVP release has been created.
