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
static char sLocalModNames[MODS_MAX_ACTIVE_SCRIPTS][128];
static char sLocalModDescriptions[MODS_MAX_ACTIVE_SCRIPTS][2048];
static char sLocalModCategories[MODS_MAX_ACTIVE_SCRIPTS][64];
static char sLocalModIncompatible[MODS_MAX_ACTIVE_SCRIPTS][128];
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

static int mods_ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

static int mods_ascii_strcasecmp(const char *a, const char *b) {
    int ca;
    int cb;
    if (a == NULL && b == NULL) { return 0; }
    if (a == NULL) { return -1; }
    if (b == NULL) { return 1; }
    while (*a != '\0' || *b != '\0') {
        ca = mods_ascii_tolower((unsigned char)*a++);
        cb = mods_ascii_tolower((unsigned char)*b++);
        if (ca != cb) {
            return ca - cb;
        }
        if (ca == 0) { break; }
    }
    return 0;
}

static int mods_ascii_strncasecmp(const char *a, const char *b, size_t n) {
    int ca;
    int cb;
    if (n == 0) { return 0; }
    if (a == NULL && b == NULL) { return 0; }
    if (a == NULL) { return -1; }
    if (b == NULL) { return 1; }
    while (n-- > 0) {
        ca = mods_ascii_tolower((unsigned char)*a++);
        cb = mods_ascii_tolower((unsigned char)*b++);
        if (ca != cb) {
            return ca - cb;
        }
        if (ca == 0) { break; }
    }
    return 0;
}

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

static void mods_string_trim_trailing_newlines(char *s) {
    size_t len;
    if (s == NULL) { return; }
    len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c == '\n' || c == '\r') {
            s[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}

static bool mods_parse_bool(const char *value, bool defaultValue) {
    if (value == NULL) { return defaultValue; }
    while (*value == ' ' || *value == '\t') { value++; }
    if (*value == '\0') { return defaultValue; }
    if (mods_ascii_strcasecmp(value, "false") == 0 || strcmp(value, "0") == 0) { return false; }
    if (mods_ascii_strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0) { return true; }
    return defaultValue;
}

static void mods_copy_lua_header_string(char *dst, size_t dstSize, const char *src) {
    size_t out = 0;
    if (dst == NULL || dstSize == 0) { return; }
    dst[0] = '\0';
    if (src == NULL) { return; }

    while (*src == ' ' || *src == '\t') { src++; }
    for (; *src != '\0' && out + 1 < dstSize; src++) {
        if (src[0] == '\\') {
            // Match the most common Lua-style escapes used in mod headers, so the parsed text
            // matches the runtime strings the author likely tested with.
            if (src[1] == 'n') { dst[out++] = '\n'; src++; continue; }
            if (src[1] == 't') { dst[out++] = '\t'; src++; continue; }
            if (src[1] == 'r') { dst[out++] = '\r'; src++; continue; }
            if (src[1] == '\\') { dst[out++] = '\\'; src++; continue; }
        }
        dst[out++] = src[0];
    }
    dst[out] = '\0';
    mods_string_trim_trailing_newlines(dst);
}

static bool mods_try_extract_header_field(char *out, size_t outSize, const char *line, const char *key) {
    const char *p;
    size_t keyLen;
    if (out == NULL || outSize == 0 || line == NULL || key == NULL) { return false; }

    // Accept either "-- key:" or "-- key :" with optional whitespace.
    p = line;
    while (*p == ' ' || *p == '\t') { p++; }
    if (p[0] != '-' || p[1] != '-') { return false; }
    p += 2;
    while (*p == ' ' || *p == '\t') { p++; }

    keyLen = strlen(key);
    if (mods_ascii_strncasecmp(p, key, keyLen) != 0) { return false; }
    p += keyLen;
    while (*p == ' ' || *p == '\t') { p++; }
    if (*p != ':') { return false; }
    p++;

    mods_copy_lua_header_string(out, outSize, p);
    return out[0] != '\0';
}

static void mods_try_parse_lua_metadata(const char *scriptPath,
                                        char *nameOut, size_t nameOutSize,
                                        char *descOut, size_t descOutSize,
                                        char *catOut, size_t catOutSize,
                                        char *incompatOut, size_t incompatOutSize,
                                        bool *pausableOut) {
    fs_file_t *file;
    char line[1024];
    bool foundAny = false;
    bool foundName = false;
    bool foundDesc = false;
    bool foundCat = false;
    bool foundIncompat = false;
    bool foundPausable = false;

    if (nameOut != NULL && nameOutSize > 0) { nameOut[0] = '\0'; }
    if (descOut != NULL && descOutSize > 0) { descOut[0] = '\0'; }
    if (catOut != NULL && catOutSize > 0) { catOut[0] = '\0'; }
    if (incompatOut != NULL && incompatOutSize > 0) { incompatOut[0] = '\0'; }
    if (pausableOut != NULL) { *pausableOut = true; }

    if (scriptPath == NULL || scriptPath[0] == '\0') {
        return;
    }

    file = fs_open(scriptPath);
    if (file == NULL) {
        return;
    }

    for (int i = 0; i < 64; i++) {
        const char *end = fs_readline(file, line, sizeof(line));
        if (end == NULL) {
            break;
        }
        (void)end;
        mods_string_trim_trailing_newlines(line);

        // Skip blank lines at the top.
        const char *p = line;
        while (*p == ' ' || *p == '\t') { p++; }
        if (*p == '\0') {
            continue;
        }

        // Stop once we leave the header comment block.
        if (!(p[0] == '-' && p[1] == '-')) {
            break;
        }

        foundAny = true;
        if (!foundName && nameOut != NULL) {
            foundName = mods_try_extract_header_field(nameOut, nameOutSize, p, "name");
        }
        if (!foundDesc && descOut != NULL) {
            foundDesc = mods_try_extract_header_field(descOut, descOutSize, p, "description");
        }
        if (!foundCat && catOut != NULL) {
            foundCat = mods_try_extract_header_field(catOut, catOutSize, p, "category");
        }
        if (!foundIncompat && incompatOut != NULL) {
            foundIncompat = mods_try_extract_header_field(incompatOut, incompatOutSize, p, "incompatible");
        }
        if (!foundPausable && pausableOut != NULL) {
            char tmp[32];
            if (mods_try_extract_header_field(tmp, sizeof(tmp), p, "pausable")) {
                *pausableOut = mods_parse_bool(tmp, true);
                foundPausable = true;
            }
        }

        if (foundName && foundDesc && foundCat && foundIncompat && (pausableOut == NULL || foundPausable)) {
            break;
        }
    }

    fs_close(file);
    (void)foundAny;
}

static void mods_build_compat_views(void) {
    size_t activeCount = 0;

    gLocalMods.entries = gLocalMods.entry_ptrs;
    gLocalMods.entryCount = (u16)gLocalMods.available_script_count;
    gLocalMods.size = gLocalMods.available_script_count;

    for (size_t i = 0; i < gLocalMods.available_script_count; i++) {
        struct Mod *mod = &gLocalMods.entry_objects[i];
        bool pausable = true;
        memset(mod, 0, sizeof(*mod));

        mods_extract_name_from_path(gLocalMods.available_script_paths[i], sLocalModNames[i], sizeof(sLocalModNames[i]));
        sLocalModDescriptions[i][0] = '\0';
        sLocalModCategories[i][0] = '\0';
        sLocalModIncompatible[i][0] = '\0';

        mods_try_parse_lua_metadata(gLocalMods.available_script_paths[i],
                                   sLocalModNames[i], sizeof(sLocalModNames[i]),
                                   sLocalModDescriptions[i], sizeof(sLocalModDescriptions[i]),
                                   sLocalModCategories[i], sizeof(sLocalModCategories[i]),
                                   sLocalModIncompatible[i], sizeof(sLocalModIncompatible[i]),
                                   &pausable);

        mod->name = sLocalModNames[i];
        mod->description = (sLocalModDescriptions[i][0] != '\0') ? sLocalModDescriptions[i] : (char *)sDefaultModDescription;
        mod->category = (sLocalModCategories[i][0] != '\0') ? sLocalModCategories[i] : (char *)sDefaultModCategory;
        mod->incompatible = (sLocalModIncompatible[i][0] != '\0') ? sLocalModIncompatible[i] : NULL;
        snprintf(mod->relativePath, sizeof(mod->relativePath), "%s",
                 gLocalMods.available_script_paths[i] != NULL ? gLocalMods.available_script_paths[i] : "");
        mod->index = (s32)i;
        mod->enabled = gLocalMods.available_script_enabled[i];
        mod->selectable = true;
        mod->pausable = pausable;
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
