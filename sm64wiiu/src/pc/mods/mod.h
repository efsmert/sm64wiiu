#ifndef MOD_H
#define MOD_H

#include <stdbool.h>
#include <stddef.h>
#include "pc/platform.h"

struct ModFile {
    char relativePath[SYS_MAX_PATH];
    size_t size;
    bool isLoadedLuaModule;
};

struct Mod {
    char* name;
    char* incompatible;
    char* category;
    char* description;
    char relativePath[SYS_MAX_PATH];
    char basePath[SYS_MAX_PATH];
    struct ModFile* files;
    int index;
    unsigned short fileCount;
    unsigned short fileCapacity;
    bool isDirectory;
    bool enabled;
    bool selectable;
    bool renderBehindHud;
    bool pausable;
    bool ignoreScriptWarnings;
    bool showedScriptWarning;
    size_t size;
    unsigned char customBehaviorIndex;
};

#endif
