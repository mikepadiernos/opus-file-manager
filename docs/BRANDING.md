# 🎨 Plasma Opus branding

**[Application](../README.md)** · **[Documentation](README.md)** ·
**[Roadmap](ROADMAP.md#m19-branding)**

Plasma Opus has its own name, logo and application icon. File, device, place and
action icons always come from the active desktop icon theme; only the
application's own identity is drawn here.

## Identity

| Item          | Value                                                         |
| ------------- | ------------------------------------------------------------- |
| Name          | Plasma Opus                                                   |
| Summary       | Dual-pane file manager for KDE Plasma                         |
| Desktop entry | `org.kde.opusfilemanager.desktop`                             |
| Icon name     | `org.kde.opusfilemanager`, `org.kde.opusfilemanager-symbolic` |
| Executable    | `opus-file-manager`                                           |
| D-Bus service | `org.kde.opus-file-manager`                                   |

The name and summary are defined once in `src/core/appidentity.cpp` and are
translated through the application catalogue, which also translates the
desktop entry and AppStream metadata.

## Mark

The mark is a folder carrying a golden spiral of nested circles: many
documents, one composed whole. The folder takes the desktop accent colour
through the `ColorScheme-Accent` class, so it follows the Plasma accent when the
icon theme supports colour schemes and falls back to Breeze blue (`#3daee9`)
elsewhere. The spiral is always white.

## Assets

| Asset                      | File                                                            | Use                                   |
| -------------------------- | --------------------------------------------------------------- | ------------------------------------- |
| Application icon           | `src/app/org.kde.opusfilemanager.svg`                           | Scalable, 48 px and larger            |
| Small application icons    | `src/app/icons/{16x16,22x22,32x32}/org.kde.opusfilemanager.svg` | Pixel-aligned artwork for small sizes |
| Symbolic icon              | `src/app/icons/symbolic/org.kde.opusfilemanager-symbolic.svg`   | Monochrome, follows the text colour   |
| Logo for light backgrounds | `src/app/branding/plasma-opus-logo.svg`                         | Documentation and release materials   |
| Logo for dark backgrounds  | `src/app/branding/plasma-opus-logo-dark.svg`                    | Documentation and release materials   |

The icons install to the `hicolor` theme so that any icon theme can override
them. The small sizes simplify the spiral to rings that stay legible on the
pixel grid; the symbolic icon cuts the ring out of a monochrome folder and uses
`ColorScheme-Text`, so it follows light and dark palettes.

The logos combine the mark with a wordmark: "Plasma" in the text colour
(`#232629` on light backgrounds, `#fcfcfc` on dark ones) and "Opus" in the
accent colour. Use the variant that matches the background. On the web, pick it
with `prefers-color-scheme`:

```html
<picture>
  <source
    media="(prefers-color-scheme: dark)"
    srcset="src/app/branding/plasma-opus-logo-dark.svg"
  />
  <img
    src="src/app/branding/plasma-opus-logo.svg"
    alt="Plasma Opus"
    width="384"
    height="80"
  />
</picture>
```

Keep clear space of at least the folder tab's height around the logo, and do not
recolour, stretch or re-typeset the wordmark.

## Presentation

- **About:** Help ▸ About Plasma Opus shows the application icon, name,
  version and summary from the shared identity.
- **Startup presentation:** an optional card with the logo, version and
  summary appears over the window for two seconds at startup. It is off by
  default. It never delays or blocks use: the window opens and accepts input
  as usual, clicks pass through the card, it never takes focus, and any key,
  click, scroll or touch dismisses it. It fades out only when animations are
  enabled, and in high contrast it uses the colour scheme's own colours with a
  solid border. The logo can follow the colour scheme or use dark text on a
  light card or light text on a dark card; the folder and "Opus" follow the
  accent colour.

  Until [M20](ROADMAP.md#m20-branding-settings) adds these options to
  Preferences, set them in `~/.config/opus-file-managerrc`:

  ```ini
  [Branding]
  StartupPresentation=true
  # Automatic, DarkText or LightText
  LogoVariant=Automatic
  ```

- **Release materials:** release notes start from the
  [release notes template](releases/TEMPLATE.md), which uses the logo variant
  for the reader's colour scheme, the shared name and summary, and the
  section order used by previous releases.

## Licence

All Plasma Opus branding assets are original work, licensed under
GPL-3.0-or-later like the rest of the project, and carry SPDX headers.

The wordmark was set in [Noto Sans](https://fonts.google.com/noto/specimen/Noto+Sans)
(Light and Bold) and converted to outlines, so the logos render identically
without the font installed. Noto Sans is licensed under the SIL Open Font
License 1.1, which permits using the font to create artwork such as logos; the
outlined shapes contain no font software.
