#ifndef HARDCODED_H
#define HARDCODED_H

#include "types.h"
#include "level_table.h"

struct LevelValues {
    u8 fixCollisionBugs;
    enum LevelNum entryLevel;
};

extern struct LevelValues gLevelValues;

#endif
