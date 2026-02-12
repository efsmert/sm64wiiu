#include "sm64.h"
#include "pc/lua/utils/smlua_level_utils.h"
#include "pc/debuglog.h"
#include "game/area.h"
#include "data/dynos.c.h"

#define MIN_AREA_INDEX 0

static struct CustomLevelInfo* sCustomLevelHead = NULL;
static s16 sCustomLevelNumNext = CUSTOM_LEVEL_NUM_START;
static s32 sCustomLevelRegisterModIndex = 0;

void smlua_level_util_reset(void) {
    struct CustomLevelInfo* node = sCustomLevelHead;
    while (node != NULL) {
        struct CustomLevelInfo* next = node->next;
        free(node->scriptEntryName);
        free(node->fullName);
        free(node->shortName);
        free(node);
        node = next;
    }
    sCustomLevelHead = NULL;
    sCustomLevelNumNext = CUSTOM_LEVEL_NUM_START;
    sCustomLevelRegisterModIndex = 0;
}

void smlua_level_util_set_register_mod_index(s32 modIndex) {
    if (modIndex < 0) {
        modIndex = 0;
    }
    sCustomLevelRegisterModIndex = modIndex;
}

void smlua_level_util_change_area(s32 areaIndex) {
    if (areaIndex >= MIN_AREA_INDEX && areaIndex < MAX_AREAS && gAreas[areaIndex].unk04 != NULL) {
        change_area(areaIndex);
    }
}

struct CustomLevelInfo* smlua_level_util_get_info(s16 levelNum) {
    struct CustomLevelInfo* node = sCustomLevelHead;
    while (node != NULL) {
        if (node->levelNum == levelNum) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

struct CustomLevelInfo* smlua_level_util_get_info_from_short_name(const char* shortName) {
    if (shortName == NULL) { return NULL; }
    struct CustomLevelInfo* node = sCustomLevelHead;
    while (node != NULL) {
        if (node->shortName != NULL && strcmp(node->shortName, shortName) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static struct CustomLevelInfo* smlua_level_util_get_info_from_script(const char* scriptEntryName) {
    if (scriptEntryName == NULL) { return NULL; }
    struct CustomLevelInfo* node = sCustomLevelHead;
    while (node != NULL) {
        if (node->scriptEntryName != NULL && strcmp(node->scriptEntryName, scriptEntryName) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

struct CustomLevelInfo* smlua_level_util_get_info_from_course_num(u8 courseNum) {
    struct CustomLevelInfo* node = sCustomLevelHead;
    while (node != NULL) {
        if (node->courseNum == courseNum) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

s16 level_register(const char* scriptEntryName, s16 courseNum, const char* fullName, const char* shortName, u32 acousticReach, u32 echoLevel1, u32 echoLevel2, u32 echoLevel3) {
    if (scriptEntryName == NULL || fullName == NULL || shortName == NULL) {
        LOG_LUA("level_register: missing required params");
        return 0;
    }

    struct CustomLevelInfo* existing = smlua_level_util_get_info_from_script(scriptEntryName);
    if (existing != NULL) {
        return existing->levelNum;
    }

    LevelScript* script = dynos_get_level_script(scriptEntryName);
    if (script == NULL) {
        LOG_LUA("level_register: missing DynOS level script '%s'", scriptEntryName);
        return 0;
    }

    struct CustomLevelInfo* info = calloc(1, sizeof(*info));
    if (info == NULL) {
        LOG_LUA("level_register: OOM");
        return 0;
    }

    info->script = script;
    info->scriptEntryName = strdup(scriptEntryName);
    info->courseNum = courseNum;
    info->levelNum = sCustomLevelNumNext++;
    info->fullName = strdup(fullName);
    info->shortName = strdup(shortName);
    info->acousticReach = acousticReach;
    info->echoLevel1 = echoLevel1;
    info->echoLevel2 = echoLevel2;
    info->echoLevel3 = echoLevel3;
    info->modIndex = sCustomLevelRegisterModIndex;
    info->next = NULL;

    if (sCustomLevelHead == NULL) {
        sCustomLevelHead = info;
        return info->levelNum;
    }

    struct CustomLevelInfo* tail = sCustomLevelHead;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = info;
    return info->levelNum;
}

bool level_is_vanilla_level(s16 levelNum) {
    return dynos_level_is_vanilla_level(levelNum);
}

bool warp_to_warpnode(s32 aLevel, s32 aArea, s32 aAct, s32 aWarpId) {
    return dynos_warp_to_warpnode(aLevel, aArea, aAct, aWarpId);
}

bool warp_to_level(s32 aLevel, s32 aArea, s32 aAct) {
    return dynos_warp_to_level(aLevel, aArea, aAct);
}

bool warp_restart_level(void) {
    return dynos_warp_restart_level();
}

bool warp_to_start_level(void) {
    return dynos_warp_to_start_level();
}

bool warp_exit_level(s32 aDelay) {
    return dynos_warp_exit_level(aDelay);
}

bool warp_to_castle(s32 aLevel) {
    return dynos_warp_to_castle(aLevel);
}
