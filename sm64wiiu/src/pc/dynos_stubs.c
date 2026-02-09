#include "data/dynos.c.h"

void dynos_gfx_init(void) {
}

int dynos_pack_get_count(void) {
    return 0;
}

const char* dynos_pack_get_name(s32 index) {
    (void)index;
    return "";
}

bool dynos_pack_get_enabled(s32 index) {
    (void)index;
    return false;
}

void dynos_pack_set_enabled(s32 index, bool value) {
    (void)index;
    (void)value;
}

bool dynos_pack_get_exists(s32 index) {
    (void)index;
    return false;
}
