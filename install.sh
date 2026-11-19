#!/usr/bin/env bash
# Builds and installs Opus File Manager. Re-runnable: each step only touches
# what it owns, and --uninstall removes exactly what the install wrote.
#
#   ./install.sh                 build + install for the current user (~/.local)
#   ./install.sh --default       ... and make Opus the default folder handler
#   ./install.sh --system        build + install under /usr/local (uses sudo)
#   ./install.sh --deps          print the dependency packages for this distro
#   ./install.sh --uninstall     remove the install and restore Dolphin
#
# Run ./install.sh --help for all options.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${OPUSFM_BUILD_DIR:-$SRC_DIR/build}"
DESKTOP_ID="org.kde.opusfilemanager.desktop"
MIME_TYPES=(inode/directory)

mode=install
scope=user
prefix=""
make_default=0
build_type=Release
jobs=""

usage() {
    sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
    cat <<EOF

Options:
  --user             install under ~/.local (default; no root needed)
  --system           install under /usr/local via sudo
  --prefix DIR       install under DIR instead
  --default          register Opus as the default handler for folders
  --debug            build with CMAKE_BUILD_TYPE=Debug (default: Release)
  --jobs N           parallel build jobs (default: all cores)

Set OPUSFM_BUILD_DIR to build somewhere other than ./build.
  --deps             print the packages needed on this distro and exit
  --uninstall        remove files installed from this build tree
  -h, --help         show this help
EOF
}

say() { printf '\033[1m==>\033[0m %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --user) scope=user ;;
        --system) scope=system ;;
        --prefix) shift; prefix="${1:-}"; [ -n "$prefix" ] || die "--prefix needs a directory"; scope=custom ;;
        --default) make_default=1 ;;
        --debug) build_type=Debug ;;
        --jobs) shift; jobs="${1:-}"; [ -n "$jobs" ] || die "--jobs needs a number" ;;
        --deps) mode=deps ;;
        --uninstall) mode=uninstall ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (see --help)" ;;
    esac
    shift
done

case "$scope" in
    user) prefix="$HOME/.local" ;;
    system) prefix="/usr/local" ;;
esac

# Only escalate when the prefix is not writable by the current user.
prefix_writable() {
    local dir="$prefix"
    while [ ! -e "$dir" ]; do dir="$(dirname "$dir")"; done
    [ -w "$dir" ]
}
sudo_cmd=()
if [ "$mode" != deps ] && ! prefix_writable; then
    command -v sudo >/dev/null || die "$prefix is not writable and sudo is not available"
    sudo_cmd=(sudo)
fi

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

KF_MODULES=(archive bookmarks config configwidgets coreaddons crash dbusaddons filemetadata guiaddons i18n iconthemes itemviews kio parts solid syntaxhighlighting widgetsaddons windowsystem xmlgui)

distro_id() {
    [ -r /etc/os-release ] || { echo unknown; return; }
    # shellcheck disable=SC1091
    . /etc/os-release
    echo "${ID:-unknown} ${ID_LIKE:-}"
}

print_deps() {
    local ids family="" pkgs=() m
    ids="$(distro_id)"
    case " $ids " in
        *" arch "*|*" manjaro "*|*" endeavouros "*|*" cachyos "*) family=arch ;;
        *" fedora "*|*" rhel "*|*" centos "*|*" nobara "*) family=fedora ;;
        *" debian "*|*" ubuntu "*|*" neon "*|*" linuxmint "*|*" pop "*) family=debian ;;
        *" opensuse"*|*" suse "*|*" sles "*) family=suse ;;
    esac

    case "$family" in
        arch)
            pkgs=(base-devel cmake ninja extra-cmake-modules qt6-base qt6-tools qt6-multimedia)
            for m in "${KF_MODULES[@]}"; do pkgs+=("k$m"); done
            pkgs=("${pkgs[@]/kkio/kio}")
            echo "sudo pacman -S --needed ${pkgs[*]}" ;;
        fedora)
            pkgs=(gcc-c++ cmake ninja-build extra-cmake-modules qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtmultimedia-devel)
            for m in "${KF_MODULES[@]}"; do pkgs+=("kf6-k$m-devel"); done
            pkgs=("${pkgs[@]/kf6-kkio-devel/kf6-kio-devel}")
            echo "sudo dnf install ${pkgs[*]}" ;;
        debian)
            pkgs=(build-essential cmake ninja-build extra-cmake-modules qt6-base-dev qt6-base-private-dev qt6-multimedia-dev)
            for m in "${KF_MODULES[@]}"; do pkgs+=("libkf6$m-dev"); done
            echo "sudo apt install ${pkgs[*]}" ;;
        suse)
            pkgs=(gcc-c++ cmake ninja extra-cmake-modules qt6-base-devel qt6-multimedia-devel)
            for m in "${KF_MODULES[@]}"; do pkgs+=("kf6-k$m-devel"); done
            pkgs=("${pkgs[@]/kf6-kkio-devel/kf6-kio-devel}")
            echo "sudo zypper install ${pkgs[*]}" ;;
        *)
            pkgs=("${KF_MODULES[@]/#/k}"); pkgs=("${pkgs[@]/kkio/kio}")
            echo "Unrecognised distro ($ids). You need a C++20 compiler, CMake >= 3.25, Ninja,"
            echo "Qt >= 6.8 (Core Gui Widgets DBus Test Concurrent Multimedia), extra-cmake-modules and these"
            echo "KDE Frameworks >= 6.20 development packages: ${pkgs[*]}" ;;
    esac
}

check_tools() {
    local missing=()
    for t in cmake ninja c++; do command -v "$t" >/dev/null || missing+=("$t"); done
    if [ ${#missing[@]} -gt 0 ]; then
        echo "missing build tools: ${missing[*]}" >&2
        echo "install the dependencies with:" >&2
        echo "  $(print_deps)" >&2
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Desktop database helpers
# ---------------------------------------------------------------------------

refresh_desktop_db() {
    local appdir="$1"
    command -v update-desktop-database >/dev/null && update-desktop-database -q "$appdir" 2>/dev/null || true
    command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
}

set_default_handler() {
    command -v xdg-mime >/dev/null || { echo "xdg-mime not found; skipping default handler" >&2; return; }
    for mime in "${MIME_TYPES[@]}"; do
        xdg-mime default "$DESKTOP_ID" "$mime"
    done
}

restore_default_handler() {
    command -v xdg-mime >/dev/null || return 0
    for mime in "${MIME_TYPES[@]}"; do
        if [ "$(xdg-mime query default "$mime" 2>/dev/null)" = "$DESKTOP_ID" ]; then
            if [ -n "$(find /usr/share/applications "${XDG_DATA_HOME:-$HOME/.local/share}/applications" -name org.kde.dolphin.desktop 2>/dev/null)" ]; then
                xdg-mime default org.kde.dolphin.desktop "$mime"
                echo "folder handler: restored to Dolphin"
            else
                echo "folder handler: was Opus; pick a new one in System Settings > Applications > Default Applications"
            fi
        fi
    done
}

# ---------------------------------------------------------------------------
# Modes
# ---------------------------------------------------------------------------

case "$mode" in
    deps)
        print_deps
        exit 0 ;;

    uninstall)
        [ -f "$BUILD_DIR/build.ninja" ] || die "no build tree in $BUILD_DIR; nothing to uninstall"
        say "Removing installed files"
        "${sudo_cmd[@]}" cmake --build "$BUILD_DIR" --target uninstall
        restore_default_handler
        refresh_desktop_db "$prefix/share/applications"
        say "Uninstalled. The build tree in $BUILD_DIR is kept; delete it if you want."
        exit 0 ;;
esac

check_tools

say "Configuring ($build_type) with prefix $prefix"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_TESTING=OFF \
    || { echo; echo "configure failed; if a Qt or KF6 module is missing, install:"; echo "  $(print_deps)"; exit 1; }

say "Building"
cmake --build "$BUILD_DIR" ${jobs:+--parallel "$jobs"}

say "Installing"
"${sudo_cmd[@]}" cmake --install "$BUILD_DIR"

refresh_desktop_db "$prefix/share/applications"

if [ "$make_default" = 1 ]; then
    say "Registering Opus as the default folder handler"
    set_default_handler
fi

echo
echo "binary        : $prefix/bin/opus-file-manager"
echo "desktop entry : $prefix/share/applications/$DESKTOP_ID"
if command -v xdg-mime >/dev/null; then
    echo "folder handler: $(xdg-mime query default inode/directory 2>/dev/null || echo none)"
fi
if [ "$scope" = user ] && ! case ":$PATH:" in *":$HOME/.local/bin:"*) true ;; *) false ;; esac; then
    echo
    echo "note: ~/.local/bin is not on your PATH. The launcher entry works regardless;"
    echo "      to run 'opus-file-manager' from a shell, add ~/.local/bin to PATH."
fi
[ "$make_default" = 1 ] || { echo; echo "run './install.sh --default' to make Opus open folders by default."; }
