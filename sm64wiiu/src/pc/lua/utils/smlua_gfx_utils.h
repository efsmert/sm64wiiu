#ifndef SMLUA_GFX_UTILS_H
#define SMLUA_GFX_UTILS_H

#include "types.h"
#include "geo_commands.h"

// Helpers for reading Fast3D display list command fields.
#define C0(cmd, pos, width) (((cmd)->words.w0 >> (pos)) & ((1U << (width)) - 1))
#define GFX_OP(cmd) C0(cmd, 24, 8)

// Minimal subset needed by DynOS (and Wii U Lua gfx shims).
Gfx *gfx_allocate_internal(Gfx *gfx, u32 length);
Vtx *vtx_allocate_internal(Vtx *vtx, u32 count);
u32 gfx_get_length_no_sentinel(const Gfx *gfx);
u32 gfx_get_length(const Gfx *gfx);

#endif

