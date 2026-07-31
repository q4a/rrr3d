#!/usr/bin/env python3
"""Transcode the tree's source files from CP1251 to UTF-8.

The game was written in Russian Windows and its comments are CP1251. clang
rejects those bytes, every text tool on a UTF-8 system mangles them, and `sed`
fails outright with "illegal byte sequence".

This is a one-shot migration, kept in the tree because the transform has to be
reproducible and reviewable. It is safe to re-run: files that already decode as
UTF-8 are left alone.

The run aborts without writing anything unless *every* file round-trips back to
CP1251 byte-exactly. That is the whole guarantee -- this commit changes the
encoding of the bytes and nothing else about them.

Usage:
    tools/transcode-cp1251.py [--dry-run] [root]
"""

import argparse
import os
import sys

# Text we own. Binary resources (.bmp, .ico, .aps) and Windows resource scripts
# are deliberately excluded -- .rc files are already valid UTF-8 here, and the
# binaries are not text in any encoding.
SOURCE_EXTENSIONS = {".cpp", ".c", ".h", ".inl", ".ms", ".mm"}


def find_sources(root):
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in SOURCE_EXTENSIONS:
                yield os.path.join(dirpath, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default="src")
    parser.add_argument("--dry-run", action="store_true",
                        help="verify the round-trip but write nothing")
    args = parser.parse_args()

    pending = []   # (path, original_bytes, utf8_bytes)
    skipped = 0
    failures = []

    for path in find_sources(args.root):
        original = open(path, "rb").read()

        try:
            original.decode("utf-8")
            skipped += 1
            continue
        except UnicodeDecodeError:
            pass

        try:
            text = original.decode("cp1251")
        except UnicodeDecodeError as exc:
            failures.append(f"{path}: not decodable as CP1251: {exc}")
            continue

        utf8 = text.encode("utf-8")

        # The guarantee. If this does not hold, the file was not CP1251 and we
        # would be silently corrupting it.
        if utf8.decode("utf-8").encode("cp1251") != original:
            failures.append(f"{path}: does not round-trip back to CP1251")
            continue

        pending.append((path, original, utf8))

    if failures:
        print("refusing to transcode -- %d file(s) failed verification:"
              % len(failures), file=sys.stderr)
        for message in failures:
            print("  " + message, file=sys.stderr)
        return 1

    for path, _original, utf8 in pending:
        if not args.dry_run:
            with open(path, "wb") as handle:
                handle.write(utf8)
        print(("would transcode " if args.dry_run else "transcoded ") + path)

    print("\n%d transcoded, %d already UTF-8, all round-tripped byte-exactly"
          % (len(pending), skipped))
    return 0


if __name__ == "__main__":
    sys.exit(main())
