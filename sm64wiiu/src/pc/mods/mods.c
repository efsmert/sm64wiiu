#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods.h"
#include "../fs/fs.h"
#include "../platform.h"

struct Mods gLocalMods;
static struct Mods *sActiveMods = &gLocalMods;
static char *sDynamicScriptPaths[MODS_MAX_ACTIVE_SCRIPTS];
static size_t sDynamicScriptCount = 0;

// Keeps a stable script list for the current process lifetime.
static const char *sEnabledBuiltins[] = {
    "mods/character-select-coop/main.lua",
    "mods/char-select-the-originals/main.lua",
    "mods/cheats.lua",
    "mods/day-night-cycle/main.lua",
    "mods/faster-swimming.lua",
    "mods/personal-starcount-ex.lua",
};

// Returns true if `path` ends with `suffix`.
static bool mods_path_has_suffix(const char *path, const char *suffix) {
    size_t path_len;
    size_t suffix_len;

    if (path == NULL || suffix == NULL) {
        return false;
    }

    path_len = strlen(path);
    suffix_len = strlen(suffix);
    if (suffix_len > path_len) {
        return false;
    }
    return strcmp(path + path_len - suffix_len, suffix) == 0;
}

// Returns true if `mods` virtual path should be treated as a root Lua script.
static bool mods_is_root_script_candidate(const char *path) {
    const char *rest;
    const char *slash;
    const char *leaf;

    if (path == NULL || strncmp(path, "mods/", 5) != 0) {
        return false;
    }
    if (!mods_path_has_suffix(path, ".lua") && !mods_path_has_suffix(path, ".luac")) {
        return false;
    }

    rest = path + 5;
    slash = strchr(rest, '/');
    if (slash == NULL) {
        // Single-file mod: mods/foo.lua
        return true;
    }

    // Folder mod: only accept mods/<name>/main.lua(.luac) as root script.
    leaf = slash + 1;
    if (strchr(leaf, '/') != NULL) {
        return false;
    }
    return strcmp(leaf, "main.lua") == 0 || strcmp(leaf, "main.luac") == 0;
}

// Deterministic path sort for stable mod load ordering.
static int mods_compare_paths(const void *a, const void *b) {
    const char *lhs = *(const char *const *)a;
    const char *rhs = *(const char *const *)b;
    if (lhs == NULL && rhs == NULL) {
        return 0;
    }
    if (lhs == NULL) {
        return -1;
    }
    if (rhs == NULL) {
        return 1;
    }
    return strcmp(lhs, rhs);
}

// Returns true if virtual file can be opened from currently mounted search paths.
static bool mods_script_exists_in_vfs(const char *script_path) {
    fs_file_t *script = fs_open(script_path);
    if (script == NULL) {
        return false;
    }
    fs_close(script);
    return true;
}

// Frees heap-owned discovered script paths collected during last mods_init().
static void mods_clear_dynamic_script_paths(void) {
    for (size_t i = 0; i < sDynamicScriptCount; i++) {
        free(sDynamicScriptPaths[i]);
        sDynamicScriptPaths[i] = NULL;
    }
    sDynamicScriptCount = 0;
}

// Returns true if script path is already present in the local active list.
static bool mods_has_script_path(const char *script_path) {
    for (size_t i = 0; i < gLocalMods.script_count; i++) {
        if (strcmp(gLocalMods.script_paths[i], script_path) == 0) {
            return true;
        }
    }
    return false;
}

// Adds one script path to gLocalMods with optional ownership copy for discovered entries.
static bool mods_try_add_script(const char *script_path, bool own_copy) {
    char *copy = NULL;

    if (script_path == NULL || script_path[0] == '\0') {
        return false;
    }
    if (gLocalMods.script_count >= MODS_MAX_ACTIVE_SCRIPTS) {
        return false;
    }
    if (mods_has_script_path(script_path)) {
        return false;
    }

    if (own_copy) {
        if (sDynamicScriptCount >= MODS_MAX_ACTIVE_SCRIPTS) {
            return false;
        }
        copy = sys_strdup(script_path);
        if (copy == NULL) {
            return false;
        }
        sDynamicScriptPaths[sDynamicScriptCount++] = copy;
        gLocalMods.script_paths[gLocalMods.script_count++] = copy;
        return true;
    }

    gLocalMods.script_paths[gLocalMods.script_count++] = script_path;
    return true;
}

// Adds a built-in script only if it exists in the mounted virtual filesystem.
static void mods_try_add_builtin(const char *script_path) {
    if (!mods_script_exists_in_vfs(script_path)) {
        return;
    }
    (void)mods_try_add_script(script_path, false);
}

// Discovers root scripts under `mods/` to support user-added Lua mods on Wii U.
static size_t mods_discover_root_scripts(void) {
    fs_pathlist_t files = fs_enumerate("mods", true);
    size_t added = 0;

    if (files.paths == NULL || files.numpaths <= 0) {
        fs_pathlist_free(&files);
        return 0;
    }

    qsort(files.paths, (size_t)files.numpaths, sizeof(char *), mods_compare_paths);

    for (int i = 0; i < files.numpaths; i++) {
        const char *path = files.paths[i];
        if (!mods_is_root_script_candidate(path)) {
            continue;
        }
        if (!mods_script_exists_in_vfs(path)) {
            continue;
        }
        if (mods_try_add_script(path, true)) {
            added++;
        }
    }

    fs_pathlist_free(&files);
    return added;
}

// Selects which mod set should be consumed by Lua/runtime systems.
void mods_activate(struct Mods *mods) {
    sActiveMods = (mods != NULL) ? mods : &gLocalMods;
}

// Builds the default built-in mod set for single-player Wii U runtime.
void mods_init(void) {
    size_t builtin_count;
    size_t discovered_count;

    mods_clear_dynamic_script_paths();
    memset(&gLocalMods, 0, sizeof(gLocalMods));

    for (size_t i = 0; i < sizeof(sEnabledBuiltins) / sizeof(sEnabledBuiltins[0]); i++) {
        mods_try_add_builtin(sEnabledBuiltins[i]);
    }
    builtin_count = gLocalMods.script_count;
    discovered_count = mods_discover_root_scripts();

    mods_activate(&gLocalMods);
    printf("mods: activated %u scripts (%u built-in, %u discovered)\n",
           (unsigned)gLocalMods.script_count,
           (unsigned)builtin_count,
           (unsigned)discovered_count);
}

// Clears active mod references during shutdown.
void mods_shutdown(void) {
    mods_clear_dynamic_script_paths();
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
