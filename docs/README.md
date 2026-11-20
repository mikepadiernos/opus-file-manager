# Opus documentation

Start with the [project README](../README.md) for features, build instructions
and important limitations. It describes the current `main` branch; the latest
published MVP is [v0.2.0](https://github.com/mikepadiernos/opus-file-manager/releases/tag/v0.2.0).
Use the documents below for details.

## Project documents

| Document | Purpose |
| --- | --- |
| [Scope and objective](philosophy/scope.md) | Product direction and boundaries |
| [Inspirations](philosophy/inspirations.md) | Sources and their influence |
| [Roadmap](ROADMAP.md) | Architecture, milestone status and planned features |
| [Pending tasks](TODO.md) | Blocked work and handoffs |
| [Reported bugs](BUGS.md) | Bug reports and completion status |
| [Caveats and limitations](CAVEATS.md) | Restrictions, dependencies and workarounds |
| [Edge cases](EDGE.md) | Potential boundary conditions and test scenarios |
| [Translations](../po/README.md) | Catalogue extraction, installation and fallback |
| [Project instructions](../CLAUDE.md) | Development conventions and workflows |

The next planned milestone is [advanced search](ROADMAP.md#m15-advanced-search).
Later plans include [UI/UX polish](ROADMAP.md#m30-ui-and-ux-polish), including
accessibility, motion controls, performance improvements and functional and
security testing. See the [bug log](BUGS.md) for open reports and
[edge cases](EDGE.md) for unconfirmed scenarios rather than treating plans or
test candidates as completed work.

## Behaviour references

These inventories describe a reviewed Max File Manager checkout, not the
current state of Opus or a guarantee of current upstream behaviour. Opus
adapts the reference to KDE; see the roadmap for implementation status:

- [User interface](parity/ui.md)
- [Backend and file operations](parity/backend.md)
