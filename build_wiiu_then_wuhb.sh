#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"

make -C sm64wiiu -j4
make -C sm64wiiu wuhb
