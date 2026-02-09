#include <stdio.h>
#include <string.h>

#include "mods_utils.h"

void mods_size_enforce(struct Mods* mods) {
    (void)mods;
}

void mods_update_selectable(void) {
    for (size_t i = 0; i < gLocalMods.entryCount; i++) {
        if (gLocalMods.entries != NULL && gLocalMods.entries[i] != NULL) {
            gLocalMods.entries[i]->selectable = true;
        }
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
