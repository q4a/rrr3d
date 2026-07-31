#!/usr/bin/env python3
"""Fail the build on #include paths that only work on Windows.

Two faults, both invisible on MSVC and on a case-insensitive filesystem, and
both fatal on Linux or a case-sensitive volume:

  * backslash separators -- `#include "game\\TraceGfx.h"`. MSVC accepts them;
    clang and gcc do not.
  * the wrong case -- `#include "px/PhysX.h"` for a file named `Physx.h`.

Only includes that resolve to a file inside src/ are checked. Anything that does
not resolve here belongs to a system or extern header and is none of our
business.

Usage:
    tools/check-includes.py [root]

Exits non-zero, and prints the correct spelling, on the first fault found.
"""

import argparse
import os
import re
import sys

SOURCE_EXTENSIONS = {".cpp", ".c", ".h", ".inl", ".mm"}

# Third-party trees whose include style is not ours to police. Paths are
# relative to the root being checked.
VENDORED = ()

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*(["<])([^">]*)([">])')


def find_sources(root):
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root)
        if any(rel == v or rel.startswith(v + os.sep) for v in VENDORED):
            dirnames[:] = []
            continue
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in SOURCE_EXTENSIONS:
                yield os.path.join(dirpath, name)


def find_include_roots(root):
    """The `header`/`include` directories each target puts on its own path.

    Derived from the tree rather than listed, so a new library does not silently
    escape the check. The including file's own directory is added per-file.
    """
    roots = []
    for entry in sorted(os.listdir(root)):
        target = os.path.join(root, entry)
        if not os.path.isdir(target):
            continue
        roots.append(target)
        for sub in ("header", "include"):
            candidate = os.path.join(target, sub)
            if os.path.isdir(candidate):
                roots.append(candidate)
    return roots


def resolve(base, relative):
    """Return the on-disk path for `relative` under `base`, matching case-insensitively.

    Returns (actual_path, exact) -- `exact` is False when the file exists but is
    spelled differently on disk.
    """
    current = base
    exact = True
    for part in relative.split("/"):
        if not part or part == ".":
            continue
        if part == "..":
            current = os.path.dirname(current)
            continue
        try:
            entries = os.listdir(current)
        except (NotADirectoryError, FileNotFoundError):
            return None, False
        if part in entries:
            current = os.path.join(current, part)
            continue
        lowered = {e.lower(): e for e in entries}
        if part.lower() in lowered:
            current = os.path.join(current, lowered[part.lower()])
            exact = False
            continue
        return None, False
    return current, exact


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default="src")
    args = parser.parse_args()

    include_roots = find_include_roots(args.root)
    errors = []

    for path in find_sources(args.root):
        with open(path, "r", encoding="utf-8", newline="") as handle:
            for lineno, line in enumerate(handle, 1):
                match = INCLUDE_RE.match(line)
                if not match:
                    continue
                _opener, included, _closer = match.groups()

                if "\\" in included:
                    errors.append(
                        "%s:%d: backslash in #include -- use '%s'"
                        % (path, lineno, re.sub(r"\\+", "/", included)))
                    continue

                for base in [os.path.dirname(path)] + include_roots:
                    actual, exact = resolve(base, included)
                    if actual is None or not os.path.isfile(actual):
                        continue
                    if not exact:
                        correct = os.path.relpath(actual, base).replace(os.sep, "/")
                        errors.append(
                            "%s:%d: wrong case in #include \"%s\" -- the file is '%s'"
                            % (path, lineno, included, correct))
                    break

    for message in errors:
        print(message, file=sys.stderr)

    if errors:
        print("\n%d bad #include(s)" % len(errors), file=sys.stderr)
        return 1

    print("include paths OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
