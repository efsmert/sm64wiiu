#ifndef SM64_PC_LOADING_H
#define SM64_PC_LOADING_H

// Wii U port: DynOS pack generation code in Co-op DX references a loading
// screen/progress UI that does not exist in this port. Provide minimal stubs so
// DynOS sources can compile; pack generation is disabled by default via
// `configSkipPackGeneration`.

#include "types.h"

struct LoadingSegment {
    char str[256];
    f32 percentage;
};

extern struct LoadingSegment gCurrLoadingSegment;

static inline void loading_screen_reset_progress_bar(void) {
}

#define LOADING_SCREEN_MUTEX(code) do { code; } while (0)

#endif

