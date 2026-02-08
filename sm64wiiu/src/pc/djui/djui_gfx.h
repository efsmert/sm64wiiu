#pragma once

#include <PR/ultratypes.h>

f32 djui_gfx_get_scale(void);
void djui_gfx_displaylist_begin(void);
void djui_gfx_displaylist_end(void);
void djui_gfx_position_translate(f32 *x, f32 *y);
void djui_gfx_scale_translate(f32 *x, f32 *y);

