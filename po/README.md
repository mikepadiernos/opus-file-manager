# Translations

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](../docs/README.md)
[![Project README](https://img.shields.io/badge/project-README-3daee9?style=flat-square)](../README.md)
[![Milestones M13–M14](https://img.shields.io/badge/milestones-M13%E2%80%93M14-8957e5?style=flat-square)](../docs/ROADMAP.md#m13-translations)
[![Caveats](https://img.shields.io/badge/caveats-translation-bf8700?style=flat-square)](../docs/CAVEATS.md#translation-format-overrides)

How to choose the interface language and regional formats, and how to extract,
update and install translation catalogues.

> [!NOTE]
> Restart Plasma Opus to fully apply translation settings. Catalogues can be
> incomplete; untranslated messages fall back to the source language. See the
> [translation limitations](../docs/CAVEATS.md#translation-format-overrides).

## Choose a language

Plasma Opus follows the system interface language by default. Change it in
**Preferences › Translation**, independently of regional formats. These
settings are stored in the `[Translation]` group of the configuration.

| Tab                 | Setting            | Choices                                                                |
| ------------------- | ------------------ | ---------------------------------------------------------------------- |
| Language and Layout | Interface language | The system language or a specific language                             |
| Language and Layout | Regional formats   | The system locale or a specific locale                                 |
| Language and Layout | Layout direction   | Follow interface language, Left to right or Right to left              |
| Language and Layout | Digit shapes       | Follow regional locale, Western, Eastern Arabic-Indic or Custom digits |
| Dates               | Calendar           | Follow regional locale, Gregorian or Jalali                            |
| Dates               | Date and time      | Custom Qt date and time formats; empty uses the listing default        |

## Catalogues

Put each catalogue at `po/<language>/opus-file-manager.po`. No particular
language catalogue is required; the repository currently includes Persian
(`po/fa/opus-file-manager.po`). The `opusfm-translationtest` suite compiles
that catalogue with `msgfmt --check`, so keep it valid.

### Extract messages

Build the `extract-messages` target to generate `opus-file-manager.pot` in the
build directory:

```bash
cmake --build build --target extract-messages
```

The target runs `scripts/extract-messages.sh`, which needs `extractrc` and
`xgettext`. It extracts C++ messages, XMLGUI and KConfig resource strings, and
the desktop entry and AppStream metadata. The dev container provides these
tools:

```bash
tools/dev-container/run.sh cmake --build build-container --target extract-messages
```

### Update a catalogue

Merge the new template into a language catalogue with `msgmerge`, preserving
contexts, plural forms and `%1`-style placeholders:

```bash
msgmerge --update po/<language>/opus-file-manager.po build/opus-file-manager.pot
```

### Build and install

KI18n compiles catalogues during the build (`ki18n_install(po)`) and installs
them under the configured locale directory. The build also merges their
translations into the installed desktop entry and AppStream metadata, so
launchers and software centres show the translated name and description.
`KF_SKIP_PO_PROCESSING=ON` disables catalogue installation and these
translations.
