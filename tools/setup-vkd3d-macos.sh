#!/bin/bash
#
# Build libvkd3d-shader (arm64 static library) into extern/vkd3d for the macOS port.
#
# Why this is needed:
#   The game's 23 .fx files are compiled at runtime by D3DXCreateEffect, with a
#   different set of #define macros each time -- see Shader::MacroBlock. That
#   needs a real HLSL compiler in the process, targeting Direct3D 9 shader
#   model 3, which is what DXVK's D3D9 front-end consumes.
#
# Why vkd3d-shader:
#   - It compiles HLSL to D3D bytecode natively. Measured on this game's
#     shaders: all 48 entry points across the 20 .fx files that declare
#     techniques compile to vs_3_0/ps_3_0.
#   - Its d3dbc writer emits the standard CTAB constant table, so parameter
#     names can be mapped to shader registers the same way D3DX does it.
#   - The alternative was Microsoft's d3dcompiler_43.dll under Wine, which is
#     what tools/shader-pipeline used to validate the path. That is a fine
#     investigation tool and cannot ship: it is neither native nor ours.
#
# DXC and glslang were the other candidates. Both target DXBC or SPIR-V; neither
# emits shader model 3, which is the only thing a D3D9 device accepts.
#
# extern/ is gitignored, so nothing here is a checked-in patch. No vkd3d source
# is modified -- this is build configuration only.
#
# Usage:  tools/setup-vkd3d-macos.sh
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VKD3D_DIR="$REPO_ROOT/extern/vkd3d"
VKD3D_URL="https://gitlab.winehq.org/wine/vkd3d.git"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script is for macOS" >&2
    exit 1
fi

# vkd3d's configure wants all of these. bison must be 3.x -- the bison macOS
# ships is 2.3, which cannot build vkd3d's HLSL grammar. widl comes from
# mingw-w64 and generates headers from vkd3d's .idl files.
#
# vulkan-headers, spirv-headers and molten-vk are needed only to get through
# configure, which has no option to build the shader library alone. Nothing in
# what we link against calls Vulkan.
#
# spirv-headers supplies three of configure's checks at once -- spirv.h,
# GLSL.std.450.h and the "SPIR-V headers are too old" version test. It is a
# separate formula from vulkan-headers, which is easy to miss because a machine
# that has ever built anything Vulkan-adjacent already has it.
MISSING=()
for pkg in bison automake libtool mingw-w64 vulkan-headers spirv-headers molten-vk; do
    brew list "$pkg" >/dev/null 2>&1 || MISSING+=("$pkg")
done
if (( ${#MISSING[@]} )); then
    echo "==> installing build dependencies: ${MISSING[*]}"
    brew install "${MISSING[@]}"
fi

# vkd3d's configure hard-fails without the Perl JSON module: it generates
# include/private/spirv_grammar.h from spirv.core.grammar.json, which is part of
# vkd3d-shader itself and not of the demos. macOS ships perl but not this
# module, and JSON::PP -- which IS core -- is not what configure looks for.
#
# Missed until CI ran, because the machine this was developed on happened to
# have it already. That is the whole reason the macOS job exists.
if ! perl -MJSON -e '1' >/dev/null 2>&1; then
    echo "==> installing the Perl JSON module"
    brew list cpanminus >/dev/null 2>&1 || brew install cpanminus
    cpanm --notest --local-lib="$HOME/perl5" JSON

    # PERL5LIB directly, rather than through local::lib -- which is itself a
    # CPAN module and is not necessarily installed, so the eval that was here
    # first failed silently and left @INC untouched. The module installed fine
    # and the check right below still said it was missing.
    export PERL5LIB="$HOME/perl5/lib/perl5${PERL5LIB:+:$PERL5LIB}"
fi

if ! perl -MJSON -e '1' >/dev/null 2>&1; then
    echo "error: the Perl JSON module is still missing; vkd3d cannot configure" >&2
    exit 1
fi

# Headers, checked by presence rather than by formula.
#
# `brew list <formula>` answers "is it installed", which is not the question --
# a formula can be installed and unlinked, in which case nothing is on the
# include path and configure fails exactly as it does on a machine that never
# had it. Same lesson as the Perl module above: check the capability, not the
# package.
BREW_INCLUDE="$(brew --prefix)/include"
for header in vulkan/vulkan.h spirv/unified1/spirv.h spirv/unified1/GLSL.std.450.h; do
    if [[ ! -f "$BREW_INCLUDE/$header" ]]; then
        echo "error: $BREW_INCLUDE/$header is missing." >&2
        echo "       vkd3d's configure needs it. If the formula is installed," >&2
        echo "       it may only need linking: brew link vulkan-headers spirv-headers" >&2
        exit 1
    fi
done

BISON_BIN="$(brew --prefix bison)/bin"
BREW_PREFIX="$(brew --prefix)"
WIDL="$BREW_PREFIX/bin/x86_64-w64-mingw32-widl"

if [[ ! -x "$WIDL" ]]; then
    echo "error: widl not found at $WIDL" >&2
    exit 1
fi

if [[ ! -d "$VKD3D_DIR" ]]; then
    echo "==> cloning vkd3d"
    git clone --depth 1 "$VKD3D_URL" "$VKD3D_DIR"
else
    echo "==> $VKD3D_DIR already present, skipping clone"
fi

export PATH="$BISON_BIN:$PATH"

if [[ ! -f "$VKD3D_DIR/configure" ]]; then
    echo "==> generating the build system"
    ( cd "$VKD3D_DIR" && ./autogen.sh >/dev/null 2>&1 )
fi

echo "==> configuring"
mkdir -p "$VKD3D_DIR/build"
(
    cd "$VKD3D_DIR/build"
    ../configure \
        --disable-demos \
        --disable-tests \
        --disable-doxygen-doc \
        --without-ncurses \
        WIDL="$WIDL" \
        CPPFLAGS="-I$BREW_PREFIX/include" \
        LDFLAGS="-L$BREW_PREFIX/lib" \
        >/dev/null
)

echo "==> building"
make -C "$VKD3D_DIR/build" -j"$(sysctl -n hw.ncpu)" >/dev/null 2>&1 || true

LIB="$VKD3D_DIR/build/.libs/libvkd3d-shader.a"
if [[ ! -f "$LIB" ]]; then
    echo "error: libvkd3d-shader.a was not produced" >&2
    exit 1
fi

echo "==> done."
printf "      %-32s %s\n" "$(basename "$LIB")" "$(lipo -archs "$LIB")"
echo "      headers: $VKD3D_DIR/include"
