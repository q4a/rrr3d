#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 '/path/to/Motor Rock'" >&2
    exit 64
fi

source_root=$1
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "${script_dir}/.." && pwd)
target_root="${project_root}/resources/game-data"

if [[ ! -d "${source_root}/Data/GUI" ]]; then
    echo "Motor Rock Data/GUI was not found under: ${source_root}" >&2
    exit 66
fi

required_assets=(
    "Data/GUI/mainFrame.dds"
    "Data/GUI/topPanel5.png"
    "Data/GUI/bottomPanel5.png"
    "Data/GUI/mainItemSel5.png"
    "Data/english.txt"
    "Data/russian.txt"
    "game.xml"
    "db.xml"
)
for asset in "${required_assets[@]}"; do
    if [[ ! -f "${source_root}/${asset}" ]]; then
        echo "Required game resource is missing: ${source_root}/${asset}" >&2
        exit 66
    fi
done

if [[ -e "${target_root}/Data" ]]; then
    echo "Refusing to merge into an existing resource tree:" >&2
    echo "  ${target_root}/Data" >&2
    echo "Move it aside before importing a replacement set." >&2
    exit 73
fi

mkdir -p "${target_root}/Data"
rsync -a \
    --exclude '.DS_Store' \
    --exclude 'Thumbs.db' \
    "${source_root}/Data/" "${target_root}/Data/"

static_configs=(
    "game.xml"
    "db.xml"
    "garage.xml"
    "race.xml"
    "workshop.xml"
    "tournamet.xml"
    "achievment.xml"
)
for config in "${static_configs[@]}"; do
    cp -p "${source_root}/${config}" "${target_root}/${config}"
done

catalog_tmp="${target_root}/legacy-assets.catalog.tmp"
(
    cd "${target_root}"
    find Data -type f -print
    printf '%s\n' "${static_configs[@]}"
) | LC_ALL=C sort | while IFS= read -r asset; do
    bytes=$(stat -f '%z' "${target_root}/${asset}")
    printf '%s\t%s\n' "${bytes}" "${asset}"
done > "${catalog_tmp}"
mv "${catalog_tmp}" "${target_root}/legacy-assets.catalog"

asset_count=$(wc -l < "${target_root}/legacy-assets.catalog" | tr -d ' ')
asset_bytes=$(awk -F '\t' '{ total += $1 } END { printf "%.0f", total }' \
    "${target_root}/legacy-assets.catalog")

echo "Imported ${asset_count} files (${asset_bytes} bytes) into:"
echo "  ${target_root}"
echo "Windows executables, DLLs, logs, profiles, Thumbs.db, and .DS_Store were not copied."
