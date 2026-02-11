#ifndef SKYBOX_H
#define SKYBOX_H

#include <PR/ultratypes.h>
#include <PR/gbi.h>

#include "types.h"

// DynOS: custom background tile pointers for modded skyboxes.
extern Texture* gCustomSkyboxPtrList[80];

Gfx *create_skybox_facing_camera(s8 player, s8 background, f32 fov,
                                 f32 posX, f32 posY, f32 posZ,
                                 f32 focX, f32 focY, f32 focZ);

#endif // SKYBOX_H
