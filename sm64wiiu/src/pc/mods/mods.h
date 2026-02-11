#ifndef SM64_PC_MODS_H
#define SM64_PC_MODS_H

#include <stdbool.h>
#include <stddef.h>
#include <PR/ultratypes.h>
#include "mod.h"

#define MODS_MAX_ACTIVE_SCRIPTS 128

struct Mods {
    const char *available_script_paths[MODS_MAX_ACTIVE_SCRIPTS];
    bool available_script_enabled[MODS_MAX_ACTIVE_SCRIPTS];
    size_t available_script_count;
    const char *script_paths[MODS_MAX_ACTIVE_SCRIPTS];
    size_t script_count;

    // Donor compatibility view used by DJUI mod panels.
    struct Mod entry_objects[MODS_MAX_ACTIVE_SCRIPTS];
    struct Mod *entry_ptrs[MODS_MAX_ACTIVE_SCRIPTS];
    struct Mod **entries;
    u16 entryCount;
    size_t size;
};

extern struct Mods gLocalMods;
extern struct Mods gRemoteMods;
extern struct Mods gActiveMods;
extern char gRemoteModsBasePath[];

void mods_activate(struct Mods *mods);
void mods_init(void);
void mods_shutdown(void);
void mods_refresh_local(void);
void mods_enable(char *relativePath);

size_t mods_get_available_script_count(void);
const char *mods_get_available_script_path(size_t index);
bool mods_get_available_script_enabled(size_t index);
bool mods_set_available_script_enabled(size_t index, bool enabled);

size_t mods_get_active_script_count(void);
const char *mods_get_active_script_path(size_t index);

u16 mods_get_enabled_count(void);
u16 mods_get_character_select_count(void);
bool mods_get_all_pausable(void);
bool mods_generate_remote_base_path(void);

// Activates DynOSBIN assets (.lvl/.bin/.col/.tex/.bhv) for currently enabled mods.
// Called by the Wii U Lua runtime before mod scripts execute.
void mods_activate_dynos_assets(void);

#endif
