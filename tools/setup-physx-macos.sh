#!/bin/bash
#
# Build PhysX 4.1 (arm64 static libraries) into extern/physx41 for the macOS port.
#
# Why 4.1 and why this fork:
#   - The game currently uses PhysX 2.8.4, which is closed source with no 64-bit
#     or macOS build. There is nothing to shim to, so migration is forced.
#   - PhysX 5 has no macOS support at all -- its public presets are Linux and
#     Windows only.
#   - colincornaby/PhysX-Gameworks-Apple is PhysX 4.1 (BSD-3) carrying the
#     Apple Silicon patches NVIDIA never merged.
#
# extern/ is gitignored, so the local patches below live here rather than in the
# checkout. They are all build-system only; no PhysX source is modified.
#
# Usage:  tools/setup-physx-macos.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PHYSX_DIR="$REPO_ROOT/extern/physx41"
FORK_URL="https://github.com/colincornaby/PhysX-Gameworks-Apple.git"
FORK_BRANCH="ApplePlatformPatches"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script is for macOS" >&2
    exit 1
fi

if [[ ! -d "$PHYSX_DIR" ]]; then
    echo "==> cloning PhysX 4.1 (Apple fork)"
    git clone --depth 1 --branch "$FORK_BRANCH" --single-branch "$FORK_URL" "$PHYSX_DIR"
else
    echo "==> $PHYSX_DIR already present, skipping clone"
fi

MAC_CMAKE="$PHYSX_DIR/physx/source/compiler/cmake/mac/CMakeLists.txt"

echo "==> patching the mac build configuration"
python3 - "$MAC_CMAKE" <<'PATCH'
import pathlib, sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

# 1. Force arm64. The default is a universal build, and the x86_64 slice fails
#    to compile under current Xcode. This project only ships arm64.
old_arch = '''IF(PX_OUTPUT_ARCH STREQUAL "x86")
\tSET(OSX_BITNESS "-arch x86_64 -msse2")
\tSET(CMAKE_OSX_ARCHITECTURES "x86_64")
ELSEIF(PX_OUTPUT_ARCH STREQUAL "arm")
\tSET(OSX_BITNESS "-arch arm64")
\tSET(CMAKE_OSX_ARCHITECTURES "arm64")
ELSE()
\tSET(OSX_BITNESS "-arch x86_64 -arch arm64 -msse2")
\tSET(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
ENDIF()'''
new_arch = '''# rrr3d: arm64 only -- the universal default fails on the x86_64 slice.
SET(OSX_BITNESS "-arch arm64")
SET(CMAKE_OSX_ARCHITECTURES "arm64")'''
if old_arch in text:
    text = text.replace(old_arch, new_arch, 1)
    print("    forced arm64")

# 2. 10.9 predates Apple Silicon.
if 'SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")' in text:
    text = text.replace('SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")',
                        'SET(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")', 1)
    print("    deployment target 11.0")

# 3. PhysX builds with -Weverything -Werror. Against a much newer clang than
#    this code was written for, that keeps promoting brand-new warnings
#    (-Wmissing-include-dirs, then -Wsuggest-override, ...) into build
#    failures. We are not maintaining PhysX's warning hygiene.
if " -Werror " in text:
    text = text.replace(" -Werror ", " ")
    print("    removed -Werror")

# Xcode references generated DerivedSources directories that do not exist.
if "-Wno-missing-include-dirs" not in text:
    text = text.replace("-Weverything -Wno-unknown-warning-option",
                        "-Weverything -Wno-unknown-warning-option -Wno-missing-include-dirs", 1)
    print("    suppressed -Wmissing-include-dirs")

path.write_text(text, encoding="utf-8")
PATCH

# PhysX's generator script calls `python`, which macOS does not provide.
PYSHIM="$(mktemp -d)"
ln -sf "$(command -v python3)" "$PYSHIM/python"
trap 'rm -rf "$PYSHIM"' EXIT

echo "==> generating projects"
rm -rf "$PHYSX_DIR/physx/compiler/mac64"
( cd "$PHYSX_DIR/physx" && PATH="$PYSHIM:$PATH" ./generate_projects.sh mac64 >/dev/null )

echo "==> building (this takes a few minutes)"
# The snippet executables link against GLUT/OpenGL and are not needed; if they
# fail, the SDK libraries are still produced.
cmake --build "$PHYSX_DIR/physx/compiler/mac64" --config release >/dev/null 2>&1 || true

LIB_DIR="$PHYSX_DIR/physx/bin/mac.x86_64/release"
if [[ ! -f "$LIB_DIR/libPhysX_static_64.a" ]]; then
    echo "error: PhysX libraries were not produced" >&2
    exit 1
fi

echo "==> done. arm64 static libraries in:"
echo "    $LIB_DIR"
# The directory is named mac.x86_64 by PhysX's own naming convention; the
# binaries really are arm64.
for lib in "$LIB_DIR"/libPhysX*.a; do
    printf "      %-44s %s\n" "$(basename "$lib")" "$(lipo -archs "$lib")"
done
