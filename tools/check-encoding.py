#!/usr/bin/env python3
"""Verify that every source file under src/ is valid UTF-8.

The tree was Windows-1251 until the CP1251 -> UTF-8 transcode. Anything that
reintroduces a non-UTF-8 file breaks clang, makes grep treat the file as
binary, and gets silently corrupted by tools that assume UTF-8.

Run from the repository root:  python3 tools/check-encoding.py
"""

import pathlib
import sys

SOURCE_SUFFIXES = {".cpp", ".h", ".inl", ".ms"}

# Vendored third-party headers. We do not control their contents, and a future
# upstream change should not fail this project's CI.
VENDORED = (
    "XPlatform/header/windows",
    "XPlatform/header/directx",
)


def is_vendored(path: pathlib.Path, root: pathlib.Path) -> bool:
    rel = path.relative_to(root).as_posix()
    return any(rel.startswith(v) for v in VENDORED)



def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent / "src"
    if not root.is_dir():
        print(f"error: {root} not found; run from the repository root", file=sys.stderr)
        return 2

    checked = 0
    bad = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        if is_vendored(path, root):
            continue
        checked += 1
        try:
            path.read_bytes().decode("utf-8")
        except UnicodeDecodeError as exc:
            bad.append((path, exc))

    if bad:
        print(f"{len(bad)} of {checked} source files are not valid UTF-8:")
        for path, exc in bad:
            print(f"  {path}: byte {exc.start}: {exc.reason}")
        print("\nConvert them with:  iconv -f CP1251 -t UTF-8 <file>")
        return 1

    print(f"all {checked} source files are valid UTF-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
