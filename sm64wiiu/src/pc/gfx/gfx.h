#ifndef GFX_H
#define GFX_H

// Co-op DX provides this shared header with basic gfx-side helpers/macros.
// DynOS texture conversion code expects the SCALE_* macros, so provide a small
// Wii U-compatible subset here.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define SUPPORT_CHECK(x) assert(x)

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_5_8(VAL_) (((VAL_) * 0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_) * 0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_) * 0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

// Texture cache node shared by gfx backend and DynOS texture manager.
// Keep this layout in sync with `src/pc/gfx/gfx_pc.c`.
struct TextureHashmapNode {
    struct TextureHashmapNode *next;

    const void *texture_addr;
    uint8_t fmt, siz;

    uint32_t texture_id;
    uint8_t cms, cmt;
    bool linear_filter;
};

#endif
