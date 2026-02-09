#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mods.h"
#include "../fs/fs.h"
#include "../platform.h"
#include "../configfile.h"

struct Mods gLocalMods;
struct Mods gRemoteMods;
struct Mods gActiveMods;
char gRemoteModsBasePath[SYS_MAX_PATH] = "";
static struct Mods *sActiveMods = &gLocalMods;
static char *sDynamicScriptPaths[MODS_MAX_ACTIVE_SCRIPTS];
static size_t sDynamicScriptCount = 0;
static char sLocalModNames[MODS_MAX_ACTIVE_SCRIPTS][MAX_CONFIG_STRING];
static const char sDefaultModDescription[] = "Wii U donor compatibility mod entry.";
static const char sDefaultModCategory[] = "misc";

// Keep a deterministic script catalog for the current process lifetime.
static const char *sBuiltinScripts[] = {
    "mods/character-select-coop/main.lua",
    "mods/char-select-the-originals/main.lua",
    "mods/cheats.lua",
    "mods/day-night-cycle/main.lua",
    "mods/faster-swimming.lua",
    "mods/personal-starcount-ex.lua",
};

static void mods_extract_name_from_path(const char *path, char *out, size_t outSize) {
    const char *start = NULL;
    const char *leaf = NULL;
    const char *dot = NULL;
    size_t len = 0;
    size_t i = 0;

    if (out == NULL || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        snprintf(out, outSize, "Mod");
        return;
    }

    start = path;
    if (strncmp(start, "mods/", 5) == 0) {
        start += 5;
    }

    leaf = strrchr(start, '/');
    if (leaf != NULL) {
        start = leaf + 1;
    }
    dot = strrchr(start, '.');
    len = (dot != NULL && dot > start) ? (size_t)(dot - start) : strlen(start);
    if (len == 0) {
        snprintf(out, outSize, "Mod");
        return;
    }
    if (len >= outSize) {
        len = outSize - 1;
    }

    for (i = 0; i < len; i++) {
        char c = start[i];
        if (c == '_' || c == '-') {
            out[i] = ' ';
        } else if (i == 0 || out[i - 1] == ' ') {
            out[i] = (char)toupper((unsigned char)c);
        } else {
            out[i] = c;
        }
    }
    out[len] = '\0';
}

static void mods_build_compat_views(void) {
    size_t activeCount = 0;

    gLocalMods.entries = gLocalMods.entry_ptrs;
    gLocalMods.entryCount = (u16)gLocalMods.available_script_count;
    gLocalMods.size = gLocalMods.available_script_count;

    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        struct Mod *mod = &gLocalMods.entry_objects[i];
        memset(mod, 0, sizeof(*mod));
        mods_extract_name_from_path(gLocalMods.available_script_paths[i], sLocalModNames[i], sizeof(sLocalModNames[i]));
        mod->name = sLocalModNames[i];
        mod->description = (char *)sDefaultModDescription;
        mod->category = (char *)sDefaultModCategory;
        snprintf(mod->relativePath, sizeof(mod->relativePath), "%s",
                 gLocalMods.available_script_paths[i] != NULL ? gLocalMods.available_script_paths[i] : "");
        mod->index = (s32)i;
        mod->enabled = gLocalMods.available_script_enabled[i];
        mod->selectable = true;
        mod->pausable = true;
        mod->size = 0;
        gLocalMods.entry_ptrs[i] = mod;
    }

    memset(&gActiveMods, 0, sizeof(gActiveMods));
    gActiveMods.entries = gActiveMods.entry_ptrs;
    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        if (!gLocalMods.available_script_enabled[i]) {
            continue;
        }
        if (activeCount >= MODS_MAX_ACTIVE_SCRIPTS) {
            break;
        }
        gActiveMods.entry_ptrs[activeCount++] = gLocalMods.entry_ptrs[i];
    }
    gActiveMods.entryCount = (u16)activeCount;
    gActiveMods.size = activeCount;
}

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

// Returns true if script path already exists in local script catalog.
static bool mods_has_available_script_path(const char *script_path) {
    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        if (strcmp(gLocalMods.available_script_paths[i], script_path) == 0) {
            return true;
        }
    }
    return false;
}

// Adds one script path to local script catalog with optional ownership copy.
static bool mods_try_add_available_script(const char *script_path, bool own_copy) {
    char *copy = NULL;
    size_t slot = 0;

    if (script_path == NULL || script_path[0] == '\0') {
        return false;
    }
    if (gLocalMods.available_script_count >= MODS_MAX_ACTIVE_SCRIPTS) {
        return false;
    }
    if (mods_has_available_script_path(script_path)) {
        return false;
    }

    slot = gLocalMods.available_script_count;
    if (own_copy) {
        if (sDynamicScriptCount >= MODS_MAX_ACTIVE_SCRIPTS) {
            return false;
        }
        copy = sys_strdup(script_path);
        if (copy == NULL) {
            return false;
        }
        sDynamicScriptPaths[sDynamicScriptCount++] = copy;
        gLocalMods.available_script_paths[slot] = copy;
    } else {
        gLocalMods.available_script_paths[slot] = script_path;
    }

    // Default policy matches Co-op DX host-mod panel behavior: mods start disabled.
    gLocalMods.available_script_enabled[slot] = false;
    gLocalMods.available_script_count++;
    return true;
}

// Rebuilds active script array from local enabled state.
static void mods_rebuild_active_script_list(void) {
    size_t out_count = 0;

    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        const char *script_path = gLocalMods.available_script_paths[i];

        if (!gLocalMods.available_script_enabled[i]) {
            continue;
        }
        if (script_path == NULL || !mods_script_exists_in_vfs(script_path)) {
            gLocalMods.available_script_enabled[i] = false;
            continue;
        }
        if (out_count >= MODS_MAX_ACTIVE_SCRIPTS) {
            break;
        }

        gLocalMods.script_paths[out_count++] = script_path;
    }

    gLocalMods.script_count = out_count;
    mods_build_compat_views();
}

// Adds a built-in script only if it exists in the mounted virtual filesystem.
static bool mods_try_add_builtin(const char *script_path) {
    if (!mods_script_exists_in_vfs(script_path)) {
        return false;
    }
    return mods_try_add_available_script(script_path, false);
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
        if (mods_try_add_available_script(path, true)) {
            added++;
        }
    }

    fs_pathlist_free(&files);
    return added;
}

// Selects which mod set should be consumed by Lua/runtime systems.
void mods_activate(struct Mods *mods) {
    sActiveMods = (mods != NULL) ? mods : &gLocalMods;
    if (sActiveMods == &gLocalMods) {
        mods_build_compat_views();
    }
}

// Builds local script catalog and default active set for single-player Wii U runtime.
void mods_init(void) {
    size_t builtin_count = 0;
    size_t discovered_count = 0;

    mods_clear_dynamic_script_paths();
    memset(&gLocalMods, 0, sizeof(gLocalMods));

    for (size_t i = 0; i < sizeof(sBuiltinScripts) / sizeof(sBuiltinScripts[0]); i++) {
        if (mods_try_add_builtin(sBuiltinScripts[i])) {
            builtin_count++;
        }
    }
    discovered_count = mods_discover_root_scripts();
    mods_rebuild_active_script_list();

    mods_activate(&gLocalMods);
    printf("mods: cataloged %u scripts (%u built-in, %u discovered), enabled %u\n",
           (unsigned)gLocalMods.available_script_count,
           (unsigned)builtin_count,
           (unsigned)discovered_count,
           (unsigned)gLocalMods.script_count);
}

// Clears mod references during shutdown.
void mods_shutdown(void) {
    mods_clear_dynamic_script_paths();
    memset(&gLocalMods, 0, sizeof(gLocalMods));
    memset(&gActiveMods, 0, sizeof(gActiveMods));
    memset(&gRemoteMods, 0, sizeof(gRemoteMods));
    sActiveMods = &gLocalMods;
}

size_t mods_get_available_script_count(void) {
    return gLocalMods.available_script_count;
}

const char *mods_get_available_script_path(size_t index) {
    if (index >= gLocalMods.available_script_count) {
        return NULL;
    }
    return gLocalMods.available_script_paths[index];
}

bool mods_get_available_script_enabled(size_t index) {
    if (index >= gLocalMods.available_script_count) {
        return false;
    }
    return gLocalMods.available_script_enabled[index];
}

bool mods_set_available_script_enabled(size_t index, bool enabled) {
    if (index >= gLocalMods.available_script_count) {
        return false;
    }
    if (gLocalMods.available_script_enabled[index] == enabled) {
        return false;
    }

    gLocalMods.available_script_enabled[index] = enabled;
    mods_rebuild_active_script_list();
    return true;
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

void mods_refresh_local(void) {
    mods_rebuild_active_script_list();
}

void mods_enable(char *relativePath) {
    if (relativePath == NULL) {
        return;
    }
    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        const char *path = gLocalMods.available_script_paths[i];
        if (path != NULL && strcmp(path, relativePath) == 0) {
            gLocalMods.available_script_enabled[i] = true;
            mods_rebuild_active_script_list();
            return;
        }
    }
}

u16 mods_get_enabled_count(void) {
    u16 count = 0;
    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        if (gLocalMods.available_script_enabled[i]) {
            count++;
        }
    }
    return count;
}

u16 mods_get_character_select_count(void) {
    u16 count = 0;
    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        const char *path = gLocalMods.available_script_paths[i];
        if (!gLocalMods.available_script_enabled[i] || path == NULL) {
            continue;
        }
        if (strstr(path, "character-select") != NULL || strstr(path, "char-select") != NULL) {
            count++;
        }
    }
    return count;
}

bool mods_get_all_pausable(void) {
    return true;
}

bool mods_generate_remote_base_path(void) {
    gRemoteModsBasePath[0] = '\0';
    return false;
}
