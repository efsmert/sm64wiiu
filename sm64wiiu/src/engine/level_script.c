#include <ultra64.h>
#ifdef NO_SEGMENTED_MEMORY
#include <string.h>
#endif

#include "sm64.h"
#include "audio/external.h"
#include "buffers/framebuffers.h"
#include "buffers/zbuffer.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/mario.h"
#include "game/memory.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/level_update.h"
#include "game/profiler.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "goddard/renderer.h"
#include "behavior_table.h"
#include "geo_layout.h"
#include "graph_node.h"
#include "level_script.h"
#include "level_commands.h"
#include "level_misc_macros.h"
#include "math_util.h"
#include "surface_collision.h"
#include "surface_load.h"

#ifndef TARGET_N64
#include "data/dynos.c.h"
#include "pc/lua/smlua.h"
#ifdef TARGET_WII_U
#include <whb/log.h>
#endif
#endif

#define CMD_GET(type, offset) (*(type *) (CMD_PROCESS_OFFSET(offset) + (u8 *) sCurrentCmd))

// These are equal
#define CMD_NEXT ((struct LevelCommand *) ((u8 *) sCurrentCmd + (sCurrentCmd->size << CMD_SIZE_SHIFT)))
#define NEXT_CMD ((struct LevelCommand *) ((sCurrentCmd->size << CMD_SIZE_SHIFT) + (u8 *) sCurrentCmd))

struct LevelCommand {
    /*00*/ u8 type;
    /*01*/ u8 size;
    /*02*/ // variable sized argument data
};

enum ScriptStatus { SCRIPT_RUNNING = 1, SCRIPT_PAUSED = 0, SCRIPT_PAUSED2 = -1 };

static uintptr_t sStack[32];

static struct AllocOnlyPool *sLevelPool = NULL;

static u16 sDelayFrames = 0;
static u16 sDelayFrames2 = 0;

static s16 sCurrAreaIndex = -1;

#ifndef TARGET_N64
// Co-op DX semantics: models loaded before the first FREE_LEVEL_POOL are permanent.
// Wii U clears MODEL_POOL_LEVEL when allocating a new level pool; putting shared models
// (Mario, doors, trees, etc.) into MODEL_POOL_LEVEL can wipe their geo and make them invisible.
static u8 sFinishedLoadingPerm = false;
#endif

static uintptr_t *sStackTop = sStack;
static uintptr_t *sStackBase = NULL;

static s16 sScriptStatus;
static s32 sRegister;
static struct LevelCommand *sCurrentCmd;

static inline bool level_cmd_swap_scalar_fields(void) {
    // Keep vanilla opcodes on native-endian reads. Restrict scalar swapping to
    // DynOS Lua extension opcodes that carry token-indexed scalar payloads.
    if (gLevelScriptModIndex < 0 || sCurrentCmd == NULL) {
        return false;
    }
    return (sCurrentCmd->type >= 0x3F && sCurrentCmd->type <= 0x44);
}

static inline u16 level_cmd_read_u16(u32 offset) {
    u16 value;
    memcpy(&value, (CMD_PROCESS_OFFSET(offset) + (u8 *) sCurrentCmd), sizeof(value));
    if (level_cmd_swap_scalar_fields()) {
        value = __builtin_bswap16(value);
    }
    return value;
}

static inline s16 level_cmd_read_s16(u32 offset) {
    s16 value;
    memcpy(&value, (CMD_PROCESS_OFFSET(offset) + (u8 *) sCurrentCmd), sizeof(value));
    if (level_cmd_swap_scalar_fields()) {
        value = (s16) __builtin_bswap16((u16) value);
    }
    return value;
}

static inline u32 level_cmd_read_u32(u32 offset) {
    u32 value;
    memcpy(&value, (CMD_PROCESS_OFFSET(offset) + (u8 *) sCurrentCmd), sizeof(value));
    if (level_cmd_swap_scalar_fields()) {
        value = __builtin_bswap32(value);
    }
    return value;
}

static inline s32 level_cmd_read_s32(u32 offset) {
    s32 value;
    memcpy(&value, (CMD_PROCESS_OFFSET(offset) + (u8 *) sCurrentCmd), sizeof(value));
    if (level_cmd_swap_scalar_fields()) {
        value = (s32) __builtin_bswap32((u32) value);
    }
    return value;
}

// DynOS: track currently executing level script for token/mod lookups.
LevelScript* gLevelScriptActive = NULL;
s32 gLevelScriptModIndex = -1;

#ifdef USE_SYSTEM_MALLOC
static struct MemoryPool *sMemPoolForGoddard;
#endif

static s32 eval_script_area(s32 arg) {
    return (sWarpDest.areaIdx == arg);
}

static s32 eval_script_op(s8 op, s32 arg) {
    s32 result = 0;

    switch (op) {
        case 0:
            result = sRegister & arg;
            break;
        case 1:
            result = !(sRegister & arg);
            break;
        case 2:
            result = sRegister == arg;
            break;
        case 3:
            result = sRegister != arg;
            break;
        case 4:
            result = sRegister < arg;
            break;
        case 5:
            result = sRegister <= arg;
            break;
        case 6:
            result = sRegister > arg;
            break;
        case 7:
            result = sRegister >= arg;
            break;
    }

    return result;
}

static void level_cmd_load_and_execute(void) {
    main_pool_push_state();
    load_segment(level_cmd_read_s16(2), CMD_GET(void *, 4), CMD_GET(void *, 8), MEMORY_POOL_LEFT);

    *sStackTop++ = (uintptr_t) NEXT_CMD;
    *sStackTop++ = (uintptr_t) sStackBase;
    sStackBase = sStackTop;

    sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 12));
}

static void level_cmd_exit_and_execute(void) {
    void *targetAddr = CMD_GET(void *, 12);

    main_pool_pop_state();
    main_pool_push_state();

    load_segment(level_cmd_read_s16(2), CMD_GET(void *, 4), CMD_GET(void *, 8),
            MEMORY_POOL_LEFT);

    sStackTop = sStackBase;
    sCurrentCmd = segmented_to_virtual(targetAddr);
}

static void level_cmd_exit(void) {
    main_pool_pop_state();

    sStackTop = sStackBase;
    sStackBase = (uintptr_t *) *(--sStackTop);
    sCurrentCmd = (struct LevelCommand *) *(--sStackTop);
}

static void level_cmd_sleep(void) {
    sScriptStatus = SCRIPT_PAUSED;

    if (sDelayFrames == 0) {
        sDelayFrames = level_cmd_read_s16(2);
    } else if (--sDelayFrames == 0) {
        sCurrentCmd = CMD_NEXT;
        sScriptStatus = SCRIPT_RUNNING;
    }
}

static void level_cmd_sleep2(void) {
    sScriptStatus = SCRIPT_PAUSED2;

    if (sDelayFrames2 == 0) {
        sDelayFrames2 = level_cmd_read_s16(2);
    } else if (--sDelayFrames2 == 0) {
        sCurrentCmd = CMD_NEXT;
        sScriptStatus = SCRIPT_RUNNING;
    }
}

static void level_cmd_jump(void) {
    sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 4));
}

static void level_cmd_jump_and_link(void) {
    *sStackTop++ = (uintptr_t) NEXT_CMD;
    sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 4));
}

static void level_cmd_return(void) {
    sCurrentCmd = (struct LevelCommand *) *(--sStackTop);
}

static void level_cmd_jump_and_link_push_arg(void) {
    *sStackTop++ = (uintptr_t) NEXT_CMD;
    *sStackTop++ = level_cmd_read_s16(2);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_jump_repeat(void) {
    s32 val = *(sStackTop - 1);

    if (val == 0) {
        sCurrentCmd = (struct LevelCommand *) *(sStackTop - 2);
    } else if (--val != 0) {
        *(sStackTop - 1) = val;
        sCurrentCmd = (struct LevelCommand *) *(sStackTop - 2);
    } else {
        sCurrentCmd = CMD_NEXT;
        sStackTop -= 2;
    }
}

static void level_cmd_loop_begin(void) {
    *sStackTop++ = (uintptr_t) NEXT_CMD;
    *sStackTop++ = 0;
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_loop_until(void) {
    if (eval_script_op(CMD_GET(u8, 2), level_cmd_read_s32(4)) != 0) {
        sCurrentCmd = CMD_NEXT;
        sStackTop -= 2;
    } else {
        sCurrentCmd = (struct LevelCommand *) *(sStackTop - 2);
    }
}

static void level_cmd_jump_if(void) {
    if (eval_script_op(CMD_GET(u8, 2), level_cmd_read_s32(4)) != 0) {
        sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 8));
    } else {
        sCurrentCmd = CMD_NEXT;
    }
}

static void level_cmd_jump_and_link_if(void) {
    if (eval_script_op(CMD_GET(u8, 2), level_cmd_read_s32(4)) != 0) {
        *sStackTop++ = (uintptr_t) NEXT_CMD;
        sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 8));
    } else {
        sCurrentCmd = CMD_NEXT;
    }
}

static void level_cmd_skip_if(void) {
    if (eval_script_op(CMD_GET(u8, 2), level_cmd_read_s32(4)) == 0) {
        do {
            sCurrentCmd = CMD_NEXT;
        } while (sCurrentCmd->type == 0x0F || sCurrentCmd->type == 0x10);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_skip(void) {
    do {
        sCurrentCmd = CMD_NEXT;
    } while (sCurrentCmd->type == 0x10);

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_skippable_nop(void) {
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_call(void) {
    typedef s32 (*Func)(s16, s32);
    Func func = CMD_GET(Func, 4);
    sRegister = func(level_cmd_read_s16(2), sRegister);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_call_loop(void) {
    typedef s32 (*Func)(s16, s32);
    Func func = CMD_GET(Func, 4);
    sRegister = func(level_cmd_read_s16(2), sRegister);

    if (sRegister == 0) {
        sScriptStatus = SCRIPT_PAUSED;
    } else {
        sScriptStatus = SCRIPT_RUNNING;
        sCurrentCmd = CMD_NEXT;
    }
}

static void level_cmd_set_register(void) {
    sRegister = level_cmd_read_s16(2);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_push_pool_state(void) {
    main_pool_push_state();
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_pop_pool_state(void) {
    main_pool_pop_state();
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_to_fixed_address(void) {
    load_to_fixed_pool_addr(CMD_GET(void *, 4), CMD_GET(void *, 8), CMD_GET(void *, 12));
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_raw(void) {
    load_segment(level_cmd_read_s16(2), CMD_GET(void *, 4), CMD_GET(void *, 8),
            MEMORY_POOL_LEFT);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_mio0(void) {
    load_segment_decompress(level_cmd_read_s16(2), CMD_GET(void *, 4), CMD_GET(void *, 8));
    sCurrentCmd = CMD_NEXT;
}

#ifdef USE_SYSTEM_MALLOC
static void *alloc_for_goddard(u32 size) {
    return mem_pool_alloc(sMemPoolForGoddard, size);
}

static void free_for_goddard(void *ptr) {
    mem_pool_free(sMemPoolForGoddard, ptr);
}
#endif

static void level_cmd_load_mario_head(void) {
#ifdef USE_SYSTEM_MALLOC
    sMemPoolForGoddard = mem_pool_init(0, 0);
    gdm_init(alloc_for_goddard, free_for_goddard);
    gdm_setup();
    gdm_maketestdl(level_cmd_read_s16(2));
#else
    // TODO: Fix these hardcoded sizes
    void *addr = main_pool_alloc(DOUBLE_SIZE_ON_64_BIT(0xE1000), MEMORY_POOL_LEFT);
    if (addr != NULL) {
        gdm_init(addr, DOUBLE_SIZE_ON_64_BIT(0xE1000));
        gd_add_to_heap(gZBuffer, sizeof(gZBuffer)); // 0x25800
        gd_add_to_heap(gFrameBuffer0, 3 * sizeof(gFrameBuffer0)); // 0x70800
        gdm_setup();
        gdm_maketestdl(level_cmd_read_s16(2));
    } else {
    }
#endif

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_mio0_texture(void) {
    load_segment_decompress_heap(level_cmd_read_s16(2), CMD_GET(void *, 4), CMD_GET(void *, 8));
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_init_level(void) {
    init_graph_node_start(NULL, (struct GraphNodeStart *) &gObjParentGraphNode);
    clear_objects();
    clear_areas();
    main_pool_push_state();

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_clear_level(void) {
    clear_objects();
    clear_area_graph_nodes();
    clear_areas();
    main_pool_pop_state();

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_alloc_level_pool(void) {
#ifndef TARGET_N64
    // Clear DynOS level-scoped model pool when a new level pool is allocated.
    dynos_model_clear_pool(MODEL_POOL_LEVEL);
    dynos_smlua_clear_gfx_command_cache();
#endif
    if (sLevelPool == NULL) {
#ifdef USE_SYSTEM_MALLOC
        sLevelPool = alloc_only_pool_init();
#else
        sLevelPool = alloc_only_pool_init(main_pool_available() - sizeof(struct AllocOnlyPool),
                                          MEMORY_POOL_LEFT);
#endif
    }
    // Co-op DX parity: always point each mario state at its SpawnInfo slot.
    for (s32 i = 0; i < MAX_PLAYERS; i++) {
        gMarioStates[i].spawnInfo = &gPlayerSpawnInfos[i];
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_free_level_pool(void) {
    s32 i;

#ifndef TARGET_N64
    if (!sFinishedLoadingPerm) {
        sFinishedLoadingPerm = true;
    }
#endif

#ifndef USE_SYSTEM_MALLOC
    // Wii U stability: main_pool_realloc can relocate alloc-only pools, which
    // invalidates SpawnInfo/terrain pointers captured in gAreas during script
    // parse. Custom DynOS stages with many OBJECT_EXT commands can then crash
    // when objects are spawned after this command.
    //
    // Keep the level pool in place and just drop the handle.
#ifndef TARGET_WII_U
    alloc_only_pool_resize(sLevelPool, sLevelPool->usedSpace);
#endif
#endif
    sLevelPool = NULL;

    for (i = 0; i < MAX_AREAS; i++) {
        if (gAreaData[i].terrainData != NULL) {
            alloc_surface_pools();
            break;
        }
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_begin_area(void) {
    u8 areaIndex = CMD_GET(u8, 2);
    void *geoLayoutAddr = CMD_GET(void *, 4);

    if (areaIndex < MAX_AREAS) {
#ifdef TARGET_WII_U
        static s32 sBeginAreaEnterLogs = 0;
        if (sBeginAreaEnterLogs < 64) {
            const u8 *geoBytes = (const u8 *) geoLayoutAddr;
            WHBLogPrintf("level_script: begin_area enter area=%u geo=%p cmd=%p bytes=%02X %02X %02X %02X",
                         areaIndex, geoLayoutAddr, sCurrentCmd,
                         (unsigned) sCurrentCmd->type,
                         (unsigned) sCurrentCmd->size,
                         (unsigned) ((u8 *) sCurrentCmd)[2],
                         (unsigned) ((u8 *) sCurrentCmd)[3]);
            if ((uintptr_t) geoLayoutAddr >= 0x10000000u) {
                WHBLogPrintf("level_script: begin_area geo bytes area=%u geo=%p data=%02X %02X %02X %02X %02X %02X %02X %02X",
                             areaIndex, geoLayoutAddr,
                             (unsigned) geoBytes[0], (unsigned) geoBytes[1],
                             (unsigned) geoBytes[2], (unsigned) geoBytes[3],
                             (unsigned) geoBytes[4], (unsigned) geoBytes[5],
                             (unsigned) geoBytes[6], (unsigned) geoBytes[7]);
            }
            sBeginAreaEnterLogs++;
        }
#endif
        struct GraphNodeRoot *screenArea = NULL;
#ifndef TARGET_N64
#ifdef TARGET_WII_U
        // DynOS custom levels can provide raw geo pointers in high memory.
        // Parse those directly via the level pool if DynOS model loading fails.
        if ((uintptr_t)geoLayoutAddr >= 0x10000000u) {
            screenArea = (struct GraphNodeRoot *) process_geo_layout(sLevelPool, geoLayoutAddr);
        }
#endif
        if (screenArea == NULL) {
        u32 id = 0;
        screenArea = (struct GraphNodeRoot *) dynos_model_load_geo(&id, MODEL_POOL_LEVEL, geoLayoutAddr, false);
        }
#else
        screenArea = (struct GraphNodeRoot *) process_geo_layout(sLevelPool, geoLayoutAddr);
#endif
        if (screenArea == NULL) {
#ifdef TARGET_WII_U
            static s32 sBeginAreaFailLogs = 0;
            if (sBeginAreaFailLogs < 32) {
                WHBLogPrintf("level_script: begin_area failed area=%u geo=%p", areaIndex, geoLayoutAddr);
                sBeginAreaFailLogs++;
            }
#else
            printf("level_script: begin_area failed area=%u geo=%p\n", areaIndex, geoLayoutAddr);
#endif
            sCurrentCmd = CMD_NEXT;
            return;
        }
        struct GraphNodeCamera *node = (struct GraphNodeCamera *) screenArea->views[0];
        if (node == NULL) {
            static s32 sBeginAreaNullCameraLogs = 0;
            if (sBeginAreaNullCameraLogs < 32) {
#ifdef TARGET_WII_U
                WHBLogPrintf("level_script: begin_area no camera area=%u geo=%p root=%p", areaIndex, geoLayoutAddr, screenArea);
#else
                printf("level_script: begin_area no camera area=%u geo=%p root=%p\n", areaIndex, geoLayoutAddr, screenArea);
#endif
                sBeginAreaNullCameraLogs++;
            }
        }

        sCurrAreaIndex = areaIndex;
        screenArea->areaIndex = areaIndex;
        gAreas[areaIndex].unk04 = screenArea;
        gAreas[areaIndex].nextSyncID = 10;

        if (node != NULL) {
            gAreas[areaIndex].camera = (struct Camera *) node->config.camera;
        } else {
            gAreas[areaIndex].camera = NULL;
        }
    } else {
#ifdef TARGET_WII_U
        static s32 sBeginAreaInvalidIndexLogs = 0;
        if (sBeginAreaInvalidIndexLogs < 32) {
            WHBLogPrintf("level_script: begin_area invalid area=%u geo=%p cmd=%p bytes=%02X %02X %02X %02X",
                         areaIndex, geoLayoutAddr, sCurrentCmd,
                         (unsigned) sCurrentCmd->type,
                         (unsigned) sCurrentCmd->size,
                         (unsigned) ((u8 *) sCurrentCmd)[2],
                         (unsigned) ((u8 *) sCurrentCmd)[3]);
            sBeginAreaInvalidIndexLogs++;
        }
#endif
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_end_area(void) {
    sCurrAreaIndex = -1;
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_model_from_dl(void) {
    s16 val1 = level_cmd_read_s16(2) & 0x0FFF;
    s16 val2 = ((u16)level_cmd_read_s16(2)) >> 12;
    void *val3 = CMD_GET(void *, 4);

    if (val1 < 256) {
        gLoadedGraphNodes[val1] =
            (struct GraphNode *) init_graph_node_display_list(sLevelPool, 0, val2, val3);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_model_from_geo(void) {
    s16 arg0 = level_cmd_read_s16(2);
    void *arg1 = CMD_GET(void *, 4);

    if (arg0 < 256) {
#ifndef TARGET_N64
        u32 id = (u32) arg0;
        struct GraphNode *node = dynos_model_load_geo(
            &id,
            sFinishedLoadingPerm ? MODEL_POOL_LEVEL : MODEL_POOL_PERMANENT,
            arg1,
            true);
        // Maintain vanilla lookup table for code paths that still reference gLoadedGraphNodes directly.
        gLoadedGraphNodes[arg0] = node;
#else
        gLoadedGraphNodes[arg0] = process_geo_layout(sLevelPool, arg1);
#endif
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_23(void) {
    union {
        s32 i;
        f32 f;
    } arg2;

    s16 model = level_cmd_read_s16(2) & 0x0FFF;
    s16 arg0H = ((u16)level_cmd_read_s16(2)) >> 12;
    void *arg1 = CMD_GET(void *, 4);
    // load an f32, but using an integer load instruction for some reason (hence the union)
    arg2.i = level_cmd_read_s32(8);

    if (model < 256) {
#ifndef TARGET_N64
        // Store the scaled node in the DynOS model table so later lookups can use dynos_model_get_geo().
        u32 id = (u32) model;
        struct GraphNode *node = dynos_model_store_geo(
            &id,
            MODEL_POOL_LEVEL,
            arg1,
            (struct GraphNode *) init_graph_node_scale(sLevelPool, 0, arg0H, arg1, arg2.f));
        gLoadedGraphNodes[model] = node;
#else
        // GraphNodeScale has a GraphNode at the top. This
        // is being stored to the array, so cast the pointer.
        gLoadedGraphNodes[model] =
            (struct GraphNode *) init_graph_node_scale(sLevelPool, 0, arg0H, arg1, arg2.f);
#endif
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_init_mario(void) {
    // Co-op DX semantics: INIT_MARIO defines a player spawn list, not a single mario spawn.
    UNUSED u32 behaviorArg = level_cmd_read_u32(4);
    void *behaviorScript = CMD_GET(void *, 8);
    u16 slot = CMD_GET(u8, 3);

    struct GraphNode *unk18 = NULL;
#ifndef TARGET_N64
    unk18 = dynos_model_get_geo(slot);
#endif
    if (unk18 == NULL && slot < 256) {
        unk18 = gLoadedGraphNodes[slot];
    }

    // Wii U currently runs a single-player mario update loop (gMarioState points at index 0).
    // Spawning all MAX_PLAYERS mario objects like PC Co-op DX does will break rendering/gameplay.
    // We still initialize the whole table for compatibility, but only link/spawn player 0.
    for (s32 i = 0; i < MAX_PLAYERS; i++) {
        struct SpawnInfo *spawnInfo = &gPlayerSpawnInfos[i];
        vec3s_set(spawnInfo->startPos, 0, 0, 0);
        vec3s_set(spawnInfo->startAngle, 0, 0, 0);

        spawnInfo->activeAreaIndex = -1;
        spawnInfo->areaIndex = (i == 0) ? 0 : -1;
        spawnInfo->behaviorArg = (u32) i | ((u32) 1 << 31);
        spawnInfo->behaviorScript = behaviorScript;
        spawnInfo->unk18 = unk18;
        spawnInfo->next = NULL;
        spawnInfo->syncID = 0;
    }
    gPlayerSpawnInfos[0].next = NULL;

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_place_object(void) {
    u8 val7 = 1 << (gCurrActNum - 1);
    u16 model;
    struct SpawnInfo *spawnInfo;

    if (sCurrAreaIndex != -1 && ((CMD_GET(u8, 2) & val7) || CMD_GET(u8, 2) == 0x1F)) {
        model = CMD_GET(u8, 3);
        spawnInfo = alloc_only_pool_alloc(sLevelPool, sizeof(struct SpawnInfo));

        spawnInfo->startPos[0] = level_cmd_read_s16(4);
        spawnInfo->startPos[1] = level_cmd_read_s16(6);
        spawnInfo->startPos[2] = level_cmd_read_s16(8);

        spawnInfo->startAngle[0] = level_cmd_read_s16(10) * 0x8000 / 180;
        spawnInfo->startAngle[1] = level_cmd_read_s16(12) * 0x8000 / 180;
        spawnInfo->startAngle[2] = level_cmd_read_s16(14) * 0x8000 / 180;

        spawnInfo->areaIndex = sCurrAreaIndex;
        spawnInfo->activeAreaIndex = sCurrAreaIndex;

        spawnInfo->behaviorArg = level_cmd_read_u32(16);
        spawnInfo->behaviorScript = CMD_GET(void *, 20);
        spawnInfo->unk18 = NULL;
#ifndef TARGET_N64
        spawnInfo->unk18 = dynos_model_get_geo(model);
#endif
        if (spawnInfo->unk18 == NULL && model < 256) {
            spawnInfo->unk18 = gLoadedGraphNodes[model];
        }
        spawnInfo->next = gAreas[sCurrAreaIndex].objectSpawnInfos;
        spawnInfo->syncID = gAreas[sCurrAreaIndex].nextSyncID;
        gAreas[sCurrAreaIndex].nextSyncID += 10;

        gAreas[sCurrAreaIndex].objectSpawnInfos = spawnInfo;
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_create_warp_node(void) {
    if (sCurrAreaIndex != -1) {
        struct ObjectWarpNode *warpNode =
            alloc_only_pool_alloc(sLevelPool, sizeof(struct ObjectWarpNode));

        warpNode->node.id = CMD_GET(u8, 2);
        warpNode->node.destLevel = CMD_GET(u8, 3) + CMD_GET(u8, 6);
        warpNode->node.destArea = CMD_GET(u8, 4);
        warpNode->node.destNode = CMD_GET(u8, 5);

        warpNode->object = NULL;

        warpNode->next = gAreas[sCurrAreaIndex].warpNodes;
        gAreas[sCurrAreaIndex].warpNodes = warpNode;
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_create_instant_warp(void) {
    s32 i;
    struct InstantWarp *warp;

    if (sCurrAreaIndex != -1) {
        if (gAreas[sCurrAreaIndex].instantWarps == NULL) {
            gAreas[sCurrAreaIndex].instantWarps =
                alloc_only_pool_alloc(sLevelPool, 4 * sizeof(struct InstantWarp));

            for (i = INSTANT_WARP_INDEX_START; i < INSTANT_WARP_INDEX_STOP; i++) {
                gAreas[sCurrAreaIndex].instantWarps[i].id = 0;
            }
        }

        warp = gAreas[sCurrAreaIndex].instantWarps + CMD_GET(u8, 2);

        warp[0].id = 1;
        warp[0].area = CMD_GET(u8, 3);

        warp[0].displacement[0] = level_cmd_read_s16(4);
        warp[0].displacement[1] = level_cmd_read_s16(6);
        warp[0].displacement[2] = level_cmd_read_s16(8);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_terrain_type(void) {
    if (sCurrAreaIndex != -1) {
        gAreas[sCurrAreaIndex].terrainType |= level_cmd_read_s16(2);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_create_painting_warp_node(void) {
    s32 i;
    struct WarpNode *node;

    if (sCurrAreaIndex != -1) {
        if (gAreas[sCurrAreaIndex].paintingWarpNodes == NULL) {
            gAreas[sCurrAreaIndex].paintingWarpNodes =
                alloc_only_pool_alloc(sLevelPool, 45 * sizeof(struct WarpNode));

            for (i = 0; i < 45; i++) {
                gAreas[sCurrAreaIndex].paintingWarpNodes[i].id = 0;
            }
        }

        node = &gAreas[sCurrAreaIndex].paintingWarpNodes[CMD_GET(u8, 2)];

        node->id = 1;
        node->destLevel = CMD_GET(u8, 3) + CMD_GET(u8, 6);
        node->destArea = CMD_GET(u8, 4);
        node->destNode = CMD_GET(u8, 5);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_3A(void) {
    struct UnusedArea28 *val4;

    if (sCurrAreaIndex != -1) {
        if ((val4 = gAreas[sCurrAreaIndex].unused28) == NULL) {
            val4 = gAreas[sCurrAreaIndex].unused28 =
                alloc_only_pool_alloc(sLevelPool, sizeof(struct UnusedArea28));
        }

        val4->unk00 = level_cmd_read_s16(2);
        val4->unk02 = level_cmd_read_s16(4);
        val4->unk04 = level_cmd_read_s16(6);
        val4->unk06 = level_cmd_read_s16(8);
        val4->unk08 = level_cmd_read_s16(10);
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_create_whirlpool(void) {
    struct Whirlpool *whirlpool;
    s32 index = CMD_GET(u8, 2);
    s32 beatBowser2 =
        (save_file_get_flags() & (SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR)) != 0;

    if (CMD_GET(u8, 3) == 0 || (CMD_GET(u8, 3) == 1 && !beatBowser2)
        || (CMD_GET(u8, 3) == 2 && beatBowser2) || (CMD_GET(u8, 3) == 3 && gCurrActNum >= 2)) {
        if (sCurrAreaIndex != -1 && index < 2) {
            if ((whirlpool = gAreas[sCurrAreaIndex].whirlpools[index]) == NULL) {
                whirlpool = alloc_only_pool_alloc(sLevelPool, sizeof(struct Whirlpool));
                gAreas[sCurrAreaIndex].whirlpools[index] = whirlpool;
            }

            vec3s_set(whirlpool->pos, level_cmd_read_s16(4), level_cmd_read_s16(6), level_cmd_read_s16(8));
            whirlpool->strength = level_cmd_read_s16(10);
        }
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_blackout(void) {
    osViBlack(CMD_GET(u8, 2));
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_gamma(void) {
    osViSetSpecialFeatures(CMD_GET(u8, 2) == 0 ? OS_VI_GAMMA_OFF : OS_VI_GAMMA_ON);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_terrain_data(void) {
    if (sCurrAreaIndex != -1) {
#ifndef NO_SEGMENTED_MEMORY
        gAreas[sCurrAreaIndex].terrainData = segmented_to_virtual(CMD_GET(void *, 4));
#else
        Collision *data;
        u32 size;

        // The game modifies the terrain data and must be reset upon level reload.
        data = segmented_to_virtual(CMD_GET(void *, 4));
        size = get_area_terrain_size(data) * sizeof(Collision);
        gAreas[sCurrAreaIndex].terrainData = alloc_only_pool_alloc(sLevelPool, size);
        memcpy(gAreas[sCurrAreaIndex].terrainData, data, size);
#endif
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_rooms(void) {
    if (sCurrAreaIndex != -1) {
        gAreas[sCurrAreaIndex].surfaceRooms = segmented_to_virtual(CMD_GET(void *, 4));
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_macro_objects(void) {
    if (sCurrAreaIndex != -1) {
#ifndef NO_SEGMENTED_MEMORY
        gAreas[sCurrAreaIndex].macroObjects = segmented_to_virtual(CMD_GET(void *, 4));
#else
        // The game modifies the macro object data (for example marking coins as taken),
        // so it must be reset when the level reloads.
        MacroObject *data = segmented_to_virtual(CMD_GET(void *, 4));
        s32 len = 0;
        while (data[len++] != MACRO_OBJECT_END()) {
            len += 4;
        }
        gAreas[sCurrAreaIndex].macroObjects = alloc_only_pool_alloc(sLevelPool, len * sizeof(MacroObject));
        memcpy(gAreas[sCurrAreaIndex].macroObjects, data, len * sizeof(MacroObject));
#endif
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_area(void) {
    s16 areaIndex = CMD_GET(u8, 2);
    UNUSED void *unused = (u8 *) sCurrentCmd + 4;

    stop_sounds_in_continuous_banks();
    load_area(areaIndex);

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_unload_area(void) {
    unload_area();
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_mario_start_pos(void) {
    gMarioSpawnInfo->areaIndex = CMD_GET(u8, 2);

#if IS_64_BIT
    vec3s_set(gMarioSpawnInfo->startPos, level_cmd_read_s16(6), level_cmd_read_s16(8), level_cmd_read_s16(10));
#else
    gMarioSpawnInfo->startPos[0] = level_cmd_read_s16(6);
    gMarioSpawnInfo->startPos[1] = level_cmd_read_s16(8);
    gMarioSpawnInfo->startPos[2] = level_cmd_read_s16(10);
#endif
    vec3s_set(gMarioSpawnInfo->startAngle, 0, level_cmd_read_s16(4) * 0x8000 / 180, 0);

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_2C(void) {
    unload_mario_area();
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_2D(void) {
    area_update_objects();
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_transition(void) {
    if (gCurrentArea != NULL) {
        play_transition(CMD_GET(u8, 2), CMD_GET(u8, 3), CMD_GET(u8, 4), CMD_GET(u8, 5), CMD_GET(u8, 6));
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_nop(void) {
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_show_dialog(void) {
    if (sCurrAreaIndex != -1) {
        if (CMD_GET(u8, 2) < 2) {
            gAreas[sCurrAreaIndex].dialog[CMD_GET(u8, 2)] = CMD_GET(u8, 3);
        }
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_music(void) {
    if (sCurrAreaIndex != -1) {
        gAreas[sCurrAreaIndex].musicParam = level_cmd_read_s16(2);
        gAreas[sCurrAreaIndex].musicParam2 = level_cmd_read_s16(4);
    }
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_set_menu_music(void) {
    set_background_music(0, level_cmd_read_s16(2), 0);
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_38(void) {
    fadeout_music(level_cmd_read_s16(2));
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_get_or_set_var(void) {
    if (CMD_GET(u8, 2) == 0) {
        switch (CMD_GET(u8, 3)) {
            case 0:
                gCurrSaveFileNum = sRegister;
                break;
            case 1:
                gCurrCourseNum = sRegister;
                break;
            case 2:
                gCurrActNum = sRegister;
                break;
            case 3:
                gCurrLevelNum = sRegister;
                break;
            case 4:
                gCurrAreaIndex = sRegister;
                break;
        }
    } else {
        switch (CMD_GET(u8, 3)) {
            case 0:
                sRegister = gCurrSaveFileNum;
                break;
            case 1:
                sRegister = gCurrCourseNum;
                break;
            case 2:
                sRegister = gCurrActNum;
                break;
            case 3:
                sRegister = gCurrLevelNum;
                break;
            case 4:
                sRegister = gCurrAreaIndex;
                break;
        }
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_advdemo(void) {
    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_cleardemoptr(void) {
    gCurrDemoInput = NULL;
    sCurrentCmd = CMD_NEXT;
}

#ifndef TARGET_N64
static bool level_cmd_read_lua_integer(const char *varName, uintptr_t *outValue) {
    lua_State *L = smlua_get_state();
    int top;

    if (L == NULL || varName == NULL || varName[0] == '\0' || outValue == NULL) {
        return false;
    }

    top = lua_gettop(L);
    lua_getglobal(L, varName);

    if (lua_isinteger(L, -1) || lua_isnumber(L, -1)) {
        *outValue = (uintptr_t) lua_tointeger(L, -1);
        lua_settop(L, top);
        return true;
    }

    if (lua_islightuserdata(L, -1)) {
        *outValue = (uintptr_t) lua_touserdata(L, -1);
        lua_settop(L, top);
        return true;
    }

    lua_settop(L, top);
    return false;
}

static bool resolve_lua_param_token(uintptr_t *param, u32 tokenIndex) {
    const char *paramStr = dynos_level_get_token(tokenIndex);
    if (paramStr == NULL) {
        printf("level_script: lua token missing index=%u\n", tokenIndex);
        return false;
    }
    if (!level_cmd_read_lua_integer(paramStr, param)) {
        printf("level_script: lua value missing token=%s\n", paramStr);
        return false;
    }
    return true;
}

#define get_lua_param_u8(name, flag) \
    u8 name = CMD_GET(u8, flag##_OFFSET(cmdType)); \
    if (luaParams & (flag)) { \
        uintptr_t name##Param = 0; \
        if (!resolve_lua_param_token(&name##Param, level_cmd_read_u32(flag##_OFFSET(cmdType)))) { \
            sCurrentCmd = CMD_NEXT; \
            return; \
        } \
        name = (u8) name##Param; \
    }

#define get_lua_param_s16(name, flag) \
    s16 name = level_cmd_read_s16(flag##_OFFSET(cmdType)); \
    if (luaParams & (flag)) { \
        uintptr_t name##Param = 0; \
        if (!resolve_lua_param_token(&name##Param, level_cmd_read_u32(flag##_OFFSET(cmdType)))) { \
            sCurrentCmd = CMD_NEXT; \
            return; \
        } \
        name = (s16) name##Param; \
    }

#define get_lua_param_u32(name, flag) \
    u32 name = level_cmd_read_u32(flag##_OFFSET(cmdType)); \
    if (luaParams & (flag)) { \
        uintptr_t name##Param = 0; \
        if (!resolve_lua_param_token(&name##Param, level_cmd_read_u32(flag##_OFFSET(cmdType)))) { \
            sCurrentCmd = CMD_NEXT; \
            return; \
        } \
        name = (u32) name##Param; \
    }

#define get_lua_param_s32(name, flag) \
    s32 name = level_cmd_read_s32(flag##_OFFSET(cmdType)); \
    if (luaParams & (flag)) { \
        uintptr_t name##Param = 0; \
        if (!resolve_lua_param_token(&name##Param, level_cmd_read_u32(flag##_OFFSET(cmdType)))) { \
            sCurrentCmd = CMD_NEXT; \
            return; \
        } \
        name = (s32) name##Param; \
    }

#define get_lua_param_ptr(name, flag) \
    uintptr_t name = CMD_GET(uintptr_t, flag##_OFFSET(cmdType)); \
    if (luaParams & (flag)) { \
        if (!resolve_lua_param_token(&name, level_cmd_read_u32(flag##_OFFSET(cmdType)))) { \
            sCurrentCmd = CMD_NEXT; \
            return; \
        } \
    }

static void level_cmd_place_object_ext_lua_params(void) {
    u8 val7 = 1 << (gCurrActNum - 1);
    struct SpawnInfo *spawnInfo;
    u8 cmdType = sCurrentCmd->type;
    u16 luaParams = (
        cmdType == 0x3F ? OBJECT_EXT_LUA_BEHAVIOR : (
        cmdType == 0x40 ? OBJECT_EXT_LUA_BEHAVIOR | OBJECT_EXT_LUA_MODEL : (
        level_cmd_read_u16(2)
    )));

    get_lua_param_u8(acts, OBJECT_EXT_LUA_ACTS);

    if (sCurrAreaIndex != -1 && ((acts & val7) || acts == 0x1F)) {
        spawnInfo = alloc_only_pool_alloc(sLevelPool, sizeof(struct SpawnInfo));
        if (spawnInfo == NULL) {
            sCurrentCmd = CMD_NEXT;
            return;
        }

        get_lua_param_u32(modelId, OBJECT_EXT_LUA_MODEL);
        get_lua_param_s16(posX, OBJECT_EXT_LUA_POS_X);
        get_lua_param_s16(posY, OBJECT_EXT_LUA_POS_Y);
        get_lua_param_s16(posZ, OBJECT_EXT_LUA_POS_Z);
        get_lua_param_s16(angleX, OBJECT_EXT_LUA_ANGLE_X);
        get_lua_param_s16(angleY, OBJECT_EXT_LUA_ANGLE_Y);
        get_lua_param_s16(angleZ, OBJECT_EXT_LUA_ANGLE_Z);
        get_lua_param_u32(behParam, OBJECT_EXT_LUA_BEH_PARAMS);
        get_lua_param_ptr(behavior, OBJECT_EXT_LUA_BEHAVIOR);

        spawnInfo->startPos[0] = posX;
        spawnInfo->startPos[1] = posY;
        spawnInfo->startPos[2] = posZ;
        spawnInfo->startAngle[0] = (angleX * 0x8000) / 180;
        spawnInfo->startAngle[1] = (angleY * 0x8000) / 180;
        spawnInfo->startAngle[2] = (angleZ * 0x8000) / 180;
        spawnInfo->areaIndex = sCurrAreaIndex;
        spawnInfo->activeAreaIndex = sCurrAreaIndex;
        spawnInfo->behaviorArg = behParam;

        spawnInfo->unk18 = dynos_model_get_geo(modelId);
        if (spawnInfo->unk18 == NULL && modelId < 256) {
            spawnInfo->unk18 = gLoadedGraphNodes[modelId];
        }

        if (luaParams & OBJECT_EXT_LUA_BEHAVIOR) {
            const BehaviorScript *resolvedBehavior = get_behavior_from_id((enum BehaviorId) behavior);
            if (resolvedBehavior == NULL) {
                static s32 sLuaBehaviorResolveFailLogs = 0;
                if (sLuaBehaviorResolveFailLogs < 64) {
                    printf("level_script: object_ext unresolved behavior id=0x%08X cmd=0x%02X area=%d\n",
                           (unsigned int) behavior, (unsigned int) cmdType, (int) sCurrAreaIndex);
                    sLuaBehaviorResolveFailLogs++;
                }
                sCurrentCmd = CMD_NEXT;
                return;
            }
            spawnInfo->behaviorScript = (void *) resolvedBehavior;
        } else {
            spawnInfo->behaviorScript = (void *) behavior;
        }

        if (spawnInfo->behaviorScript == NULL) {
            static s32 sLuaBehaviorNullLogs = 0;
            if (sLuaBehaviorNullLogs < 64) {
                printf("level_script: object_ext null behavior cmd=0x%02X area=%d\n",
                       (unsigned int) cmdType, (int) sCurrAreaIndex);
                sLuaBehaviorNullLogs++;
            }
            sCurrentCmd = CMD_NEXT;
            return;
        }

        spawnInfo->next = gAreas[sCurrAreaIndex].objectSpawnInfos;
        spawnInfo->syncID = gAreas[sCurrAreaIndex].nextSyncID;
        gAreas[sCurrAreaIndex].nextSyncID += 10;
        gAreas[sCurrAreaIndex].objectSpawnInfos = spawnInfo;
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_load_model_from_geo_ext(void) {
    s16 modelSlot = level_cmd_read_s16(2);
    const char* geoName = dynos_level_get_token(level_cmd_read_u32(4));

    if (modelSlot >= 0 && modelSlot < 256 && geoName != NULL) {
        const void *geo = dynos_geolayout_get(geoName);
        if (geo != NULL) {
            gLoadedGraphNodes[modelSlot] = process_geo_layout(sLevelPool, (void *) geo);
        } else {
            printf("level_script: geo_ext not found '%s'\n", geoName);
        }
    }

    sCurrentCmd = CMD_NEXT;
}

static void level_cmd_jump_area_ext(void) {
    if (eval_script_area(level_cmd_read_s32(4))) {
        sCurrentCmd = segmented_to_virtual(CMD_GET(void *, 8));
    } else {
        sCurrentCmd = CMD_NEXT;
    }
}

static void level_cmd_show_dialog_ext(void) {
    if (sCurrAreaIndex != -1) {
        u8 luaParams = CMD_GET(u8, 2);
        u8 cmdType = sCurrentCmd->type;
        (void) cmdType;

        get_lua_param_u8(index, SHOW_DIALOG_EXT_LUA_INDEX);
        get_lua_param_s32(dialogId, SHOW_DIALOG_EXT_LUA_DIALOG);

        if (index < 2) {
            gAreas[sCurrAreaIndex].dialog[index] = dialogId;
        }
    }

    sCurrentCmd = CMD_NEXT;
}
#endif

static void (*LevelScriptJumpTable[])(void) = {
    /*00*/ level_cmd_load_and_execute,
    /*01*/ level_cmd_exit_and_execute,
    /*02*/ level_cmd_exit,
    /*03*/ level_cmd_sleep,
    /*04*/ level_cmd_sleep2,
    /*05*/ level_cmd_jump,
    /*06*/ level_cmd_jump_and_link,
    /*07*/ level_cmd_return,
    /*08*/ level_cmd_jump_and_link_push_arg,
    /*09*/ level_cmd_jump_repeat,
    /*0A*/ level_cmd_loop_begin,
    /*0B*/ level_cmd_loop_until,
    /*0C*/ level_cmd_jump_if,
    /*0D*/ level_cmd_jump_and_link_if,
    /*0E*/ level_cmd_skip_if,
    /*0F*/ level_cmd_skip,
    /*10*/ level_cmd_skippable_nop,
    /*11*/ level_cmd_call,
    /*12*/ level_cmd_call_loop,
    /*13*/ level_cmd_set_register,
    /*14*/ level_cmd_push_pool_state,
    /*15*/ level_cmd_pop_pool_state,
    /*16*/ level_cmd_load_to_fixed_address,
    /*17*/ level_cmd_load_raw,
    /*18*/ level_cmd_load_mio0,
    /*19*/ level_cmd_load_mario_head,
    /*1A*/ level_cmd_load_mio0_texture,
    /*1B*/ level_cmd_init_level,
    /*1C*/ level_cmd_clear_level,
    /*1D*/ level_cmd_alloc_level_pool,
    /*1E*/ level_cmd_free_level_pool,
    /*1F*/ level_cmd_begin_area,
    /*20*/ level_cmd_end_area,
    /*21*/ level_cmd_load_model_from_dl,
    /*22*/ level_cmd_load_model_from_geo,
    /*23*/ level_cmd_23,
    /*24*/ level_cmd_place_object,
    /*25*/ level_cmd_init_mario,
    /*26*/ level_cmd_create_warp_node,
    /*27*/ level_cmd_create_painting_warp_node,
    /*28*/ level_cmd_create_instant_warp,
    /*29*/ level_cmd_load_area,
    /*2A*/ level_cmd_unload_area,
    /*2B*/ level_cmd_set_mario_start_pos,
    /*2C*/ level_cmd_2C,
    /*2D*/ level_cmd_2D,
    /*2E*/ level_cmd_set_terrain_data,
    /*2F*/ level_cmd_set_rooms,
    /*30*/ level_cmd_show_dialog,
    /*31*/ level_cmd_set_terrain_type,
    /*32*/ level_cmd_nop,
    /*33*/ level_cmd_set_transition,
    /*34*/ level_cmd_set_blackout,
    /*35*/ level_cmd_set_gamma,
    /*36*/ level_cmd_set_music,
    /*37*/ level_cmd_set_menu_music,
    /*38*/ level_cmd_38,
    /*39*/ level_cmd_set_macro_objects,
    /*3A*/ level_cmd_3A,
    /*3B*/ level_cmd_create_whirlpool,
    /*3C*/ level_cmd_get_or_set_var,
    /*3D*/ level_cmd_advdemo,
    /*3E*/ level_cmd_cleardemoptr,
#ifndef TARGET_N64
    /*3F*/ level_cmd_place_object_ext_lua_params,
    /*40*/ level_cmd_place_object_ext_lua_params,
    /*41*/ level_cmd_load_model_from_geo_ext,
    /*42*/ level_cmd_jump_area_ext,
    /*43*/ level_cmd_place_object_ext_lua_params,
    /*44*/ level_cmd_show_dialog_ext,
#endif
};

// Canonical copy used if the jump table is corrupted by memory scribbles during
// heavy DynOS mod loads. The symptom is jumping into unrelated functions due to
// overwritten function pointers.
static void (*LevelScriptJumpTableCanonical[])(void) = {
    /*00*/ level_cmd_load_and_execute,
    /*01*/ level_cmd_exit_and_execute,
    /*02*/ level_cmd_exit,
    /*03*/ level_cmd_sleep,
    /*04*/ level_cmd_sleep2,
    /*05*/ level_cmd_jump,
    /*06*/ level_cmd_jump_and_link,
    /*07*/ level_cmd_return,
    /*08*/ level_cmd_jump_and_link_push_arg,
    /*09*/ level_cmd_jump_repeat,
    /*0A*/ level_cmd_loop_begin,
    /*0B*/ level_cmd_loop_until,
    /*0C*/ level_cmd_jump_if,
    /*0D*/ level_cmd_jump_and_link_if,
    /*0E*/ level_cmd_skip_if,
    /*0F*/ level_cmd_skip,
    /*10*/ level_cmd_skippable_nop,
    /*11*/ level_cmd_call,
    /*12*/ level_cmd_call_loop,
    /*13*/ level_cmd_set_register,
    /*14*/ level_cmd_push_pool_state,
    /*15*/ level_cmd_pop_pool_state,
    /*16*/ level_cmd_load_to_fixed_address,
    /*17*/ level_cmd_load_raw,
    /*18*/ level_cmd_load_mio0,
    /*19*/ level_cmd_load_mario_head,
    /*1A*/ level_cmd_load_mio0_texture,
    /*1B*/ level_cmd_init_level,
    /*1C*/ level_cmd_clear_level,
    /*1D*/ level_cmd_alloc_level_pool,
    /*1E*/ level_cmd_free_level_pool,
    /*1F*/ level_cmd_begin_area,
    /*20*/ level_cmd_end_area,
    /*21*/ level_cmd_load_model_from_dl,
    /*22*/ level_cmd_load_model_from_geo,
    /*23*/ level_cmd_23,
    /*24*/ level_cmd_place_object,
    /*25*/ level_cmd_init_mario,
    /*26*/ level_cmd_create_warp_node,
    /*27*/ level_cmd_create_painting_warp_node,
    /*28*/ level_cmd_create_instant_warp,
    /*29*/ level_cmd_load_area,
    /*2A*/ level_cmd_unload_area,
    /*2B*/ level_cmd_set_mario_start_pos,
    /*2C*/ level_cmd_2C,
    /*2D*/ level_cmd_2D,
    /*2E*/ level_cmd_set_terrain_data,
    /*2F*/ level_cmd_set_rooms,
    /*30*/ level_cmd_show_dialog,
    /*31*/ level_cmd_set_terrain_type,
    /*32*/ level_cmd_nop,
    /*33*/ level_cmd_set_transition,
    /*34*/ level_cmd_set_blackout,
    /*35*/ level_cmd_set_gamma,
    /*36*/ level_cmd_set_music,
    /*37*/ level_cmd_set_menu_music,
    /*38*/ level_cmd_38,
    /*39*/ level_cmd_set_macro_objects,
    /*3A*/ level_cmd_3A,
    /*3B*/ level_cmd_create_whirlpool,
    /*3C*/ level_cmd_get_or_set_var,
    /*3D*/ level_cmd_advdemo,
    /*3E*/ level_cmd_cleardemoptr,
#ifndef TARGET_N64
    /*3F*/ level_cmd_place_object_ext_lua_params,
    /*40*/ level_cmd_place_object_ext_lua_params,
    /*41*/ level_cmd_load_model_from_geo_ext,
    /*42*/ level_cmd_jump_area_ext,
    /*43*/ level_cmd_place_object_ext_lua_params,
    /*44*/ level_cmd_show_dialog_ext,
#endif
};

struct LevelCommand *level_script_execute(struct LevelCommand *cmd) {
    sScriptStatus = SCRIPT_RUNNING;
    sCurrentCmd = cmd;
#ifndef TARGET_N64
    // Clear stale DynOS script context between executions; DynOS_SwapCmd will
    // repopulate this when the current script pointer is custom.
    gLevelScriptModIndex = -1;
    gLevelScriptActive = NULL;
#endif

    void (**jumpTable)(void) = LevelScriptJumpTable;
    if (jumpTable[0] != level_cmd_load_and_execute ||
        jumpTable[1] != level_cmd_exit_and_execute ||
        jumpTable[2] != level_cmd_exit) {
        jumpTable = LevelScriptJumpTableCanonical;
    }

    while (sScriptStatus == SCRIPT_RUNNING) {
#ifndef TARGET_N64
        sCurrentCmd = (struct LevelCommand *) dynos_swap_cmd(sCurrentCmd);
        void *dynosCurrCmd = (void *) sCurrentCmd;
#endif

        if (sCurrentCmd->type < ARRAY_COUNT(LevelScriptJumpTableCanonical)) {
            jumpTable[sCurrentCmd->type]();
        } else {
            sScriptStatus = SCRIPT_PAUSED;
        }

#ifndef TARGET_N64
        void *dynosNextCmd = dynos_update_cmd(dynosCurrCmd);
        if (dynosNextCmd) {
            sCurrentCmd = (struct LevelCommand *) dynosNextCmd;
        }
#endif
    }

    profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);
    init_rcp();
    render_game();
    end_master_display_list();
    alloc_display_list(0);

    return sCurrentCmd;
}
