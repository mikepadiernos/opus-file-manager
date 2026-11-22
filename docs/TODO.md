# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Workspace vault editing (#565)

[#565](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/565)
is in Backlog. Named workspace profiles are not implemented in the current
checkout, and the built-in Markdown and backlinks feature
[#564](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/564)
is still in Development on `feature/M23-integrations`.

Implement workspace profiles ([#200](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/200))
and the editor's vault features first. Then associate an assigned local
location with a workspace, integrate vault navigation and editor state, and
provide workspace settings ([#566](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/566)).
Test switching with unsaved edits, incomplete scans, duplicate names, link
resolution, unavailable or removed roots, symlink boundaries and workspace
deletion. Preserve the underlying files and existing Obsidian configuration.
