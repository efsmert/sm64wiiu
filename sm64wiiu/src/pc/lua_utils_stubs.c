#include <stdbool.h>

#include "audio/external.h"
#include "pc/configfile.h"
#include "pc/pc_main.h"
#include "pc/lua/utils/smlua_misc_utils.h"
#include "pc/lua/utils/smlua_audio_utils.h"

static bool sPopupDisabledOverrideEnabled = false;
static bool sPopupDisabledOverrideValue = false;

bool djui_is_popup_disabled(void) {
    return sPopupDisabledOverrideEnabled ? sPopupDisabledOverrideValue : configDisablePopups;
}

void djui_set_popup_disabled_override(bool value) {
    sPopupDisabledOverrideEnabled = true;
    sPopupDisabledOverrideValue = value;
}

void djui_reset_popup_disabled_override(void) {
    sPopupDisabledOverrideEnabled = false;
    sPopupDisabledOverrideValue = false;
}

void audio_custom_update_volume(void) {
    bool has_focus = true;
    bool should_mute = false;
    f32 music_volume;
    f32 sfx_volume;
    f32 env_volume;

    if (wm_api != NULL && wm_api->has_focus != NULL) {
        has_focus = wm_api->has_focus();
    }

    gMasterVolume = ((f32)configMasterVolume / 127.0f) * ((f32)gLuaVolumeMaster / 127.0f);
    music_volume = ((f32)configMusicVolume / 127.0f) * ((f32)gLuaVolumeLevel / 127.0f);
    sfx_volume = ((f32)configSfxVolume / 127.0f) * ((f32)gLuaVolumeSfx / 127.0f);
    env_volume = ((f32)configEnvVolume / 127.0f) * ((f32)gLuaVolumeEnv / 127.0f);

    should_mute = (configMuteFocusLoss && !has_focus) || (gMasterVolume <= 0.0f);
    if (should_mute) {
        music_volume = 0.0f;
        sfx_volume = 0.0f;
        env_volume = 0.0f;
    }

    set_sequence_player_volume(SEQ_PLAYER_LEVEL, music_volume);
    set_sequence_player_volume(SEQ_PLAYER_SFX, sfx_volume);
    set_sequence_player_volume(SEQ_PLAYER_ENV, env_volume);
}

void audio_custom_shutdown(void) {
}

void smlua_audio_custom_deinit(void) {
}
