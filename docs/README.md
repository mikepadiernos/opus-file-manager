# 📚 Plasma Opus documentation

Find the right guide for using, building and planning Plasma Opus.

**[Application](../README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Refinements](refinements/README.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Contents

- [Start here](#start-here)
- [Plan and track](#plan-and-track)
- [Integrations](#integrations)
- [Behaviour references](#behaviour-references)

## Start here

|     | Guide                                      | What it covers                                  |
| --- | ------------------------------------------ | ----------------------------------------------- |
| 🏠  | [Project README](../README.md)             | Features, installation and safety               |
| 📸  | [Screenshots](screenshots/README.md)       | Captured views and how they were made           |
| 🌐  | [Translations](../po/README.md)            | Catalogue extraction, installation and fallback |
| 🎨  | [Branding](BRANDING.md)                    | Logo, icons, presentation and release notes     |
| 🎯  | [Scope and objective](philosophy/scope.md) | Product direction and boundaries                |
| 💡  | [Inspirations](philosophy/inspirations.md) | Sources and their influence                     |
| 📐  | [Interface design](philosophy/designs.md)  | Planned structure, panes and workspaces         |

## Plan and track

|     | Document                                      | What it tracks                                      |
| --- | --------------------------------------------- | --------------------------------------------------- |
| 🗺️  | [Roadmap](ROADMAP.md)                         | Architecture, milestone status and planned features |
| ⏳  | [Pending tasks](TODO.md)                      | Blocked work and handoffs                           |
| 🐞  | [Reported bugs](BUGS.md)                      | Bug reports and completion status                   |
| ⚠️  | [Caveats and limitations](CAVEATS.md)         | Restrictions, dependencies and workarounds          |
| 🧪  | [Edge cases](EDGE.md)                         | Potential boundary conditions and test scenarios    |
| ✨  | [M32 refinement plans](refinements/README.md) | Editable designs and proposed execution order       |
| 🦀  | [Rust prototype](rust-prototype.md)           | M21 query-parser pilot, findings and criteria       |
| 🛠️  | [Project instructions](../CLAUDE.md)          | Development conventions and workflows               |

> [!IMPORTANT]
> The roadmap defines milestone scope and completion; the
> [project board](https://github.com/users/mikepadiernos/projects/3) tracks
> issue status. Refinement plans are proposals, pending tasks are handoffs, and
> edge cases are test candidates—not confirmed bugs. Development branches can
> contain work not yet on `main`.

## Integrations

These plans extend [M27](ROADMAP.md#m27-integrations),
[M28](ROADMAP.md#m28-integration-settings) and the Nextcloud milestones
[M29](ROADMAP.md#m29-nextcloud-integration) and
[M30](ROADMAP.md#m30-nextcloud-settings). They are not implemented.

|     | Plan                                                   | What it covers                                  |
| --- | ------------------------------------------------------ | ----------------------------------------------- |
| 🖥️  | [KDE Plasma](integrations/kde-plasma.md)               | Desktop integration plans and boundaries        |
| ✉️  | [Plasma Impresario](integrations/plasma-impresario.md) | Mail, calendar, notes and shared file workflows |
| ☁️  | [Nextcloud](integrations/nextcloud.md)                 | Nextcloud app workflows and considered features |

## Behaviour references

These inventories describe a reviewed Max File Manager checkout, not current
Plasma Opus behaviour or necessarily the latest upstream version. Plasma Opus
adapts the reference to KDE; use the roadmap for implementation status.

- 🧭 [User interface](parity/ui.md)
- ⚙️ [Backend and file operations](parity/backend.md)
