#!/bin/bash
#
# Clone and build FAudio (arm64 static library) into extern/faudio.
#
# Why this replaces the Homebrew dylib:
#
#   The crash that stopped a race within a second of starting was an
#   out-of-bounds write inside FAudio's resampler. Address Sanitizer named the
#   function and could say nothing else -- "BUS on unknown address", because a
#   Homebrew dylib is uninstrumented and the sanitizer cannot see its buffers.
#   That is the entire reason the investigation had to proceed by elimination,
#   eight runs per condition, and why five plausible fixes were tried and all
#   were wrong.
#
#   Building FAudio from source puts it inside the sanitizer. The report then
#   names the buffer and the line instead of an address nobody owns. This is a
#   measurement instrument before it is a packaging decision.
#
#   It also lets tools/patches/faudio/faudio.patch apply, which is how the fix
#   is carried until it lands upstream -- the same arrangement
#   tools/patches/d3d9metal/d9mt.patch already uses for d9mt.
#
# Pinned to a tag rather than a moving branch, for the reason every other
# dependency here is pinned: a build that changes under you is not a
# measurement. 26.08 is the version this port has been running against, and
# upstream master is byte-identical to it in the file the patch touches.
#
# SDL3 is FAudio's platform backend and is already a dependency of XPlatform,
# so this adds no new library to the process -- only a second consumer of the
# one that is there.
#
# extern/ is gitignored. Nothing here modifies FAudio's source except the
# committed patch, which is applied on top and reported when it is.
#
# Usage:  tools/setup-faudio-macos.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAUDIO_DIR="$REPO_ROOT/extern/faudio"
FAUDIO_URL="https://github.com/FNA-XNA/FAudio.git"
FAUDIO_TAG="26.08"
PATCH="$REPO_ROOT/tools/patches/faudio/faudio.patch"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script is for macOS" >&2
    exit 1
fi

if ! brew list sdl3 >/dev/null 2>&1; then
    echo "==> installing sdl3"
    brew install sdl3
fi

if [[ ! -d "$FAUDIO_DIR" ]]; then
    echo "==> cloning FAudio $FAUDIO_TAG"
    git clone --depth 1 --branch "$FAUDIO_TAG" "$FAUDIO_URL" "$FAUDIO_DIR"
else
    echo "==> $FAUDIO_DIR already present, skipping clone"
fi

# Applied with --reverse --check first, so re-running the script on an already
# patched tree is a no-op rather than a failure. A patch that neither applies
# nor is already applied is a real error and stops here.
if [[ -f "$PATCH" ]]; then
    if git -C "$FAUDIO_DIR" apply --reverse --check "$PATCH" 2>/dev/null; then
        echo "==> patch already applied"
    else
        echo "==> applying $(basename "$PATCH")"
        git -C "$FAUDIO_DIR" apply "$PATCH"
    fi
else
    echo "==> no patch to apply ($PATCH not present)"
fi

# Two builds, because a sanitized library and an unsanitized one cannot be the
# same file and choosing between them by hand is a trap.
#
# A library built with -fsanitize=address is not linkable into a program built
# without it, and vice versa. With one output directory, whichever variant was
# built last silently becomes what every preset links -- so the asan build would
# quietly stop being instrumented the moment anyone rebuilt for the debug
# preset, which is precisely the blindness this script exists to remove.
#
# So: build/ and build-asan/, and src/XPlatform/CMakeLists.txt picks by whether
# the compile flags carry the sanitizer. Static in both cases, so the archive is
# absorbed into libXPlatform and the process has exactly one FAudio.
#
# FAudio is small -- a few seconds each -- so building both unconditionally is
# cheaper than reasoning about which one is current.
build_variant()
{
    local dir="$1"
    local cflags="$2"

    echo "==> configuring $(basename "$dir")"
    cmake -S "$FAUDIO_DIR" -B "$dir" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="${FAUDIO_BUILD_TYPE:-Debug}" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_C_FLAGS="$cflags" \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_UTILS=OFF \
        >/dev/null

    echo "==> building $(basename "$dir")"
    cmake --build "$dir" -j"$(sysctl -n hw.ncpu)" >/dev/null

    if [[ ! -f "$dir/libFAudio.a" ]]; then
        echo "error: $dir/libFAudio.a was not produced" >&2
        exit 1
    fi
}

build_variant "$FAUDIO_DIR/build" ""
build_variant "$FAUDIO_DIR/build-asan" "-fsanitize=address -fno-omit-frame-pointer -g"

echo "==> done."
for d in build build-asan; do
    printf "      %-24s %s\n" "$d/libFAudio.a" \
        "$(lipo -archs "$FAUDIO_DIR/$d/libFAudio.a")"
done
echo "      headers: $FAUDIO_DIR/include"
