# Plasma Opus documentation

Find the right guide for using, building and planning Plasma Opus.

**[Application](../README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Refinements](refinements/README.md)**

## Contents

- [Start here](#start-here)
- [Plan and track](#plan-and-track)
- [Behaviour references](#behaviour-references)

## Start here

| Guide                                      | What it covers                                  |
| ------------------------------------------ | ----------------------------------------------- |
| [Project README](../README.md)             | Features, installation and safety               |
| [Translations](../po/README.md)            | Catalogue extraction, installation and fallback |
| [Scope and objective](philosophy/scope.md) | Product direction and boundaries                |
| [Inspirations](philosophy/inspirations.md) | Sources and their influence                     |
| [Interface design](philosophy/designs.md)  | Planned structure, panes and workspaces         |

## Plan and track

| Document                                      | What it tracks                                      |
| --------------------------------------------- | --------------------------------------------------- |
| [Roadmap](ROADMAP.md)                         | Architecture, milestone status and planned features |
| [Pending tasks](TODO.md)                      | Blocked work and handoffs                           |
| [Reported bugs](BUGS.md)                      | Bug reports and completion status                   |
| [Caveats and limitations](CAVEATS.md)         | Restrictions, dependencies and workarounds          |
| [Edge cases](EDGE.md)                         | Potential boundary conditions and test scenarios    |
| [M32 refinement plans](refinements/README.md) | Editable designs and proposed execution order       |
| [Project instructions](../CLAUDE.md)          | Development conventions and workflows               |

> [!IMPORTANT] The roadmap defines milestone scope and completion. Refinement
> plans are proposals, pending tasks are handoffs, and edge cases are test
> candidates—not confirmed bugs. Development branches can contain work not yet
> on `main`.

## Behaviour references

These inventories describe a reviewed Max File Manager checkout, not current
Plasma Opus behaviour or necessarily the latest upstream version. Plasma Opus
adapts the reference to KDE; use the roadmap for implementation status.

- [User interface](parity/ui.md)
- [Backend and file operations](parity/backend.md)
