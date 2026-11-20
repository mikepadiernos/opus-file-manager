# Translations

Build the `extract-messages` target to generate `opus-file-manager.pot` in the
build directory. It extracts C++ messages and XMLGUI and KConfig resource
strings. Update language catalogues with `msgmerge`, preserving contexts,
plural forms and `%1`-style placeholders.

Put each catalogue at `po/<language>/opus-file-manager.po`. KI18n compiles
catalogues during the build and installs them under the configured locale
directory. `KF_SKIP_PO_PROCESSING=ON` disables catalogue installation.
