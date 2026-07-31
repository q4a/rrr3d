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
MINGW_RAW = "https://raw.githubusercontent.com/mingw-w64/mingw-w64/{ref}/mingw-w64-headers/include/{name}"

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
    "d3d9.h",
    "d3d9types.h",
    "d3d9caps.h",
]

WINDOWS_DIR = Path("src/XPlatform/header/windows")
DIRECTX_DIR = Path("src/XPlatform/header/directx")


def fetch(url):
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout


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
        data = fetch(MINGW_RAW.format(ref=args.mingw_ref, name=name))
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
