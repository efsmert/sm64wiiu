#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_LOG="$HOME/Library/Application Support/Cemu/log.txt"
DEFAULT_ELF="$ROOT_DIR/build/us_wiiu/sm64.us.elf"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<'EOF'
Usage: tools/wiiu_decode_cemu_crash.sh [cemu_log_path] [elf_path]

Decodes the most recent Cemu crash block (IP/LR/ReturnAddr values) into
function/file:line entries using powerpc-eabi-addr2line.
EOF
  exit 0
fi

LOG_PATH="${1:-$DEFAULT_LOG}"
ELF_PATH="${2:-$DEFAULT_ELF}"
ADDR2LINE_TOOL="${ADDR2LINE_TOOL:-/opt/devkitpro/devkitPPC/bin/powerpc-eabi-addr2line}"

if [[ ! -x "$ADDR2LINE_TOOL" ]]; then
  if command -v powerpc-eabi-addr2line >/dev/null 2>&1; then
    ADDR2LINE_TOOL="$(command -v powerpc-eabi-addr2line)"
  else
    echo "error: powerpc-eabi-addr2line not found. Set ADDR2LINE_TOOL or install devkitPPC." >&2
    exit 1
  fi
fi

if [[ ! -f "$LOG_PATH" ]]; then
  echo "error: Cemu log not found at: $LOG_PATH" >&2
  exit 1
fi

if [[ ! -f "$ELF_PATH" ]]; then
  echo "error: ELF not found at: $ELF_PATH" >&2
  exit 1
fi

CRASH_BLOCK="$(awk '
  /^Error: signal / {
    capture = 1
    block = ""
  }
  capture {
    block = block $0 "\n"
  }
  END {
    printf "%s", block
  }
' "$LOG_PATH")"

if [[ -z "$CRASH_BLOCK" ]]; then
  echo "No crash block (\"Error: signal\") found in: $LOG_PATH"
  exit 0
fi

ADDRESSES="$(printf "%s" "$CRASH_BLOCK" | awk '
  /IP 0x[0-9A-Fa-f]+/ {
    for (i = 1; i <= NF; i++) {
      if ($i == "IP" && (i + 1) <= NF && $(i + 1) ~ /^0x[0-9A-Fa-f]+$/) {
        print $(i + 1)
      }
      if ($i == "LR" && (i + 1) <= NF && $(i + 1) ~ /^0x[0-9A-Fa-f]+$/) {
        print $(i + 1)
      }
    }
  }
  /ReturnAddr [0-9A-Fa-f]{8}/ {
    for (i = 1; i <= NF; i++) {
      if ($i == "ReturnAddr" && (i + 1) <= NF && $(i + 1) ~ /^[0-9A-Fa-f]{8}$/) {
        print "0x" $(i + 1)
      }
    }
  }
' | awk '!seen[$0]++')"

if [[ -z "$ADDRESSES" ]]; then
  echo "Found crash block, but no IP/LR/ReturnAddr values were parsed."
  exit 1
fi

echo "Crash decode"
echo "  log: $LOG_PATH"
echo "  elf: $ELF_PATH"
echo

ADDR_ARRAY=()
while IFS= read -r addr; do
  if [[ -n "$addr" ]]; then
    ADDR_ARRAY+=("$addr")
  fi
done <<<"$ADDRESSES"

"$ADDR2LINE_TOOL" -a -p -f -C -e "$ELF_PATH" "${ADDR_ARRAY[@]}"

MAP_PATH="$(dirname "$ELF_PATH")/sm64.us.wiiu.map"
if [[ -f "$MAP_PATH" ]]; then
  echo
  echo "Map file: $MAP_PATH"
fi
