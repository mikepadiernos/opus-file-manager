#!/bin/sh
# Run a command in the headless Linux development container.
#
# Usage: tools/dev-container/run.sh [--rebuild] [command [args...]]
#
# Without a command, this opens an interactive shell. The repository is mounted
# at /src, and ccache data persists in a named Docker volume.
#
# The image extends plasma-impresario-dev. When that image is missing, this
# builds it with Plasma Impresario's tools/dev-container/run.sh, found through
# IMPRESARIO_DIR (default: a plasma-impresario checkout beside this one). The
# Plasma Opus image is rebuilt automatically when the base image changes;
# --rebuild forces a rebuild. To refresh the base image itself, run
# Impresario's run.sh with --rebuild.

set -eu

image=plasma-opus-dev
base_image=plasma-impresario-dev
ccache_volume=plasma-opus-ccache
label=io.github.mikepadiernos.plasma-opus.base-image-id
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
impresario_dir=${IMPRESARIO_DIR:-$root/../plasma-impresario}

rebuild=0
if [ "${1-}" = "--rebuild" ]; then
    rebuild=1
    shift
fi

platform=$(docker version --format '{{.Server.Os}}/{{.Server.Arch}}')

if ! docker image inspect "$base_image" >/dev/null 2>&1; then
    base_run="$impresario_dir/tools/dev-container/run.sh"
    if [ ! -x "$base_run" ]; then
        echo "error: the $base_image image is missing and $base_run was not found." >&2
        echo "Set IMPRESARIO_DIR to a Plasma Impresario checkout." >&2
        exit 1
    fi
    "$base_run" true
fi

base_id=$(docker image inspect --format '{{.Id}}' "$base_image")
built_from=$(docker image inspect --format "{{index .Config.Labels \"$label\"}}" "$image" 2>/dev/null || true)

if [ "$rebuild" = 1 ] || [ "$built_from" != "$base_id" ]; then
    docker build --platform "$platform" \
        --build-arg "BASE_IMAGE=$base_image" \
        --build-arg "BASE_IMAGE_ID=$base_id" \
        -t "$image" -f "$here/Containerfile" "$here"
fi

if [ "$#" -eq 0 ]; then
    set -- bash
fi

tty_flags=-i
if [ -t 0 ] && [ -t 1 ]; then
    tty_flags=-it
fi

# CMake's KDEGitCommitHooks installs Git hooks that use container-only paths.
# Keep them out of the host checkout.
hooks_mount=
if [ -d "$root/.git/hooks" ]; then
    hooks_mount=--tmpfs=/src/.git/hooks
fi

exec docker run --rm "$tty_flags" \
    --platform "$platform" \
    --shm-size=1g \
    -v "$root:/src" \
    -v "$ccache_volume:/ccache" \
    ${hooks_mount:+"$hooks_mount"} \
    "$image" "$@"
