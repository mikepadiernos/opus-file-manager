#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 OUTPUT.pot" >&2
    exit 2
fi

resources=$(mktemp)
rewritten=$(mktemp)
sources=$(mktemp)
trap 'rm -f -- "$resources" "$rewritten" "$sources"' EXIT

find src -type f \( -name '*.ui' -o -name '*.rc' -o -name '*.kcfg' \) -exec extractrc {} + > "$resources"
sed -E 's@^// i18n: file: (.*):([0-9]+)$@#line \2 "\1"@' "$resources" > "$rewritten"
find src -type f \( -name '*.cpp' -o -name '*.h' \) > "$sources"
LC_ALL=C sort -o "$sources" "$sources"
printf '%s\n' "$rewritten" >> "$sources"

xgettext --language=C++ --from-code=UTF-8 --kde \
    --keyword=i18n:1 --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3 \
    --add-comments=i18n --files-from="$sources" \
    --package-name=opus-file-manager --output="$1"

# Desktop entry keys and AppStream metadata, merged back by msgfmt at build time.
xgettext --from-code=UTF-8 --join-existing --language=Desktop \
    --package-name=opus-file-manager --output="$1" \
    src/app/org.kde.opusfilemanager.desktop.in
xgettext --from-code=UTF-8 --join-existing \
    --package-name=opus-file-manager --output="$1" \
    src/app/org.kde.opusfilemanager.metainfo.xml
