#!/usr/bin/env python3
"""Vendor Wine's D3DX9 math implementation into src/MathLib.

Why at all: the game's math is D3DX, and D3DX ships as d3dx9.lib -- a closed
32-bit Windows binary with no source and no build for anything else. Wine
reimplements it under LGPL-2.1+, which this GPL-3 project can use. Taking the
implementation wholesale means **zero call sites change**, so no matrix or
quaternion convention can shift underneath the game.

Why a script rather than a one-time copy: this lands in src/ rather than in
gitignored extern/, so the adaptation has to be visible and re-derivable. Run it
again to move to a newer Wine.

Only the implementation is taken here. The declarations come from MinGW via
tools/vendor-directx-headers.py, and MinGW's D3DX headers *are* Wine's -- same
authors, same text -- so the two halves agree by construction.

Two adaptations, both mechanical:

  * math.c includes d3dx9_private.h for Wine's debug channel. TRACE and WARN
    become no-ops -- 121 call sites, none of which this project wants.

  * math.c implements ID3DXMatrixStack, a COM object needing IUnknown, REFIID,
    CONTAINING_RECORD and a vtable. Nothing in this game has ever called
    D3DXCreateMatrixStack, so the whole class is dropped rather than dragging COM
    scaffolding in for dead code.

Usage:
    tools/vendor-wine-d3dx9math.py [--ref wine-10.0]
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAW = "https://raw.githubusercontent.com/wine-mirror/wine/{ref}/{path}"

# The implementation only. The declarations come from MinGW via
# tools/vendor-directx-headers.py -- and MinGW's D3DX headers are Wine's, so the
# two halves match by construction rather than by luck.
FILES = {
    "dlls/d3dx9_36/math.c":  Path("src/MathLib/source/d3dx9math.c"),
}

SOURCE_PROLOGUE = '''/*
 * Vendored from Wine by tools/vendor-wine-d3dx9math.py, with three changes
 * recorded in that script: d3dx9_private.h replaced by the shims below, TRACE
 * and WARN silenced, and the ID3DXMatrixStack COM class removed as dead code.
 */
#include <windows.h>
#include <d3dx9.h>

#include <stdlib.h>
#include <string.h>

#define TRACE(...) do { } while (0)
#define WARN(...)  do { } while (0)
#define FIXME(...) do { } while (0)
#define ERR(...)   do { } while (0)

/*
 * Wine picks these up from windef.h. The engine defines NOMINMAX on purpose so
 * that std::min and std::max are the ones in scope, so they are defined here --
 * in this C file only -- rather than in a shared header.
 */
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
'''


def fetch(ref, path):
    url = RAW.format(ref=ref, path=path)
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout.decode("utf-8")


def adapt_source(text):
    # Replace Wine's private header and debug channel with the shims above.
    pattern = re.compile(
        r'#include <float\.h>\s*\n+#include "d3dx9_private\.h"\s*\n+'
        r'WINE_DEFAULT_DEBUG_CHANNEL\(d3dx\);\s*\n')
    if not pattern.search(text):
        raise SystemExit("math.c: prologue not found -- check the ref")
    text = pattern.sub("#include <float.h>\n\n" + SOURCE_PROLOGUE + "\n", text, 1)

    # Drop the matrix-stack struct, its size constant, and the whole COM class.
    text = re.sub(
        r'struct ID3DXMatrixStackImpl\s*\{.*?\n\};\s*\n+'
        r'static const unsigned int INITIAL_STACK_SIZE = 32;\s*\n',
        "", text, count=1, flags=re.S)

    start = text.find("static inline struct ID3DXMatrixStackImpl *impl_from_ID3DXMatrixStack")
    end = text.find("/*_________________D3DXPLANE________________*/")
    if start < 0 or end < 0 or end <= start:
        raise SystemExit("math.c: matrix-stack region not found -- check the ref")
    text = text[:start] + text[end:]

    # Check for the implementation type rather than the interface name: the
    # latter appears in the prologue comment above, which is not a code
    # reference and must not trip the guard.
    if "ID3DXMatrixStackImpl" in text:
        raise SystemExit("math.c: the matrix stack survived the cut")

    # Drop D3DXSHProjectCubeMap. It walks a real cube texture through
    # IDirect3DCubeTexture9 and Wine's internal pixel-format tables, none of
    # which exist here -- and no spherical-harmonics function in this file is
    # called by the game. The prototype stays in the vendored header; dropping
    # only the definition is harmless because nothing calls it.
    start = text.find("/*\n * The following implementation of D3DXSHProjectCubeMap")
    end = text.find("FLOAT* WINAPI D3DXSHRotate(")
    if start < 0 or end < 0 or end <= start:
        raise SystemExit("math.c: D3DXSHProjectCubeMap not found -- check the ref")
    text = text[:start] + text[end:]

    if "D3DXSHProjectCubeMap" in text:
        raise SystemExit("math.c: D3DXSHProjectCubeMap survived the cut")
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="wine-10.0",
                        help="Wine git ref to vendor from (default: %(default)s)")
    args = parser.parse_args()

    for upstream, target in FILES.items():
        text = fetch(args.ref, upstream)
        if upstream.endswith("math.c"):
            text = adapt_source(text)
        target.write_text(text, encoding="utf-8")
        print("%-24s -> %s (%d lines)" % (upstream, target, text.count("\n") + 1))

    return 0


if __name__ == "__main__":
    sys.exit(main())
