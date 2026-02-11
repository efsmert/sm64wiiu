#ifndef SMLUA_MODEL_UTILS_H
#define SMLUA_MODEL_UTILS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimal surface needed by DynOS model manager on Wii U. The full Co-op DX
// implementation tracks extended model IDs by GeoLayout name; that wiring is
// ported incrementally.
void smlua_model_util_register_model_id(u32 id, const void *asset);

#ifdef __cplusplus
}
#endif

#endif

