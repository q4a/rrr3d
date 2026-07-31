#!/usr/bin/env python3
"""Turn in-class `static const` members with initialisers into `static constexpr`.

The tree declares 113 class members as

    static const int cFoo = 3;

and never defines them out of line. That is an MSVC extension: the standard says
an odr-used static data member needs a definition, and every other compiler
fails at link time with an undefined symbol. In C++17 `static constexpr` members
are implicitly inline, so the declaration *is* the definition and the problem
goes away without adding 113 definitions to .cpp files.

Only lines whose type is a single token are touched, which is every case in this
tree (int, unsigned, DWORD, wchar_t, and two enums). Members declared without an
initialiser are left alone -- those already have out-of-line definitions, and
converting them would be a redefinition.

Verified before use: no name is both initialised in-class and defined out of
line, so this cannot collide with an existing definition.

Usage:
    tools/static-const-to-constexpr.py [--dry-run] [root]
"""

import argparse
import os
import re
import sys

HEADER_EXTENSIONS = {".h", ".inl"}

# Leading whitespace is required: it is what distinguishes a class member from a
# namespace-scope `static const`, which means internal linkage and must not
# change. A single-token type keeps arrays and templates out.
MEMBER_RE = re.compile(
    r"^(\s+)static const ([A-Za-z_][A-Za-z0-9_:]*\s+[A-Za-z_][A-Za-z0-9_]*\s*=)")


def find_headers(root):
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in HEADER_EXTENSIONS:
                yield os.path.join(dirpath, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default="src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    total_files = 0
    total_members = 0

    for path in find_headers(args.root):
        with open(path, "r", encoding="utf-8", newline="") as handle:
            lines = handle.readlines()

        changed = 0
        for index, line in enumerate(lines):
            fixed, n = MEMBER_RE.subn(r"\1static constexpr \2", line)
            if n:
                lines[index] = fixed
                changed += n

        if not changed:
            continue

        total_files += 1
        total_members += changed
        print("%-4d %s" % (changed, path))

        if not args.dry_run:
            with open(path, "w", encoding="utf-8", newline="") as handle:
                handle.writelines(lines)

    print("\n%d member(s) across %d file(s)%s"
          % (total_members, total_files, " (dry run)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
