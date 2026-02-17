# SM64 Wii U Port Project

## Overview
This repo contains a native Wii U port of SM64 (`sm64wiiu/`) and a desktop donor/reference project (`sm64coopdx/`).

Overall goal: port the enhanced feature set from `sm64coopdx` into `sm64wiiu` while keeping the Wii U build bootable/playable. Primary deliverables are Wii U binaries: `.rpx` and `.wuhb`.

## Repo Layout
- `sm64wiiu/`: the Wii U-target project (this is what ships/boots on console or in Cemu).
- `sm64coopdx/`: donor/reference project for enhanced features (desktop-focused implementation to port from).
- `flood-expanded-release/`: Flood mod data stored in-repo for convenient local testing (mirrors what would normally be on the emulated SD card).

## Build (From Repo Root)
Main build (RPX then WUHB):

```bash
./build_wiiu_then_wuhb.sh
```

Clean + rebuild commands:
```bash
make -C sm64wiiu clean
./build_wiiu_then_wuhb.sh
```

Crash-debug build (generates extra debug artifacts like `.elf` and map):
```bash
make -C sm64wiiu clean
WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh
```

Expected outputs:
- `sm64wiiu/build/us_wiiu/sm64.us.rpx`
- `sm64wiiu/build/us_wiiu/sm64.us.wuhb`
- `sm64wiiu/build/us_wiiu/sm64.us.elf` (symbol-rich ELF for address resolution)
- `sm64wiiu/build/us_wiiu/sm64.us.wiiu.map` (linker map)

## Cemu Logs / Crash Debugging (macOS)
Canonical log path:

```bash
export CEMU_LOG="$HOME/Library/Application Support/Cemu/log.txt"
```

Quick triage (last crash block + nearby context + filtered lines + optional addr2line decode):

```bash
./sm64wiiu/tools/cemu_log_triage.sh
```

Manual crash decode (addr2line):

```bash
bash sm64wiiu/tools/wiiu_decode_cemu_crash.sh "$CEMU_LOG" sm64wiiu/build/us_wiiu/sm64.us.elf
```
