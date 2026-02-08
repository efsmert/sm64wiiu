# Phase 1 Lua Port Queue

Prioritization:
- `P0`: donor symbols missing on Wii U and referenced by current Wii U built-in mods
- `P1`: donor symbols missing on Wii U (general parity)
- `P2`: lower immediate impact (typically network/DJUI heavy paths)

## P1 - `sm64coopdx/src/pc/lua/smlua_functions_autogen.c`

- Missing symbols: 1800
- Referenced by built-in mods: none
- Symbol preview: `anim_spline_init`, `anim_spline_poll`, `apply_platform_displacement`, `apply_water_current`, `approach_camera_height`, `area_create_warp_node`, `area_get_any_warp_node`, `area_get_warp_node`, `area_get_warp_node_from_params`, `audio_sample_destroy`, `audio_stream_destroy`, `audio_stream_get_frequency`, `audio_stream_get_looping`, `audio_stream_get_position`, `audio_stream_get_tempo`, `audio_stream_get_volume`, `audio_stream_pause`, `audio_stream_set_position`, `audio_stream_set_speed`, `audio_stream_set_tempo`, `begin_braking_action`, `begin_walking_action`, `bhv_1up_common_init`, `bhv_1up_hidden_in_pole_loop`, `bhv_1up_hidden_in_pole_spawner_loop`
- ... plus 1775 more

