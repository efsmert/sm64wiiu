#ifndef HARDCODED_H
#define HARDCODED_H

#include "types.h"
#include "level_table.h"

struct LevelValues {
    u8 fixCollisionBugs;
    u8 fixCollisionBugsRoundedCorners;
    enum LevelNum entryLevel;
    u16 pssSlideStarTime;
    u16 metalCapDuration;
    u16 metalCapDurationCotmc;
    u16 vanishCapDurationVcutm;
    s16 floorLowerLimit;
    s16 floorLowerLimitMisc;
    s16 floorLowerLimitShadow;
};

extern struct LevelValues gLevelValues;

#endif
