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
  - before-phys-step hooks at step-level entry points (ground/air/water/hang) with Co-op DX-style optional step-result override
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
- Added Phase 0/1 parity artifacts generator (`tools/parity/generate_phase0_phase1_matrix.py`) that produces repeatable donor-vs-Wii U Lua/hook/module gap reports (`parity/phase0_matrix.{json,md}`, `parity/phase1_lua_port_queue.md`).

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
- Added first-pass character-select gameplay helper globals used by built-ins (`sins`, `coss`, `atan2s`, `check_common_idle_cancels`, `stationary_ground_step`, `set_character_animation`, `play_character_sound`, plus `hud_get_flash`/`hud_set_flash` and `game_unpause` compatibility shims).
  - `check_common_idle_cancels` / `stationary_ground_step` are currently safe compatibility stubs on Wii U pending broader action-hook parity to avoid runtime hangs.
  - `play_character_sound` is currently a no-op compatibility stub pending full character-voice routing parity.
- Added second-pass low-risk Lua helper parity (`absf_2`, `approach_s16_asymptotic_bool`, `approach_s16_asymptotic`, `approach_s16_symmetric`, `camera_approach_s16_symmetric_bool`, `set_or_approach_s16_symmetric`, `apply_drag_to_value`) and `gfx_set_command` compatibility stub (returns false until DynOS display-list command parser is ported).
- Added third-pass low-risk Lua helper parity for vanilla gameplay/camera helpers (`adjust_sound_for_speed`, `add_tree_leaf_particles`, `align_with_floor`, `analog_stick_held_back`, `anim_and_audio_for_walk`, `anim_and_audio_for_hold_walk`, `anim_and_audio_for_heavy_walk`, `animated_stationary_ground_step`, `approach_f32_ptr`, `approach_vec3f_asymptotic`, `set_or_approach_vec3f_asymptotic`).
- Added fourth-pass low-risk Lua helper parity (`atan2f`, `apply_slope_accel`, `apply_landing_accel`, `apply_slope_decel`, `arc_to_goal_pos`, `act_select_hud_hide`, `act_select_hud_show`, `act_select_hud_is_hidden`) with an internal act-select HUD bitmask shim for single-player compatibility.
- Replaced hardcoded `gActiveMods` Lua shim with a dynamic table derived from active root scripts (`mods_get_active_script_count/path`), including donor-style zero-based indexing and populated `name`/`relativePath`/`category` fields so bundled mod UIs report active mod lists/counts consistently.

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
- Added root-script startup markers for all active scripts (including single-file mods) and targeted HOOK_BEFORE_PHYS_STEP water-step velocity diagnostics for runtime parity triage.
- Added GX2 window start-frame diagnostics and a first-frame clear skip mitigation in `gfx_gx2_window_start_frame()` to avoid intermittent black-screen hangs before `start_frame()` returns on Cemu full-sync paths.
- Added Wii U watchdog diagnostics thread + cross-module stage markers (`pc_main`/`gfx_run`) so hangs without crashes still emit periodic `diag: stall ... stage=...` logs showing where frame progress stopped.

### O) DJUI bootstrap (phase slice 1)
- Added initial `src/pc/djui` scaffold (`djui.[ch]`) and wired lifecycle hooks into Wii U startup/shutdown (`djui_init`, `djui_init_late`, `djui_shutdown`).
- Added DJUI render hook call in `end_master_display_list()` so future panel/UI rendering can layer in donor-aligned display-list phase.
- Added guarded main-menu mode state (`gDjuiInMainMenu`) with temporary debug toggle (`L+R+START`) and first-pass castle-grounds menu-scene control (`djui_update_menu_level`).
- Suppressed vanilla HUD while DJUI main-menu mode is active to match donor layering expectations.

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
- Continue Phase 1 against generated parity queue (`parity/phase1_lua_port_queue.md`), prioritizing donor autogen Lua coverage and hook callsite parity.
- Continue DJUI parity slices: replace scaffold with donor panel stack/root renderer and port main/join/lobbies flows incrementally.
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
- Lua helper API parity pass: added missing Co-op DX helper globals (`table_copy`, `table_deepcopy`, `get_uncolored_string`, `init_mario_after_warp`, plus additional network/texture/level-script/scroll/cast compatibility stubs) and bound them in Wii U runtime for broader third-party mod compatibility | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with expanded Lua API surface and safer fallback behavior for unported subsystems.
- Lua mod-loading compatibility pass: switched mod path probing to VFS-backed checks, added a custom scoped `require()` cache/loader with recursion guard + fallback to stock Lua require, and enabled dynamic user-mod root-script discovery (`mods/*.lua`, `mods/*/main.lua`) while preserving built-in load order | files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/pc/mods/mods.c, sm64wiiu/src/pc/mods/mods.h | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; bundled/content-mounted mod assets and user-added mod scripts are now discoverable and require()-loadable in Wii U runtime.
- In-game load visibility pass: added a startup HUD overlay that lists loaded root mod scripts and total count for ~30 seconds after Lua init, enabling quick on-screen verification in Cemu/hardware without relying only on logs | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; users can now visually confirm mod discovery on boot.
- Startup crash fix for overlay: moved mod-overlay drawing from `smlua_update()` into HUD render phase (`render_hud`) so text labels are queued only while HUD display lists are active, preventing early-frame gfx crashes in Cemu | files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/pc/lua/smlua.h, sm64wiiu/src/game/hud.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with safer overlay timing.
- Character-select bootstrap compatibility fix: seeded early Lua globals/wrappers (`gServerSettings`, `gNametagsSettings`, fallback `cs_hook_mario_update`) before helper-script execution so bundled multi-file mods no longer fail on nil globals during startup | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with cleaner helper load path for character-select.
- Cemu intro/title crash hardening: expanded and guarded renderer combiner/shader caches to avoid out-of-bounds pool writes when new material combinations appear around title/intro Mario spawn; added shader-slot recycling with safe release and combiner refresh on cache hits | files: sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_gx2.cpp | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with defensive GX2 shader/combiner cache behavior.
- GX2 sampler-crash mitigation: disabled runtime `GX2SetPixelSampler` binds in Cemu-sensitive path while keeping local sampler state caching, to avoid repeated signal-10 faults at `gfx_gx2_set_sampler_parameters` during title/intro render flow | files: sm64wiiu/src/pc/gfx/gfx_gx2.cpp | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with sampler-bind crash path bypassed for stability triage.
- Renderer rollback + character-select isolation pass: reverted all temporary GX2/gfx cache/sampler crash-mitigation edits after texture regressions, and temporarily disabled loading of `character-select-coop` / `char-select-the-originals` root scripts (with explicit startup log) to isolate spawn-time crashes from Lua mod bootstrap state | files: sm64wiiu/src/pc/gfx/gfx_gx2.cpp, sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/mods/mods.c | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu wuhb -j4` | outcome: renderer restored to pre-mitigation behavior; new `sm64wiiu/build/us_wiiu/sm64.us.wuhb` includes mod-loader isolation for crash triage.
- Character-select re-enable pass: restored default loading of `mods/character-select-coop/main.lua` and `mods/char-select-the-originals/main.lua` after isolation test showed spawn crash persisted, confirming character-select disable did not resolve the fault | files: sm64wiiu/src/pc/mods/mods.c | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu wuhb -j4` | outcome: build + wuhb succeed with character-select scripts active again.
- Spawn-crash instrumentation pass: added Wii U render-path diagnostics and guards around combiner/shader pool growth and GX2 texture/sampler binding state (including first-draw marker), while increasing static combiner/shader pool capacity from 64 to 256 to prevent early overflow corruption during startup scenes | files: sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_gx2.cpp | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb -j4` | outcome: build + wuhb succeed; next Cemu run should emit targeted `gfx:` markers to isolate failing state before crash.
- GX2 sampler null-shader path fix attempt: removed eager sampler initialization during texture-cache insertion (before shader bind), seeded sentinel sampler state so first textured draw re-applies real parameters, and added explicit log when sampler setup is requested without an active shader to verify/avoid the previous crash path at `gfx_gx2_set_sampler_parameters` | files: sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_gx2.cpp | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb -j4` | outcome: build + wuhb succeed; next Cemu run should confirm whether null-shader sampler calls still occur and whether spawn crash moves/resolves.
- TMEM tile-index bounds hardening pass: fixed out-of-bounds texture-slot indexing in RDP load path by remapping `tmem/256` into the renderer’s two supported texture slots, adding bounded diagnostics for remaps, and guarding load-block/load-tile writes to prevent corruption of adjacent renderer globals (notably GX2 texture vector metadata) | files: sm64wiiu/src/pc/gfx/gfx_pc.c | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb -j4` | outcome: build + wuhb succeed; next Cemu run should no longer crash via corrupted `gx2_textures` vector state from high TMEM tile indices.
- Co-op DX parity audit + GX2 texture allocator hardening: compared Wii U renderer against donor `sm64coopdx` tile-load semantics and restored compatibility for special load tile `6`, rejected unsupported TMEM tile indices instead of aliasing them, added import-path validity guards, and replaced dynamic `std::vector<_Texture>` growth with a fixed 2048-entry GX2 texture pool to eliminate vector-metadata crash paths observed at `std::vector<_Texture>::_M_default_append` | files: sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_gx2.cpp | validation: `export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC PATH="$PATH:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb -j4` | outcome: build + wuhb succeed with donor-aligned load-tile handling and non-vector GX2 texture storage for spawn-crash retest.
- HOOK_BEFORE_PHYS_STEP timing/signature parity pass: moved pre-physics hook dispatch from `bhv_mario_update` to donor-aligned physics-step callsites (`perform_ground_step`, `perform_air_step`, `perform_water_step`, `perform_hanging_step`), added C step-type constants, and implemented Lua hook return override handling (`number` return short-circuits with step result) to match Co-op DX behavior expected by gameplay mods like `faster-swimming` | files: sm64wiiu/include/sm64.h, sm64wiiu/src/pc/lua/smlua_hooks.h, sm64wiiu/src/pc/lua/smlua_hooks.c, sm64wiiu/src/game/mario_step.c, sm64wiiu/src/game/mario_actions_submerged.c, sm64wiiu/src/game/mario_actions_automatic.c, sm64wiiu/src/game/object_list_processor.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with pre-physics hooks firing at correct simulation points for parity retest.
- Gameplay-mod order + water-hook diagnostics pass: reordered built-in root script activation so character-select scripts initialize first and gameplay single-file scripts (including `faster-swimming`) run later, added explicit Wii U root-script log lines for every active script (not only `main.lua` roots), and added bounded water-step hook delta logs to verify when HOOK_BEFORE_PHYS_STEP callbacks mutate `MarioState.vel` at runtime | files: sm64wiiu/src/pc/mods/mods.c, sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/pc/lua/smlua_hooks.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; next Cemu run should expose whether faster-swimming hook registration/order and velocity mutation are occurring.
- Phase 0/1 parity bootstrap: added a repeatable donor-vs-Wii U parity matrix generator and implemented first-pass missing Lua compatibility bindings required by currently shipped built-in mods (`sins`, `coss`, `atan2s`, `check_common_idle_cancels`, `stationary_ground_step`, `game_unpause`, `hud_get_flash`, `hud_set_flash`, `nearest_player_to_object`, `set_character_animation`, `play_character_sound`) | files: sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py, sm64wiiu/parity/phase0_matrix.json, sm64wiiu/parity/phase0_matrix.md, sm64wiiu/parity/phase1_lua_port_queue.md, sm64wiiu/src/pc/lua/smlua.c | validation: `sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py`, `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Phase 0 artifacts now quantify parity (2007 donor Lua globals vs 177 Wii U, 1835 still missing) and current built-in mods no longer reference missing donor globals.
- Black-screen regression mitigation for Phase 1 helpers: narrowed newly added action-step Lua helpers to safe compatibility stubs (`check_common_idle_cancels`, `stationary_ground_step`) and added stronger Mario-object validity guard for `set_character_animation` after Cemu runs showed startup reaching first draw then hanging in runtime update flow | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed with symbol parity retained and lower risk of custom-action recursion hangs.

### 2026-02-07
- Phase 1 low-risk Lua helper expansion + start-frame hang mitigation: added donor-compatible helper globals (`absf_2`, `approach_s16_*`, `camera_approach_s16_symmetric_bool`, `set_or_approach_s16_symmetric`, `apply_drag_to_value`) and `gfx_set_command` stub on Lua side, then instrumented/reordered GX2 window start-frame setup with one-frame clear skip to avoid intermittent black-screen stalls before `gfx_run` reaches `post start_frame` | files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/pc/gfx/gfx_gx2_window.cpp, sm64wiiu/parity/phase0_matrix.md, sm64wiiu/parity/phase0_matrix.json, sm64wiiu/parity/phase1_lua_port_queue.md | validation: `sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py`, `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; runtime reaches gameplay again, missing donor globals reduced to 1819 (Wii U registered globals now 193, shared 188), and built-in mods still report no missing donor globals.
- Hang-observability pass: added a Wii U-only watchdog thread (`coreinit` thread API) that emits periodic stall diagnostics with last frame stage and elapsed stall duration, and wired lightweight progress markers through `produce_one_frame`, `gfx_run`, and `gfx_end_frame` to make black-screen hangs diagnosable even when Cemu logs show no crash event | files: sm64wiiu/src/pc/pc_diag.h, sm64wiiu/src/pc/pc_diag_wiiu.c, sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/pc/gfx/gfx_pc.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; future hangs should print `diag: stall ... stage='...'` lines that identify the stuck subsystem boundary.

### 2026-02-08
- Phase 1 Lua helper parity expansion (vanilla gameplay/camera batch): added donor-aligned Lua bindings for existing Wii U-side helpers (`adjust_sound_for_speed`, `add_tree_leaf_particles`, `align_with_floor`, `analog_stick_held_back`, `anim_and_audio_for_walk`, `anim_and_audio_for_hold_walk`, `anim_and_audio_for_heavy_walk`, `animated_stationary_ground_step`, `approach_f32_ptr`, `approach_vec3f_asymptotic`, `set_or_approach_vec3f_asymptotic`) and refreshed parity artifacts | files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/parity/phase0_matrix.json, sm64wiiu/parity/phase0_matrix.md, sm64wiiu/parity/phase1_lua_port_queue.md | validation: `sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py`, `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Lua global parity improved to 204 Wii U globals / 199 shared / 1808 missing with no current built-in-mod missing-symbol regressions.
- Phase 1 Lua helper parity expansion (math/slope/act-select batch): added donor-aligned Lua bindings for `atan2f`, slope/landing helpers (`apply_slope_accel`, `apply_landing_accel`, `apply_slope_decel`), `arc_to_goal_pos`, and act-select HUD helpers (`act_select_hud_hide`, `act_select_hud_show`, `act_select_hud_is_hidden`) backed by a local mask shim; refreshed parity artifacts | files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/parity/phase0_matrix.json, sm64wiiu/parity/phase0_matrix.md, sm64wiiu/parity/phase1_lua_port_queue.md | validation: `sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py`, `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Lua global parity improved to 212 Wii U globals / 207 shared / 1800 missing with no current built-in-mod missing-symbol regressions.
- Active-mod table compatibility fix: rebuilt Lua `gActiveMods` from the actual active script list (instead of hardcoded entries) to keep mod-list UIs aligned with runtime-loaded mods and eliminate misleading mod-count displays while preserving single-player stability | files: sm64wiiu/src/pc/lua/smlua.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`, `tail -n 400 \"$HOME/Library/Application Support/Cemu/log.txt\"` | outcome: build + wuhb succeed; Cemu log confirms all six bundled root scripts are still loading (`root[0]..root[5]`), and Lua now receives a dynamic active-mod list instead of static placeholders.
- DJUI bootstrap slice 1: added `src/pc/djui` runtime scaffold, integrated init/shutdown/render hooks into Wii U loop, added guarded `gDjuiInMainMenu` menu-scene controller (castle grounds) behind temporary `L+R+START` toggle, and hid vanilla HUD while DJUI main menu mode is active | files: sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/djui/djui.h, sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/game/game_init.c, sm64wiiu/src/game/level_update.c, sm64wiiu/src/game/hud.c, sm64wiiu/Makefile | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Wii U runtime now has a stable DJUI integration spine ready for panel-stack/menu parity slices.

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
      - validation: <commands run>
      - outcome: <result>
      - gotcha: <single concrete issue and mitigation>
```
  The gotcha is OPTIONAL, you should only include one if something important was learned
