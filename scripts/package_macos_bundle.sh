#!/bin/sh

set -eu

fail()
{
    printf 'bundle packaging failed: %s\n' "$1" >&2
    exit 1
}

if [ "$#" -ne 2 ]; then
    fail "usage: $0 /path/to/RRR3d.app /path/to/output.zip"
fi

source_app=$1
archive_name=$2
script_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

[ -d "$source_app" ] || fail "application bundle is missing: $source_app"
archive_directory=$(dirname "$archive_name")
[ -d "$archive_directory" ] ||
    fail "archive output directory is missing: $archive_directory"
archive_directory=$(CDPATH= cd -- "$archive_directory" && pwd)
archive_name="$archive_directory/$(basename "$archive_name")"

case "$archive_name" in
    *.zip) ;;
    *) fail "transport archive must use the .zip extension" ;;
esac

"$script_directory/verify_macos_bundle.sh" "$source_app"

packaging_root=$(/usr/bin/mktemp -d \
    "${TMPDIR:-/tmp}/rrr3d-bundle-package.XXXXXX")
temporary_archive="$archive_name.tmp.$$"
cleanup()
{
    if [ -n "${packaging_root:-}" ] && [ -d "$packaging_root" ]; then
        /usr/bin/find "$packaging_root" -depth -delete
    fi
    if [ -n "${temporary_archive:-}" ] && [ -f "$temporary_archive" ]; then
        /usr/bin/find "$temporary_archive" -delete
    fi
}
trap cleanup EXIT HUP INT TERM

clean_app="$packaging_root/RRR3d.app"
/usr/bin/ditto --noextattr --norsrc --noqtn --noacl \
    "$source_app" "$clean_app"
/usr/bin/xattr -cr "$clean_app"
/usr/bin/codesign --verify --deep --strict "$clean_app" ||
    fail "clean transport copy has an invalid signature"

/usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl \
    "$clean_app" "$temporary_archive"
/usr/bin/unzip -tq "$temporary_archive" >/dev/null ||
    fail "generated ZIP archive is invalid"
/bin/mv -f "$temporary_archive" "$archive_name"

printf 'Created signed macOS transport archive: %s\n' "$archive_name"
