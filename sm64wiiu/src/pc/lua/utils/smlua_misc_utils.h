#ifndef SMLUA_MISC_UTILS_H
#define SMLUA_MISC_UTILS_H

#include <stdbool.h>

bool djui_is_popup_disabled(void);
void djui_set_popup_disabled_override(bool value);
void djui_reset_popup_disabled_override(void);

#endif
