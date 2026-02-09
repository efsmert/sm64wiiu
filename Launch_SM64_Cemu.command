#!/usr/bin/env bash
set -euo pipefail

DEFAULT_WUHB_PATH="/Users/samiareski/sm64wiiu/sm64wiiu/build/us_wiiu/sm64.us.wuhb"
WUHB_PATH="${1:-$DEFAULT_WUHB_PATH}"

if [[ ! -f "$WUHB_PATH" ]]; then
  echo "Error: WUHB file not found: $WUHB_PATH" >&2
  exit 1
fi

launch_cemu() {
  if command -v cemu >/dev/null 2>&1; then
    cemu "-g" "$WUHB_PATH" >/dev/null 2>&1 &
  elif [[ -x "/Applications/Cemu.app/Contents/MacOS/Cemu" ]]; then
    "/Applications/Cemu.app/Contents/MacOS/Cemu" "-g" "$WUHB_PATH" >/dev/null 2>&1 &
  else
    open -a Cemu --args "-g" "$WUHB_PATH"
  fi
}

focus_cemu() {
  local i
  for i in {1..50}; do
    if osascript -e 'tell application "System Events" to (name of processes) contains "Cemu"' 2>/dev/null | grep -q "true"; then
      break
    fi
    sleep 0.1
  done

  osascript -e 'tell application "Cemu" to activate' >/dev/null 2>&1 || true
}

wait_for_cemu_exit() {
  local i
  for i in {1..600}; do
    if osascript -e 'tell application "System Events" to (name of processes) contains "Cemu"' 2>/dev/null | grep -q "true"; then
      break
    fi
    sleep 0.1
  done

  while osascript -e 'tell application "System Events" to (name of processes) contains "Cemu"' 2>/dev/null | grep -q "true"; do
    sleep 0.5
  done
}

schedule_terminal_window_close() {
  if [[ "${TERM_PROGRAM:-}" == "Apple_Terminal" ]]; then
    osascript <<'APPLESCRIPT' >/dev/null 2>&1 &
delay 0.35
tell application "Terminal"
  if (count of windows) > 0 then
    close front window saving no
  end if
end tell
APPLESCRIPT
  fi
}

launch_cemu
focus_cemu
wait_for_cemu_exit
schedule_terminal_window_close
