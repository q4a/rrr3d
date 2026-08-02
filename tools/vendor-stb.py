#!/usr/bin/env python3
"""Vendor stb_image and stb_truetype into src/XPlatform/vendor.

Two single-header libraries from https://github.com/nothings/stb, both dual-
licensed public domain / MIT (the licence text is at the bottom of each header).

  stb_image.h     decodes the 213 .png and 2 .jpg textures the game ships.
                  The other 309 are .dds, which d3dx_texture.cpp parses
                  directly -- stb does not read DDS, and block-compressed data
                  wants to reach the GPU still compressed rather than be
                  decoded to RGBA on the way.

  stb_truetype.h  rasterises glyphs for ID3DXFont. Not a debug-only concern:
                  graph::TextFont is a first-class engine resource and every
                  piece of UI text in the game renders through it.

  stb_dxt.h       encodes BC1/BC3 blocks for D3DXFilterTexture. The engine
                  calls that on block-compressed textures whose level 0 it has
                  just filled, so the mip chain has to be built in the
                  compressed domain -- decode, box filter in RGBA, re-encode --
                  and this is the re-encode. 201 of the game's textures reach
                  it, which is essentially every world, track and car surface.

Chosen over macOS ImageIO and CoreText deliberately. Both would work and would
be less code, but both are macOS-only, and the point of this port is a codebase
that is cross-platform with the minimum of platform-specific parts. These
compile the same everywhere.

Committed rather than fetched into extern/. extern/ is gitignored on the
principle that a dependency arriving by script has to be *built*; these are
headers with no build system, so the script is a provenance record and the
files live in the tree.

All are used unmodified. d3dx_texture.cpp compiles stb_image with the decoders
the game does not need switched off (STBI_NO_*), which is configuration rather
than a change to the source.

Usage:
    tools/vendor-stb.py
"""

import hashlib
import subprocess
import sys
from pathlib import Path

# Pinned by commit, so the URL cannot drift under us, and by hash, so a
# compromised or rewritten commit is caught rather than trusted.
COMMIT = "f0569113c93ad095470c54bf34a17b36646bbbb5"

BASE = "https://raw.githubusercontent.com/nothings/stb/%s/" % COMMIT

FILES = {
    "stb_image.h":
        "594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3",
    "stb_truetype.h":
        "ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab",
    "stb_dxt.h":
        "807667ef98e0fd749cdb65cca0c2d980bc148109d2fed6f1873c81ae0f449933",
}

DEST = Path("src/XPlatform/vendor")


def fetch(url):
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout


def main():
    if not Path("src/XPlatform").is_dir():
        raise SystemExit("run this from the repository root")

    DEST.mkdir(parents=True, exist_ok=True)

    for name, expected in FILES.items():
        data = fetch(BASE + name)
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise SystemExit(
                "%s: sha256 mismatch\n  expected %s\n  got      %s"
                % (name, expected, digest))

        (DEST / name).write_bytes(data)
        print("%s  %s (%d bytes)" % (digest[:12], DEST / name, len(data)))

    print("\nAll unmodified. See %s/README.md." % DEST)
    return 0


if __name__ == "__main__":
    sys.exit(main())
