#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DEFAULT_LOG="${HOME}/Library/Application Support/Cemu/log.txt"
DEFAULT_ELF="${ROOT_DIR}/build/us_wiiu/sm64.us.elf"

LOG_PATH="${1:-${CEMU_LOG:-$DEFAULT_LOG}}"
ELF_PATH="${2:-${SM64_WIIU_ELF:-$DEFAULT_ELF}}"

NEAR_LINES="${CEMU_TRIAGE_NEAR:-200}"
CRASH_LINES="${CEMU_TRIAGE_CRASH_LINES:-260}"
PATTERN="${CEMU_TRIAGE_PATTERN:-Error: signal|IP 0x|LR 0x|ReturnAddr|Base:|RPXHash|TitleId|dynos:|lua:|geo_layout:|terrain:|find_ceil|find_floor|camera_course_processing|Object collisions|Reallocating object vertex data|flood_}"

usage() {
  cat <<'EOF'
Usage: tools/cemu_log_triage.sh [cemu_log_path] [elf_path]

Defaults:
  cemu_log_path: $CEMU_LOG or "$HOME/Library/Application Support/Cemu/log.txt"
  elf_path:      $SM64_WIIU_ELF or "build/us_wiiu/sm64.us.elf"

Env knobs:
  CEMU_TRIAGE_NEAR        lines before crash to include (default: 200)
  CEMU_TRIAGE_CRASH_LINES lines from crash block to print (default: 260)
  CEMU_TRIAGE_PATTERN     regex for "interesting lines" filter
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ ! -f "$LOG_PATH" ]]; then
  echo "error: Cemu log not found: $LOG_PATH" >&2
  exit 1
fi

have_rg=0
if command -v rg >/dev/null 2>&1; then
  have_rg=1
fi

last_crash_line_number() {
  if [[ "$have_rg" == "1" ]]; then
    rg -n "Error: signal" "$LOG_PATH" | tail -n 1 | cut -d: -f1 || true
  else
    grep -nE "Error: signal" "$LOG_PATH" | tail -n 1 | cut -d: -f1 || true
  fi
}

print_interesting() {
  local start="${1}"
  local end="${2}"
  if [[ "$have_rg" == "1" ]]; then
    sed -n "${start},${end}p" "$LOG_PATH" | rg -n "$PATTERN" || true
  else
    sed -n "${start},${end}p" "$LOG_PATH" | grep -nE "$PATTERN" || true
  fi
}

echo "Cemu log triage"
echo "  log: $LOG_PATH"
echo "  elf: $ELF_PATH"
echo "  env: CEMU_LOG=${CEMU_LOG:-<unset>} SM64_WIIU_ELF=${SM64_WIIU_ELF:-<unset>}"
echo

echo "== Tail game info (last ~1200 lines) =="
if [[ "$have_rg" == "1" ]]; then
  tail -n 1200 "$LOG_PATH" | rg -n "Base:|RPXHash:|TitleId:|Game:|Error: signal" | tail -n 60 || true
else
  tail -n 1200 "$LOG_PATH" | grep -nE "Base:|RPXHash:|TitleId:|Game:|Error: signal" | tail -n 60 || true
fi
echo

crash_line="$(last_crash_line_number)"
if [[ -z "$crash_line" ]]; then
  echo "No crash block (\"Error: signal\") found."
  echo
  echo "== Interesting lines (last 500 lines) =="
  if [[ "$have_rg" == "1" ]]; then
    tail -n 500 "$LOG_PATH" | rg -n "$PATTERN" || true
  else
    tail -n 500 "$LOG_PATH" | grep -nE "$PATTERN" || true
  fi
  exit 0
fi

start_line=$(( crash_line - NEAR_LINES ))
if (( start_line < 1 )); then start_line=1; fi
end_line=$(( crash_line + CRASH_LINES ))

echo "== Context (${NEAR_LINES} lines before crash) =="
sed -n "${start_line},${crash_line}p" "$LOG_PATH" | tail -n $(( NEAR_LINES + 1 )) || true
echo

echo "== Crash block (first ${CRASH_LINES} lines) =="
tail -n +"$crash_line" "$LOG_PATH" | head -n "$CRASH_LINES" || true
echo

echo "== Interesting lines (context + crash window) =="
print_interesting "$start_line" "$end_line"
echo

if [[ -f "$ELF_PATH" && -f "${ROOT_DIR}/tools/wiiu_decode_cemu_crash.sh" ]]; then
  echo "== Crash decode (addr2line) =="
  bash "${ROOT_DIR}/tools/wiiu_decode_cemu_crash.sh" "$LOG_PATH" "$ELF_PATH" || true
else
  echo "Crash decode skipped (missing ELF or decoder script)."
  echo "  decoder: ${ROOT_DIR}/tools/wiiu_decode_cemu_crash.sh"
  echo "  elf:     $ELF_PATH"
fi

