#!/usr/bin/env bash
set -euo pipefail

# Reproducible local gate for Wii U artifacts (RPX + WUHB + Cemu sync copy).
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPPC="${DEVKITPPC:-$DEVKITPRO/devkitPPC}"
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"

make -C "$(dirname "$0")" -j4
make -C "$(dirname "$0")" sync-builtin-mod-assets
make -C "$(dirname "$0")" wuhb
make -C "$(dirname "$0")" cemu-sync

ls -la \
  "$(dirname "$0")/build/us_wiiu/sm64.us.rpx" \
  "$(dirname "$0")/build/us_wiiu/sm64.us.wuhb" \
  "$(dirname "$0")/sm64wiiu_cemu/code/sm64.us.rpx"
