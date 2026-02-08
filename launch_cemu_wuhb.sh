#!/usr/bin/env bash
set -euo pipefail

DEFAULT_WUHB_PATH="/Users/samiareski/sm64wiiu/sm64wiiu/build/us_wiiu/sm64.us.wuhb"
WUHB_PATH="${1:-$DEFAULT_WUHB_PATH}"

if [[ ! -f "$WUHB_PATH" ]]; then
  echo "Error: WUHB file not found: $WUHB_PATH" >&2
  exit 1
fi

if command -v cemu >/dev/null 2>&1; then
  exec cemu "$WUHB_PATH"
elif [[ -x "/Applications/Cemu.app/Contents/MacOS/Cemu" ]]; then
  exec "/Applications/Cemu.app/Contents/MacOS/Cemu" "-g" "$WUHB_PATH"
else
  exec open -a Cemu --args "$WUHB_PATH"
fi
