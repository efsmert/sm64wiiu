#ifndef PC_MAIN_H
#define PC_MAIN_H

#include <PR/ultratypes.h>
#include "gfx/gfx_window_manager_api.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct GfxWindowManagerAPI *wm_api;
extern f32 gMasterVolume;
extern u8 gLuaVolumeMaster;
extern u8 gLuaVolumeLevel;
extern u8 gLuaVolumeSfx;
extern u8 gLuaVolumeEnv;

void produce_one_dummy_frame(void (*callback)(), u8 clearColorR, u8 clearColorG, u8 clearColorB);
void game_deinit(void);
void game_exit(void);

#ifdef __cplusplus
}
#endif

#endif
