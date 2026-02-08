#ifndef SM64_PC_MODS_H
#define SM64_PC_MODS_H

#include <stdbool.h>
#include <stddef.h>

#define MODS_MAX_ACTIVE_SCRIPTS 128

struct Mods {
    const char *available_script_paths[MODS_MAX_ACTIVE_SCRIPTS];
    bool available_script_enabled[MODS_MAX_ACTIVE_SCRIPTS];
    size_t available_script_count;
    const char *script_paths[MODS_MAX_ACTIVE_SCRIPTS];
    size_t script_count;
};

extern struct Mods gLocalMods;

void mods_activate(struct Mods *mods);
void mods_init(void);
void mods_shutdown(void);

size_t mods_get_available_script_count(void);
const char *mods_get_available_script_path(size_t index);
bool mods_get_available_script_enabled(size_t index);
bool mods_set_available_script_enabled(size_t index, bool enabled);

size_t mods_get_active_script_count(void);
const char *mods_get_active_script_path(size_t index);

#endif
