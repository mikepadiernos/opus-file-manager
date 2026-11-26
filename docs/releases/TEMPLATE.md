# Release notes template

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Latest release](https://img.shields.io/github/v/release/mikepadiernos/plasma-opus?style=flat-square&color=3daee9)](https://github.com/mikepadiernos/plasma-opus/releases/latest)
[![Branding](https://img.shields.io/badge/branding-assets-1D99F3?style=flat-square)](../BRANDING.md)
[![Caveats](https://img.shields.io/badge/caveats-limitations-bf8700?style=flat-square)](../CAVEATS.md)

The starting point for GitHub release notes. It uses the logo variant for the
reader's colour scheme, the shared name and summary, and the section order of
previous releases.

## How to use it

1. Before tagging, update the project version in `CMakeLists.txt`
   (`project(opus-file-manager VERSION …)`) and add a matching `<release>`
   entry at the top of `src/app/org.kde.opusfilemanager.metainfo.xml`. The
   `opusfm-appidentitytest` suite checks that they match.
2. Title the release `v<version> — <short name>`, for example
   `v0.3.0 — Branding`. Previous releases are `v0.1.0 — First MVP` and
   `v0.2.0 — M14 MVP`.
3. Copy everything below the line into the GitHub release body, replace the
   bracketed text, including `[version]` in the URLs, and remove sections that
   do not apply.

<!--
  Release notes template. Copy everything below the line into the GitHub
  release body, replace the bracketed text and remove sections that do not
  apply. See "How to use it" above for the title and version steps.
-->

---

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/mikepadiernos/plasma-opus/v[version]/src/app/branding/plasma-opus-logo-dark.svg">
    <img src="https://raw.githubusercontent.com/mikepadiernos/plasma-opus/v[version]/src/app/branding/plasma-opus-logo.svg" alt="Plasma Opus" width="384" height="80">
  </picture>
</p>

**Plasma Opus [version]** · Dual-pane file manager for KDE Plasma

[One or two sentences: which milestones this release covers and what it builds on.]

### Highlights

- [Feature, in user terms]

### Caveats

[Known limitations of the new features, linked to
`https://github.com/mikepadiernos/plasma-opus/blob/v[version]/docs/CAVEATS.md`.]

### Install

Build and install for the current user with `./install.sh`; see the
[README](https://github.com/mikepadiernos/plasma-opus/blob/v[version]/README.md#build-and-run)
for dependencies and options.
