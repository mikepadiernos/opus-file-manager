#!/usr/bin/env bash
set -euo pipefail

tmpdir="$(mktemp -d)"
trap 'rm -f -- "$tmpdir/install.sh" "$tmpdir/os-release"; rmdir -- "$tmpdir"' EXIT
sed "s|/etc/os-release|$tmpdir/os-release|g" "$1" > "$tmpdir/install.sh"

check() {
    local id="$1" like="$2" required="$3" optional="$4" output line
    printf 'ID=%s\nID_LIKE="%s"\n' "$id" "$like" > "$tmpdir/os-release"
    output="$(HOME="$tmpdir" bash "$tmpdir/install.sh" --deps)"
    line="$(printf '%s\n' "$output" | grep -Fm1 "$required")" || return 1
    [[ "$output" == *"$optional"* ]] || return 1
    [[ "$output" == *"Qt Quick Compiler Extensions are optional"* ]] || return 1
    [[ "$output" == *"Distribution package versions must meet"* ]] || return 1
    printf '%s\n' "$line"
}

arch="$(check cachyos arch 'sudo pacman -S --needed' 'perl-image-exiftool')"
[[ " $arch " == *' solid '* && " $arch " == *' syntax-highlighting '* ]]
[[ " $arch " != *' ksolid '* && " $arch " != *' ksyntaxhighlighting '* ]]
[[ " $arch " == *' gettext '* ]]
[[ " $arch " == *' kglobalaccel '* && " $arch " == *' kstatusnotifieritem '* && " $arch " == *' plasma-activities '* ]]

fedora="$(check fedora '' 'sudo dnf install' 'kf6-baloo-devel')"
[[ " $fedora " == *' kf6-solid-devel '* && " $fedora " == *' kf6-syntax-highlighting-devel '* ]]
[[ " $fedora " == *' kf6-kglobalaccel-devel '* && " $fedora " == *' plasma-activities-devel '* ]]
[[ " $fedora " != *' kf6-ksolid-devel '* && " $fedora " != *' kf6-ksyntaxhighlighting-devel '* ]]

debian="$(check debian '' 'sudo apt install' 'libkf6baloo-dev')"
[[ " $debian " == *' libkf6solid-dev '* && " $debian " == *' libkf6syntaxhighlighting-dev '* ]]
[[ " $debian " == *' libkf6statusnotifieritem-dev '* && " $debian " == *' libplasmaactivities-dev '* ]]

suse="$(check opensuse-tumbleweed suse 'sudo zypper install' 'install Konsole')"
[[ " $suse " == *' kf6-solid-devel '* && " $suse " == *' kf6-syntax-highlighting-devel '* ]]
[[ " $suse " == *' plasma6-activities-devel '* ]]

printf 'ID=unknown\n' > "$tmpdir/os-release"
output="$(HOME="$tmpdir" bash "$tmpdir/install.sh" --deps)"
[[ "$output" == *'libarchive'* && "$output" == *'MultimediaWidgets, Network, Xml'* ]]
