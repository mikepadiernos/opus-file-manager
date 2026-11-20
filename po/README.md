# Translations

Opus follows the system interface language by default. Preferences →
Translation can select a language independently of regional formats and can
follow the language's layout direction or force left-to-right or right-to-left.
Restart Opus to fully apply changes. See the
[translation limitations](../docs/CAVEATS.md#translation-format-overrides).

Build the `extract-messages` target to generate `opus-file-manager.pot` in the
build directory. It extracts C++ messages and XMLGUI and KConfig resource
strings. Update language catalogues with `msgmerge`, preserving contexts,
plural forms and `%1`-style placeholders.

Put each catalogue at `po/<language>/opus-file-manager.po`. KI18n compiles
catalogues during the build and installs them under the configured locale
directory. `KF_SKIP_PO_PROCESSING=ON` disables catalogue installation.
No particular language catalogue is required. Existing catalogues can be
incomplete; untranslated messages fall back to the source language.
