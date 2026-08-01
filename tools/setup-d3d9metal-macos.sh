#!/bin/bash
#
# Fetch and patch the D3D9-on-Metal stack into extern/, for the macOS port.
#
# The stack, and how the pieces fit:
#
#   D3D9 API -> DXVK front-end -> DxvkContext -> d9mt's Metal backend
#            -> winemetal ABI -> src/MetalBridge -> Metal
#
# ONE clone, not two. d9mt carries its own vendored snapshot of DXVK v2.7.1
# under vendor/dxvk, documented in vendor/dxvk/DXVK-VERSION, and that is the
# configuration d9mt supports: its own manifest records an include-closure
# check showing every quoted include reachable from the 36 d3d9 translation
# units resolves inside that tree.
#
# Fetching DXVK separately looked reasonable and was wrong. It produced two
# copies of the same v2.7.1 on the include path -- the classic way to compile
# one header against another's declarations -- and it duplicated four patches
# d9mt had already applied. It also broke d9mt's own relative includes, which
# reach into ../../vendor/dxvk and are correct as they stand.
#
# It also means the Vulkan and SPIR-V headers arrive populated: upstream DXVK
# carries them as submodules, and d9mt's snapshot has them filled in.
#
# No Wine. d9mt targets Wine and reaches Metal through winemetal, an ABI whose
# job is crossing the wow64 boundary. There is no boundary here, so
# src/MetalBridge implements that ABI directly against Metal and the boundary
# disappears. Nothing fetched here depends on Wine -- d9mt's front-end never
# did; its only Wine references were comments.
#
# WHY THIS IS A SCRIPT rather than a vendored tree:
#
#   This is about 40,000 lines that are not ours. Checking them in would put a
#   second project's history in this repository and make tracking upstream a
#   merge rather than a re-run. So extern/ is gitignored and this script is the
#   record -- every change lives in tools/patches/d3d9metal/ as a patch, which
#   is both the documentation and the mechanism.
#
# WHAT THE PATCHES ARE, and the counts are small enough to state exactly:
#
#   dxvk.patch    5 files, against d9mt's vendored DXVK. The macOS and arm64
#                 fixes d9mt had not already made -- it carries four of its
#                 own, listed in vendor/dxvk/DXVK-VERSION.
#
#   d9mt.patch    7 files. D9MT_API, which is __declspec on Windows and empty
#                 here because there is no DLL boundary, and the handful of
#                 places the Metal backend needs adjusting for a native build.
#                 Notably NOT include rebasing: upstream's relative includes
#                 are correct for this layout and are left alone.
#
#                 One change is a real upstream bug rather than a porting
#                 adjustment: the async PSO worker threads outlive static
#                 destruction, and were using other translation units' statics
#                 after free -- the logf mutex (SIGABRT on every single run)
#                 and spirv-cross's illegal-name set (SIGSEGV). They are now
#                 joined in ~DxvkDevice. See the comment on shutdownPsoWorkers
#                 in d9mt_context.cpp.
#
#   Two files are added rather than patched: d9mt_fetrace.h and
#   d9mt_wsi_bootstrap.cpp, which defines Win32WSI instead of taking it from
#   DXVK's Win32 file -- see that file for why.
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

# --- d9mt (which carries DXVK) -----------------------------------------------

if [[ -d "$EXTERN/d9mt" && $FORCE -eq 0 ]]; then
    echo "d9mt already present; pass --force to refetch"
else
    rm -rf "$EXTERN/d9mt"
    echo "fetching d9mt $D9MT_REV..."
    git clone --quiet "$D9MT_URL" "$EXTERN/d9mt"
    ( cd "$EXTERN/d9mt" && git -c advice.detachedHead=false checkout --quiet "$D9MT_REV" )

    echo "patching d9mt's vendored DXVK..."
    ( cd "$EXTERN/d9mt" && patch -p1 --batch --forward --silent < "$PATCHES/dxvk.patch" )

    echo "patching d9mt..."
    ( cd "$EXTERN/d9mt" && patch -p1 --batch --forward --silent < "$PATCHES/d9mt.patch" )
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

check "$EXTERN/d9mt/vendor/dxvk/src/d3d9/d3d9_device.cpp"
check "$EXTERN/d9mt/vendor/dxvk/include/spirv/include/spirv/unified1/spirv.hpp"
check "$EXTERN/d9mt/vendor/dxvk/include/vulkan/include/vulkan/vulkan_core.h"
check "$EXTERN/d9mt/vendor/dxvk/src/dxvk/dxvk_dummy_frag.h"
check "$EXTERN/d9mt/vendor/dxvk/generated/d3d9_convert_nv12.h"
check "$EXTERN/d9mt/src/d3d9fe/d9mt_device.cpp"
check "$EXTERN/d9mt/src/d3d9fe/d9mt_wsi_bootstrap.cpp"
check "$EXTERN/d9mt/src/winemetal.h"
check "$EXTERN/d9mt/vendor/spirv-cross"

[[ $fail -eq 0 ]] || exit 1

echo
echo "d3d9metal ready:"
echo "  extern/d9mt              $D9MT_REV"
echo "  extern/d9mt/vendor/dxvk  $DXVK_TAG (d9mt's snapshot)"
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
