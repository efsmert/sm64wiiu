#!/usr/bin/env bash
set -euo pipefail

# Mirrors the agreed built-in Co-op DX mod payload into Wii U runtime/content paths.
# This keeps Wii U asset parity reproducible across sessions and machines.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DONOR_MODS="${REPO_ROOT}/sm64coopdx/mods"
WIIU_MODS="${REPO_ROOT}/sm64wiiu/mods"
WIIU_CONTENT_MODS="${REPO_ROOT}/sm64wiiu/content/mods"
WIIU_LANG="${REPO_ROOT}/sm64wiiu/lang"
WIIU_CONTENT_LANG="${REPO_ROOT}/sm64wiiu/content/lang"

if [[ ! -d "${DONOR_MODS}" ]]; then
    echo "error: donor mods path not found: ${DONOR_MODS}" >&2
    exit 1
fi

mkdir -p "${WIIU_MODS}" "${WIIU_CONTENT_MODS}"

# Keeps assumptions in SUMMARY.md/plan aligned with shipped built-ins.
declare -a BUILTIN_ITEMS=(
    "cheats.lua"
    "faster-swimming.lua"
    "personal-starcount-ex.lua"
    "day-night-cycle"
    "character-select-coop"
    "char-select-the-originals"
)

for item in "${BUILTIN_ITEMS[@]}"; do
    src="${DONOR_MODS}/${item}"
    dst_mods="${WIIU_MODS}/${item}"
    dst_content="${WIIU_CONTENT_MODS}/${item}"

    if [[ ! -e "${src}" ]]; then
        echo "warning: missing donor item, skipping: ${src}" >&2
        continue
    fi

    if [[ -d "${src}" ]]; then
        rsync -a --delete "${src}/" "${dst_mods}/"
        rsync -a --delete "${src}/" "${dst_content}/"
    else
        install -m 0644 "${src}" "${dst_mods}"
        install -m 0644 "${src}" "${dst_content}"
    fi
done

# Keep donor DJUI language files packaged in WUHB content so runtime DLANG
# lookups resolve under /vol/content/lang on Wii U/Cemu.
if [[ -d "${WIIU_LANG}" ]]; then
    mkdir -p "${WIIU_CONTENT_LANG}"
    rsync -a --delete "${WIIU_LANG}/" "${WIIU_CONTENT_LANG}/"
else
    echo "warning: missing Wii U language source folder, skipping: ${WIIU_LANG}" >&2
fi

echo "synced built-in mod payload from ${DONOR_MODS}"
