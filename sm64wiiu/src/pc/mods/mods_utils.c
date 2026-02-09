#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods_utils.h"

void mods_size_enforce(struct Mods* mods) {
    (void)mods;
}

static bool mods_incompatible_match(const struct Mod *a, const struct Mod *b) {
    char *ai;
    char *bi;
    char *atoken;
    char *btoken;
    char *arest = NULL;
    bool matched = false;

    if (a == NULL || b == NULL || a->incompatible == NULL || b->incompatible == NULL) {
        return false;
    }
    if (a->incompatible[0] == '\0' || b->incompatible[0] == '\0') {
        return false;
    }

    ai = malloc(strlen(a->incompatible) + 1);
    bi = malloc(strlen(b->incompatible) + 1);
    if (ai == NULL || bi == NULL) {
        free(ai);
        free(bi);
        return false;
    }
    snprintf(ai, strlen(a->incompatible) + 1, "%s", a->incompatible);
    snprintf(bi, strlen(b->incompatible) + 1, "%s", b->incompatible);

    for (atoken = strtok_r(ai, " ", &arest); atoken != NULL && !matched; atoken = strtok_r(NULL, " ", &arest)) {
        char *brest = NULL;
        for (btoken = strtok_r(bi, " ", &brest); btoken != NULL; btoken = strtok_r(NULL, " ", &brest)) {
            if (strcmp(atoken, btoken) == 0) {
                matched = true;
                break;
            }
        }
        // Reset b-tokenization for the next A token.
        snprintf(bi, strlen(b->incompatible) + 1, "%s", b->incompatible);
    }

    free(ai);
    free(bi);
    return matched;
}

void mods_update_selectable(void) {
    bool changed = false;
    size_t count = gLocalMods.entryCount;

    if (gLocalMods.entries == NULL) {
        return;
    }
    if (count > gLocalMods.available_script_count) {
        count = gLocalMods.available_script_count;
    }

    for (size_t i = 0; i < count; i++) {
        struct Mod *mod = gLocalMods.entries[i];
        if (mod == NULL) {
            continue;
        }

        // Host-mod checkboxes bind to mod->enabled; local runtime binds to available_script_enabled.
        if (gLocalMods.available_script_enabled[i] != mod->enabled) {
            gLocalMods.available_script_enabled[i] = mod->enabled;
            changed = true;
        }
        mod->selectable = true;
    }

    for (size_t i = 0; i < count; i++) {
        struct Mod *mod = gLocalMods.entries[i];
        if (mod == NULL || mod->enabled) {
            continue;
        }

        for (size_t j = 0; j < count; j++) {
            struct Mod *mod2 = gLocalMods.entries[j];
            if (j == i || mod2 == NULL || !mod2->enabled) {
                continue;
            }
            if (mods_incompatible_match(mod, mod2)) {
                mod->selectable = false;
                break;
            }
        }
    }

    mods_size_enforce(&gLocalMods);

    if (changed) {
        mods_refresh_local();
    }
}

void mods_delete_tmp(void) {
}

bool mod_file_full_path(char* destination, struct Mod* mod, struct ModFile* modFile) {
    if (destination == NULL || mod == NULL || modFile == NULL) {
        return false;
    }
    return snprintf(destination, SYS_MAX_PATH - 1, "%s/%s", mod->basePath, modFile->relativePath) >= 0;
}

bool mod_file_create_directories(struct Mod* mod, struct ModFile* modFile) {
    (void)mod;
    (void)modFile;
    return true;
}

bool str_starts_with(const char *string, const char *prefix) {
    if (string == NULL || prefix == NULL) {
        return false;
    }
    return strncmp(string, prefix, strlen(prefix)) == 0;
}

bool str_ends_with(const char *string, const char *suffix) {
    size_t sl;
    size_t su;
    if (string == NULL || suffix == NULL) {
        return false;
    }
    sl = strlen(string);
    su = strlen(suffix);
    if (su > sl) {
        return false;
    }
    return strcmp(string + sl - su, suffix) == 0;
}

bool path_ends_with(const char* path, const char* suffix) {
    return str_ends_with(path, suffix);
}

char* extract_lua_field(char* fieldName, char* buffer) {
    size_t len;
    if (fieldName == NULL || buffer == NULL) {
        return NULL;
    }
    len = strlen(fieldName);
    if (strncmp(fieldName, buffer, len) == 0) {
        return buffer + len;
    }
    return NULL;
}

void normalize_path(char* path) {
    if (path == NULL) {
        return;
    }
    while (*path != '\0') {
        if (*path == '\\') {
            *path = '/';
        }
        path++;
    }
}

bool concat_path(char* destination, char* path, char* fname) {
    if (destination == NULL || path == NULL || fname == NULL) {
        return false;
    }
    return snprintf(destination, SYS_MAX_PATH - 1, "%s/%s", path, fname) >= 0;
}

char* path_basename(char* path) {
    char* base;
    if (path == NULL) {
        return NULL;
    }
    base = strrchr(path, '/');
    return base != NULL ? base + 1 : path;
}

void path_get_folder(char* path, char* outpath) {
    char* base;
    size_t len;
    if (path == NULL || outpath == NULL) {
        return;
    }
    base = path_basename(path);
    len = (size_t)(base - path);
    memcpy(outpath, path, len);
    outpath[len] = '\0';
}

int path_depth(const char* path) {
    int depth = 0;
    if (path == NULL) {
        return 0;
    }
    while (*path != '\0') {
        if (*path == '/' || *path == '\\') {
            depth++;
        }
        path++;
    }
    return depth;
}

void resolve_relative_path(const char* base, const char* path, char* output) {
    if (output == NULL) {
        return;
    }
    if (base == NULL || path == NULL) {
        output[0] = '\0';
        return;
    }
    snprintf(output, SYS_MAX_PATH - 1, "%s/%s", base, path);
    normalize_path(output);
}

bool path_is_relative_to(const char* fullPath, const char* baseDir) {
    if (fullPath == NULL || baseDir == NULL) {
        return false;
    }
    return strncmp(fullPath, baseDir, strlen(baseDir)) == 0;
}

bool directory_sanity_check(struct dirent* dir, char* dirPath, char* outPath) {
    if (dir == NULL || dirPath == NULL || outPath == NULL) {
        return false;
    }
    return concat_path(outPath, dirPath, dir->d_name);
}
