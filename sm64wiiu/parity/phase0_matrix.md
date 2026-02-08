# Phase 0 Parity Matrix (Co-op DX -> Wii U)

- Donor root: `sm64coopdx`
- Wii U root: `sm64wiiu`

## Lua Symbol Parity

- Donor registered globals: **2007**
- Wii U registered globals: **204**
- Shared globals: **199**
- Missing on Wii U: **1808**
- Wii U-only globals: **5**

### Missing Globals Used By Current Wii U Built-In Mods

- None

## Hook Surface Parity

- HOOK_* enum count (donor): **60**
- HOOK_* enum count (wiiu): **60**
- Shared HOOK_*: **60**
- Missing HOOK_* on Wii U enum: **0**
- ACTION_HOOK_* count (donor): **2**
- ACTION_HOOK_* count (wiiu): **0**

### Hook Dispatch Callsite Coverage

- Donor total hook callsites (`src/game`, `src/engine`, `src/audio`): **93**
- Wii U total hook callsites (`src/game`, `src/engine`, `src/audio`): **20**
- Unique hook call helpers in donor: **4**
- Unique hook call helpers in Wii U: **11**
- Missing unique hook helper calls on Wii U (preview): `smlua_call_action_hook`, `smlua_call_behavior_hook`, `smlua_call_hook`

## Module Tree Parity

- `src/pc/lua`: donor 46, wiiu 7, present 7, missing 39
- `src/pc/mods`: donor 14, wiiu 2, present 2, missing 12
- `src/pc/djui`: donor 132, wiiu 0, present 0, missing 132
- `src/pc/network`: donor 72, wiiu 0, present 0, missing 72

## High-Level Phase 1 Queue

- P1 `sm64coopdx/src/pc/lua/smlua_functions_autogen.c`: 1808 missing symbols
