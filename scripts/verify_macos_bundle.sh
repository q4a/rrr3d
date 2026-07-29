#!/bin/sh

set -eu

fail()
{
    printf 'bundle verification failed: %s\n' "$1" >&2
    exit 1
}

if [ "$#" -ne 1 ]; then
    fail "usage: $0 /path/to/RRR3d.app"
fi

source_app=$1
[ -d "$source_app" ] || fail "application bundle is missing: $source_app"

# File-provider-backed folders may attach Finder metadata immediately after a
# build. Validate an xattr-free transport copy so the signature check measures
# the bundle contents rather than metadata owned by the containing filesystem.
verification_root=$(/usr/bin/mktemp -d \
    "${TMPDIR:-/tmp}/rrr3d-bundle-verify.XXXXXX")
cleanup()
{
    if [ -n "${verification_root:-}" ] && [ -d "$verification_root" ]; then
        /usr/bin/find "$verification_root" -depth -delete
    fi
}
trap cleanup EXIT HUP INT TERM

app="$verification_root/RRR3d.app"
/usr/bin/ditto --noextattr --norsrc --noqtn --noacl "$source_app" "$app"
/usr/bin/xattr -cr "$app"

contents="$app/Contents"
binary="$contents/MacOS/RRR3d"
plist="$contents/Info.plist"
resources="$contents/Resources"
frameworks="$contents/Frameworks"

[ -x "$binary" ] || fail "arm64 executable is missing: $binary"
[ -f "$plist" ] || fail "Info.plist is missing"
[ -d "$frameworks" ] || fail "Contents/Frameworks is missing"
[ -f "$resources/RRR3d.icns" ] || fail "application icon is missing"
[ -f "$resources/Licenses/THIRD_PARTY_NOTICES.md" ] ||
    fail "project and third-party licence notices are missing"
[ -f "$resources/game-data/legacy-assets.catalog" ] ||
    fail "game-data catalog is missing"
[ -f "$resources/game-data/manifest.cfg" ] ||
    fail "game-data manifest is missing"

/usr/bin/plutil -lint "$plist" >/dev/null || fail "Info.plist is invalid"

plist_read()
{
    /usr/libexec/PlistBuddy -c "Print :$1" "$plist"
}

[ "$(plist_read CFBundleIdentifier)" = "org.rrr3d.motorrock" ] ||
    fail "unexpected bundle identifier"
[ "$(plist_read CFBundleExecutable)" = "RRR3d" ] ||
    fail "unexpected executable name"
[ "$(plist_read LSMinimumSystemVersion)" = "13.0" ] ||
    fail "unexpected minimum macOS version"
[ "$(plist_read NSHighResolutionCapable)" = "true" ] ||
    fail "high-resolution support is disabled"

architectures=$(/usr/bin/lipo -archs "$binary")
[ "$architectures" = "arm64" ] ||
    fail "expected arm64-only executable, found: $architectures"

minimum=$(/usr/bin/vtool -show-build "$binary" |
    /usr/bin/awk '/^[[:space:]]*minos / { print $2; exit }')
[ "$minimum" = "13.0" ] ||
    fail "expected LC_BUILD_VERSION minos 13.0, found: $minimum"

dependencies=$(/usr/bin/otool -L "$binary" | /usr/bin/tail -n +2 |
    /usr/bin/awk '{ print $1 }')
if printf '%s\n' "$dependencies" |
    /usr/bin/grep -E '(^/opt/homebrew|^/usr/local|/build/|\\.dll$|\\.lib$)' >/dev/null
then
    fail "binary contains a local, Homebrew, or Windows dependency"
fi
if printf '%s\n' "$dependencies" |
    /usr/bin/grep -Ev '^(/System/Library/|/usr/lib/|@rpath/|@loader_path/|@executable_path/)' >/dev/null
then
    fail "binary contains an external absolute dependency"
fi

/usr/bin/codesign --verify --deep --strict "$app" ||
    fail "code signature is invalid"

asset_count=$(/usr/bin/find "$resources/game-data" -type f | /usr/bin/wc -l |
    /usr/bin/tr -d ' ')
catalog_count=$(/usr/bin/wc -l < \
    "$resources/game-data/legacy-assets.catalog" | /usr/bin/tr -d ' ')
expected_asset_count=$((catalog_count + 4))
[ "$catalog_count" -eq 1196 ] ||
    fail "unexpected game-data catalog size: $catalog_count"
[ "$asset_count" -eq "$expected_asset_count" ] ||
    fail "game-data must contain exactly $expected_asset_count cataloged files; found $asset_count"

printf 'RRR3d.app verified: arm64, minos %s, %s assets, signed, autonomous dependencies\n' \
    "$minimum" "$asset_count"
