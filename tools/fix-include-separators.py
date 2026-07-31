#!/usr/bin/env python3
"""Rewrite backslash separators in #include paths to forward slashes.

MSVC accepts `#include "game\TraceGfx.h"`; clang and gcc do not. Forward
slashes work on every compiler including MSVC, so this is a one-way fix.

Both spellings appear in this tree -- `"snd\Audio.h"`, where the backslash is an
invalid escape the compiler lets through, and `"snd\\Audio.h"`, where it is a
real escaped backslash. Both become `/`.

Only the path between the delimiters is touched, so backslashes anywhere else on
the line are left alone. tools/check-includes.py is the CI gate that keeps them
from coming back.

Usage:
    tools/fix-include-separators.py [--dry-run] [root]
"""

import argparse
import os
import re
import sys

SOURCE_EXTENSIONS = {".cpp", ".c", ".h", ".inl", ".mm"}

# The delimiters are captured so the quoted and angled forms are rewritten by
# one pattern without either being able to match across the other's closing mark.
INCLUDE_RE = re.compile(r'(^\s*#\s*include\s*)(["<])([^">]*)([">])')


def find_sources(root):
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in SOURCE_EXTENSIONS:
                yield os.path.join(dirpath, name)


def fix_line(line):
    match = INCLUDE_RE.match(line)
    if not match:
        return line, 0
    prefix, opener, path, closer = match.groups()
    if "\\" not in path:
        return line, 0
    fixed = re.sub(r"\\+", "/", path)
    return (prefix + opener + fixed + closer + line[match.end():], 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default="src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    total_files = 0
    total_includes = 0

    for path in find_sources(args.root):
        # Text mode with newline='' so CRLF endings survive untouched.
        with open(path, "r", encoding="utf-8", newline="") as handle:
            lines = handle.readlines()

        fixed_lines = []
        changed = 0
        for line in lines:
            fixed, n = fix_line(line)
            fixed_lines.append(fixed)
            changed += n

        if not changed:
            continue

        total_files += 1
        total_includes += changed
        print("%-4d %s" % (changed, path))

        if not args.dry_run:
            with open(path, "w", encoding="utf-8", newline="") as handle:
                handle.writelines(fixed_lines)

    print("\n%d include(s) across %d file(s)%s"
          % (total_includes, total_files, " (dry run)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
