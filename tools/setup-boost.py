#!/usr/bin/env python3
"""Fetch the pinned Boost headers into extern/boost169.

Why pinned, and why this version:

The bundled Boost is 1.59 (2015). It predates C++17 and uses std::auto_ptr,
which C++17 removed, so it cannot be compiled at all now.

The system Boost is far too new in the other direction: boost::asio::io_service
and basic_socket_acceptor::get_io_service() were both removed in 1.70, and
NetLib uses both -- io_service as a member type, get_io_service() in three
places. Building against a current Boost would mean rewriting the networking
code to io_context and get_executor(), which is a refactor with no way to test
it short of a live multiplayer session.

1.69 is the last release that has io_service and get_io_service() *and* handles
C++17. So NetLib compiles unchanged, and both platforms use the identical
version rather than one each.

Headers only: NetLib uses Asio, bind and system, and all three are header-only
in 1.69. (boost/thread/thread.hpp was included by stdafx.h but no boost::thread
symbol appears anywhere in the tree, so that include was dropped rather than
dragging in a library that has to be built.)

extern/ is gitignored, which is why this is a script rather than a checkout.

Usage:
    tools/setup-boost.py [--force]
"""

import argparse
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

VERSION = "1.69.0"
UNDERSCORED = VERSION.replace(".", "_")
URL = ("https://archives.boost.io/release/%s/source/boost_%s.tar.gz"
       % (VERSION, UNDERSCORED))

DEST = Path("extern/boost169")
INCLUDE = DEST / "include"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true",
                        help="re-extract even if the headers are already there")
    args = parser.parse_args()

    if INCLUDE.joinpath("boost", "version.hpp").exists() and not args.force:
        print("%s already present; pass --force to re-extract" % INCLUDE)
        return 0

    INCLUDE.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        archive = Path(tmp) / ("boost_%s.tar.gz" % UNDERSCORED)
        print("==> downloading Boost %s" % VERSION)
        if subprocess.run(["curl", "-fL", "--progress-bar", "-o", str(archive), URL]).returncode:
            raise SystemExit("failed to download %s" % URL)

        print("==> extracting headers")
        prefix = "boost_%s/boost/" % UNDERSCORED
        with tarfile.open(archive) as tar:
            members = [m for m in tar.getmembers() if m.name.startswith(prefix)]
            if not members:
                raise SystemExit("archive does not contain %s" % prefix)
            for member in members:
                member.name = member.name[len("boost_%s/" % UNDERSCORED):]
            tar.extractall(INCLUDE, members=members, filter="data")

    version_hpp = INCLUDE / "boost" / "version.hpp"
    if not version_hpp.exists():
        raise SystemExit("extraction produced no boost/version.hpp")

    # Fail loudly rather than leaving a subtly wrong tree behind.
    text = version_hpp.read_text(encoding="utf-8")
    if "#define BOOST_LIB_VERSION \"1_69\"" not in text:
        raise SystemExit("extracted headers are not Boost 1.69")

    acceptor = INCLUDE / "boost" / "asio" / "basic_socket_acceptor.hpp"
    if "get_io_service" not in acceptor.read_text(encoding="utf-8"):
        raise SystemExit("get_io_service() is missing -- this Boost is too new")

    print("==> Boost %s headers in %s" % (VERSION, INCLUDE))
    return 0


if __name__ == "__main__":
    sys.exit(main())
