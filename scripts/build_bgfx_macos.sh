#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "${script_dir}/.." && pwd)

bgfx_commit=65551a7db19240b4d105f09e665190d196243d92
bx_commit=5a2b876258ab5843d5e1dfde695b127baf9e354a
bimg_commit=da38bface6384cdd7f69733b08fb58e57a63cffa

bgfx_root=${1:-"${repo_dir}/build/bgfx-macos"}
bgfx_source=${bgfx_root}/bgfx
bx_source=${bgfx_root}/bx
bimg_source=${bgfx_root}/bimg

for tool in git make xcrun; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Missing build tool: ${tool}" >&2
        exit 1
    fi
done

clone_pinned()
{
    name=$1
    url=$2
    commit=$3
    destination=$4

    if [ ! -d "${destination}/.git" ]; then
        git clone --filter=blob:none "${url}" "${destination}"
    fi

    origin=$(git -C "${destination}" remote get-url origin)
    if [ "${origin}" != "${url}" ]; then
        echo "Unexpected ${name} origin: ${origin}" >&2
        exit 1
    fi

    if [ -n "$(git -C "${destination}" status --porcelain)" ]; then
        echo "Refusing to replace modified ${name} checkout: ${destination}" >&2
        exit 1
    fi

    git -C "${destination}" fetch --depth=1 origin "${commit}"
    git -C "${destination}" checkout --detach "${commit}"
}

mkdir -p "${bgfx_root}"
clone_pinned bgfx https://github.com/bkaradzic/bgfx.git \
    "${bgfx_commit}" "${bgfx_source}"
clone_pinned bx https://github.com/bkaradzic/bx.git \
    "${bx_commit}" "${bx_source}"
clone_pinned bimg https://github.com/bkaradzic/bimg.git \
    "${bimg_commit}" "${bimg_source}"

generator=${bx_source}/tools/bin/darwin/genie
if [ ! -x "${generator}" ]; then
    echo "bgfx project generator is missing: ${generator}" >&2
    exit 1
fi

(
    cd "${bgfx_source}"
    BX_DIR="${bx_source}" BIMG_DIR="${bimg_source}" \
    MACOSX_DEPLOYMENT_TARGET=13.0 \
        "${generator}" --with-tools --with-macos=13.0 \
        --gcc=osx-arm64 gmake
)

jobs=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
MACOSX_DEPLOYMENT_TARGET=13.0 make \
    -C "${bgfx_source}/.build/projects/gmake-osx-arm64" \
    bgfx bimg bimg_decode shaderc config=release -j"${jobs}"

echo "Pinned bgfx/Metal toolchain built in ${bgfx_root}"
echo "Configure with -DRRR3D_BGFX_ROOT=${bgfx_root}"
