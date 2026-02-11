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
- Added mounted-content root-script discovery plus a local mod catalog (`available` vs `active`) with runtime-enable flags.
- Default mod policy is now disabled-at-boot; DJUI enables selected mods before gameplay and Lua reload applies the toggle set.

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
- Updated multi-file script execution to donor-style top-level lexical order across `.lua` and `.luac` siblings (including `main.lua` in-order), fixing compiled mod startup dependencies such as Flood Expanded `z-api.lua` expecting `main.lua` globals to exist first.
- Extended Wii U Lua loader to accept common PC-built `.luac` bytecode (endianness + 64-bit `size_t`), enabling compiled-only mods like Flood Expanded to execute on big-endian/32-bit Wii U builds.
- Added follow-up Flood Lua runtime parity bindings (`get_dialog_id`, `get_ttc_speed_setting`, `set_ttc_speed_setting`, `degrees_to_sm64`, `camera_config_is_free_cam_enabled`, `camera_romhack_set_collisions`, `djui_is_playerlist_open`) plus a safe `gfx_get_from_name` nil-stub for UV-scroll scripts that gracefully disable when display-list introspection is unavailable.

### K) HUD/dialog/interaction slices
- Dialog override conversion/render integration for common charset.
- Hook-aware dialog open-veto + state cleanup.
- DJUI-like HUD text/texture bridge and color handling improvements.
- Reset DJUI HUD Z accumulator per HUD render so mod HUD elements do not clip out over time; added HUD font fallback so mods printing `@` for the `x` glyph render correctly on vanilla US HUD LUTs.
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
- Hardened VFS Lua load buffers (NUL-termination, size guards, text+bytecode mode).
- Disabled fast-math for Lua objects (`-fno-fast-math`) to prevent Lua VM corruption.
- Added deterministic fallback companion loading for known multi-file mods.
- Added explicit main-script startup markers for runtime diagnosis.
- Added root-script startup markers for all active scripts (including single-file mods) and targeted HOOK_BEFORE_PHYS_STEP water-step velocity diagnostics for runtime parity triage.
- Added GX2 window start-frame diagnostics and a first-frame clear skip mitigation in `gfx_gx2_window_start_frame()` to avoid intermittent black-screen hangs before `start_frame()` returns on Cemu full-sync paths.
- Added Wii U watchdog diagnostics thread + cross-module stage markers (`pc_main`/`gfx_run`) so hangs without crashes still emit periodic `diag: stall ... stage=...` logs showing where frame progress stopped.

### O) DJUI bootstrap (phase slice 1)
- Added initial `src/pc/djui` scaffold (`djui.[ch]`) and wired lifecycle hooks into Wii U startup/shutdown (`djui_init`, `djui_init_late`, `djui_shutdown`).
- Added DJUI render hook call in `end_master_display_list()` so future panel/UI rendering can layer in donor-aligned display-list phase.
- Added pause-panel host quit flow parity: confirming `STOP HOSTING`/`DISCONNECT` now closes pause panels, runs network shutdown, and returns to DJUI main menu.
- Added guarded main-menu mode state (`gDjuiInMainMenu`) with temporary debug toggle (`L+R+START`) and first-pass castle-grounds menu-scene control (`djui_update_menu_level`).
- Suppressed vanilla HUD while DJUI main-menu mode is active to match donor layering expectations.
- Switched startup entry flow to donor-style `level_main_scripts_entry` boot (castle grounds target) instead of the vanilla intro-script loop.
- Added a first playable DJUI menu shell for Wii U parity validation:
  - default boot into DJUI main menu scene
  - custom menu overlay pages (`MAIN`, `LOBBY`, `MODS`) with controller navigation
  - active mod list rendering from runtime mod activation set
  - menu-driven transition into gameplay (`ENTER GAME` / `START`)
- Suppressed vanilla menu/dialog rendering and credits text while DJUI main-menu mode is active, and blocked act-select while in menu mode.
- Upgraded the shell to a panelized donor-style flow using existing Wii U menu draw primitives:
  - dimmed castle-scene backdrop + two-panel overlay layout
  - lobby-select page (`LOBBIES`) with offline-safe lobby entries
  - lobby detail page with donor-like route to gameplay and mods view
  - mod page with selectable rows and clear placeholder messaging for pending runtime toggle parity
- Replaced vanilla pause-box menu decorators (which include arrow triangles) with custom background-only panel box draws for DJUI main-menu pages to avoid transient startup arrow artifacts.
- Added DJUI main-menu input-neutral gating on open/init so held startup buttons do not immediately dismiss menu mode into gameplay; selection labels now use stronger visible markers (`>> ... <<`) for controller-driven parity testing.
- Replaced rainbow `print_text` overlay path for DJUI pages with non-rainbow menu-font rendering (`print_generic_string` over `dl_ia_text_begin/end`) plus ASCII->dialog conversion helpers, so DJUI menu pages no longer look like vanilla colorful HUD text.
- Added Co-op-DX-style button-row framing and highlighted selected states on main/lobby pages, and shifted main menu action set toward donor flow (`HOST`, `JOIN`, `MODS`, `ENTER GAME`) while keeping host/join offline-safe placeholders.
- Fixed DJUI row/text render ordering (panel geometry pass before IA-text pass) to prevent menu-label corruption, and upgraded `MODS` page from read-only to real on/off toggles backed by the mod catalog and apply-on-start Lua reload.
- Added explicit main-page footer anchor spacing so `ACTIVE MODS` / control hints stay below button rows and do not overlap menu options during donor-style panel rendering.
- DJUI overlay labels continue using `print_generic_string`; a direct `print_menu_generic_string` swap is currently incompatible with this dialog-string path on Wii U US and produces unreadable glyph bars.
- Added donor-aligned main-action branching: `HOST` now opens local host lobby flow, `JOIN` opens lobby list flow, and `OPTIONS` opens a dedicated options page scaffold (camera/controls/display/sound/misc/back) instead of jumping directly to mods.
- Split `HOST` into its own dedicated page (separate from join-lobby details) and added a shared secondary-row anchor mapping so non-main page button bars align behind text labels consistently.
- Removed forced same-level startup warps in DJUI menu-scene control by gating `initiate_warp()` on level/area mismatch only, preventing the first-frame over-water camera jump before menu pose lock is applied.
- Added donor-inspired `HOST SAVE` and `HOST SETTINGS` subpages behind `HOST` actions, with Wii U-safe controller navigation/back-stack behavior, live save-slot metadata display/selection, and a local host-settings scaffold with cycle/toggle semantics for parity-driven UI progression.
- Reworked `HOST` page structure toward donor `djui_panel_host` semantics by replacing the old two-column placeholder list with a centered panel row stack (`NETWORK SYSTEM`, `PORT`, `PASSWORD`, `SAVE SLOT`, `SETTINGS`, `MODS`, `BACK`, `HOST`) driven by a single top-left row/text anchor model, including local state toggles for network/port/password scaffolding while keeping Wii U flow offline-safe.
- Replaced ad-hoc page routing with a reusable DJUI page-stack model (`push/pop/reset`) in the Wii U menu shell so nested panels now unwind consistently via stack semantics and root `MAIN` no longer closes on `B`.
- Added pause-screen parity hook for menu recovery: course pause options now include `MAIN MENU` and selecting it exits pause back into DJUI main menu instead of only resume/exit-course behavior.
- Routed post-star `SAVE & QUIT` on Wii U to DJUI main-menu flow instead of the vanilla SM64 front-end reset path, matching donor menu ownership expectations for modded sessions.
- Added dual-path DJUI migration gate and donor-foundation modules: `djui.h` now exposes donor-path control (`gDjuiUseDonorStack`, set/get helpers), legacy menu path remains default, and initial donor-oriented framework files were added (`djui_types`, `djui_base`, `djui_root`, `djui_rect`, `djui_flow_layout`, `djui_text`, `djui_three_panel`, `djui_panel`, `djui_panel_menu`, `djui_donor`) so panel-by-panel donor migration can proceed without destabilizing existing Wii U boot/menu behavior.
- Added donor-stack interaction slice with controller-edge semantics and panel primitives: strict edge-driven interactable update (`A` click-on-release, `B` back on rising edge), cursor/focus wiring, donor-style button widgets, animated panel-stack transitions, and first donor `panel_main` scaffold (`HOST/JOIN/OPTIONS/QUIT`) backed by new `djui_panel_main` + `djui_button` modules.
- Hardened active legacy menu input path against repeated face-button activations by deriving menu `pressed` edges from `buttonDown` transitions and persisting per-frame last-button state, so transient `buttonPressed` anomalies on Wii U no longer chain unintended A/B menu actions.
- Added donor-stack panel-transition input masking (ignore held A/B until release) plus C-button dpad support in the interactable path so Wii U D-pad mapping navigates correctly and held inputs no longer chain unintended panel transitions.
- Reworked donor `three_panel`/`flow_layout` composition wiring to remove duplicate child-layer creation and align menu panel/body placement with Co-op DX panel-stack semantics; added extra donor-interactable release masking and an in-loop Mario input clamp while main menu is active.
- Expanded donor DJUI compile/runtime surface to include broad panel/widget coverage (`checkbox`, `slider`, `selectionbox`, `inputbox`, `image`, `progress`, `paginated`, `bind`, host/join/options/player/modlist/pause/language/rules panels) and added Wii U-safe compatibility shims across missing donor subsystems (`pc/network`, `pc/mods`, `pc/lua/utils`, `pc/{ini,thread,update_checker,debuglog}`, DynOS/map helpers) so donor UI paths remain visible without backend crashes.
- Completed donor DJUI link/runtime wiring fixes for Wii U parity: restored donor global state symbols (`gDjuiRoot`, pause/mod labels, theme/shutdown/player-menu flags), added root assignment safety, exposed missing render/runtime symbols (`gRenderingInterpolated`, `gChangeLevelTransition`, `gd_texture_hand_open`), exported required HUD textures through `bin/custom_textures.c`, and added a local `hmap_*` implementation used by donor unicode paths; both `make` and `wuhb` now pass with donor stack default-enabled and legacy runtime toggle still available.
- Fixed donor DJUI texture-format fallback and donor render-path init state on Wii U: fallback texture override now handles 32b assets correctly (logo/fonts), donor render now applies shared DJUI HUD/display-list setup, and viewport reset is mapped to Wii U’s `D_8032CF00` symbol to avoid unresolved fullscreen viewport references.
- Replaced donor DJUI fallback-macro emulation with donor custom GBI command support on Wii U (`gbi_extension` + `gfx_pc.c` handlers for `G_TEXOVERRIDE_DJUI`, `G_TEXCLIP_DJUI`, `G_EXECUTE_DJUI`, `G_TEXADDR_DJUI`, `G_VTX_EXT`, `G_TRI2_EXT`) and added direct large-atlas override upload path to prevent first-frame `gfx_run_dl` stalls/hangs from invalid/oversized placeholder texture imports.
- Synced donor DJUI language payload into packaged Wii U content (`content/lang`) and bound Wii U `sys_resource_path()` to `/vol/content`, so donor language/panel lookups resolve from mounted WUHB assets instead of cwd-dependent paths.
- Added renderer display-list guardrails (`gfx_run_dl` depth/command budget + branch/null abort diagnostics) to prevent hard hangs when malformed/recursive display-list chains are encountered during donor DJUI bring-up.
- Restored donor interactable controller feed in the Wii U input path by routing `osContGetReadData()` to `gInteractablePad` when `gInteractableOverridePad` is active, so donor DJUI panel navigation can consume pad input again.
- Corrected Wii U controller backend selection in `controller_entry_point` (use `TARGET_WII_U` path instead of accidentally falling through `__WIIU__` mismatch), so Wii U builds initialize/read `controller_wiiu` rather than SDL in donor DJUI menu flow.
- Mapped donor join actions to deterministic Wii U stub behavior: client join availability is now surfaced through `network_client_available()`, join-direct/lobby actions fail fast with donor error UI instead of entering reconnect wait loops, and join panels disable unavailable client actions while retaining navigation/back flow.
- Re-aligned donor main-panel quit label to `MAIN.QUIT` (donor key) after renderer glyph clipping/override fixes removed the malformed lower-case glyph path on Wii U.
- Aligned Wii U DJUI compatibility defaults with donor visuals by setting default DJUI theme to dark (`configDjuiTheme = 1`), so first-boot panel styling matches Co-op DX.
- Hardened donor DJUI glyph stability on Wii U by clamping/rounding per-char texture clip percentages and adding a small glyph clip margin, plus syncing override texture metadata (`fmt/siz/line_size_bytes`) after DJUI override uploads so text-at-edge rows (`BACK`/`QUIT` and selectionbox values) avoid clip artifacts and stale tile state.
- Wired donor DJUI option/mod controls to persistent runtime state: expanded Wii U config option serialization for donor panel keys, added `enable-mod:` queue read/write + post-`mods_init()` apply, synced host-mod checkbox state back into canonical `available_script_enabled`, and connected display runtime toggles for texture filtering, force-4:3 viewport/scissor behavior, and draw-distance scaling.
- Wired donor menu-options runtime behavior into the active donor backend: menu-level selection/randomization/staff-roll/music selectors now drive `djui_donor_update_menu_level()` with donor-scene presets (warp target + camera/mario pose), so option changes are reflected while the donor main menu is live instead of staying hardcoded to castle grounds.
- Restored donor runtime overlays in the donor path (`djui_fps_display`, `djui_ctx_display`, `djui_lua_profiler`) and added FPS update plumbing in `pc_main`, so related display/profiler options are no longer inert in Wii U donor mode.
- Hardened host-mod selector parity by routing checkbox toggles through `mods_set_available_script_enabled()` before selectable recompute/save, ensuring donor host-mod panel toggles mutate canonical runtime script-enable state directly.
- Adjusted host-mod activation timing to match Wii U session expectations: startup remains modless even with persisted `enable-mod:` selections, and selected mods are now applied in-session when `HOST`/`APPLY` is confirmed in donor host flow.
- Hardened donor menu-scene stability for non-castle levels by using deferred level transition routing (`gChangeLevelTransition`) for level mismatches, reserving direct `initiate_warp()` only for same-level area switches, and hard-locking Lakitu focus/position/speeds during active menu scenes to avoid startup wobble/respawn loops.
- Added Wii U renderer safety guards for malformed frame data: clamp zero texture dimensions in `gfx_pc.c` UV normalization and skip GX2 draw submission when transient VBO allocation fails.
- Hardened GX2 frame-end pacing safety: Wii U now keeps the stable 30Hz swap cadence even when the DJUI `vsync` toggle is disabled (with one-time log warning), and swap-end now always waits for pending flips with bounded retries to avoid unsynced buffer corruption.
- Added first 60fps interpolation plumbing slice for Wii U pacing: moved non-N64 display-list submission control out of `display_and_vsync()` and into `pc_main` render pacing, added a donor-style multi-present frame loop (currently no-op interpolation data path) that keeps 30Hz simulation while allowing multiple presents per update, and enabled GX2 swap-interval `1` only when interpolation mode is active with 60Hz+ target framerate.
- Menu interpolation cadence follow-up: restored donor-like interpolation flag cadence in `pc_main` while gating only matrix interpolation in `gfx_pc` during donor main-menu mode, so panel animations can advance on interpolated subframes without reintroducing matrix-pair flicker in menu transitions.
- Menu interpolation cadence correction: kept `gRenderingInterpolated` enabled across interpolated subframes (donor-style) and switched DJUI interactable-update gating to run on the final subframe (`delta >= 1.0`), while matrix interpolation remains explicitly gated by `delta < 1.0` and non-menu context to preserve menu flicker mitigation.
- Renderer log-crash mitigation: disabled high-volume formatted renderer diagnostics in Wii U `gfx_pc.c` (`WHBLogPrintf` hot-path traces compile to no-op) after Cemu traces showed intermittent launch crashes in `_svfprintf_r` during startup render logging.
- Restored donor-style in-game pause handoff into DJUI: pause now shows the `R Button - Options` hint path and `R` opens the DJUI pause/options panel while paused (including donor-stack render/update visibility for that panel outside main-menu mode).

## 6) Active Compatibility/Stability Decisions
- Full Co-op DX networking is not shipped yet on Wii U (runtime-first strategy).
- Runtime `.m64` injection in `smlua_audio_utils_replace_sequence` remains guarded for stability; sequence aliasing remains enabled.
- Main-script companion execution now follows donor-style top-level lexical order for `.lua`/`.luac` files; deterministic fallback manifests remain only as a Wii U safety fallback for known built-ins when directory enumeration is unavailable.
- Donor DJUI now depends on donor GBI-extension opcode handling in the renderer; fallback `gDPLoadTextureBlock` emulation for texture override is considered unstable for Wii U donor atlases.
- Wii U network stubs intentionally reject `NT_CLIENT`; donor join flows must gate on `network_client_available()` and provide immediate UI error feedback to avoid indefinite “joining” waits.
- Donor DJUI settings now persist through `sm64config.txt` (including enabled host mods via `enable-mod:` entries); queued mod enables are restored after startup Lua init so boot stays modless, and selected host mods are applied on donor `HOST`/`APPLY`.
- Wii U currently keeps swap interval fixed to the stable 30Hz cadence; true unsynced/60Hz frame pacing remains deferred until Co-op DX interpolation/frame-timing support is ported end-to-end.

## 7) Known Gotchas
- Wii U is big-endian; apply byte swaps where required.
- GPU cache coherency is manual (`DCFlushRange`, invalidate as needed).
- ProcUI/WHB lifecycle correctness is mandatory for HOME/background stability.
- `WHBLogPrint` is non-variadic; use `WHBLogPrintf` for formatted logs.
- `.luac` bytecode is normally not portable across endianness and `size_t` width; Wii U build patches Lua 5.3.6 undump to accept typical PC chunks (little-endian + 64-bit `size_t`) so compiled-only mods can run.
- Build success does not guarantee runtime stability; validate startup and gameplay paths.
- Donor DJUI `gDPSetTextureOverrideDjui` cannot be safely emulated via vanilla `gDPLoadTextureBlock` for large font/logo atlases; use opcode-executed override upload in `gfx_pc.c` or menu rendering can corrupt/hang on frame 1.
- Donor DJUI language files must be present in WUHB mounted content (`/vol/content/lang/*.ini`); if `content/lang` is stale/missing, UI falls back to raw keys and panel text can look wrong.
- Without explicit client-availability checks, donor join panels can enter `JOIN MESSAGE` wait state forever on Wii U stubs; fail fast and surface `LOBBY_JOIN_FAILED` instead.
- Donor DJUI clip commands are encoded as 0..255 percentages; on Wii U float-space drift can push values slightly out of range and wrap on `u8` conversion, causing blocky/missing glyphs near panel bounds unless values are clamped before emit/consume.
- Donor menu-level changes should only force warps while play mode is normal and area data is loaded; issuing repeated level/area warps during transition phases can cause visible respawn loops and unstable camera settle in Cemu.

## 8) Remaining Work / Next Focus
- Continue non-network gameplay/mod parity with stability-first validation.
- Expand missing Lua API/cobject coverage when runtime behavior proves necessity.
- Continue Phase 1 against generated parity queue (`parity/phase1_lua_port_queue.md`), prioritizing donor autogen Lua coverage and hook callsite parity.
- Validate donor-default DJUI runtime in Cemu/hardware across all major panels/widgets (including host/join stub flows) and collect crash/log findings before removing remaining legacy assumptions.
- Continue donor backend parity hardening for panel behavior and remaining runtime-only options (notably interpolation/audio/camera backend parity) while keeping legacy runtime toggle as safety fallback until donor stack is proven stable on Wii U.
- Validate parity-focused changes on Wii U runtime behavior.
- Networking phase remains deferred until runtime parity is stable.
- Port Co-op DX interpolation/timer pacing to Wii U so menu/display `60fps` options are truly functional (without simulation speed-up or swap-path instability).

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
- DJUI main-menu flow slice 2: switched boot flow to donor-style main-scripts entry, defaulted Wii U into DJUI menu-scene mode, added custom controllable main/lobby/mod-list overlay pages, and bypassed vanilla menu/dialog rendering while DJUI main menu is active to align with Co-op DX startup behavior. | files: sm64wiiu/levels/entry.c, sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/game/area.c, sm64wiiu/src/game/level_update.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Wii U now boots directly into castle-scene DJUI menu mode with in-game transition and runtime mod-list visibility.
- DJUI visual/lobby slice 3: replaced the text-only shell with a panelized donor-style menu presentation and expanded lobby flow (`MAIN -> LOBBIES -> LOBBY -> MODS`) using offline-safe lobby entries; mod rows are now selectable in UI with explicit placeholder status for pending toggle/network parity. | files: sm64wiiu/src/pc/djui/djui.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Wii U menu now looks/flows closer to Co-op DX while remaining stable and incremental.
- DJUI menu UX stabilization slice 4: removed pause-menu arrow primitives from DJUI panel rendering, added startup neutral-input lock to prevent accidental immediate menu-close on boot, and strengthened visible option-selection markers while keeping donor-style castle-scene overlay flow intact. | files: sm64wiiu/src/pc/djui/djui.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; startup no longer should flash pause-arrow artifacts and menu selection state is now clearly indicated for Wii U controller testing.
- DJUI visual parity slice 5: migrated menu text drawing off the rainbow HUD label pipeline to white menu-font rendering with donor-style panel overlay composition and explicit selected-row star markers, so main-menu/lobby/mod pages no longer present as vanilla SM64 rainbow text. | files: sm64wiiu/src/pc/djui/djui.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; DJUI pages now render with non-rainbow UI text suitable for ongoing Co-op DX menu-parity work.
- DJUI interaction/visual slice 6: added boxed button-like rows with stronger selected-state tinting and transitioned main-menu options toward donor semantics (`HOST`, `JOIN`, `MODS`, `ENTER GAME`), routing host/join into current offline lobby flow with explicit placeholder status messages to preserve stability during incremental parity work. | files: sm64wiiu/src/pc/djui/djui.c | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; menu now reads more like Co-op DX button panels while keeping Wii U runtime stable and testable.
- DJUI mod-toggle + text-visibility slice 7: moved row-box drawing out of IA-text mode to fix missing/corrupted menu labels, implemented a real local mod catalog (`available` + enabled flags) with default-disabled startup policy, wired MODS page toggles to that catalog, and apply pending toggle sets by reinitializing Lua when leaving DJUI main menu for gameplay. | files: sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/mods/mods.c, sm64wiiu/src/pc/mods/mods.h, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; menu text remains visible and mods can now be enabled/disabled from DJUI with active-count parity updates.
- DJUI main-menu visual parity slice 8: reshaped the main menu to a donor-style centered single-panel layout with centered wide button rows and centered labels (`HOST`, `JOIN`, `OPTIONS`, `QUIT`), removed right-column debug/detail text from the main page, and aligned row/text anchors so options render directly on their button bars like Co-op DX. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; Wii U main menu now visually tracks the Co-op DX composition more closely and resolves the prior row/label misalignment.
- DJUI main-menu alignment touch-up: adjusted centered main-menu option label baseline relative to row centers so text sits within each button bar instead of drifting above it in Cemu. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main menu button text and bars should now be vertically aligned.
- DJUI main-menu donor-row parity touch-up: ported Co-op DX-like row visibility semantics so unselected options keep visible borders/fills (instead of blending into the panel), removed temporary `> <` label markers, and re-tuned centered label baseline/row width anchors to align text with each button bar in Cemu. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main menu now presents explicit four-row button structure with highlight-driven selection like donor behavior.
- DJUI main-menu row/text anchor unification: replaced ad-hoc label Y tuning with shared main-row start/step constants used by both row geometry and text rendering, so future row-spacing tweaks cannot reintroduce button/label drift. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main menu option labels now stay locked to their corresponding bars.
- DJUI main-menu fixed-offset correction: applied explicit row-label offset (`DJUI_ROW_TEXT_Y_OFFSET`) to match actual vanilla box primitive vertical placement in Cemu so option labels land on their intended bars instead of two rows high. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; row labels should now align with highlighted bars during menu navigation.
- DJUI bar-vs-text anchor split: reverted main-option text to centered panel anchors and shifted only row-geometry anchors (`DJUI_MAIN_ROW_DRAW_Y_OFFSET`) so button bars move behind stable text instead of pushing labels into HUD/status lines. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; menu labels retain centered vertical placement while row bars track those labels.
- DJUI main-menu alignment refactor (donor/vanilla math): removed ad-hoc main-page row offsets and derived row center from shared text-row anchors using the same `row center -> text top (-2)` relationship as vanilla pause menu rendering, so row geometry and labels stay coupled across spacing tweaks. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main-page bars and labels now use one coordinate model instead of independent offsets.
- DJUI main-menu stack correction: applied a measured stack-level row-geometry correction (`DJUI_MAIN_ROW_STACK_CORRECTION`) on top of the shared text-to-row mapping to compensate for this Wii U path’s box primitive placement, bringing the four option bars back under `HOST/JOIN/OPTIONS/QUIT` labels. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main-option bar stack should no longer render two rows below labels.
- DJUI main-menu donor-style panel render pivot: replaced main-page row rendering with top-left panel/button layout primitives (shared row box + text relationship) and donor-dark button colors/border semantics, so main menu now follows `djui_panel_main`-style box composition rather than center/scale offset hacks. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main-page geometry/text are now driven by a single panel layout model for further donor parity work.
- DJUI main-menu composition slice (title/footer in-panel): moved main-page title and control/footer lines into the panel bounds and re-tuned button row vertical anchors against that in-panel layout, removing the floating header/footer look while preserving donor-style centered panel/button composition. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main-page text blocks now read as one contained menu panel.
- DJUI main-menu footer overlap fix: anchored main-page footer lines from panel bottom with larger vertical spacing so `ACTIVE MODS` and control hints no longer collide with `QUIT` row text in Cemu. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; footer/status text should now render below action rows instead of overlapping them.
- DJUI text-size polish slice: switched DJUI overlay text rendering from `print_generic_string` to `print_menu_generic_string`, reducing menu text footprint to better match donor readability while keeping existing panel/row coordinates unchanged. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; DJUI main-menu labels and footer text should render slightly smaller and cleaner in Cemu.
- DJUI text renderer rollback: reverted the `print_menu_generic_string` swap and restored `print_generic_string` for DJUI labels after Cemu showed unreadable bar glyphs, confirming this path needs encoding-aware conversion before use. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main-menu text readability is restored while preserving current panel alignment.
- DJUI main-flow parity slice 9: split main menu behavior closer to donor semantics by routing `HOST` to local host-lobby actions, `JOIN` to lobby-list selection, and `OPTIONS` to a dedicated options panel scaffold with pending category placeholders and `BACK`, while keeping existing mods/lobby toggle paths stable. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; main menu now has distinct host/join/options flows for incremental donor panel parity work.
- DJUI host-layout parity slice 10: introduced a dedicated `HOST` page scaffold (`SAVE SLOT`, `SETTINGS`, `MODS`, `BACK`, `HOST`) and separated it from join-lobby detail flow, while applying a shared secondary-page row-anchor conversion (`text Y -> row anchor`) so host/join/options/mod rows align with their labels instead of drifting below text. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; host screen structure now more closely tracks donor intent and row/text alignment on secondary panels is corrected.
- DJUI startup camera-jump fix: changed menu-scene warp handling so `sDjuiMenuWarpPending` no longer forces a same-level warp at boot; it now only warps when current level/area differs from the menu destination, eliminating the visible over-water pre-jump before castle-spawn lock. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; next boot should show the menu scene already at spawn without the startup camera travel.
- DJUI host-subpanel parity slice 11: replaced `HOST` placeholder actions with navigable `HOST SAVE` and `HOST SETTINGS` pages, added donor-style back routing (`B` returns to host page), save-slot row rendering with live existence/star-count metadata, in-menu save-slot selection (`gCurrSaveFileNum`), and a local host-settings value scaffold (interaction/star policy/toggles/player count) for incremental donor parity without introducing networking risk. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; host flow now has real subpage navigation and selectable save/settings semantics instead of pending placeholders.
- DJUI host-panel donor-structure slice 12: replaced the host page’s legacy list/panel composition with a donor-shaped centered host panel and unified row renderer (full-width setup rows + split footer actions), ported visible host control scaffolding (`NETWORK SYSTEM`, `PORT`, `PASSWORD`) and wired `A` actions to cycle/toggle those states while preserving existing save/settings/mods subpage routing and offline-safe host start behavior. | files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; host page now follows donor structure more closely and no longer depends on the old drifting secondary-row anchor path.
- DJUI page-stack + pause-return slice 13: introduced explicit page-stack navigation in Wii U DJUI (`push/pop/reset`) and migrated panel transitions to stack semantics so root main menu ignores `B` instead of dropping into gameplay; added a new pause-screen `MAIN MENU` option that returns directly to DJUI main menu from gameplay pause flow. | files: sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/game/area.h, sm64wiiu/src/game/ingame_menu.c, sm64wiiu/src/game/level_update.c, SUMMARY.md | validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb` | outcome: build + wuhb succeed; pause now has an in-game path back to DJUI main menu and nested menu back behavior is stack-driven.
- DJUI input debounce + held-nav cadence slice 14: added explicit A/B release debounce guards to stop held-button repeat across panel transitions, and aligned vertical menu hold-repeat timing with donor-style cadence (initial hold delay + steady repeat) for D-pad/C-buttons/stick navigation.
  - files: sm64wiiu/src/pc/djui/djui.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4 && make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; menu navigation should no longer chain unintended page actions while A/B is held.
  - gotcha: running `make` and `make wuhb` concurrently can race on RPX strip artifacts; run sequentially.
- DJUI dual-path donor foundation slice 15: introduced a compile-safe donor migration spine with a default-off runtime gate (`gDjuiUseDonorStack`) and dispatch hooks in the existing DJUI lifecycle (`init/init_late/update/render/shutdown/open/close/update_menu_level`), then added first-pass donor-style core/panel primitives (`djui_types`, `djui_base`, `djui_root`, `djui_rect`, `djui_flow_layout`, `djui_text`, `djui_three_panel`, `djui_panel`, `djui_panel_menu`, `djui_donor`) for incremental panel-stack migration while preserving current legacy menu behavior.
  - files: sm64wiiu/src/pc/djui/djui.h, sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/djui/djui_donor.h, sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/djui/djui_types.h, sm64wiiu/src/pc/djui/djui_interactable.h, sm64wiiu/src/pc/djui/djui_base.h, sm64wiiu/src/pc/djui/djui_base.c, sm64wiiu/src/pc/djui/djui_root.h, sm64wiiu/src/pc/djui/djui_root.c, sm64wiiu/src/pc/djui/djui_gfx.h, sm64wiiu/src/pc/djui/djui_gfx.c, sm64wiiu/src/pc/djui/djui_rect.h, sm64wiiu/src/pc/djui/djui_rect.c, sm64wiiu/src/pc/djui/djui_flow_layout.h, sm64wiiu/src/pc/djui/djui_flow_layout.c, sm64wiiu/src/pc/djui/djui_text.h, sm64wiiu/src/pc/djui/djui_text.c, sm64wiiu/src/pc/djui/djui_three_panel.h, sm64wiiu/src/pc/djui/djui_three_panel.c, sm64wiiu/src/pc/djui/djui_panel.h, sm64wiiu/src/pc/djui/djui_panel.c, sm64wiiu/src/pc/djui/djui_panel_menu.h, sm64wiiu/src/pc/djui/djui_panel_menu.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; legacy DJUI remains the active path by default, with donor framework now available for incremental cutover slices.
- DJUI donor interaction/panel-main slice 16: added donor-style interactable runtime (`djui_interactable.c`), button widget layer (`djui_button.[ch]`), and first donor panel entry (`djui_panel_main.[ch]`), then wired donor update/render through interactable+cursor+panel stack; in parallel, strengthened legacy menu A/B edge handling via `buttonDown`-derived pressed edges to eliminate repeated page activations from held or noisy inputs.
  - files: sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/djui/djui_root.h, sm64wiiu/src/pc/djui/djui_root.c, sm64wiiu/src/pc/djui/djui_interactable.h, sm64wiiu/src/pc/djui/djui_interactable.c, sm64wiiu/src/pc/djui/djui_button.h, sm64wiiu/src/pc/djui/djui_button.c, sm64wiiu/src/pc/djui/djui_panel_main.h, sm64wiiu/src/pc/djui/djui_panel_main.c, sm64wiiu/src/pc/djui/djui_panel.c, sm64wiiu/src/pc/djui/djui_panel_menu.c, sm64wiiu/src/pc/djui/djui_base.c, sm64wiiu/src/pc/djui/djui_rect.c, sm64wiiu/src/pc/djui/djui_text.c, sm64wiiu/src/pc/djui/djui_cursor.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor panel stack now has working controller edge semantics and main panel scaffold, while legacy active menu input is less prone to accidental double-activation.

### 2026-02-08
- DJUI donor input hold fix: masked held A/B across panel transitions and allowed C-button dpad navigation in the donor interactable path to stop long-press chain transitions and make Wii U D-pad mapping work.
  - files: sm64wiiu/src/pc/djui/djui_interactable.c, sm64wiiu/src/pc/djui/djui_interactable.h, sm64wiiu/src/pc/djui/djui_panel.c, SUMMARY.md
  - validation: not run (analysis-only change)
  - outcome: should prevent held-button menu chaining and restore D-pad navigation for Wii U mapping.
- DJUI donor menu-scene control lock parity: implemented donor-stack `update_menu_level` menu-scene enforcement (warp-to-menu destination, fixed spawn/camera pose, per-frame Mario/controller zeroing) so player input no longer drives Mario while donor main-menu panels are active.
  - files: sm64wiiu/src/pc/djui/djui_donor.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor DJUI menu mode now matches legacy/co-op-dx expectation that gameplay control is suppressed while menu is open.
- DJUI donor base-render parity hardening: ported core `djui_base` clipping/border-child semantics closer to donor behavior (parent clip propagation, clipped border rendering, render-abort when clip is empty, child-render failure gating, and interactable global-pointer cleanup on destroy) to reduce panel overdraw/stack artifacts during donor panel transitions.
  - files: sm64wiiu/src/pc/djui/djui_base.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor panel stack now uses stricter clip-aware base rendering aligned with Co-op DX structure.
- DJUI donor panel-stack alignment slice: removed auto-generated `three_panel` children and switched menu body construction to donor-style explicit header/body creation, ported flow-layout child-placement semantics, and added release-gated interactable clicks/back plus per-frame gameplay input clamp during main-menu mode.
  - files: sm64wiiu/src/pc/djui/djui_three_panel.c, sm64wiiu/src/pc/djui/djui_flow_layout.c, sm64wiiu/src/pc/djui/djui_panel_menu.c, sm64wiiu/src/pc/djui/djui_interactable.c, sm64wiiu/src/game/level_update.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor stack now avoids duplicate panel-layer overlap and should stop chained A/B transitions while suppressing Mario control during menu panels.
- DJUI donor panel sizing correction: fixed `djui_panel_menu` to use donor-consistent relative-height scale (`1.0f`, not `150.0f`) and restored donor header minimum size (`64`) so menu rows are no longer clipped/reordered by an oversized panel extent.
  - files: sm64wiiu/src/pc/djui/djui_panel_menu.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; main-menu option order/alignment should now track the intended top-down donor layout (HOST first).
- DJUI donor-y-axis compatibility correction: restored Wii U-oriented `three_panel` and `flow_layout` vertical placement semantics (top-down in current render space) and returned menu panel dimensions/header offsets to the known-good envelope so option ordering and box/text anchors no longer invert.
  - files: sm64wiiu/src/pc/djui/djui_three_panel.c, sm64wiiu/src/pc/djui/djui_flow_layout.c, sm64wiiu/src/pc/djui/djui_panel_menu.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; main menu should render in forward order (`HOST`, `JOIN`, `OPTIONS`, `QUIT`) with bars aligned to text rows again.
- DJUI donor input + panel-layout follow-up: removed cursor-to-focus hard-capture in donor cursor navigation (which blocked repeated directional movement) and switched three-panel sizing to donor-like padding-aware section allocation in Wii U Y-space so button rows can move both directions and avoid header/body spacing drift.
  - files: sm64wiiu/src/pc/djui/djui_cursor.c, sm64wiiu/src/pc/djui/djui_three_panel.c, SUMMARY.md
  - validation: `make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; dpad/analog navigation should no longer be one-way, and row stack spacing should be more stable against panel padding.
- DJUI donor parity bulk sync + Wii U compatibility bridge: imported and wired donor DJUI modules broadly, added missing subsystem shims and symbol adapters (network/mods/lua utils/ini-thread-update helpers, DynOS map helpers, donor globals/texture/runtime symbols), and resolved final linker blockers so donor-default path and packaging both build cleanly again.
  - files: sm64wiiu/src/pc/djui/*, sm64wiiu/src/pc/network/*, sm64wiiu/src/pc/mods/*, sm64wiiu/src/pc/lua/utils/*, sm64wiiu/src/pc/utils/hmap.c, sm64wiiu/src/game/level_update.c, sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/goddard/renderer.c, sm64wiiu/bin/custom_textures.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4 -B`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; `sm64wiiu/build/us_wiiu/sm64.us.rpx` and `sm64wiiu/build/us_wiiu/sm64.us.wuhb` are produced with donor DJUI stack compiling by default and legacy toggle retained.
  - gotcha: donor HUD texture externs assume globally-visible symbols, but this tree keeps many as `static` in `bin/segment2.c`; exporting donor-required aliases in `bin/custom_textures.c` avoids linker failures without destabilizing vanilla segment data.
- DJUI donor visual fix (blank labels/garbled logo): corrected Wii U fallback texture override path so 32b donor textures are loaded as 32b (instead of forced 16b), then aligned donor render setup with shared DJUI HUD/reset/display-list init and Wii U fullscreen viewport symbol mapping.
  - files: sm64wiiu/src/pc/djui/djui_gfx.c, sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/djui/djui_hud_utils.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor main menu should no longer show the corrupted blue logo or missing text caused by incorrect RGBA32 texture handling.
- DJUI donor opcode-path stabilization (frame-1 hang fix): ported donor `gbi_extension` command surface into Wii U (`gbi_extension.h` + renderer command handlers), added DJUI override/clipping execution and direct large-atlas upload path in `gfx_pc.c`, and hardened textured-triangle path against null rejected imports so donor menu textures no longer rely on unsafe placeholder/fallback behavior that could stall in `gfx_run_dl`.
  - files: sm64wiiu/include/PR/gbi.h, sm64wiiu/include/PR/gbi_extension.h, sm64wiiu/src/pc/gfx/gfx_pc.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor DJUI now uses donor opcode semantics on Wii U for texture override/clipping instead of fragile macro fallback emulation.
  - gotcha: donor texture override commands reference large atlases that exceed the classic 4KB N64 import assumptions; processing them through standard placeholder load/import can leave stale/null texture nodes and destabilize frame 1.
- DJUI language-content sync + run-dl watchdog hardening: extended Wii U content sync to mirror `lang/*.ini` into `content/lang`, made Wii U resource path resolve to `/vol/content`, and added defensive display-list recursion/command-budget abort guards in `gfx_run_dl` to prevent frame-1 hangs from malformed donor UI command chains.
  - files: sm64wiiu/tools/sync_builtin_mod_assets.sh, sm64wiiu/Makefile, sm64wiiu/src/pc/platform.c, sm64wiiu/src/pc/gfx/gfx_pc.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor panel text can load language INIs from packaged content and bad display-list loops now fail fast with diagnostics instead of hanging Cemu indefinitely.
- DJUI donor pad-input recovery + quit-label fallback: routed controller readback to donor interactable pad when override is active (`gInteractableOverridePad`), and changed the donor main-panel quit button label source from `MAIN.QUIT` to `MAIN.QUIT_TITLE` to avoid malformed lowercase quit glyph rendering on Wii U.
  - files: sm64wiiu/src/game/game_init.c, sm64wiiu/src/pc/djui/djui_panel_main.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor DJUI main menu can read Wii U controller input again and quit row now uses an uppercase donor key path.
  - gotcha: donor interactable updates consume `gInteractablePad`, not `gControllerPads[0]`; if override mode is active and this feed is skipped, the menu appears alive but cannot be controlled.
- Wii U controller backend macro fix for donor DJUI input: corrected controller implementation selection to use `TARGET_WII_U` (plus `__WIIU__` fallback) so Wii U builds no longer silently choose SDL input backend in `osContGetReadData` flow.
  - files: sm64wiiu/src/pc/controller/controller_entry_point.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; Wii U controller backend is now selected in the input pipeline for donor DJUI menu navigation.
  - gotcha: `wut.specs` defines `ESPRESSO`/`TARGET_WII_U` in this project, not `__WIIU__`; relying only on `__WIIU__` can silently route Wii U builds to non-Wii-U input backends.
- DJUI donor menu-input recovery follow-up: aligned donor interaction update timing with donor render order (cursor update first, interactables second), pinned override-pad activation to active donor menu panels, added cursor fallback to first interactable when input base is missing, and exported `gd_texture_hand_closed` for donor cursor click-state rendering on Wii U.
  - files: sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/djui/djui_cursor.c, sm64wiiu/src/goddard/renderer.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor menu cursor/input path now has a deterministic fallback and panel-active override wiring for Wii U retest.
  - gotcha: donor cursor uses `gd_texture_hand_closed`, but this tree had it `static` in Goddard renderer; keeping it internal breaks donor cursor linkage once the closed-hand path is exercised.
- DJUI join-action mapping + quit glyph fallback: added Wii U client-availability gating for donor join panels (`join`, `join_direct`, `join_lobbies`) so unavailable network actions fail fast with clear UI errors/disabled buttons, and restored main `QUIT` row label to the stable uppercase donor key for Wii U font reliability.
  - files: sm64wiiu/src/pc/network/network.h, sm64wiiu/src/pc/network_stubs.c, sm64wiiu/src/pc/djui/djui_panel_join.c, sm64wiiu/src/pc/djui/djui_panel_join_direct.c, sm64wiiu/src/pc/djui/djui_panel_join_lobbies.c, sm64wiiu/src/pc/djui/djui_panel_main.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor menu options now map to concrete behavior on Wii U stubs (no join hang path), and quit label rendering is stabilized for the main panel.

### 2026-02-09
- DJUI glyph clip artifact hardening: clamped/rounded DJUI clip percentages in `djui_gfx`, added a tiny per-glyph clip margin in `djui_text`, and synced override texture metadata in `gfx_pc` so bottom-row/back-button glyphs and selectionbox text no longer degrade into black block fragments near clip bounds.
  - files: sm64wiiu/src/pc/djui/djui_gfx.c, sm64wiiu/src/pc/djui/djui_text.c, sm64wiiu/src/pc/gfx/gfx_pc.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor DJUI text clipping path is now deterministic on Wii U and prepared for runtime retest of `QUIT`/`BACK`/selectionbox labels.
  - gotcha: clip percentages are byte-packed in display-list commands, so any out-of-range float at emit time can wrap and clip the wrong glyph region.
- DJUI main-menu donor visual parity follow-up: removed the temporary `MAIN.QUIT_TITLE` fallback and restored donor `MAIN.QUIT` in `djui_panel_main`, then aligned Wii U donor compatibility defaults to dark DJUI theme (`configDjuiTheme = 1`) so first-boot button/text styling matches donor baseline.
  - files: sm64wiiu/src/pc/djui/djui_panel_main.c, sm64wiiu/src/pc/configfile_djui_compat.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; main menu now uses donor-case quit text (`Quit`) and donor-default dark panel theme on Wii U.
- DJUI option/backend wiring slice: expanded Wii U config persistence to donor option keys, added `enable-mod:` queued apply at startup, synchronized host-mod checkbox state to canonical mod-enable arrays, and wired display controls (`force_4by3`, `texture_filtering`, `draw_distance`) into active runtime paths.
  - files: sm64wiiu/src/pc/configfile.c, sm64wiiu/src/pc/configfile_djui_compat.c, sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/pc/mods/mods_utils.c, sm64wiiu/src/pc/djui/djui_panel_host_mods.c, sm64wiiu/src/pc/djui/djui_panel_menu.c, sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_pc.h, sm64wiiu/src/engine/behavior_script.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; options/host-mod toggles now persist and affect runtime state in Wii U donor DJUI flow.
- DJUI donor menu-option + host-mod runtime wiring follow-up: moved donor menu scene/music/random/staff-roll handling into `djui_donor_update_menu_level` with donor presets, re-enabled donor fps/ctx/lua overlay runtime updates in donor mode, and routed host-mod checkbox toggles through canonical `mods_set_available_script_enabled`.
  - files: sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/djui/djui_panel_host_mods.c, sm64wiiu/src/pc/djui/djui_panel_menu_options.h, sm64wiiu/src/pc/pc_main.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor menu options now drive active menu-level runtime state (instead of fixed castle-only behavior) and host-mod toggles now update canonical script-enable flags directly.
  - gotcha: running `make` and `make wuhb` in parallel can race RPX strip/pack (`powerpc-eabi-strip ... invalid operation`); run them sequentially.
- Donor menu-scene hang mitigation + renderer guards: moved donor menu-level changes to deferred level transitions (`gChangeLevelTransition`) for level swaps, limited direct warps to area-only changes, and added explicit Lakitu focus/position/speed locks while menu mode is active; also added GX2/renderer safety checks for zero texture dimensions and null transient VBO allocation to reduce frame-1 stall risk.
  - files: sm64wiiu/src/pc/djui/djui_donor.c, sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/gfx/gfx_gx2.cpp, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; donor menu-level changes now follow level-update transition flow for level swaps, area-only warps stop repeating, and renderer guards remain in place for malformed startup draw data.
- Wii U menu-level option unlock with startup safety retained: removed the temporary Wii U force-lock of donor menu-level/random/staff-roll controls while keeping Wii U framerate safety defaults (`vsync=true`, `framerate_mode=auto`, `interpolation=accurate`) enforced in config/display paths to preserve stable launches.
  - files: sm64wiiu/src/pc/configfile.c, sm64wiiu/src/pc/djui/djui_panel_menu_options.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; menu-level options are editable again on Wii U while launch-stable display defaults remain guarded.
- Host-mod apply-now wiring in donor host panel: kept persisted host-mod selections restored at boot without auto-loading Lua scripts, and wired donor host actions so selected mods reload into the current runtime when `HOST` (new server) or `APPLY` (already hosting) is pressed.
  - files: sm64wiiu/src/pc/djui/djui_panel_host.c, sm64wiiu/src/pc/djui/djui_panel_host_message.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; host-mod selections remain persisted for next boot, while runtime activation now occurs on explicit host/apply action instead of requiring restart.
- Wii U interpolation pacing slice 1: moved non-N64 display-list submission out of `display_and_vsync`, added first donor-style multi-present pacing loop in `pc_main`, and allowed GX2 swap interval `1` only for interpolation-enabled 60Hz+ targets while retaining 30Hz simulation timing.
  - files: sm64wiiu/src/game/game_init.c, sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/pc/gfx/gfx_gx2_window.cpp, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; runtime verification pending Cemu test for launch stability and behavior under interpolation/framerate settings.
- Menu interpolation flag/matrix decouple slice: removed menu-wide interpolation disable in `pc_main` and switched `gfx_pc` matrix interpolation to a dedicated active check (`gRenderingInterpolated && !gDjuiInMainMenu`), keeping menu matrix flicker mitigation while allowing donor panel transition timing to run on interpolated frames.
  - files: sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/pc/gfx/gfx_pc.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; ready for Cemu retest of menu smoothness (should improve) while preserving the no-flicker menu rendering path.
- Interpolation flag semantic alignment slice: changed `pc_main` to keep `gRenderingInterpolated` true across all interpolation subframes (matching donor pacing semantics), moved matrix interpolation activation to `gRenderingDelta < 1.0` and non-menu context in `gfx_pc`, and updated donor DJUI input update gating to run on the final subframe to retain stable once-per-tick input handling.
  - files: sm64wiiu/src/pc/pc_main.c, sm64wiiu/src/pc/gfx/gfx_pc.c, sm64wiiu/src/pc/djui/djui_donor.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; ready for Cemu retest to verify whether donor menu transitions now subjectively animate at 60fps while keeping previous no-flicker menu behavior.
- Renderer startup crash guard slice: replaced Wii U `WHBLogPrintf` calls in `gfx_pc.c` hot render/import paths with a compile-time-disabled macro (`GFX_WIIU_VERBOSE_LOGS=0`) to remove `_svfprintf_r` launch-crash risk while preserving render logic.
  - files: sm64wiiu/src/pc/gfx/gfx_pc.c, SUMMARY.md
  - validation: `export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"; make -C sm64wiiu -j4`, `make -C sm64wiiu wuhb`
  - outcome: build + wuhb succeed; ready for Cemu relaunch retest with renderer trace logging disabled.

### 2026-02-10
- Mod HUD persistence + HUD `x` glyph parity: reset the DJUI HUD per-frame Z accumulator before Lua HUD hooks and fixed the HUD font fallback mapping so the `@` “x” glyph used by built-in mods renders on vanilla US HUD LUTs.
  - files: sm64wiiu/src/game/hud.c, sm64wiiu/src/pc/djui/djui.h, sm64wiiu/src/pc/djui/djui.c, sm64wiiu/src/pc/djui/djui_font.c, SUMMARY.md
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build + wuhb succeed; ready for Cemu retest of mod HUD persistence + star-counter `x` glyph.

### 2026-02-11
- Pause/menu + Flood enablement pass: ensured in-game pause always renders course options (so `MAIN MENU` can be selected even when `ACT_FLAG_PAUSE_EXIT` is not set), routed post-star `SAVE & QUIT` back into DJUI main menu on Wii U, expanded Lua compatibility bindings used by Flood-style menus, fixed multi-file mod script ordering to donor-style lexical `.lua`/`.luac` execution, and patched Lua 5.3.6 undump to accept typical PC `.luac` chunks (little-endian + 64-bit `size_t`) on Wii U.
  - files: sm64wiiu/src/game/ingame_menu.c, sm64wiiu/src/game/mario_actions_cutscene.c, sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/third_party/lua-5.3.6/src/lundump.c, SUMMARY.md
  - validation: `tail -n 300 "$HOME/Library/Application Support/Cemu/log.txt"`, `./build_wiiu_then_wuhb.sh`
  - outcome: build + wuhb succeed; pause has a stable `MAIN MENU` route back into DJUI, and compiled-only mods like Flood Expanded are now loadable on big-endian Wii U.
- Flood bytecode load + donor pause-options parity follow-up: switched Wii U VFS Lua chunk load mode to accept both text and bytecode (`"bt"`) so `.luac` Flood modules no longer fail with `mode is 't'`, then restored donor pause-entry behavior by wiring `R` in pause flow and rendering/updating donor DJUI pause overlays/panels outside main-menu mode.
  - files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/game/ingame_menu.c, sm64wiiu/src/pc/djui/djui_donor.c, SUMMARY.md
  - validation: `tail -n 350 "$HOME/Library/Application Support/Cemu/log.txt"`, `./build_wiiu_then_wuhb.sh`
  - outcome: build + wuhb succeed; Flood `.luac` modules are no longer blocked by text-only load mode, and pause `R Button - Options` path is back on donor stack for in-game settings access.
- Flood runtime API + pause host-exit follow-up: added missing Lua API bindings observed in Cemu logs, added `MarioState.area` with Area/Camera cobject field access (including `area.camera.cutscene` path), and fixed pause `STOP HOSTING` confirmation to route back to DJUI main menu after shutdown.
  - files: sm64wiiu/src/pc/lua/smlua.c, sm64wiiu/src/pc/lua/smlua_cobject.c, sm64wiiu/src/pc/lua/smlua_cobject.h, sm64wiiu/src/pc/djui/djui_panel_pause.c, SUMMARY.md
  - validation: `tail -n 220 "$HOME/Library/Application Support/Cemu/log.txt"`, `./build_wiiu_then_wuhb.sh`
  - outcome: build + wuhb succeed; prior repeating Flood hook failures (`get_dialog_id`, `set_ttc_speed_setting`, `m.area`) are addressed in runtime bindings/cobjects, and stop-host now returns to main menu.

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
