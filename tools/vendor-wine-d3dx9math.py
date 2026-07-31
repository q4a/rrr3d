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

Three adaptations, all mechanical:

  * d3dx9math.h includes the whole of d3dx9.h upstream. This port needs only the
    D3D9 base types the math structs derive from, which XPlatform supplies.

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

FILES = {
    "include/d3dx9math.h":   Path("src/MathLib/header/d3d/d3dx9math.h"),
    "include/d3dx9math.inl": Path("src/MathLib/header/d3d/d3dx9math.inl"),
    "dlls/d3dx9_36/math.c":  Path("src/MathLib/source/d3dx9math.c"),
}

HEADER_ADAPTATION = ('''/*
 * Vendored from Wine by tools/vendor-wine-d3dx9math.py. Upstream includes the
 * whole of d3dx9.h here; this port needs only the D3D9 base types that the math
 * structs derive from -- D3DVECTOR, D3DMATRIX and D3DCOLORVALUE.
 *
 * windows.h first, because the DirectX headers name the Windows scalar types
 * and do not include anything themselves -- the ordering the DirectX SDK
 * assumes on Windows too. d3d9.h rather than d3d9types.h because the
 * implementation also uses D3D_OK and D3DERR_INVALIDCALL, which live there.
 */
#include <windows.h>
#include <d3d9.h>''')

SOURCE_PROLOGUE = '''/*
 * Vendored from Wine by tools/vendor-wine-d3dx9math.py, with three changes
 * recorded in that script: d3dx9_private.h replaced by the shims below, TRACE
 * and WARN silenced, and the ID3DXMatrixStack COM class removed as dead code.
 */
#include "d3d/d3dx9math.h"

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


def adapt_header(text):
    if '#include "d3dx9.h"' not in text:
        raise SystemExit("d3dx9math.h: upstream include not found -- check the ref")
    text = text.replace('#include "d3dx9.h"', HEADER_ADAPTATION, 1)

    # Drop the ID3DXMatrixStack COM interface and D3DXCreateMatrixStack, whose
    # implementation was removed from math.c for the same reason: nothing in
    # this game calls them, and declaring them needs DECLARE_INTERFACE_,
    # DEFINE_GUID, IUnknown and the rest of the COM preamble.
    start = text.find("typedef interface ID3DXMatrixStack *LPD3DXMATRIXSTACK;")
    end = text.find('#include "d3dx9math.inl"')
    if start < 0 or end < 0 or end <= start:
        raise SystemExit("d3dx9math.h: matrix-stack block not found -- check the ref")
    text = text[:start] + text[end:]

    if "ID3DXMatrixStack" in text:
        raise SystemExit("d3dx9math.h: the matrix stack survived the cut")
    return text


def adapt_inl(text):
    """Match the SDK's signature for the two plane-dot helpers.

    Wine types D3DXPlaneDotCoord and D3DXPlaneDotNormal as taking a
    D3DXVECTOR4*; Microsoft's SDK types them as D3DXVECTOR3*, and the game was
    written against the SDK, so it passes vectors. Both implementations read
    only x, y and z, so this is a signature change and not a behaviour one.

    D3DXPlaneDot is left alone -- it reads w and takes a VECTOR4 in both.
    """
    changed = 0
    for name in ("D3DXPlaneDotCoord", "D3DXPlaneDotNormal"):
        before = "FLOAT %s(const D3DXPLANE *pp, const D3DXVECTOR4 *pv)" % name
        after = "FLOAT %s(const D3DXPLANE *pp, const D3DXVECTOR3 *pv)" % name
        if before not in text:
            raise SystemExit("d3dx9math.inl: %s not found -- check the ref" % name)
        text = text.replace(before, after)
        changed += 1
    if changed != 2:
        raise SystemExit("d3dx9math.inl: expected two plane-dot signatures")
    return text


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
    # called by the game. Its prototype is removed from the header for C++
    # callers; the definition has to go too or the object will not link.
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
        if upstream.endswith("d3dx9math.h"):
            text = adapt_header(text)
        elif upstream.endswith("d3dx9math.inl"):
            text = adapt_inl(text)
        elif upstream.endswith("math.c"):
            text = adapt_source(text)
        target.write_text(text, encoding="utf-8")
        print("%-24s -> %s (%d lines)" % (upstream, target, text.count("\n") + 1))

    return 0


if __name__ == "__main__":
    sys.exit(main())
