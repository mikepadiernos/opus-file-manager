# Screenshots

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../README.md)
[![Project README](https://img.shields.io/badge/project-README-3daee9?style=flat-square)](../../README.md#screenshots)
[![Branding](https://img.shields.io/badge/branding-assets-1D99F3?style=flat-square)](../BRANDING.md)

The screenshots shown in the [project README](../../README.md#screenshots),
with what each one shows and how they were captured.

> [!NOTE]
> These screenshots were captured headless in the
> [dev container](../../README.md#dev-container), so they show no KWin
> decorations, blur or Plasma theme. They use the Plasma Opus Dark colour scheme
> with Breeze widgets and icons, and synthetic demo files. The Card view and
> settings pages were recaptured after the M18 and M20 merges.

## Gallery

| Screenshot                                 | Shows                                                      |
| ------------------------------------------ | ---------------------------------------------------------- |
| [Dual panes](dual-pane.png)                | Side-by-side panes with the Places and folder tree sidebar |
| [Card view](card-view.png)                 | Both panes in Card view with thumbnails and file details   |
| [Image preview](image-preview.png)         | The preview panel showing the selected image               |
| [Code preview](code-preview.png)           | Syntax-highlighted source in the preview panel             |
| [Embedded terminal](embedded-terminal.png) | A Konsole terminal tab below the panes                     |
| [Image gallery](image-gallery.png)         | The gallery with image information and thumbnails          |
| [Batch rename](batch-rename.png)           | A numbered rename template and its preview                 |
| [Content chart](content-chart.png)         | Folder sizes as a treemap and a list                       |
| [Theme settings](settings-themes.png)      | The Themes page and its tabs in Preferences                |
| [Branding settings](settings-branding.png) | The startup presentation option with a live logo preview   |

The hero image in the [project README](../../README.md) is
[`preview.png`](../../preview.png): stacked panes with an image preview.

There are no screenshots yet of advanced search, native tagging or Yazi mode
(M21–M26).

## Updating screenshots

- The repository has no capture script in `tools/` or `scripts/`. The dev
  container provides Qt's offscreen platform but no Plasma session, so a
  recapture there matches the conditions in the note above.
- Keep the file names. The AppStream metadata in
  `src/app/org.kde.opusfilemanager.metainfo.xml` links `dual-pane.png`,
  `image-preview.png` and `embedded-terminal.png` from `main`, so software
  centres show them.
- Update the table above and the gallery in the
  [project README](../../README.md#screenshots) when adding or replacing a
  screenshot.
