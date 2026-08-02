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
#   names the buffer and the line instead of an address nobody owns. It also
#   makes the version a decision this repository records rather than whatever
#   Homebrew happens to have installed -- which, as below, is the fix.
#
# ---------------------------------------------------------------------------
# WHY 26.06 AND NOT THE NEWEST RELEASE
# ---------------------------------------------------------------------------
#
# This is a deliberate downgrade. Homebrew ships 26.08; that version, and 26.07,
# cannot play this game's engine sound without corrupting memory.
#
# FAudio rewrote its resampler in June 2026 to interpolate the start of each
# quantum from the previous quantum's last two samples -- its "taps"
# (5acd7526, "Store taps of the last two samples when resampling", 2026-06-19,
# first released in 26.07). That rewrite introduced two defects, both on the
# frequency-ratio path, and both were measured here rather than guessed at:
#
#   1. The tap loops are unbounded against the buffer they write into. They emit
#      one output frame per iteration and stop when the source offset reaches
#      the previous sample count, advancing by the resample step each time, so
#      they run about (leftover samples / step) times -- while the destination
#      is sized to the *output* quantum, which does not depend on the ratio. A
#      low enough ratio overruns it. AddressSanitizer, with FAudio instrumented:
#      heap-buffer-overflow, WRITE of size 4, zero bytes past a 1764-byte
#      region, which is a 441-frame quantum of floats exactly. Measured with
#      src/AudioSweep: ratios of 0.004 and below overrun, 0.008 and above do
#      not.
#
#   2. toDecode is computed as (offset + 1 - totalSamples) into a uint64_t, and
#      goes negative when the ratio falls between one quantum and the next --
#      the source is consumed more slowly than it was, so the samples already
#      decoded exceed what the new ratio needs. It wraps to about 2^64 and trips
#      the decodeSamples assertion in FAudio_INTERNAL_DecodeBuffers. Measured at
#      steps of 0.97-0.99, i.e. the *top* of the rev sweep. A race on 26.08
#      survived 1 run in 8 with this alone.
#
# The game drives exactly this: every car in db.xml carries
# <rpmFreqRange>0 1</rpmFreqRange>, and SoundMotor::OnMotor computes the ratio
# as x + alpha*(y - x) (GameBase.cpp:1099), so the ratio *is* alpha -- a sweep
# from 0.0 at minRPM to 1.0 at maxRPM, rising and falling with the engine. Both
# defects are on that path and neither is avoidable from outside the library.
#
# 26.06 predates the rewrite: it has no tap loops at all, and it computes
# toDecode as resampleSamples * resampleStep -- a plain product with nothing to
# underflow. Both defects are absent by construction rather than worked around.
#
# The alternative was to patch 26.08, and that is ruled out: FAudio's
# contribution policy forbids AI-generated code, so a patch written here could
# never go upstream, and carrying a permanent private fork of an audio library
# to avoid a two-month-old regression is the worse trade.
#
# What the downgrade costs: two months of upstream fixes, most of that same
# resampler rework. Revisit when a release lands that fixes both -- as of 26.08
# neither is fixed, and upstream master is byte-identical to 26.08 in this file.
# src/AudioSweep is the check: point this script at a newer tag, rebuild, and
# run it under the asan preset.
#
# SDL3 is FAudio's platform backend and is already a dependency of XPlatform,
# so this adds no new library to the process -- only a second consumer of the
# one that is there.
#
# extern/ is gitignored, and nothing here modifies FAudio's source. This builds
# an unmodified upstream release.
#
# Usage:  tools/setup-faudio-macos.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAUDIO_DIR="$REPO_ROOT/extern/faudio"
FAUDIO_URL="https://github.com/FNA-XNA/FAudio.git"
# See the header. Not the newest on purpose.
FAUDIO_TAG="26.06"

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

# A clone left from an earlier run is on whatever tag that run wanted, so move
# it. Checked rather than assumed: the difference between 26.06 and 26.08 here
# is the difference between working audio and a corrupted heap.
CURRENT="$(git -C "$FAUDIO_DIR" describe --tags --exact-match 2>/dev/null || echo none)"
if [[ "$CURRENT" != "$FAUDIO_TAG" ]]; then
    echo "==> switching from $CURRENT to $FAUDIO_TAG"
    git -C "$FAUDIO_DIR" fetch --depth 1 origin "refs/tags/$FAUDIO_TAG:refs/tags/$FAUDIO_TAG" 2>/dev/null || true
    git -C "$FAUDIO_DIR" checkout -q --force "$FAUDIO_TAG"
    rm -rf "$FAUDIO_DIR/build" "$FAUDIO_DIR/build-asan"
fi

# Unmodified upstream. If this reports anything, something has edited the tree
# and the version this repository thinks it is testing is not the one it built.
if ! git -C "$FAUDIO_DIR" diff --quiet; then
    echo "warning: extern/faudio has local modifications:" >&2
    git -C "$FAUDIO_DIR" diff --stat >&2
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
