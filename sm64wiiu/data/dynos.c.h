#ifndef DYNOS_C_H
#define DYNOS_C_H

#include <stdbool.h>
#include <PR/ultratypes.h>

void dynos_gfx_init(void);
int dynos_pack_get_count(void);
const char* dynos_pack_get_name(s32 index);
bool dynos_pack_get_enabled(s32 index);
void dynos_pack_set_enabled(s32 index, bool value);
bool dynos_pack_get_exists(s32 index);

#endif
