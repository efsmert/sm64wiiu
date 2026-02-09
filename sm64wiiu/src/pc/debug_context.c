#include "debug_context.h"

static u32 sCtxDepth[CTX_MAX] = { 0 };
static f64 sCtxTime[CTX_MAX] = { 0 };

void debug_context_begin(enum DebugContext ctx) {
    if (ctx > CTX_NONE && ctx < CTX_MAX) {
        sCtxDepth[ctx]++;
    }
}

void debug_context_end(enum DebugContext ctx) {
    if (ctx > CTX_NONE && ctx < CTX_MAX && sCtxDepth[ctx] > 0) {
        sCtxDepth[ctx]--;
    }
}

void debug_context_reset(void) {
    for (u32 i = 0; i < CTX_MAX; i++) {
        sCtxDepth[i] = 0;
        sCtxTime[i] = 0.0;
    }
}

bool debug_context_within(enum DebugContext ctx) {
    if (ctx <= CTX_NONE || ctx >= CTX_MAX) {
        return false;
    }
    return sCtxDepth[ctx] > 0;
}

void debug_context_set_time(enum DebugContext ctx, f64 time) {
    if (ctx > CTX_NONE && ctx < CTX_MAX) {
        sCtxTime[ctx] = time;
    }
}

f64 debug_context_get_time(enum DebugContext ctx) {
    if (ctx <= CTX_NONE || ctx >= CTX_MAX) {
        return 0.0;
    }
    return sCtxTime[ctx];
}
