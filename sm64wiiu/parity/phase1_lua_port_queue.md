# Phase 1 Lua Port Queue

Prioritization:
- `P0`: donor symbols missing on Wii U and referenced by current Wii U built-in mods
- `P1`: donor symbols missing on Wii U (general parity)
- `P2`: lower immediate impact (typically network/DJUI heavy paths)

## P1 - `sm64coopdx/src/pc/lua/smlua_functions_autogen.c`

- Missing symbols: 1808
- Referenced by built-in mods: none
- Symbol preview: `act_select_hud_hide`, `act_select_hud_is_hidden`, `act_select_hud_show`, `anim_spline_init`, `anim_spline_poll`, `apply_landing_accel`, `apply_platform_displacement`, `apply_slope_accel`, `apply_slope_decel`, `apply_water_current`, `approach_camera_height`, `arc_to_goal_pos`, `area_create_warp_node`, `area_get_any_warp_node`, `area_get_warp_node`, `area_get_warp_node_from_params`, `atan2f`, `audio_sample_destroy`, `audio_stream_destroy`, `audio_stream_get_frequency`, `audio_stream_get_looping`, `audio_stream_get_position`, `audio_stream_get_tempo`, `audio_stream_get_volume`, `audio_stream_pause`
- ... plus 1783 more

