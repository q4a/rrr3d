#!/usr/bin/env python3
"""Vendor TinyXML 2.5.3 into src/TinyXml.

LexStd's lslSerialFileXml.cpp is the only consumer, and it is the whole XML
layer: db.xml, every map, every saved game. The tree used to link a prebuilt
extern/tinyxml/lib/tinyxmld_STL.lib, which does not exist off Windows and is not
in the repository on any platform, so the library is built from source instead.

That is a smaller change than it sounds. TinyXML is four .cpp files with no
dependencies, under the zlib licence, and version 2.5.3 is from 2007 and will
not be moving. Vendoring it removes a gitignored binary from the build entirely.

The version is not a preference. extern/tinyxml/include/tinyxml.h in a checkout
that has the original Windows dependencies declares TIXML_{MAJOR,MINOR,PATCH}
2/5/3, and the headers in this tarball are byte-identical to it once CRLF line
endings are normalised -- verified before this script was written. So this is
the same TinyXML the game has always parsed its data with, not merely one with
the same version number.

Only the six library files are taken; docs/ and xmltest.cpp are not.

Usage:
    tools/vendor-tinyxml.py
"""

import hashlib
import io
import subprocess
import sys
import tarfile
from pathlib import Path

URL = "https://downloads.sourceforge.net/project/tinyxml/tinyxml/2.5.3/tinyxml_2_5_3.tar.gz"

# The upstream tarball. SourceForge has no tags to pin, so the hash is the pin.
SHA256 = "c0b88bc5413e41ed566dab9f7a192e0e9e6060cc13ebdeab4766168559620d2b"

FILES = [
    "tinystr.h",
    "tinystr.cpp",
    "tinyxml.h",
    "tinyxml.cpp",
    "tinyxmlerror.cpp",
    "tinyxmlparser.cpp",
]

DEST = Path("src/TinyXml")


def fetch(url):
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout


def main():
    data = fetch(URL)

    digest = hashlib.sha256(data).hexdigest()
    if digest != SHA256:
        raise SystemExit(
            "tarball hash mismatch\n  expected %s\n  got      %s" % (SHA256, digest))

    DEST.mkdir(parents=True, exist_ok=True)

    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
        for name in FILES:
            member = archive.extractfile("tinyxml/" + name)
            if member is None:
                raise SystemExit("tinyxml/%s missing from the tarball" % name)

            # Upstream ships CRLF. Normalised so check-encoding and every diff
            # in this repository see the same line endings everywhere else does.
            text = member.read().replace(b"\r\n", b"\n")
            (DEST / name).write_bytes(text)
            print("%-20s -> %s" % (name, DEST / name))

    # The version macros are the reason this vendoring is safe to do at all --
    # if they ever move, the assumption that this matches the game's original
    # dependency has silently stopped holding.
    header = (DEST / "tinyxml.h").read_text(encoding="utf-8", errors="replace")
    for macro, value in (("MAJOR", 2), ("MINOR", 5), ("PATCH", 3)):
        expected = "const int TIXML_%s_VERSION = %d;" % (macro, value)
        if expected not in header:
            raise SystemExit("tinyxml.h is not 2.5.3 -- %r not found" % expected)

    print("\nvendored TinyXML 2.5.3 (%d files)" % len(FILES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
