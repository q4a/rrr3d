#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "${script_dir}/.." && pwd)

dxvk_commit=c3dd74be6baec53786d4e064a572185b70347a17
dxvk_url=https://github.com/doitsujin/dxvk.git
dxvk_root=${1:-"${repo_dir}/build/dxvk-macos"}
dxvk_source=${dxvk_root}/source
dxvk_build=${dxvk_root}/build
dxvk_install=${dxvk_root}/install
dxvk_patch=${repo_dir}/cmake/patches/dxvk-2.7.1-macos.patch

for tool in git meson ninja glslang pkg-config; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Missing build tool: ${tool}" >&2
        exit 1
    fi
done

mkdir -p "${dxvk_root}"

if [ ! -d "${dxvk_source}/.git" ]; then
    git clone --filter=blob:none "${dxvk_url}" "${dxvk_source}"
fi

current_origin=$(git -C "${dxvk_source}" remote get-url origin)
if [ "${current_origin}" != "${dxvk_url}" ]; then
    echo "Unexpected DXVK origin: ${current_origin}" >&2
    exit 1
fi

git -C "${dxvk_source}" fetch --depth=1 origin "${dxvk_commit}"
git -C "${dxvk_source}" checkout --detach "${dxvk_commit}"
git -C "${dxvk_source}" submodule update --init --recursive
git -C "${dxvk_source}" submodule foreach --recursive \
    'git reset --hard HEAD'

if git -C "${dxvk_source}" apply --check "${dxvk_patch}" 2>/dev/null; then
    git -C "${dxvk_source}" apply "${dxvk_patch}"
elif ! git -C "${dxvk_source}" apply --reverse --check \
        "${dxvk_patch}" 2>/dev/null; then
    echo "DXVK source has changes that conflict with the macOS patch" >&2
    exit 1
fi

setup_mode=setup
if [ -f "${dxvk_build}/build.ninja" ]; then
    setup_mode=setup
    set -- --reconfigure
elif [ -f "${dxvk_build}/meson-private/coredata.dat" ]; then
    setup_mode=setup
    set -- --wipe
else
    set --
fi

MACOSX_DEPLOYMENT_TARGET=13.0 meson "${setup_mode}" "$@" \
    "${dxvk_build}" "${dxvk_source}" \
    --buildtype=debugoptimized \
    --prefix="${dxvk_install}" \
    --libdir=lib \
    -Denable_d3d8=false \
    -Denable_d3d9=true \
    -Denable_d3d10=false \
    -Denable_d3d11=false \
    -Denable_dxgi=false \
    -Dnative_sdl3=enabled \
    -Dnative_sdl2=disabled \
    -Dnative_glfw=disabled

MACOSX_DEPLOYMENT_TARGET=13.0 meson compile -C "${dxvk_build}"
meson install -C "${dxvk_build}"

echo "DXVK Native ${dxvk_commit} installed in ${dxvk_install}"
echo "Configure RRR3D with -DRRR3D_DXVK_NATIVE_ROOT=${dxvk_install}"
