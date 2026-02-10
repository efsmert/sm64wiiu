#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"

if make -C sm64wiiu -j4; then
  make -C sm64wiiu wuhb
fi
