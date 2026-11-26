#!/usr/bin/env bash
# Builds and installs Plasma Opus. Re-runnable: each step only touches
# what it owns, and --uninstall removes exactly what the install wrote.
#
#   ./install.sh                 build + install for the current user (~/.local)
#   ./install.sh --default       ... and make Plasma Opus the default folder handler
#   ./install.sh --system        build + install under /usr/local (uses sudo)
#   ./install.sh --deps          print build and optional feature dependencies
#   ./install.sh --uninstall     remove the install and restore Dolphin
#
# Run ./install.sh --help for all options.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${OPUSFM_BUILD_DIR:-$SRC_DIR/build-plasma}"
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
  --default          register Plasma Opus as the default handler for folders
                     and "Show in folder" (FileManager1) requests
  --debug            build with CMAKE_BUILD_TYPE=Debug (default: Release)
  --jobs N           parallel build jobs (default: all cores)

Set OPUSFM_BUILD_DIR to use a build tree other than ./build-plasma.
  --deps             print build and optional feature packages and exit
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

KF_MODULES=(archive bookmarks config configwidgets coreaddons crash dbusaddons filemetadata globalaccel guiaddons i18n iconthemes itemviews kio parts solid statusnotifieritem syntaxhighlighting texteditor widgetsaddons windowsystem xmlgui)

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

    echo "Build dependencies (Qt >= 6.8 with Jalali calendar support; KDE Frameworks >= 6.20):"
    case "$family" in
        arch)
            pkgs=(base-devel cmake ninja extra-cmake-modules gettext qt6-base qt6-tools qt6-multimedia libarchive)
            for m in "${KF_MODULES[@]}"; do pkgs+=("k$m"); done
            pkgs=("${pkgs[@]/kkio/kio}")
            pkgs=("${pkgs[@]/ksolid/solid}")
            pkgs=("${pkgs[@]/ksyntaxhighlighting/syntax-highlighting}")
            pkgs+=(plasma-activities)
            echo "  sudo pacman -S --needed ${pkgs[*]}" ;;
        fedora)
            pkgs=(gcc-c++ cmake ninja-build extra-cmake-modules gettext qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtmultimedia-devel libarchive-devel)
            for m in "${KF_MODULES[@]}"; do pkgs+=("kf6-k$m-devel"); done
            pkgs=("${pkgs[@]/kf6-kkio-devel/kf6-kio-devel}")
            pkgs=("${pkgs[@]/kf6-ksolid-devel/kf6-solid-devel}")
            pkgs=("${pkgs[@]/kf6-ksyntaxhighlighting-devel/kf6-syntax-highlighting-devel}")
            pkgs+=(plasma-activities-devel)
            echo "  sudo dnf install ${pkgs[*]}" ;;
        debian)
            pkgs=(build-essential cmake ninja-build extra-cmake-modules gettext qt6-base-dev qt6-base-private-dev qt6-multimedia-dev libarchive-dev)
            for m in "${KF_MODULES[@]}"; do pkgs+=("libkf6$m-dev"); done
            pkgs+=(libplasmaactivities-dev)
            echo "  sudo apt install ${pkgs[*]}" ;;
        suse)
            pkgs=(gcc-c++ cmake ninja extra-cmake-modules gettext-tools qt6-base-devel qt6-multimedia-devel libarchive-devel)
            for m in "${KF_MODULES[@]}"; do pkgs+=("kf6-k$m-devel"); done
            pkgs=("${pkgs[@]/kf6-kkio-devel/kf6-kio-devel}")
            pkgs=("${pkgs[@]/kf6-ksolid-devel/kf6-solid-devel}")
            pkgs=("${pkgs[@]/kf6-ksyntaxhighlighting-devel/kf6-syntax-highlighting-devel}")
            pkgs+=(plasma6-activities-devel)
            echo "  sudo zypper install ${pkgs[*]}" ;;
        *)
            echo "  Unrecognised distro ($ids). Install a C++20 compiler, CMake >= 3.25, Ninja,"
            echo "  gettext, libarchive, Extra CMake Modules, Qt 6 (Core, Gui, Widgets, DBus,"
            echo "  Test, Concurrent, Multimedia, MultimediaWidgets, Network, Xml), and KDE"
            echo "  Frameworks development modules: ${KF_MODULES[*]}, and Plasma Activities" ;;
    esac

    echo
    echo "Optional dependencies (install only the features you need):"
    case "$family" in
        arch)
            echo "  Indexed search, at build and runtime: sudo pacman -S --needed baloo"
            echo "  Embedded terminal: sudo pacman -S --needed konsole"
            echo "  Media metadata editing: sudo pacman -S --needed perl-image-exiftool"
            echo "  Qt Vulkan headers and loader: sudo pacman -S --needed vulkan-headers vulkan-icd-loader" ;;
        fedora)
            echo "  Indexed search, at build and runtime: sudo dnf install kf6-baloo-devel"
            echo "  Embedded terminal: sudo dnf install konsole"
            echo "  Media metadata editing: sudo dnf install perl-Image-ExifTool"
            echo "  Qt Vulkan headers and loader: sudo dnf install vulkan-headers vulkan-loader-devel" ;;
        debian)
            echo "  Indexed search, at build and runtime: sudo apt install libkf6baloo-dev"
            echo "  Embedded terminal: sudo apt install konsole"
            echo "  Media metadata editing: sudo apt install libimage-exiftool-perl"
            echo "  Qt Vulkan headers and loader: sudo apt install libvulkan-dev" ;;
        suse)
            echo "  Indexed search, at build and runtime: install the KF6 Baloo development and runtime packages."
            echo "  Embedded terminal: install Konsole."
            echo "  Media metadata editing: install perl-Image-ExifTool."
            echo "  Qt Vulkan support: install Vulkan headers and loader development files." ;;
        *)
            echo "  Baloo development and runtime packages enable indexed search."
            echo "  Konsole provides the embedded terminal; ExifTool enables media metadata editing."
            echo "  Vulkan headers and the loader enable Qt's optional Vulkan integration." ;;
    esac
    echo "  Document previews and media playback may need additional KParts or Qt Multimedia backends."
    echo "  Qt Quick Compiler Extensions are optional and are not required by Plasma Opus."
    echo "  Distribution package versions must meet the minimums above."
}

check_tools() {
    local missing=()
    for t in cmake ninja c++; do command -v "$t" >/dev/null || missing+=("$t"); done
    if [ ${#missing[@]} -gt 0 ]; then
        echo "missing build tools: ${missing[*]}" >&2
        echo "install the dependencies with:" >&2
        print_deps >&2
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

# A per-user D-Bus activation file, found before system ones, so requests such
# as "Show in folder" start Plasma Opus when no file manager is running.
FM1_SERVICE="${XDG_DATA_HOME:-$HOME/.local/share}/dbus-1/services/org.kde.opusfilemanager.FileManager1.service"

install_file_manager_service() {
    mkdir -p "$(dirname "$FM1_SERVICE")"
    printf '[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=%s/bin/opus-file-manager --file-manager-service\n' "$prefix" > "$FM1_SERVICE"
    echo "file manager service: $FM1_SERVICE"
}

remove_file_manager_service() {
    if [ -f "$FM1_SERVICE" ] && grep -q 'opus-file-manager --file-manager-service' "$FM1_SERVICE"; then
        rm -f -- "$FM1_SERVICE"
        echo "file manager service: removed $FM1_SERVICE"
    fi
}

set_default_handler() {
    install_file_manager_service
    command -v xdg-mime >/dev/null || { echo "xdg-mime not found; skipping default handler" >&2; return; }
    for mime in "${MIME_TYPES[@]}"; do
        xdg-mime default "$DESKTOP_ID" "$mime"
    done
}

restore_default_handler() {
    remove_file_manager_service
    command -v xdg-mime >/dev/null || return 0
    for mime in "${MIME_TYPES[@]}"; do
        if [ "$(xdg-mime query default "$mime" 2>/dev/null)" = "$DESKTOP_ID" ]; then
            if [ -n "$(find /usr/share/applications "${XDG_DATA_HOME:-$HOME/.local/share}/applications" -name org.kde.dolphin.desktop 2>/dev/null)" ]; then
                xdg-mime default org.kde.dolphin.desktop "$mime"
                echo "folder handler: restored to Dolphin"
            else
                echo "folder handler: was Plasma Opus; pick a new one in System Settings > Applications > Default Applications"
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
    || { echo; echo "configure failed; check the build dependencies:"; print_deps; exit 1; }

say "Building"
cmake --build "$BUILD_DIR" ${jobs:+--parallel "$jobs"}

say "Installing"
"${sudo_cmd[@]}" cmake --install "$BUILD_DIR"

refresh_desktop_db "$prefix/share/applications"

if [ "$make_default" = 1 ]; then
    say "Registering Plasma Opus as the default folder handler"
    set_default_handler
fi

echo
echo "binary        : $prefix/bin/opus-file-manager"
echo "desktop entry : $prefix/share/applications/$DESKTOP_ID"
echo "translations  : $prefix/share/locale (when built)"
if command -v xdg-mime >/dev/null; then
    echo "folder handler: $(xdg-mime query default inode/directory 2>/dev/null || echo none)"
fi
if [ "$scope" = user ] && ! case ":$PATH:" in *":$HOME/.local/bin:"*) true ;; *) false ;; esac; then
    echo
    echo "note: ~/.local/bin is not on your PATH. The launcher entry works regardless;"
    echo "      to run 'opus-file-manager' from a shell, add ~/.local/bin to PATH."
fi
[ "$make_default" = 1 ] || { echo; echo "run './install.sh --default' to make Plasma Opus open folders by default."; }
echo "run './install.sh --deps' for optional feature dependencies (Baloo must be present at configure time)."
