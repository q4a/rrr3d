#!/usr/bin/env python3
"""Vendor the Direct3D 9 and Windows/COM headers into src/XPlatform.

The engine is written against Direct3D 9 -- 438 distinct D3D identifiers across
twelve interfaces, including the Ex variants. That is a real header, not
something to hand-write, and Microsoft's own comes from a Windows SDK that is
not redistributable.

Two sources, each for the part it does best:

  * MinGW-w64 supplies the DirectX headers themselves (d3d9.h, d3d9types.h,
    d3d9caps.h). They are the standard free reimplementation of the D3D9 API
    surface and are what the rest of the world compiles D3D9 against off
    Windows.

  * DXVK supplies the Windows and COM base its native build uses --
    windows_base.h and the COM headers. This matters beyond convenience: the
    D3D9 implementation this port will run on *is* DXVK, so compiling the game
    against the same base the backend was built against removes a whole class
    of ABI mismatch.

DXVK's own windows.h is deliberately not taken. XPlatform already has one, and
it includes windows_base.h and then declares the Win32 functions this project
calls, which DXVK's does not.

extern/ is gitignored and these have to be present for the tree to compile, so
they are vendored into src/ and this script makes that re-derivable.

Usage:
    tools/vendor-directx-headers.py [--dxvk-ref v2.7.1] [--mingw-ref v12.0.0]
"""

import argparse
import subprocess
import sys
from pathlib import Path

DXVK_RAW = "https://raw.githubusercontent.com/doitsujin/dxvk/{ref}/include/native/windows/{name}"
MINGW_RAW = "https://raw.githubusercontent.com/mingw-w64/mingw-w64/{ref}/mingw-w64-headers/{dir}/{name}"

# Nearly everything lives under include/; _mingw_unicode.h is CRT plumbing that
# the D3DX headers pull in for their A/W name macros, and lives under crt/.
MINGW_DIRS = {"_mingw_unicode.h": "crt"}

# windows.h is intentionally absent -- see the note above.
DXVK_HEADERS = [
    "windows_base.h",
    "unknwn.h",
    "objbase.h",
    "oaidl.h",
    "ocidl.h",
    "ole2.h",
    "rpc.h",
    "rpcndr.h",
]

MINGW_HEADERS = [
    "_mingw_unicode.h",
    "d3d9.h",
    "d3d9types.h",
    "d3d9caps.h",
    # The D3DX family. d3dx9.h pulls in all of these, and the engine uses
    # ID3DXFont, ID3DXMesh, ID3DXEffect and the texture loaders.
    #
    # Worth knowing: MinGW's D3DX headers *are* Wine's -- same authors, same
    # text. So there is one upstream for the declarations, not two, and the
    # implementation vendored by tools/vendor-wine-d3dx9math.py matches them by
    # construction rather than by luck.
    "d3dx9.h",
    "d3dx9math.h",
    "d3dx9math.inl",
    "d3dx9core.h",
    "d3dx9xof.h",
    "d3dx9mesh.h",
    "d3dx9shader.h",
    "d3dx9effect.h",
    "d3dx9shape.h",
    "d3dx9anim.h",
    "d3dx9tex.h",
    # D3D11 and D3D12, for DXVK rather than for the game.
    #
    # dxvk/src/d3d9/d3d9_include.h includes <d3d12.h> unconditionally, for its
    # D3D9On12 interop surface -- D3D9ON12_ARGS and IDirect3DDevice9On12. The
    # game never touches any of it, but the front-end does not compile without
    # the header, and d3d12.h in turn needs the d3d11 chain.
    #
    # DXVK carries its own copies under include/native/directx. Those are not
    # used: they include a d3d9.h of their own, and having two on the path is
    # how a build ends up compiling one header against another's declarations.
    "d3d11.h",
    "d3d11_1.h",
    "d3d11_2.h",
    "d3d11_3.h",
    "d3d11_4.h",
    "d3d11sdklayers.h",
    "d3d11shader.h",
    "d3d11on12.h",
    "d3d12.h",
    "d3d12sdklayers.h",
    "d3d10.h",
    "d3d10_1.h",
    "d3d10shader.h",
    "d3d10_1shader.h",
    "d3d10misc.h",
    "d3d10effect.h",
    "d3d10sdklayers.h",
    "d3dcommon.h",
    "dxgi.h",
    "dxgi1_2.h",
    "dxgi1_3.h",
    "dxgi1_4.h",
    "dxgi1_5.h",
    "dxgi1_6.h",
    "dxgiformat.h",
    "dxgitype.h",
    "dxgicommon.h",
]

WINDOWS_DIR = Path("src/XPlatform/header/windows")
DIRECTX_DIR = Path("src/XPlatform/header/directx")


def fetch(url):
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout


def adapt_vector4_ctor(text):
    """Add the D3DXVECTOR4(D3DXVECTOR3, w) constructor the SDK has and Wine lacks.

    Microsoft's d3dx9math.h declares D3DXVECTOR4(CONST D3DXVECTOR3& xyz, FLOAT
    fw); Wine's -- and so MinGW's -- does not. The game uses it (GrassField.cpp
    builds screen-space quads that way), so it is added rather than the call
    sites being rewritten. This is the same class of deviation as the plane-dot
    signatures: a difference between two reimplementations of one API, resolved
    towards the one the game was written against.
    """
    anchor = "    D3DXVECTOR4(FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw);"
    if anchor not in text:
        raise SystemExit("d3dx9math.h: D3DXVECTOR4 constructors not found -- check --mingw-ref")
    added = anchor + "\n    D3DXVECTOR4(const struct D3DXVECTOR3& xyz, FLOAT fw);"
    text = text.replace(anchor, added, 1)

    return text


def adapt_vector4_inline(text):
    """The body for the constructor adapt_vector4_ctor declares."""
    anchor = "inline D3DXVECTOR4::D3DXVECTOR4(FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw)"
    if anchor not in text:
        raise SystemExit("d3dx9math.inl: D3DXVECTOR4 body not found -- check --mingw-ref")
    body = ("inline D3DXVECTOR4::D3DXVECTOR4(const struct D3DXVECTOR3& xyz, FLOAT fw)\n"
            "{\n"
            "    x = xyz.x;\n"
            "    y = xyz.y;\n"
            "    z = xyz.z;\n"
            "    w = fw;\n"
            "}\n\n")
    return text.replace(anchor, body + anchor, 1)


def adapt_plane_dot(text):
    """Match the SDK's signature for the two plane-dot helpers.

    Upstream types D3DXPlaneDotCoord and D3DXPlaneDotNormal as taking a
    D3DXVECTOR4*; Microsoft's SDK types them as D3DXVECTOR3*, and the game was
    written against the SDK, so it passes vectors. Both implementations read
    only x, y and z, so this is a signature change and not a behaviour one.

    D3DXPlaneDot is left alone -- it reads w and takes a VECTOR4 in both.
    """
    for name in ("D3DXPlaneDotCoord", "D3DXPlaneDotNormal"):
        before = "FLOAT %s(const D3DXPLANE *pp, const D3DXVECTOR4 *pv)" % name
        after = "FLOAT %s(const D3DXPLANE *pp, const D3DXVECTOR3 *pv)" % name
        if before not in text:
            raise SystemExit("d3dx9math.inl: %s not found -- check --mingw-ref" % name)
        text = text.replace(before, after)
    return text


def adapt_effect_pure(text):
    """Add the missing PURE to GetFunction and GetFunctionByName.

    Upstream declares these two without PURE on all three of ID3DXBaseEffect,
    ID3DXEffect and ID3DXEffectCompiler, while every other method on all three
    has it. That makes the interfaces non-abstract, so a class deriving from one
    of them needs a vtable for the interface itself -- which nothing defines, and
    the link fails with "vtable for ID3DXEffect" undefined.

    It is an upstream slip rather than a deliberate difference: an interface with
    two non-pure virtual methods and no implementation anywhere is not something
    that can be used.
    """
    count = 0
    for name, ret in (("GetFunction", "UINT index"),
                      ("GetFunctionByName", "const char *name")):
        before = "    STDMETHOD_(D3DXHANDLE, %s)(THIS_ %s);" % (name, ret)
        after = "    STDMETHOD_(D3DXHANDLE, %s)(THIS_ %s) PURE;" % (name, ret)
        count += text.count(before)
        text = text.replace(before, after)

    # ID3DXBaseEffect, ID3DXEffect, ID3DXEffectCompiler: two methods each.
    if count != 6:
        raise SystemExit(
            "d3dx9effect.h: expected 6 non-PURE GetFunction declarations, found %d "
            "-- check --mingw-ref (upstream may have fixed it)" % count)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dxvk-ref", default="v2.7.1")
    parser.add_argument("--mingw-ref", default="v12.0.0")
    args = parser.parse_args()

    WINDOWS_DIR.mkdir(parents=True, exist_ok=True)
    DIRECTX_DIR.mkdir(parents=True, exist_ok=True)

    for name in DXVK_HEADERS:
        data = fetch(DXVK_RAW.format(ref=args.dxvk_ref, name=name))
        (WINDOWS_DIR / name).write_bytes(data)
        print("dxvk  %-20s -> %s" % (name, WINDOWS_DIR / name))

    for name in MINGW_HEADERS:
        data = fetch(MINGW_RAW.format(ref=args.mingw_ref, name=name,
                                      dir=MINGW_DIRS.get(name, "include")))
        if name == "d3dx9math.inl":
            text = adapt_plane_dot(data.decode("utf-8"))
            data = adapt_vector4_inline(text).encode("utf-8")
        elif name == "d3dx9math.h":
            data = adapt_vector4_ctor(data.decode("utf-8")).encode("utf-8")
        elif name == "d3dx9effect.h":
            data = adapt_effect_pure(data.decode("utf-8")).encode("utf-8")
        (DIRECTX_DIR / name).write_bytes(data)
        print("mingw %-20s -> %s" % (name, DIRECTX_DIR / name))

    # The whole point is that these two sets agree; check the pieces each side
    # is relied on for, so a bad ref fails here rather than in a wall of
    # template errors later.
    base = (WINDOWS_DIR / "windows_base.h").read_text(encoding="utf-8", errors="replace")
    for token in ("DECLARE_INTERFACE", "STDMETHOD", "typedef float FLOAT", "DEFINE_GUID"):
        if token not in base:
            raise SystemExit("windows_base.h is missing %s -- check --dxvk-ref" % token)

    d3d9 = (DIRECTX_DIR / "d3d9.h").read_text(encoding="utf-8", errors="replace")
    for token in ("IDirect3DDevice9Ex", "IDirect3DCubeTexture9", "Direct3DCreate9"):
        if token not in d3d9:
            raise SystemExit("d3d9.h is missing %s -- check --mingw-ref" % token)

    print("\nvendored %d Windows/COM and %d DirectX headers"
          % (len(DXVK_HEADERS), len(MINGW_HEADERS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
