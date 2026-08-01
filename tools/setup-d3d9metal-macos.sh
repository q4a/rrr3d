#!/bin/bash
#
# Fetch and patch the D3D9-on-Metal stack into extern/, for the macOS port.
#
# The stack, and how the pieces fit:
#
#   D3D9 API -> DXVK front-end -> DxvkContext -> d9mt's Metal backend
#            -> winemetal ABI -> src/MetalBridge -> Metal
#
#   dxvk/   DXVK v2.7.1, the D3D9 front-end (~22,400 lines of d3d9 alone)
#   d9mt/   d9mt, which replaces DxvkDevice/DxvkContext with a Metal one
#
# No Wine. d9mt targets Wine and reaches Metal through winemetal, an ABI whose
# job is crossing the wow64 boundary. There is no boundary here, so
# src/MetalBridge implements that ABI directly against Metal and the boundary
# disappears. Nothing fetched here depends on Wine -- d9mt's front-end never
# did; its only Wine references were comments.
#
# WHY THIS IS A SCRIPT rather than a vendored tree:
#
#   Together these are about 40,000 lines that are not ours. Checking them in
#   would put a second project's history in this repository and make tracking
#   upstream a merge rather than a re-run. So extern/ is gitignored and this
#   script is the record -- every change to either tree lives in
#   tools/patches/d3d9metal/ as a patch, which is both the documentation and
#   the mechanism.
#
# WHAT THE PATCHES ARE. Every one is a 32-bit or Wine artefact rather than a
# design change, and the count is small enough to state exactly:
#
#   dxvk.patch    8 files.  Vulkan non-dispatchable handles are uint64 on i686
#                 and pointers on arm64; d9mt smuggles Metal handles through
#                 them, which is fine either way, but the cast has to change
#                 shape. Plus the Win32 bits DXVK's native build assumes.
#
#   d9mt.patch    12 files. Relative includes rebased -- d9mt reached into
#                 ../../vendor/... and assumed its own layout -- and D9MT_API,
#                 which is __declspec on Windows and empty here because there
#                 is no DLL boundary.
#
#   Three files are added rather than patched: d9mt_fetrace.h and
#   d9mt_wsi_bootstrap.cpp (Win32WSI is defined there instead of coming from
#   DXVK's Win32 file -- see the file for why), and dxvk_dummy_frag.h.
#
# Usage:  tools/setup-d3d9metal-macos.sh [--force]
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTERN="$REPO_ROOT/extern"
PATCHES="$REPO_ROOT/tools/patches/d3d9metal"

DXVK_URL="https://github.com/doitsujin/dxvk.git"
DXVK_TAG="v2.7.1"

# d9mt has no releases, so the revision is pinned by hash. Bumping it means
# re-deriving the patches against the new tree -- see REGENERATING below.
D9MT_URL="https://github.com/neo773/d9mt.git"
D9MT_REV="237e293"

FORCE=0
[[ "${1:-}" == "--force" ]] && FORCE=1

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script is for macOS" >&2
    exit 1
fi

for tool in git patch; do
    command -v "$tool" >/dev/null || { echo "error: $tool not found" >&2; exit 1; }
done

mkdir -p "$EXTERN"

# --- DXVK -------------------------------------------------------------------

if [[ -d "$EXTERN/dxvk" && $FORCE -eq 0 ]]; then
    echo "dxvk already present; pass --force to refetch"
else
    rm -rf "$EXTERN/dxvk"
    echo "fetching DXVK $DXVK_TAG..."
    # Submodules matter: include/spirv and include/vulkan are 112 headers that
    # DXVK does not carry itself, and it will not compile without them.
    git clone --quiet --depth 1 --branch "$DXVK_TAG" --recurse-submodules \
        "$DXVK_URL" "$EXTERN/dxvk"

    echo "patching DXVK..."
    ( cd "$EXTERN/dxvk" && patch -p1 --silent < "$PATCHES/dxvk.patch" )
    cp "$PATCHES/dxvk_dummy_frag.h" "$EXTERN/dxvk/src/dxvk/dxvk_dummy_frag.h"
fi

# --- d9mt -------------------------------------------------------------------

if [[ -d "$EXTERN/d9mt" && $FORCE -eq 0 ]]; then
    echo "d9mt already present; pass --force to refetch"
else
    rm -rf "$EXTERN/d9mt"
    echo "fetching d9mt $D9MT_REV..."
    git clone --quiet "$D9MT_URL" "$EXTERN/d9mt"
    ( cd "$EXTERN/d9mt" && git -c advice.detachedHead=false checkout --quiet "$D9MT_REV" )

    echo "patching d9mt..."
    ( cd "$EXTERN/d9mt" && patch -p1 --silent < "$PATCHES/d9mt.patch" )
    cp "$PATCHES/d9mt_fetrace.h" "$EXTERN/d9mt/src/d3d9fe/d9mt_fetrace.h"
    cp "$PATCHES/d9mt_wsi_bootstrap.cpp" "$EXTERN/d9mt/src/d3d9fe/d9mt_wsi_bootstrap.cpp"
fi

# --- verification -----------------------------------------------------------
#
# Cheap checks that the trees are what the build expects, so a bad tag or a
# moved file fails here rather than in a wall of compiler errors.

fail=0
check() {
    if [[ ! -e "$1" ]]; then
        echo "error: expected $1" >&2
        fail=1
    fi
}

check "$EXTERN/dxvk/src/d3d9/d3d9_device.cpp"
check "$EXTERN/dxvk/include/spirv/include/spirv/unified1/spirv.hpp"
check "$EXTERN/dxvk/include/vulkan/include/vulkan/vulkan_core.h"
check "$EXTERN/dxvk/src/dxvk/dxvk_dummy_frag.h"
check "$EXTERN/d9mt/src/d3d9fe/d9mt_device.cpp"
check "$EXTERN/d9mt/src/d3d9fe/d9mt_wsi_bootstrap.cpp"
check "$EXTERN/d9mt/src/winemetal.h"
check "$EXTERN/d9mt/vendor/spirv-cross"

[[ $fail -eq 0 ]] || exit 1

echo
echo "d3d9metal ready:"
echo "  extern/dxvk   $DXVK_TAG + $(rg -c '^--- a/' "$PATCHES/dxvk.patch") patched files"
echo "  extern/d9mt   $D9MT_REV + $(rg -c '^--- a/' "$PATCHES/d9mt.patch") patched files"
echo
echo "winemetal ABI is implemented by src/MetalBridge, not fetched."

# --- REGENERATING THE PATCHES ----------------------------------------------
#
# When either upstream moves, the patches are re-derived rather than hand-
# edited. The procedure that produced them:
#
#   1. Fetch the new upstream into a scratch directory.
#   2. Apply the old patches; fix whatever rejects by hand in that tree.
#   3. diff -u upstream/<file> patched/<file> for each changed file, with the
#      headers rewritten to a/<path> and b/<path>.
#
# The patches in tools/patches/d3d9metal were derived exactly that way, from
# the working tree on the macos-port branch, which is where this stack was
# first proven to run the game at 60fps.
