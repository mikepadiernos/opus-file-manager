# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## Header and status-bar padding (#558)

[#558](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/558)
remains in Development. Application-owned combined-panel and terminal headers,
group headings, and status indicators have been adjusted.
The eight-pixel requirement is not yet verified across all titles and headers.

Qt paints transient `QStatusBar` messages with its own inset, independent of
the indicator layouts and density margins. Native dock titles and details
column headings are also controlled by the active style. Finish by finding and
testing a way to enforce the minimum without losing native styling, dock
controls, density settings, RTL behavior or transient-message semantics; then
verify all affected surfaces and complete the issue workflow.
