# Project Summary (Condensed)

## 1) Project Scope
- Primary target: `sm64wiiu/` (Wii U, Aroma, outputs `.rpx` and `.wuhb`).
- Donor reference: local `sm64coopdx/` tree.
- End goal: gameplay and mod parity with Co-op DX on Wii U.
- Delivery strategy: keep Wii U build bootable at every step; networking comes last.

## 2) Repository Map
- `sm64wiiu/`: Wii U project under active development.
- `sm64coopdx/`: source-of-truth donor for enhanced features.
- `AGENTS.md`: workflow and gotchas for future agents.

## 3) Current State Snapshot
- Baseline Wii U build is established and reproducible.
- Port includes major runtime compatibility groundwork (FS, API uplift, Lua/mod runtime, hooks, cobject, assets).
- Main remaining objective: continue feature parity hardening while keeping startup/runtime stable.

## 4) Build and Artifacts

### Build commands (run sequentially)
```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"

make -C sm64wiiu -j4
make -C sm64wiiu wuhb
```

### Output artifacts
- `sm64wiiu/build/us_wiiu/sm64.us.rpx`
- `sm64wiiu/build/us_wiiu/sm64.us.wuhb`

## 5) Implemented Work (Deduplicated)

### A) Build system and baseline
- Added GNU Make 3.81 compatibility fixes (`Makefile`, `Makefile.split`) and generation-order fixes.
- Baseline Wii U compile is stable with valid baserom.

### B) FS + platform substrate
- Imported Co-op DX FS stack into `src/pc/fs` (`fs`, `fs_packtype_dir`, `dirtree`, `fmem`).
- Fixed donor `is_file`/`is_dir` inversion bug.
- Added `platform.[ch]` and Wii U write-path policy (`/vol/external01/wiiu/apps/sm64wiiu`, fallback cwd).
- `fs_init()` mounts `/vol/content` first on Wii U, then writable path.

### C) Save/config pathing parity
- Config moved to FS-backed I/O (`fs_open`, `fs_readline`, `fs_get_write_path`).
- EEPROM save moved to FS-backed read/write.
- `configfile_name()`/default naming aligned toward Co-op DX.

### D) Packaging and deployment
- Added/maintained Wii U packaging path (`wuhb`) and build helper script (`build_wiiu.sh`).

### E) Controller and window API uplift
- Expanded `ControllerAPI` toward Co-op DX (raw key, rumble, reconfigure, shutdown hooks).
- Updated controller fanout and backends (`keyboard`, `recorded_tas`, `wiiu`).
- Added rumble routing from `osMotorStart/osMotorStop` through controller layer.
- Extended `GfxWindowManagerAPI` shape and updated backends (`gx2`, `sdl2`, `glx`, `dxgi`, `dummy`).

### F) Rendering API uplift
- Migrated shader create/lookup API to `ColorCombiner*` interface.
- Preserved backend behavior via compatibility bridge (`cc->shader_id`).

### G) Lua + mods foundation
- Added `src/pc/lua/*`, `src/pc/mods/*`, vendored Lua 5.3.6.
- Wired lifecycle in `pc_main` (`mods_init`, `smlua_init`, update, shutdown).
- Added built-in mod activation and mounted-content loading.

### H) Hooks + callsites
- Added hook namespace + registration (`hook_event`) with bounded registry storage.
- Added dispatch + cleanup paths.
- Wired key game callsites:
  - warp/level-init
  - interact / allow-interact
  - dialog hooks
  - mario update hooks
  - object-set-model hooks.

### I) Cobject and field reflection
- Added typed Lua userdata wrappers for `MarioState` and `Object`.
- Added practical reflection subset and vec proxies with read/write support.
- Added global cobject exposure/update (`gMarioStates`, `gMarioState`, `gMarioObject`, `gCurrentObject`).

### J) Runtime compatibility surface
- Added broad compatibility set for built-in donor mods:
  - mod storage API (typed wrappers + remove)
  - HUD/DJUI compatibility helpers
  - network single-player shims
  - model/behavior helpers
  - math helpers/aliases
  - sound bridges (`play_sound`, `set_background_music`, etc.).

### K) HUD/dialog/interaction slices
- Dialog override conversion/render integration for common charset.
- Hook-aware dialog open-veto + state cleanup.
- DJUI-like HUD text/texture bridge and color handling improvements.
- Additional interaction hook coverage.

### L) Day-night-cycle and skybox parity
- Added Lua behavior runner for DNC skybox objects.
- Added skybox override/tint compatibility APIs.
- Added true custom DNC skybox assets + extraction tooling.
- Added sequence replacement alias support and guarded runtime handling.

### M) Character-select parity groundwork
- Synced built-in character-select payloads (`character-select-coop`, `char-select-the-originals`) into runtime + bundled content.
- Added asset sync helper script.
- Added compatibility functions required by character-select/day-night top-level scripts.

### N) Wii U runtime stability hardening
- Moved Lua/mod init later in startup (after core game/audio init).
- Added instruction budgets for script load/update and callback dispatch.
- Optimized object iteration helpers to avoid repeated full-list scans.
- Added recursion guard for object-set-model hooks.
- Switched Lua script loading to VFS-first path (`fs_load_file` + `luaL_loadbufferx`).
- Hardened VFS Lua load buffers (NUL-termination, size guards, text mode).
- Disabled fast-math for Lua objects (`-fno-fast-math`) to prevent Lua VM corruption.
- Added deterministic fallback companion loading for known multi-file mods.
- Added explicit main-script startup markers for runtime diagnosis.

## 6) Active Compatibility/Stability Decisions
- Full Co-op DX networking is not shipped yet on Wii U (runtime-first strategy).
- Runtime `.m64` injection in `smlua_audio_utils_replace_sequence` remains guarded for stability; sequence aliasing remains enabled.
- Companion loading prioritizes deterministic fallback lists for known built-ins when applicable.

## 7) Known Gotchas
- Wii U is big-endian; apply byte swaps where required.
- GPU cache coherency is manual (`DCFlushRange`, invalidate as needed).
- ProcUI/WHB lifecycle correctness is mandatory for HOME/background stability.
- `WHBLogPrint` is non-variadic; use `WHBLogPrintf` for formatted logs.
- Build success does not guarantee runtime stability; validate startup and gameplay paths.

## 8) Remaining Work / Next Focus
- Continue non-network gameplay/mod parity with stability-first validation.
- Expand missing Lua API/cobject coverage when runtime behavior proves necessity.
- Validate parity-focused changes on Wii U runtime behavior.
- Networking phase remains deferred until runtime parity is stable.

## 9) Milestone Ledger (Compact Chronology)

Notes:
- Milestones are tracked in compact form without repeated command output.
- Unless noted, milestones were validated by rebuilding normal Wii U outputs (`make`, `wuhb`).

### 2026-02-06
- Baseline build compatibility: GNU Make 3.81 fixes and reproducible RPX output.
- FS/platform substrate: imported Co-op DX FS stack, added `platform.[ch]`, write-path policy.
- Save/config parity: moved config + EEPROM to FS-backed I/O.
- Packaging pipeline: Wii U packaging target + build helper script.
- Controller/window uplift: expanded ControllerAPI + window-manager API; backend updates + rumble routing.
- Renderer signature pass: `GfxRenderingAPI` moved to `ColorCombiner*` with compatibility bridge.
- Dialog rendering slice: hook-aware dialog override conversion/integration.
- Interaction slice: interaction hook dispatch coverage.
- Cobject foundation + expansion: typed userdata + practical field reflection + vec proxies.
- Lua/mod foundation: vendored Lua 5.3.6, lifecycle wiring, built-in mod activation.
- Hook core + callsites: namespace, dispatch/cleanup, warp/level/dialog/interact wiring.
- Allow-interact slice: Lua veto before interaction handlers.
- Runtime compatibility slices: storage/HUD/DJUI/network-shim/model/math/sound coverage for built-ins.
- Day-night-cycle slices: load-safe compatibility, behavior runner, skybox/tint support.
- Renderer lighting/fog slice: Lua state wired into renderer.
- Sync/audio slices: sync-table watchers + sequence alias handling.
- Far-clip slice: `set_override_far` wired into projection path.
- Asset parity slices: true custom DNC skybox assets + built-in character-select payload sync.

### 2026-02-07
- Character-select compatibility slice: fixed compile blocker + added missing anim/model utility bindings.
- Startup hardening: Lua/mod init reordered after core init.
- Iteration/perf hardening: optimized Lua object iteration helpers.
- Guardrail passes: script-load and callback instruction budgets.
- Hook stability pass: object-model recursion guard.
- Constants/bootstrap stability passes:
  - risky bootstrap disabled/rolled back
  - targeted C bindings expanded
  - safer constants strategies evaluated.
- Runtime diagnostics pass: improved Lua startup/error observability.
- Loader hardening passes:
  - VFS-first Lua loading
  - buffer safety/size guards
  - text-mode load handling.
- Lua VM stability fix: `-fno-fast-math` for Lua objects.
- Compatibility/API fixes: companion fallback manifests, `mod_storage_load_bool_2`, numeric loop-point handling.
- Latest stability pass:
  - deterministic fallback companion loading for known built-ins
  - guarded `.m64` injection path with alias compatibility retained.
- Lua constants compatibility pass: enabled split autogen constants bootstrap on Wii U (preamble chunk + chunked tail with literal fallback) to expose broader Co-op DX globals/constants for bundled and future Lua mods | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; runtime should no longer miss key constants like `VERSION_NUMBER`, `FONT_COUNT`, `HUD_DISPLAY_FLAG_CAMERA`, `CHAR_ANIM_MAX`.
- Sync-table parity fix: aligned single-player Lua sync table initialization with autogen `MAX_PLAYERS` so built-in mods that iterate all player slots do not nil-index on startup | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; should clear `gPlayerSyncTable[i]` nil errors in `cheats.lua` and `personal-starcount-ex.lua`.

## 10) Required Format For Future Summary Updates

Use this low-token format only. Do not append long narrative sections.

### Update rules
- Update existing sections in place when state changes.
- Add one new ledger line only for material milestones.
- Do not repeat full command output.
- Do not duplicate unchanged artifact paths/commands.

### Required update entry template
```
### YYYY-MM-DD
- <short milestone name>: <what changed>
  - files: <key files>
  - validation: <commands run> -
  - outcome: <result>
```

### Optional extra line (only if needed)
```
  - Gotcha: <single concrete issue and mitigation>
```
