#ifndef SM64_PC_MODS_H
#define SM64_PC_MODS_H

#include <stddef.h>

struct Mods {
    const char *script_paths[32];
    size_t script_count;
};

extern struct Mods gLocalMods;

void mods_activate(struct Mods *mods);
void mods_init(void);
void mods_shutdown(void);

size_t mods_get_active_script_count(void);
const char *mods_get_active_script_path(size_t index);

#endif
