#include "smlua_gfx_utils.h"

#include <string.h>
#include <stdlib.h>

// Note: Co-op DX's gfx utilities assume f3dex2e command layouts. For the Wii U
// port we only need sentinel buffers + simple opcode checks, so keep this file
// microcode-agnostic and avoid hard #error guards.

static const Gfx SENTINEL_GFX[1] = {{{ _SHIFTL(G_ENDDL, 24, 8) | _SHIFTL(UINT32_MAX, 0, 24), UINT32_MAX }}};
static const u8  SENTINEL_VTX[sizeof(Vtx)] = {[0 ... sizeof(Vtx) - 1] = UINT8_MAX};

Gfx *gfx_allocate_internal(Gfx *gfx, u32 length) {
    if (gfx == NULL) {
        // +1 for sentinel.
        gfx = calloc(length + 1, sizeof(Gfx));
    } else {
        memset(gfx, 0, length * sizeof(Gfx));
    }
    if (gfx != NULL) {
        memcpy(gfx + length, SENTINEL_GFX, sizeof(Gfx));
    }
    return gfx;
}

Vtx *vtx_allocate_internal(Vtx *vtx, u32 count) {
    if (vtx == NULL) {
        // +1 for sentinel.
        vtx = calloc(count + 1, sizeof(Vtx));
    } else {
        memset(vtx, 0, count * sizeof(Vtx));
    }
    if (vtx != NULL) {
        memcpy(vtx + count, SENTINEL_VTX, sizeof(Vtx));
    }
    return vtx;
}

u32 gfx_get_length_no_sentinel(const Gfx *gfx) {
    if (gfx == NULL) {
        return 0;
    }

    for (u32 i = 0;; ++i) {
        u32 op = GFX_OP(gfx + i);
        switch (op) {
            case G_DL:
                // For display lists that end with branches (jumps)
                if (C0(gfx + i, 16, 1) == G_DL_NOPUSH) {
                    return i + 1;
                }
                break;
            case G_ENDDL:
                return i + 1;
        }
    }
}

u32 gfx_get_length(const Gfx *gfx) {
    if (gfx == NULL) {
        return 0;
    }

    u32 length = 0;
    for (; memcmp(gfx, SENTINEL_GFX, sizeof(Gfx)) != 0; ++length, ++gfx) {
    }
    return length;
}
