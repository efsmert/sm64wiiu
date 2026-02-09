#include <stdbool.h>

#include "pc/configfile.h"
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
}

void audio_custom_shutdown(void) {
}

void smlua_audio_custom_deinit(void) {
}
