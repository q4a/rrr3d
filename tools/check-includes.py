#!/usr/bin/env python3
"""Verify #include directives are portable.

Two things break non-MSVC builds and are invisible on Windows:

1. Backslash separators (#include "px\\Physx.h"). MSVC accepts them;
   clang and gcc do not.
2. Wrong case (#include "graph/driver/RenderDriver.h" when the directory
   is "Driver"). Works on MSVC and on case-insensitive APFS, fails on
   Linux and on a case-sensitive macOS volume.

Only includes that resolve to a file inside src/ are checked; anything
else is assumed to come from extern/ and is left alone.

Run from the repository root:  python3 tools/check-includes.py
"""

import collections
import pathlib
import re
import sys

SOURCE_SUFFIXES = {".cpp", ".h", ".inl"}

# Vendored third-party headers. We do not control their contents, and a future
# upstream change should not fail this project's CI.
VENDORED = (
    "XPlatform/header/windows",
    "XPlatform/header/directx",
)


def is_vendored(path: pathlib.Path, root: pathlib.Path) -> bool:
    rel = path.relative_to(root).as_posix()
    return any(rel.startswith(v) for v in VENDORED)

INCLUDE = re.compile(r'#\s*include\s+"([^"]+)"')


def main() -> int:
    src = pathlib.Path(__file__).resolve().parent.parent / "src"
    if not src.is_dir():
        print(f"error: {src} not found; run from the repository root", file=sys.stderr)
        return 2

    files = [p for p in src.rglob("*") if p.is_file()]

    # every path suffix of every file, so "graph/Driver/RenderDriver.h" and
    # "Driver/RenderDriver.h" both resolve
    exact = set()
    for path in files:
        parts = path.parts
        for i in range(len(parts)):
            exact.add("/".join(parts[i:]))

    insensitive = collections.defaultdict(list)
    for suffix in exact:
        insensitive[suffix.lower()].append(suffix)

    backslash, miscased = [], []
    for path in sorted(f for f in files if f.suffix in SOURCE_SUFFIXES
                       and not is_vendored(f, src)):
        for lineno, line in enumerate(path.read_text(encoding="utf-8").split("\n"), 1):
            match = INCLUDE.search(line)
            if not match:
                continue
            target = match.group(1)
            if "\\" in target:
                backslash.append((path, lineno, target))
                continue
            if target in exact:
                continue
            alternatives = insensitive.get(target.lower())
            if alternatives:
                miscased.append((path, lineno, target, alternatives[0]))

    for path, lineno, target in backslash:
        print(f"{path}:{lineno}: backslash in include: {target}")
    for path, lineno, target, correct in miscased:
        print(f"{path}:{lineno}: wrong case: {target} -> {correct}")

    if backslash or miscased:
        print(f"\n{len(backslash)} backslash, {len(miscased)} miscased include(s)")
        return 1

    print("all includes use forward slashes and correct case")
    return 0


if __name__ == "__main__":
    sys.exit(main())
