#!/usr/bin/env python3
"""Fail the build on source files that are not valid UTF-8.

The tree was CP1251 until tools/transcode-cp1251.py converted it. This gate
stops it drifting back: an editor set to the system codepage, or a file added
from an older branch, reintroduces bytes that clang rejects outright and that
make `sed` fail with "illegal byte sequence".

Usage:
    tools/check-encoding.py [root]

Exits non-zero listing every file that fails, with the byte offset.
"""

import argparse
import os
import sys

SOURCE_EXTENSIONS = {".cpp", ".c", ".h", ".inl", ".ms", ".mm"}

# Third-party trees whose encoding is not ours to police. Paths are relative to
# the root being checked.
#
# TinyXml is upstream 2.5.3 verbatim (tools/vendor-tinyxml.py) and tinystr.cpp
# carries a Latin-1 o-slash in "THIS FILE WAS ALTERED BY Tyge Lovset" -- an
# authorship notice, and not something to rewrite. Excluding the tree is also
# the durable answer: a future version bump could introduce more of the same,
# and this checker exists to keep *our* sources readable by our tooling.
VENDORED = ("TinyXml",)


def find_sources(root):
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root)
        if any(rel == v or rel.startswith(v + os.sep) for v in VENDORED):
            dirnames[:] = []
            continue
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in SOURCE_EXTENSIONS:
                yield os.path.join(dirpath, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default="src")
    args = parser.parse_args()

    checked = 0
    failures = []

    for path in find_sources(args.root):
        data = open(path, "rb").read()
        checked += 1
        try:
            data.decode("utf-8")
        except UnicodeDecodeError as exc:
            failures.append("%s: not valid UTF-8 at byte %d (0x%02x)"
                            % (path, exc.start, data[exc.start]))

    for message in failures:
        print(message, file=sys.stderr)

    if failures:
        print("\n%d file(s) are not UTF-8 -- run tools/transcode-cp1251.py"
              % len(failures), file=sys.stderr)
        return 1

    print("%d source file(s) are valid UTF-8" % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
