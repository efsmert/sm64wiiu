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
- `flood-expanded-release`: Flood mod - use to easily debug the Wii U mod-loader behavior/parity

## 3) Current State Snapshot
- Baseline Wii U build is established and reproducible.
- Port includes major runtime compatibility groundwork (FS, API uplift, Lua/mod runtime, hooks, cobject, assets).
- Main remaining objective: continue feature parity hardening while keeping startup/runtime stable.

## 4) Build and Artifacts

From repo root, run:

```bash
./build_wiiu_then_wuhb.sh
```

## 5) Implemented Work (Deduplicated)

### A) Build system and baseline
- Added GNU Make 3.81 compatibility fixes (`Makefile`, `Makefile.split`) and generation-order fixes.
- Baseline Wii U compile is stable with valid baserom.
- Added optional Wii U crash-debug build mode (`WIIU_CRASH_DEBUG=1`) with `-O0 -g3`, frame-pointer-preserving flags, and linker map output, plus `tools/wiiu_decode_cemu_crash.sh` to decode Cemu crash addresses to source lines.

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
- Added Flood follow-up parity for spectator/runtime hooks: exposed `calculate_yaw` to Lua and made `MarioState.squishTimer` writable/readable in Wii U cobject reflection to stop per-frame hook faults during active rounds.
- Reordered DynOS asset activation to occur after Lua hook table init and re-enabled custom behavior global export (`dynos_behavior_hook_all_custom_behaviors`) so `.bhv` IDs remain registered through startup.
- Added custom-level registration mod-index wiring (`smlua_level_util_set_register_mod_index`) aligned with enabled root-script order so `level_register()` records the same DynOS mod index used during asset activation.
- Restored donor-style `level_cmd_begin_area` geo loading on Wii U by removing the raw-geo bypass and always routing through `dynos_model_load_geo`, avoiding direct geo parser entry on unresolved `0x1xxxxxxx` custom-level addresses.
- Fixed DynOS geo binary load endianness handling for Wii U by preserving raw geo command-word bytes while decoding pointer tokens from swapped values, so custom geo command streams and pointer substitutions both remain valid on big-endian.

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
- Ported `define_custom_obj_fields()` behavior from donor side into Wii U cobject runtime: declared object custom fields now register typed schemas (`u32`/`s32`/`f32`) and return typed zero defaults when unset, avoiding nil arithmetic faults in Flood-style per-object counters.
- Added missing donor Lua gameplay helpers `calculate_pitch` and `mario_drop_held_object` to the Wii U compatibility surface, resolving Flood spectator hook nil-function loops during round transitions.

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
- Hardened Mario re-init against transition-time null globals (`gMarioObject`, `gMarioSpawnInfo`) and ensured `statusForCamera` fallback initialization so Lua-triggered `init_single_mario()` calls during host/countdown transitions cannot null-deref in `init_mario()`.

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
- DynOS custom behavior IDs must be exported after `smlua_bind_hooks()`/`smlua_clear_hooks()` has run; exporting `.bhv` globals before hook-state init can silently clear the custom behavior registry and surface nil behavior IDs later.
- For Cemu crash triage, use `sm64wiiu/tools/wiiu_decode_cemu_crash.sh` after reproducing; it parses the latest `Error: signal` block and resolves IP/LR/ReturnAddr against the current Wii U ELF.
- In `level_cmd_begin_area`, custom-level geo pointers that look like `0x1xxxxxxx` still need DynOS resolution; direct `process_geo_layout` on that value can fault in early geo commands (`geo_layout_cmd_node_ortho_projection`/`_perspective`) during host/start transitions.

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
- Major baseline + pipeline bring-up: toolchain/build reproducibility, Wii U packaging pipeline, initial Wii U platform/FS substrate (write policy + config/save/EEPROM on FS-backed I/O).
- Runtime parity foundation: Lua 5.3.6 + mod activation, cobject typed userdata/reflection, hook core + key callsites, and broad compatibility slices (storage/HUD/DJUI/network shims/model/math/sound + day-night-cycle + renderer lighting/fog + sync/audio + far-clip + assets).

### 2026-02-07
- Stability-first Lua runtime hardening: init ordering, iteration/perf, loader/VFS safety, instruction-budget guardrails, recursion guards, diagnostics, and bootstrap/constants strategy iteration.
- Continued parity fixes: sync-table init alignment, storage helpers, guarded `.m64` injection path retained, and compatibility surface expansions used by built-in mods.

### 2026-02-08
- Donor DJUI parity ramp: input-hold fixes, menu-scene control lock (warp/spawn/camera + input suppression), and base clipping/layout parity to reduce transition/render artifacts.
- Interpolation + renderer stability: aligned interpolation semantics (including menu-specific behavior) and disabled risky high-volume formatted renderer logs implicated in intermittent `_svfprintf_r` startup crashes.

### 2026-02-10
- HUD parity: fixed mod HUD draw persistence and restored the built-in-mod “x” glyph rendering on US HUD assets.

### 2026-02-11
- Flood Expanded enablement + pause parity: enabled Wii U Lua to load typical PC `.luac` (undump accept + `"bt"` chunk mode), expanded missing runtime bindings/cobject fields surfaced by Flood logs, and restored donor-style pause options entry (`R`) with stable main-menu routing (including stop-host returning to DJUI main menu).
- Flood custom-level warp parse fix attempt: switched DynOS level-command reads in the custom level parser from raw integer casts to typed field reads (`u8/s16/u32/pointer`) so big-endian Wii U command decoding matches engine `CMD_GET` behavior and custom warp metadata can resolve entry warps.
  - files: `sm64wiiu/data/dynos_level.cpp`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; runtime verification pending next Cemu host test log.
  - gotcha: casting a pointer-sized raw command read to `u8/s16` is endian-fragile on Wii U and can silently break level-script field decoding.
- Flood hook-runtime table shim fix: expanded Wii U single-player Lua compatibility table population to initialize all `MAX_PLAYERS` slots for `gMarioStates`, `gNetworkPlayers`, and `gPlayerSyncTable` with non-nil defaults (including player names/indices), preventing per-frame Flood hook faults (`active_player`/scoreboard nil access) after custom-level entry.
  - files: `sm64wiiu/src/pc/lua/smlua.c`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; runtime verification pending next Cemu host test log.
  - gotcha: scripts compiled for Co-op DX assume dense `MAX_PLAYERS`-indexed Lua globals even in local sessions; sparse single-player tables can stall gameplay logic without crashing.
- Flood custom-level execution layout fix + startup overlay disable: in DynOS level binary load, preserved raw level-command word byte layout for in-memory scripts while still endian-normalizing values for pointer-token decode/validation; also forced Wii U startup mod overlay lifetime to zero frames to remove transient mod-name/count HUD rendering.
  - files: `sm64wiiu/data/dynos_bin_lvl.cpp`, `sm64wiiu/src/pc/lua/smlua.c`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; runtime verification pending next Cemu host/start test.
  - gotcha: level-script command words are byte-encoded control streams, so endian-normalizing then storing the normalized integer can corrupt command byte layout on big-endian targets even when pointer markers decode correctly.
- Flood custom-object field parity slice: replaced Wii U `define_custom_obj_fields()` no-op with donor-style registration path in cobject runtime and added typed zero-default fallback for declared fields during object-field reads, so unset declared counters no longer propagate `nil` into per-frame Flood hook math.
  - files: `sm64wiiu/src/pc/lua/smlua.c`, `sm64wiiu/src/pc/lua/smlua_cobject.c`, `sm64wiiu/src/pc/lua/smlua_cobject.h`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; runtime verification pending next Cemu host/start test.
  - gotcha: treating declared custom object fields as generic dynamic-table keys returns `nil` for first read, but donor behavior expects typed zero initialization semantics for arithmetic-safe per-object counters.
- Flood spectator helper parity slice: exposed donor Lua globals `calculate_pitch` and `mario_drop_held_object` in Wii U smlua bindings so Flood spectator update/action hooks no longer fault every frame with nil global lookups during post-countdown stage start.
  - files: `sm64wiiu/src/pc/lua/smlua.c`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; runtime verification pending next Cemu host/start test.
  - gotcha: repeated nil-global hook failures can present as black-screen/frozen-frame symptoms even when rendering and audio threads remain alive.
- Flood runtime parity follow-up (yaw/squish + behavior/custom-level registration order): added Lua `calculate_yaw`, writable `MarioState.squishTimer`, moved DynOS asset activation after Lua hook init, re-enabled `.bhv` global export, and aligned custom-level registration mod indices with active root-script order to keep DynOS level/behavior lookups stable during host/start.
  - files: `sm64wiiu/src/pc/lua/smlua.c`, `sm64wiiu/src/pc/lua/smlua_cobject.c`, `sm64wiiu/src/pc/lua/utils/smlua_level_utils.c`, `sm64wiiu/src/pc/lua/utils/smlua_level_utils.h`, `sm64wiiu/src/pc/mods/mods.c`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; next runtime check should confirm Flood no longer spams `calculate_yaw`/`squishTimer` hook errors and custom-stage transition progresses past countdown.
  - gotcha: when DynOS and Lua use different mod-index ordering, custom-level metadata can register successfully but still fail at runtime resolution paths that rely on consistent per-mod IDs.

### 2026-02-12
- Flood custom-level script resolution parity slice: aligned DynOS custom level script-node selection with CoopDX fallback behavior (prefer exact/normalized match, then last-node fallback), and added bounded Wii U diagnostics for custom warp-entry misses to expose script/slot/warp-count state when `warp_to_level()` returns false.
  - files: `sm64wiiu/data/dynos_mgr_lvl.cpp`, `sm64wiiu/data/dynos_level.cpp`
  - validation: `./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; next Cemu run should show whether Flood custom-stage requests still miss warps and, if so, with direct slot/script visibility in log.
  - gotcha: selecting the first `_entry` script inside a multi-script DynOS level bundle can bind `level_register()` to a script that executes but does not expose expected spawn/warp metadata, causing repeated black-screen warp attempts.
- Wii U crash diagnostics workflow uplift: added `WIIU_CRASH_DEBUG=1` Makefile mode for symbol-rich/debug-friendly Wii U builds and linker map emission, and added `tools/wiiu_decode_cemu_crash.sh` to decode latest Cemu crash addresses into function/file:line output.
  - files: `sm64wiiu/Makefile`, `sm64wiiu/tools/wiiu_decode_cemu_crash.sh`
  - validation: `PATH="/opt/devkitpro/devkitPPC/bin:$PATH" WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh`, `sm64wiiu/tools/wiiu_decode_cemu_crash.sh | sed -n '1,80p'`
  - outcome: debug build completed with Crash Debug enabled; decoder script resolved latest Cemu crash block to concrete source locations and reported map-file availability.
- Flood host crash root-cause alignment (begin-area geo load path): removed Wii U-only raw geo bypass for `geoLayoutAddr >= 0x10000000` in `level_cmd_begin_area` and restored donor DynOS geo-load path so custom-level geo tokens resolve before graph-node parsing.
  - files: `sm64wiiu/src/engine/level_script.c`
  - validation: `PATH="/opt/devkitpro/devkitPPC/bin:$PATH" WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; next host-button repro should confirm whether the geo-layout signal-10 crash is eliminated.
  - gotcha: decoded crash IP/LR in `geo_layout_cmd_node_ortho_projection`/`geo_layout_cmd_node_perspective` with register state showing `0x10000000` strongly indicates unresolved custom-level geo addresses reaching the parser directly.
- Flood custom-geo load endian fix (DynOS): changed `DynOS_Geo_Load` to keep raw geo command bytes in memory and use swapped values only for pointer-token decoding, preventing malformed geo streams and unresolved token words from entering `process_geo_layout` on Wii U.
  - files: `sm64wiiu/data/dynos_bin_geo.cpp`
  - validation: `PATH="/opt/devkitpro/devkitPPC/bin:$PATH" WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; next Flood host repro should confirm whether the `dynos_model_load_geo`/`process_geo_layout` crash path is resolved.
  - gotcha: DynOS binary pointer tokens are authored in little-endian token order, while script/geo command streams must remain byte-faithful in memory; using one representation for both breaks big-endian runtime parsing.
- Flood host countdown crash guard (Lua -> init_single_mario path): added null guards in `init_mario()`/`init_single_mario()` for transition windows where `gMarioObject` is temporarily unavailable, plus camera-state fallback init, to avoid `signal 10` crashes in Mario init during custom behavior callbacks.
  - files: `sm64wiiu/src/game/mario.c`
  - validation: `PATH="/opt/devkitpro/devkitPPC/bin:$PATH" WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh`
  - outcome: build and WUHB packaging succeeded; next host/countdown repro should show whether crash is replaced by stable recovery or a new deeper fault.

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
