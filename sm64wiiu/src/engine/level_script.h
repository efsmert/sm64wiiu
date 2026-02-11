#ifndef LEVEL_SCRIPT_H
#define LEVEL_SCRIPT_H

#include <PR/ultratypes.h>

#include "types.h"

struct LevelCommand;

extern u8 level_script_entry[];

// DynOS: track currently executing level script for token/mod lookups.
extern LevelScript* gLevelScriptActive;
extern s32 gLevelScriptModIndex;

struct LevelCommand *level_script_execute(struct LevelCommand *cmd);

#endif // LEVEL_SCRIPT_H
