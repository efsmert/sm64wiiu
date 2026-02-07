#include <stdio.h>
#include <string.h>

#include "mods.h"
#include "../fs/fs.h"

struct Mods gLocalMods;
static struct Mods *sActiveMods = &gLocalMods;

// Keeps a stable script list for the current process lifetime.
static const char *sEnabledBuiltins[] = {
    "mods/cheats.lua",
    "mods/faster-swimming.lua",
    "mods/personal-starcount-ex.lua",
    "mods/day-night-cycle/main.lua",
    "mods/character-select-coop/main.lua",
    "mods/char-select-the-originals/main.lua",
};

// Adds a built-in script only if it exists in the mounted virtual filesystem.
static void mods_try_add_builtin(const char *script_path) {
    if (gLocalMods.script_count >= (sizeof(gLocalMods.script_paths) / sizeof(gLocalMods.script_paths[0]))) {
        return;
    }

    fs_file_t *script = fs_open(script_path);
    if (script == NULL) {
        return;
    }

    fs_close(script);
    gLocalMods.script_paths[gLocalMods.script_count++] = script_path;
}

// Selects which mod set should be consumed by Lua/runtime systems.
void mods_activate(struct Mods *mods) {
    sActiveMods = (mods != NULL) ? mods : &gLocalMods;
}

// Builds the default built-in mod set for single-player Wii U runtime.
void mods_init(void) {
    memset(&gLocalMods, 0, sizeof(gLocalMods));

    for (size_t i = 0; i < sizeof(sEnabledBuiltins) / sizeof(sEnabledBuiltins[0]); i++) {
        mods_try_add_builtin(sEnabledBuiltins[i]);
    }

    mods_activate(&gLocalMods);
    printf("mods: activated %u built-in scripts\n", (unsigned)gLocalMods.script_count);
}

// Clears active mod references during shutdown.
void mods_shutdown(void) {
    memset(&gLocalMods, 0, sizeof(gLocalMods));
    sActiveMods = &gLocalMods;
}

// Returns number of scripts in the currently active mod set.
size_t mods_get_active_script_count(void) {
    return sActiveMods != NULL ? sActiveMods->script_count : 0;
}

// Returns active script path at index, or NULL if out-of-range.
const char *mods_get_active_script_path(size_t index) {
    if (sActiveMods == NULL || index >= sActiveMods->script_count) {
        return NULL;
    }
    return sActiveMods->script_paths[index];
}
