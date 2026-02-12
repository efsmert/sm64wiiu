#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include "sm64.h"
#include "behavior_commands.h"
#include "behavior_data.h"
#include "behavior_table.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"

#include "smlua.h"
#include "smlua_hooks.h"
#include "smlua_cobject.h"
#include <lauxlib.h>
#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

#define MAX_HOOKED_REFERENCES 64
#define LUA_BEHAVIOR_FLAG (1 << 15)
#define MAX_SYNC_TABLE_CHANGE_HOOKS 64
#define MAX_MARIO_ACTION_HOOKS 128
#define MAX_BEHAVIOR_HOOKS 1024
#define MAX_HOOKED_CUSTOM_BEHAVIORS 1024
// Keep Wii U runtime safe: if a mod spins in an update hook we want it to fail fast
// with a traceback instead of stalling the frame for seconds.
#define SMLUA_CALLBACK_INSTRUCTION_BUDGET 1000000

struct LuaHookedEvent {
    int references[MAX_HOOKED_REFERENCES];
    int count;
};

static lua_State *sHookState = NULL;
static struct LuaHookedEvent sHookedEvents[HOOK_MAX];
static int sNextModMenuHandle = 1;
static bool sBeforePhysWaterHookCountLogged = false;
static int sBeforePhysWaterVelChangeLogs = 0;
static bool sHookDispatchLogged[HOOK_MAX] = { 0 };
static bool sBeforePhysStepTypeLogged[4] = { false, false, false, false };
static int sBeforePhysEarlyReturnLogs = 0;
static bool sHookStateReboundLogged = false;
static bool sHookStateUnavailableLogged = false;
static u8 sMarioFreezeTimers[MAX_PLAYERS] = { 0 };

struct LuaSyncTableChangeHook {
    int tableRef;
    int keyRef;
    int tagRef;
    int funcRef;
    int prevValueRef;
    bool active;
};

static struct LuaSyncTableChangeHook sSyncTableChangeHooks[MAX_SYNC_TABLE_CHANGE_HOOKS];
static int sSyncTableChangeHookCount = 0;

// Co-op DX behavior IDs currently required by built-in Wii U scripts.
#define SMLUA_BEHAVIOR_ID_ACT_SELECTOR 6
#define SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE 7
#define SMLUA_BEHAVIOR_ID_KOOPA 246
#define SMLUA_BEHAVIOR_ID_MARIO 281
#define SMLUA_BEHAVIOR_ID_METAL_CAP 290
#define SMLUA_BEHAVIOR_ID_NORMAL_CAP 308
#define SMLUA_BEHAVIOR_ID_VANISH_CAP 486
#define SMLUA_BEHAVIOR_ID_WING_CAP 529

struct LuaMarioActionHook {
    u32 action;
    int callbackRef;
    bool active;
};

struct LuaBehaviorHook {
    s32 behaviorId;
    int objList;
    bool sync;
    int initRef;
    int loopRef;
    bool active;
};

struct LuaHookedCustomBehavior {
    u32 behaviorId;
    u32 overrideId;
    u32 originalId;
    BehaviorScript *behavior;
    const BehaviorScript *originalBehavior;
    const char *bhvName;
    bool ownedBehavior;
};

static struct LuaMarioActionHook sMarioActionHooks[MAX_MARIO_ACTION_HOOKS];
static int sMarioActionHookCount = 0;
static struct LuaBehaviorHook sBehaviorHooks[MAX_BEHAVIOR_HOOKS];
static int sBehaviorHookCount = 0;
static struct LuaHookedCustomBehavior sHookedCustomBehaviors[MAX_HOOKED_CUSTOM_BEHAVIORS];
static int sHookedCustomBehaviorsCount = 0;
static int sObjectSetModelDispatchDepth = 0;

typedef void (*SmluaHookPushArgsFn)(lua_State *L, const void *ctx);
typedef void (*SmluaHookReadResultsFn)(lua_State *L, void *ctx);

// Keeps hook dispatch bound to the current Lua VM even after runtime reinit.
static lua_State *smlua_resolve_hook_state(void) {
    if (sHookState == NULL) {
        sHookState = smlua_get_state();
#ifdef TARGET_WII_U
        if (sHookState != NULL && !sHookStateReboundLogged) {
            WHBLogPrint("lua: hook state rebound from active VM");
            sHookStateReboundLogged = true;
        } else if (sHookState == NULL && !sHookStateUnavailableLogged) {
            WHBLogPrint("lua: hook state unavailable (Lua VM null)");
            sHookStateUnavailableLogged = true;
        }
#endif
    }
    return sHookState;
}

u8 smlua_get_mario_freeze_timer(const struct MarioState *m) {
    if (m == NULL) {
        return 0;
    }
    int idx = (int)(m - gMarioStates);
    if (idx < 0 || idx >= MAX_PLAYERS) {
        return 0;
    }
    return sMarioFreezeTimers[idx];
}

void smlua_set_mario_freeze_timer(const struct MarioState *m, u8 value) {
    if (m == NULL) {
        return;
    }
    int idx = (int)(m - gMarioStates);
    if (idx < 0 || idx >= MAX_PLAYERS) {
        return;
    }
    sMarioFreezeTimers[idx] = value;
}

static bool smlua_try_resolve_behavior_id(lua_State *L, int idx, s32 *outBehaviorId) {
    if (outBehaviorId == NULL) {
        return false;
    }
    *outBehaviorId = -1;

    if (L == NULL) {
        return false;
    }

    if (lua_isinteger(L, idx)) {
        *outBehaviorId = (s32)lua_tointeger(L, idx);
        return true;
    }

    if (lua_islightuserdata(L, idx)) {
        BehaviorScript *script = (BehaviorScript *)lua_touserdata(L, idx);
        if (script == NULL) {
            return false;
        }
        *outBehaviorId = (s32)get_id_from_behavior(script);
        return true;
    }

    if (lua_isstring(L, idx)) {
        const char *name = lua_tostring(L, idx);
        if (name == NULL || name[0] == '\0') {
            return false;
        }

        // Allow passing behavior names as strings (common in some CoopDX mods).
        lua_getglobal(L, name);
        if (!lua_isinteger(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        *outBehaviorId = (s32)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return true;
    }

    return false;
}

// Emits hook-runtime diagnostics to both stdout and Wii U OS console.
static void smlua_hook_logf(const char *fmt, ...) {
    va_list args;
#ifdef TARGET_WII_U
    char buffer[512];
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    WHBLogPrintf("%s", buffer);
#endif
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

const BehaviorScript *smlua_get_hooked_behavior_from_id(s32 id, bool returnOriginal) {
    if (id < 0) {
        return NULL;
    }

    for (int i = 0; i < sHookedCustomBehaviorsCount; i++) {
        struct LuaHookedCustomBehavior *hooked = &sHookedCustomBehaviors[i];
        if ((s32)hooked->behaviorId != id && (s32)hooked->overrideId != id) {
            continue;
        }
        if (returnOriginal) {
            return hooked->originalBehavior;
        }
        return hooked->behavior;
    }
    return NULL;
}

// DynOS compatibility: register custom behavior IDs and expose globals to Lua like CoopDX.
int smlua_hook_custom_bhv(BehaviorScript *bhvScript, const char *bhvName) {
    if (bhvScript == NULL || bhvName == NULL || bhvName[0] == '\0') {
        return 0;
    }
    if (sHookedCustomBehaviorsCount >= MAX_HOOKED_CUSTOM_BEHAVIORS) {
        smlua_hook_logf("lua: custom behavior registry exceeded max references");
        return 0;
    }

    for (int i = 0; i < sHookedCustomBehaviorsCount; i++) {
        struct LuaHookedCustomBehavior *existing = &sHookedCustomBehaviors[i];
        if (existing->behavior == bhvScript || (existing->bhvName != NULL && strcmp(existing->bhvName, bhvName) == 0)) {
            lua_State *state = smlua_resolve_hook_state();
            if (state != NULL) {
                lua_pushinteger(state, existing->behaviorId);
                lua_setglobal(state, bhvName);
            }
            return 1;
        }
    }

    u32 originalBehaviorId = get_id_from_behavior(bhvScript);
    if (originalBehaviorId == id_bhvMario) {
        smlua_hook_logf("lua: refusing to hook Mario behavior as custom");
        return 0;
    }

    bool newBehavior = originalBehaviorId >= id_bhv_max_count;
    struct LuaHookedCustomBehavior *hooked = &sHookedCustomBehaviors[sHookedCustomBehaviorsCount];
    u16 customBehaviorId = (u16)((sHookedCustomBehaviorsCount & 0x7FFF) | LUA_BEHAVIOR_FLAG);

    memset(hooked, 0, sizeof(*hooked));
    hooked->behavior = bhvScript;
    hooked->behavior[1] = (BehaviorScript)BC_B0H(0x39, customBehaviorId);
    hooked->behaviorId = customBehaviorId;
    hooked->overrideId = newBehavior ? customBehaviorId : originalBehaviorId;
    hooked->originalId = originalBehaviorId;
    hooked->originalBehavior = newBehavior ? bhvScript : get_behavior_from_id((enum BehaviorId)originalBehaviorId);
    hooked->bhvName = bhvName;
    hooked->ownedBehavior = false;
    sHookedCustomBehaviorsCount++;

    lua_State *state = smlua_resolve_hook_state();
    if (state != NULL) {
        lua_pushinteger(state, customBehaviorId);
        lua_setglobal(state, bhvName);
    }
    return 1;
}

// Aborts long-running Lua callbacks so one mod cannot hard-freeze frame/update loops.
static void smlua_callback_budget_hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    luaL_error(L, "callback exceeded instruction budget");
}

// Error handler that appends a Lua traceback to the error message.
static int smlua_traceback(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {
        msg = "<unknown>";
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

// Runs a Lua callback under an instruction budget guard.
static int smlua_pcall_with_budget(lua_State *L, int nargs, int nresults) {
    int status;
    // Insert traceback handler beneath the function + args.
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, smlua_traceback);
    lua_insert(L, base);

    lua_sethook(L, smlua_callback_budget_hook, LUA_MASKCOUNT, SMLUA_CALLBACK_INSTRUCTION_BUDGET);
    status = lua_pcall(L, nargs, nresults, base);
    lua_sethook(L, NULL, 0, 0);

    // Remove traceback handler regardless of success/failure.
    lua_remove(L, base);
    return status;
}

// Binds an integer constant into Lua's global table.
static void smlua_set_global_integer(lua_State *L, const char *name, int value) {
    lua_pushinteger(L, value);
    lua_setglobal(L, name);
}

// C binding for hook_event(eventType, callback).
static int smlua_hook_event(lua_State *L) {
    if (lua_gettop(L) != 2) {
        return 0;
    }
    if (!lua_isinteger(L, 1) || !lua_isfunction(L, 2)) {
        return 0;
    }

    int hook_type = (int)lua_tointeger(L, 1);
    if (hook_type < 0 || hook_type >= HOOK_MAX) {
        return 0;
    }

    struct LuaHookedEvent *hook = &sHookedEvents[hook_type];
    if (hook->count >= MAX_HOOKED_REFERENCES) {
        smlua_hook_logf("lua: hook '%d' exceeded max references", hook_type);
        return 0;
    }

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (ref == LUA_REFNIL) {
        return 0;
    }

    hook->references[hook->count++] = ref;
    return 0;
}

// Unrefs a registry reference only when it is valid.
static void smlua_unref_registry_ref(lua_State *L, int *ref) {
    if (L != NULL && ref != NULL && *ref != LUA_NOREF && *ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    }
    if (ref != NULL) {
        *ref = LUA_NOREF;
    }
}

// Registers a sync-table watcher that can fire when table[key] changes.
static int smlua_hook_on_sync_table_change(lua_State *L) {
    if (lua_gettop(L) != 4) {
        return 0;
    }
    if (!lua_istable(L, 1) || (!lua_isstring(L, 2) && !lua_isinteger(L, 2)) || !lua_isfunction(L, 4)) {
        return 0;
    }
    if (sSyncTableChangeHookCount >= MAX_SYNC_TABLE_CHANGE_HOOKS) {
        smlua_hook_logf("lua: sync-table hook exceeded max references");
        return 0;
    }

    struct LuaSyncTableChangeHook *watch = &sSyncTableChangeHooks[sSyncTableChangeHookCount];
    memset(watch, 0, sizeof(*watch));
    watch->tableRef = LUA_NOREF;
    watch->keyRef = LUA_NOREF;
    watch->tagRef = LUA_NOREF;
    watch->funcRef = LUA_NOREF;
    watch->prevValueRef = LUA_NOREF;

    lua_pushvalue(L, 1);
    watch->tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    if (watch->tableRef == LUA_REFNIL) {
        return 0;
    }

    lua_pushvalue(L, 2);
    watch->keyRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 3);
    watch->tagRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 4);
    watch->funcRef = luaL_ref(L, LUA_REGISTRYINDEX);

    if (watch->keyRef == LUA_REFNIL || watch->tagRef == LUA_REFNIL || watch->funcRef == LUA_REFNIL) {
        smlua_unref_registry_ref(L, &watch->tableRef);
        smlua_unref_registry_ref(L, &watch->keyRef);
        smlua_unref_registry_ref(L, &watch->tagRef);
        smlua_unref_registry_ref(L, &watch->funcRef);
        return 0;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, watch->tableRef);
    lua_rawgeti(L, LUA_REGISTRYINDEX, watch->keyRef);
    lua_gettable(L, -2);
    watch->prevValueRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);

    watch->active = true;
    sSyncTableChangeHookCount++;

    lua_pushinteger(L, sSyncTableChangeHookCount - 1);
    return 1;
}

// Compatibility stub for read-only mod-menu text rows.
static int smlua_hook_mod_menu_text(lua_State *L) {
    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        return 0;
    }
    return 0;
}

// Compatibility stub for mod-menu button rows.
static int smlua_hook_mod_menu_button(lua_State *L) {
    if (lua_gettop(L) != 2 || !lua_isstring(L, 1) || !lua_isfunction(L, 2)) {
        return 0;
    }
    lua_pushinteger(L, sNextModMenuHandle++);
    return 1;
}

// Compatibility stub for mod-menu checkbox rows.
static int smlua_hook_mod_menu_checkbox(lua_State *L) {
    if (lua_gettop(L) != 3 || !lua_isstring(L, 1) || !lua_isboolean(L, 2) || !lua_isfunction(L, 3)) {
        return 0;
    }
    lua_pushinteger(L, sNextModMenuHandle++);
    return 1;
}

// Compatibility stub for mod-menu slider rows.
static int smlua_hook_mod_menu_slider(lua_State *L) {
    if (lua_gettop(L) != 5 || !lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) ||
        !lua_isnumber(L, 4) || !lua_isfunction(L, 5)) {
        return 0;
    }
    lua_pushinteger(L, sNextModMenuHandle++);
    return 1;
}

// Compatibility stub for mod-menu inputbox rows.
static int smlua_hook_mod_menu_inputbox(lua_State *L) {
    if (lua_gettop(L) != 4 || !lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isinteger(L, 3) ||
        !lua_isfunction(L, 4)) {
        return 0;
    }
    lua_pushinteger(L, sNextModMenuHandle++);
    return 1;
}

// Converts known Co-op DX behavior IDs into vanilla behavior-script pointers.
static const BehaviorScript *smlua_behavior_from_id(s32 behavior_id) {
    const BehaviorScript *hooked = smlua_get_hooked_behavior_from_id(behavior_id, false);
    if (hooked != NULL) {
        return hooked;
    }

    switch (behavior_id) {
        case SMLUA_BEHAVIOR_ID_ACT_SELECTOR:
            return bhvActSelector;
        case SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE:
            return bhvActSelectorStarType;
        case SMLUA_BEHAVIOR_ID_KOOPA:
            return bhvKoopa;
        case SMLUA_BEHAVIOR_ID_MARIO:
            return bhvMario;
        case SMLUA_BEHAVIOR_ID_METAL_CAP:
            return bhvMetalCap;
        case SMLUA_BEHAVIOR_ID_NORMAL_CAP:
            return bhvNormalCap;
        case SMLUA_BEHAVIOR_ID_VANISH_CAP:
            return bhvVanishCap;
        case SMLUA_BEHAVIOR_ID_WING_CAP:
            return bhvWingCap;
        default:
            break;
    }

    if (behavior_id >= 0 && behavior_id < (s32)id_bhv_max_count) {
        return get_behavior_from_id((enum BehaviorId)behavior_id);
    }
    return NULL;
}

// Registers a callback for a specific action ID allocated by Lua.
static int smlua_hook_mario_action(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isinteger(L, 1) || !lua_isfunction(L, 2)) {
        return 0;
    }
    if (sMarioActionHookCount >= MAX_MARIO_ACTION_HOOKS) {
        smlua_hook_logf("lua: mario action hook exceeded max references");
        return 0;
    }

    struct LuaMarioActionHook *hook = &sMarioActionHooks[sMarioActionHookCount];
    memset(hook, 0, sizeof(*hook));
    hook->action = (u32)lua_tointeger(L, 1);
    lua_pushvalue(L, 2);
    hook->callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
    hook->active = hook->callbackRef != LUA_NOREF && hook->callbackRef != LUA_REFNIL;
    if (!hook->active) {
        hook->callbackRef = LUA_NOREF;
        return 0;
    }

    sMarioActionHookCount++;
    return 1;
}

// Registers Lua behavior init/loop callbacks for a known behavior ID.
static int smlua_hook_behavior(lua_State *L) {
    int paramCount = lua_gettop(L);
    bool noOverrideId = false;
    s32 behaviorId = -1;
    int objectList = 0;
    bool replaceBehavior = false;
    const char *bhvName = NULL;
    int initRef = LUA_NOREF;
    int loopRef = LUA_NOREF;

    if (paramCount < 5) {
        smlua_hook_logf("lua: hook_behavior requires at least 5 arguments");
        return 0;
    }
    if (sBehaviorHookCount >= MAX_BEHAVIOR_HOOKS) {
        smlua_hook_logf("lua: behavior hook exceeded max references");
        return 0;
    }
    noOverrideId = lua_isnil(L, 1);
    if (!lua_isnumber(L, 2)) {
        smlua_hook_logf("lua: hook_behavior invalid object list type");
        return 0;
    }
    objectList = (int)lua_tointeger(L, 2);
    replaceBehavior = lua_toboolean(L, 3);
    if (objectList <= 0 || objectList >= NUM_OBJ_LISTS) {
        smlua_hook_logf("lua: hook_behavior invalid object list %d", objectList);
        return 0;
    }
    if (paramCount >= 6 && lua_isstring(L, 6)) {
        bhvName = lua_tostring(L, 6);
    }

    if (noOverrideId) {
        static char sGeneratedName[64];
        struct LuaHookedCustomBehavior *hooked = NULL;
        u16 customBehaviorId = 0;
        BehaviorScript *script = NULL;

        if (sHookedCustomBehaviorsCount >= MAX_HOOKED_CUSTOM_BEHAVIORS) {
            return 0;
        }

        customBehaviorId = (u16)((sHookedCustomBehaviorsCount & 0x7FFF) | LUA_BEHAVIOR_FLAG);
        script = calloc(4, sizeof(BehaviorScript));
        if (script == NULL) {
            return 0;
        }

        script[0] = (BehaviorScript)BC_BB(0x00, objectList);
        script[1] = (BehaviorScript)BC_B0H(0x39, customBehaviorId);
        script[2] = (BehaviorScript)BC_B(0x0A);
        script[3] = (BehaviorScript)BC_B(0x0A);

        if (bhvName == NULL || bhvName[0] == '\0') {
            snprintf(sGeneratedName, sizeof(sGeneratedName), "bhvCustom%03u",
                     (unsigned)(sHookedCustomBehaviorsCount + 1));
            bhvName = sGeneratedName;
        }

        hooked = &sHookedCustomBehaviors[sHookedCustomBehaviorsCount];
        memset(hooked, 0, sizeof(*hooked));
        hooked->behaviorId = customBehaviorId;
        hooked->overrideId = customBehaviorId;
        hooked->originalId = customBehaviorId;
        hooked->behavior = script;
        hooked->originalBehavior = script;
        hooked->bhvName = bhvName;
        hooked->ownedBehavior = true;
        sHookedCustomBehaviorsCount++;

        if (bhvName != NULL && bhvName[0] != '\0') {
            lua_pushinteger(L, customBehaviorId);
            lua_setglobal(L, bhvName);
        }
        behaviorId = customBehaviorId;
    } else {
        if (!smlua_try_resolve_behavior_id(L, 1, &behaviorId)) {
            smlua_hook_logf("lua: hook_behavior could not resolve override behavior id");
            return 0;
        }
        if (smlua_behavior_from_id(behaviorId) == NULL) {
            replaceBehavior = true;
        }
    }

    if (!lua_isnil(L, 4) && !lua_isfunction(L, 4)) {
        smlua_hook_logf("lua: hook_behavior init callback must be function or nil");
        return 0;
    }
    if (!lua_isnil(L, 5) && !lua_isfunction(L, 5)) {
        smlua_hook_logf("lua: hook_behavior loop callback must be function or nil");
        return 0;
    }

    if (lua_isfunction(L, 4)) {
        lua_pushvalue(L, 4);
        initRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        loopRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    struct LuaBehaviorHook *hook = &sBehaviorHooks[sBehaviorHookCount];
    memset(hook, 0, sizeof(*hook));
    hook->behaviorId = behaviorId;
    hook->objList = objectList;
    hook->sync = replaceBehavior;
    hook->initRef = initRef;
    hook->loopRef = loopRef;
    hook->active = true;

    sBehaviorHookCount++;
    // Match CoopDX: return the resolved behavior ID so mods can assign it.
    lua_pushinteger(L, (lua_Integer)behaviorId);
    return 1;
}

// Shared dispatcher for hook callbacks with optional args and return values.
static bool smlua_dispatch_hook_callbacks(enum LuaHookedEventType hook_type, int arg_count, int result_count,
                                          SmluaHookPushArgsFn push_args, const void *arg_ctx,
                                          SmluaHookReadResultsFn read_results, void *result_ctx) {
    lua_State *L = smlua_resolve_hook_state();
    if (L == NULL || hook_type < 0 || hook_type >= HOOK_MAX) {
        return false;
    }

    struct LuaHookedEvent *hook = &sHookedEvents[hook_type];
    bool called = false;

    for (int i = 0; i < hook->count; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, hook->references[i]);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        if (push_args != NULL) {
            push_args(L, arg_ctx);
        }

        if (smlua_pcall_with_budget(L, arg_count, result_count) != LUA_OK) {
            const char *error = lua_tostring(L, -1);
            smlua_hook_logf("lua: hook %d failed: %s", hook_type, error != NULL ? error : "<unknown>");
            lua_pop(L, 1);
            continue;
        }

        if (read_results != NULL) {
            read_results(L, result_ctx);
        }

        if (result_count > 0) {
            lua_pop(L, result_count);
        }

        called = true;
    }

#ifdef TARGET_WII_U
    if (called && !sHookDispatchLogged[hook_type]) {
        WHBLogPrintf("lua: hook dispatch type=%d callbacks=%d", hook_type, hook->count);
        sHookDispatchLogged[hook_type] = true;
    }
#endif

    return called;
}

struct LuaWarpHookArgs {
    int warpType;
    int levelNum;
    int areaIdx;
    int nodeId;
    unsigned int warpArg;
};

struct LuaInteractHookArgs {
    const void *marioState;
    const void *object;
    unsigned int interactType;
    bool interactValue;
};

struct LuaDialogHookArgs {
    int dialogId;
};

struct LuaAllowInteractHookResults {
    bool *allowInteract;
};

struct LuaDialogHookResults {
    bool *openDialogBox;
    const char **dialogTextOverride;
};

struct LuaSetMarioActionHookArgs {
    const void *marioState;
    int nextAction;
    int actionArg;
};

struct LuaObjectSetModelHookArgs {
    const void *object;
    int modelId;
};

// Pushes a single MarioState pointer argument for Mario-scoped hooks.
static void smlua_push_mario_hook_args(lua_State *L, const void *ctx) {
    smlua_push_mario_state(L, ctx);
}

// Pushes warp metadata in the same positional order used by Co-op DX hooks.
static void smlua_push_warp_hook_args(lua_State *L, const void *ctx) {
    const struct LuaWarpHookArgs *args = (const struct LuaWarpHookArgs *)ctx;
    lua_pushinteger(L, args->warpType);
    lua_pushinteger(L, args->levelNum);
    lua_pushinteger(L, args->areaIdx);
    lua_pushinteger(L, args->nodeId);
    lua_pushinteger(L, args->warpArg);
}

// Pushes interaction metadata for HOOK_ON_INTERACT callbacks.
static void smlua_push_interact_hook_args(lua_State *L, const void *ctx) {
    const struct LuaInteractHookArgs *args = (const struct LuaInteractHookArgs *)ctx;
    smlua_push_mario_state(L, args->marioState);
    smlua_push_object(L, args->object);
    lua_pushinteger(L, args->interactType);
    lua_pushboolean(L, args->interactValue ? 1 : 0);
}

// Pushes interaction metadata with current allow state for HOOK_ALLOW_INTERACT.
static void smlua_push_allow_interact_hook_args(lua_State *L, const void *ctx) {
    const struct LuaInteractHookArgs *args = (const struct LuaInteractHookArgs *)ctx;
    smlua_push_mario_state(L, args->marioState);
    smlua_push_object(L, args->object);
    lua_pushinteger(L, args->interactType);
    lua_pushboolean(L, args->interactValue ? 1 : 0);
}

// Reads optional allow-interact override from callback return value.
static void smlua_read_allow_interact_hook_results(lua_State *L, void *ctx) {
    struct LuaAllowInteractHookResults *results = (struct LuaAllowInteractHookResults *)ctx;
    if (results->allowInteract != NULL && lua_isboolean(L, -1)) {
        *results->allowInteract = lua_toboolean(L, -1) != 0;
    }
}

// Pushes the dialog identifier so Lua can decide if/how to override.
static void smlua_push_dialog_hook_args(lua_State *L, const void *ctx) {
    const struct LuaDialogHookArgs *args = (const struct LuaDialogHookArgs *)ctx;
    lua_pushinteger(L, args->dialogId);
}

// Pushes set_mario_action transition arguments in Co-op DX-compatible order.
static void smlua_push_before_set_mario_action_hook_args(lua_State *L, const void *ctx) {
    const struct LuaSetMarioActionHookArgs *args = (const struct LuaSetMarioActionHookArgs *)ctx;
    smlua_push_mario_state(L, args->marioState);
    lua_pushinteger(L, args->nextAction);
    lua_pushinteger(L, args->actionArg);
}

// Pushes object/model pair for object-model replacement hooks.
static void smlua_push_object_set_model_hook_args(lua_State *L, const void *ctx) {
    const struct LuaObjectSetModelHookArgs *args = (const struct LuaObjectSetModelHookArgs *)ctx;
    smlua_push_object(L, args->object);
    lua_pushinteger(L, args->modelId);
}

// Reads optional dialog overrides from callback returns: bool, string.
static void smlua_read_dialog_hook_results(lua_State *L, void *ctx) {
    struct LuaDialogHookResults *results = (struct LuaDialogHookResults *)ctx;

    if (results->openDialogBox != NULL && lua_isboolean(L, -2)) {
        *results->openDialogBox = lua_toboolean(L, -2) != 0;
    }
    if (results->dialogTextOverride != NULL && lua_isstring(L, -1)) {
        *results->dialogTextOverride = lua_tostring(L, -1);
    }
}

// Dispatches callbacks for a zero-argument hook event.
bool smlua_call_event_hooks(enum LuaHookedEventType hook_type, ...) {
    return smlua_dispatch_hook_callbacks(hook_type, 0, 0, NULL, NULL, NULL, NULL);
}

// Dispatches level/warp lifecycle hooks with destination metadata.
bool smlua_call_event_hooks_warp(enum LuaHookedEventType hook_type, int warp_type, int level_num,
                                 int area_idx, int node_id, unsigned int warp_arg) {
    struct LuaWarpHookArgs args = { warp_type, level_num, area_idx, node_id, warp_arg };
    return smlua_dispatch_hook_callbacks(hook_type, 5, 0, smlua_push_warp_hook_args, &args, NULL, NULL);
}

// Dispatches object interaction hooks with Mario/object context pointers.
bool smlua_call_event_hooks_interact(const void *mario_state, const void *object,
                                     unsigned int interact_type, bool interact_value) {
    struct LuaInteractHookArgs args = { mario_state, object, interact_type, interact_value };
    return smlua_dispatch_hook_callbacks(HOOK_ON_INTERACT, 4, 0, smlua_push_interact_hook_args, &args,
                                         NULL, NULL);
}

// Dispatches HOOK_ALLOW_INTERACT callbacks and allows Lua to veto interaction.
bool smlua_call_event_hooks_allow_interact(const void *mario_state, const void *object,
                                           unsigned int interact_type, bool *allow_interact) {
    bool allowValue = (allow_interact == NULL) ? true : *allow_interact;
    struct LuaInteractHookArgs args = { mario_state, object, interact_type, allowValue };
    struct LuaAllowInteractHookResults results = { allow_interact };
    return smlua_dispatch_hook_callbacks(HOOK_ALLOW_INTERACT, 4, 1,
                                         smlua_push_allow_interact_hook_args, &args,
                                         smlua_read_allow_interact_hook_results, &results);
}

// Dispatches hooks that receive only a MarioState pointer payload.
bool smlua_call_event_hooks_mario(enum LuaHookedEventType hook_type, const void *mario_state) {
    return smlua_dispatch_hook_callbacks(hook_type, 1, 0, smlua_push_mario_hook_args, mario_state,
                                         NULL, NULL);
}

// Dispatches pre-physics hooks with step metadata and supports optional result override.
bool smlua_call_event_hooks_before_phys_step(const void *mario_state, int step_type,
                                             unsigned int step_arg, int *step_result_override) {
    lua_State *L = smlua_resolve_hook_state();
#ifdef TARGET_WII_U
    if (step_type >= 0 && step_type < (int)(sizeof(sBeforePhysStepTypeLogged) / sizeof(sBeforePhysStepTypeLogged[0])) &&
        !sBeforePhysStepTypeLogged[step_type]) {
        WHBLogPrintf("lua: before_phys entry step_type=%d hook_state=%p mario=%p",
                     step_type, (void *)L, mario_state);
    }
#endif
    if (L == NULL || mario_state == NULL) {
#ifdef TARGET_WII_U
        if (sBeforePhysEarlyReturnLogs < 8) {
            WHBLogPrintf("lua: before_phys early return hook_state=%p mario=%p",
                         (void *)L, mario_state);
            sBeforePhysEarlyReturnLogs++;
        }
#endif
        return false;
    }

    struct LuaHookedEvent *hook = &sHookedEvents[HOOK_BEFORE_PHYS_STEP];
    struct MarioState *mario = (struct MarioState *)mario_state;
    const bool log_water_step = (step_type == STEP_TYPE_WATER);

#ifdef TARGET_WII_U
    if (step_type >= 0 && step_type < (int)(sizeof(sBeforePhysStepTypeLogged) / sizeof(sBeforePhysStepTypeLogged[0])) &&
        !sBeforePhysStepTypeLogged[step_type]) {
        WHBLogPrintf("lua: before_phys step_type=%d hooks=%d", step_type, hook->count);
        sBeforePhysStepTypeLogged[step_type] = true;
    }
    if (log_water_step && !sBeforePhysWaterHookCountLogged) {
        WHBLogPrintf("lua: before_phys water hooks=%d", hook->count);
        sBeforePhysWaterHookCountLogged = true;
    }
#endif

    for (int i = 0; i < hook->count; i++) {
        f32 vel_before[3];
        lua_rawgeti(L, LUA_REGISTRYINDEX, hook->references[i]);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        if (log_water_step) {
            vel_before[0] = mario->vel[0];
            vel_before[1] = mario->vel[1];
            vel_before[2] = mario->vel[2];
        }

        smlua_push_mario_state(L, mario_state);
        lua_pushinteger(L, step_type);
        lua_pushinteger(L, (lua_Integer)step_arg);

        if (smlua_pcall_with_budget(L, 3, 1) != LUA_OK) {
            const char *error = lua_tostring(L, -1);
            smlua_hook_logf("lua: hook %d failed: %s", HOOK_BEFORE_PHYS_STEP,
                            error != NULL ? error : "<unknown>");
            lua_pop(L, 1);
            continue;
        }

        if (log_water_step && sBeforePhysWaterVelChangeLogs < 16) {
            const f32 dx = mario->vel[0] - vel_before[0];
            const f32 dy = mario->vel[1] - vel_before[1];
            const f32 dz = mario->vel[2] - vel_before[2];
            if (fabsf(dx) > 0.0001f || fabsf(dy) > 0.0001f || fabsf(dz) > 0.0001f) {
#ifdef TARGET_WII_U
                WHBLogPrintf("lua: before_phys water hook[%d] dvel=(%.3f, %.3f, %.3f)",
                             i, (double)dx, (double)dy, (double)dz);
#endif
                sBeforePhysWaterVelChangeLogs++;
            }
        }

        if (lua_isnumber(L, -1)) {
            if (step_result_override != NULL) {
                *step_result_override = (int)lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
            return true;
        }

        lua_pop(L, 1);
    }

    return false;
}

// Dispatches dialog hooks and lets Lua veto open + override text.
bool smlua_call_event_hooks_dialog(int dialog_id, bool *open_dialog_box,
                                   const char **dialog_text_override) {
    struct LuaDialogHookArgs args = { dialog_id };
    struct LuaDialogHookResults results = { open_dialog_box, dialog_text_override };
    return smlua_dispatch_hook_callbacks(HOOK_ON_DIALOG, 1, 2, smlua_push_dialog_hook_args, &args,
                                         smlua_read_dialog_hook_results, &results);
}

// Dispatches hooks for set_mario_action pre-transition interception.
void smlua_call_event_hooks_before_set_mario_action(const void *mario_state, int next_action,
                                                    int action_arg) {
    struct LuaSetMarioActionHookArgs args = { mario_state, next_action, action_arg };
    (void)smlua_dispatch_hook_callbacks(HOOK_BEFORE_SET_MARIO_ACTION, 3, 0,
                                        smlua_push_before_set_mario_action_hook_args, &args,
                                        NULL, NULL);
}

// Dispatches hooks after Mario action state has been committed.
void smlua_call_event_hooks_on_set_mario_action(const void *mario_state) {
    (void)smlua_dispatch_hook_callbacks(HOOK_ON_SET_MARIO_ACTION, 1, 0, smlua_push_mario_hook_args,
                                        mario_state, NULL, NULL);
}

// Dispatches object-model replacement hooks whenever object model IDs change.
void smlua_call_event_hooks_object_set_model(const void *object, int model_id) {
    struct LuaObjectSetModelHookArgs args = { object, model_id };
    // Guard against recursive model-change loops triggered by mods that
    // mutate models from within HOOK_OBJECT_SET_MODEL callbacks.
    if (sObjectSetModelDispatchDepth > 8) {
        return;
    }
    sObjectSetModelDispatchDepth++;
    (void)smlua_dispatch_hook_callbacks(HOOK_OBJECT_SET_MODEL, 2, 0,
                                        smlua_push_object_set_model_hook_args, &args, NULL, NULL);
    sObjectSetModelDispatchDepth--;
}

// Executes hook_mario_action callbacks for the current action and returns override in-loop state.
bool smlua_call_mario_action_hook(const void *mario_state, int *in_loop) {
    lua_State *L = smlua_resolve_hook_state();
    struct MarioState *m = (struct MarioState *)mario_state;
    bool called = false;

    if (L == NULL || m == NULL) {
        return false;
    }

    for (int i = 0; i < sMarioActionHookCount; i++) {
        struct LuaMarioActionHook *hook = &sMarioActionHooks[i];
        if (!hook->active || hook->action != m->action) {
            continue;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, hook->callbackRef);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        smlua_push_mario_state(L, m);
        if (smlua_pcall_with_budget(L, 1, 1) != LUA_OK) {
            const char *error = lua_tostring(L, -1);
            smlua_hook_logf("lua: mario action hook failed: %s", error != NULL ? error : "<unknown>");
            lua_pop(L, 1);
            continue;
        }

        if (in_loop != NULL) {
            if (lua_isnumber(L, -1)) {
                *in_loop = (int)lua_tointeger(L, -1);
            } else if (lua_isboolean(L, -1)) {
                *in_loop = lua_toboolean(L, -1) ? 1 : 0;
            }
        }

        lua_pop(L, 1);
        called = true;
    }

    return called;
}

// Executes hook_behavior callbacks by iterating active objects and matching behavior pointers.
void smlua_call_behavior_hooks(void) {
    lua_State *L = smlua_resolve_hook_state();
    if (L == NULL || gObjectLists == NULL) {
        return;
    }

    for (int i = 0; i < sBehaviorHookCount; i++) {
        struct LuaBehaviorHook *hook = &sBehaviorHooks[i];
        if (!hook->active) {
            continue;
        }

        const BehaviorScript *targetBehavior = smlua_behavior_from_id(hook->behaviorId);
        if (targetBehavior == NULL) {
            continue;
        }

        u32 firstList = 0;
        u32 lastList = NUM_OBJ_LISTS;
        if (hook->objList >= 0 && hook->objList < (int)NUM_OBJ_LISTS) {
            firstList = (u32)hook->objList;
            lastList = firstList + 1;
        }

        for (u32 objList = firstList; objList < lastList; objList++) {
            struct Object *head = (struct Object *)&gObjectLists[objList];
            struct Object *obj = (struct Object *)head->header.next;
            u32 sanityDepth = 0;
            while (obj != head) {
                struct Object *next = (struct Object *)obj->header.next;
                if (++sanityDepth > 20000) {
                    break;
                }

                if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED && obj->behavior == targetBehavior) {
                    if (hook->initRef != LUA_NOREF && obj->oTimer == 0) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, hook->initRef);
                        if (lua_isfunction(L, -1)) {
                            smlua_push_object(L, obj);
                            if (smlua_pcall_with_budget(L, 1, 0) != LUA_OK) {
                                const char *error = lua_tostring(L, -1);
                                smlua_hook_logf("lua: behavior init hook failed: %s",
                                                error != NULL ? error : "<unknown>");
                                lua_pop(L, 1);
                            }
                        } else {
                            lua_pop(L, 1);
                        }
                    }

                    if (hook->loopRef != LUA_NOREF) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, hook->loopRef);
                        if (lua_isfunction(L, -1)) {
                            smlua_push_object(L, obj);
                            if (smlua_pcall_with_budget(L, 1, 0) != LUA_OK) {
                                const char *error = lua_tostring(L, -1);
                                smlua_hook_logf("lua: behavior loop hook failed: %s",
                                                error != NULL ? error : "<unknown>");
                                lua_pop(L, 1);
                            }
                        } else {
                            lua_pop(L, 1);
                        }
                    }
                }

                obj = next;
            }
        }
    }
}

// Polls watched sync-table keys and dispatches callbacks when values change.
void smlua_poll_sync_table_change_hooks(void) {
    lua_State *L = smlua_resolve_hook_state();
    if (L == NULL) {
        return;
    }

    for (int i = 0; i < sSyncTableChangeHookCount; i++) {
        struct LuaSyncTableChangeHook *watch = &sSyncTableChangeHooks[i];
        if (!watch->active) {
            continue;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, watch->tableRef);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, watch->keyRef);
        lua_gettable(L, -2);
        int currentValueIndex = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, watch->prevValueRef);
        int previousValueIndex = lua_gettop(L);

        bool changed = !lua_compare(L, currentValueIndex, previousValueIndex, LUA_OPEQ);
        if (changed) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, watch->funcRef);
            if (lua_isfunction(L, -1)) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, watch->tagRef);
                lua_pushvalue(L, previousValueIndex);
                lua_pushvalue(L, currentValueIndex);

                if (smlua_pcall_with_budget(L, 3, 0) != LUA_OK) {
                    const char *error = lua_tostring(L, -1);
                    smlua_hook_logf("lua: sync-table hook failed: %s", error != NULL ? error : "<unknown>");
                    lua_pop(L, 1);
                }
            } else {
                lua_pop(L, 1);
            }

            smlua_unref_registry_ref(L, &watch->prevValueRef);
            lua_pushvalue(L, currentValueIndex);
            watch->prevValueRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        lua_pop(L, 3);
    }
}

// Exposes current callback count for one hook event type.
int smlua_get_event_hook_count(enum LuaHookedEventType hook_type) {
    if (hook_type < 0 || hook_type >= HOOK_MAX) {
        return 0;
    }
    return sHookedEvents[hook_type].count;
}

// Unrefs all callback functions for all hook categories.
void smlua_clear_hooks(lua_State *L) {
    if (L != NULL) {
        for (int i = 0; i < HOOK_MAX; i++) {
            struct LuaHookedEvent *hook = &sHookedEvents[i];
            for (int j = 0; j < hook->count; j++) {
                luaL_unref(L, LUA_REGISTRYINDEX, hook->references[j]);
            }
        }
        for (int i = 0; i < sSyncTableChangeHookCount; i++) {
            struct LuaSyncTableChangeHook *watch = &sSyncTableChangeHooks[i];
            smlua_unref_registry_ref(L, &watch->tableRef);
            smlua_unref_registry_ref(L, &watch->keyRef);
            smlua_unref_registry_ref(L, &watch->tagRef);
            smlua_unref_registry_ref(L, &watch->funcRef);
            smlua_unref_registry_ref(L, &watch->prevValueRef);
            watch->active = false;
        }
        for (int i = 0; i < sMarioActionHookCount; i++) {
            struct LuaMarioActionHook *hook = &sMarioActionHooks[i];
            if (hook->callbackRef != LUA_NOREF && hook->callbackRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, hook->callbackRef);
            }
            hook->callbackRef = LUA_NOREF;
            hook->active = false;
        }
        for (int i = 0; i < sBehaviorHookCount; i++) {
            struct LuaBehaviorHook *hook = &sBehaviorHooks[i];
            if (hook->initRef != LUA_NOREF && hook->initRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, hook->initRef);
            }
            if (hook->loopRef != LUA_NOREF && hook->loopRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, hook->loopRef);
            }
            hook->initRef = LUA_NOREF;
            hook->loopRef = LUA_NOREF;
            hook->active = false;
        }
    }
    for (int i = 0; i < sHookedCustomBehaviorsCount; i++) {
        struct LuaHookedCustomBehavior *hooked = &sHookedCustomBehaviors[i];
        if (hooked->ownedBehavior && hooked->behavior != NULL) {
            free(hooked->behavior);
            hooked->behavior = NULL;
        }
    }
    memset(sHookedEvents, 0, sizeof(sHookedEvents));
    memset(sSyncTableChangeHooks, 0, sizeof(sSyncTableChangeHooks));
    memset(sMarioActionHooks, 0, sizeof(sMarioActionHooks));
    memset(sBehaviorHooks, 0, sizeof(sBehaviorHooks));
    memset(sHookedCustomBehaviors, 0, sizeof(sHookedCustomBehaviors));
    sSyncTableChangeHookCount = 0;
    sMarioActionHookCount = 0;
    sBehaviorHookCount = 0;
    sHookedCustomBehaviorsCount = 0;
    sBeforePhysWaterHookCountLogged = false;
    sBeforePhysWaterVelChangeLogs = 0;
    sBeforePhysEarlyReturnLogs = 0;
    sHookStateReboundLogged = false;
    sHookStateUnavailableLogged = false;
    memset(sHookDispatchLogged, 0, sizeof(sHookDispatchLogged));
    memset(sBeforePhysStepTypeLogged, 0, sizeof(sBeforePhysStepTypeLogged));
    sHookState = NULL;
}

// Registers hook API and event constants in Lua.
void smlua_bind_hooks(lua_State *L) {
    smlua_clear_hooks(L);
    sHookState = L;

    lua_pushcfunction(L, smlua_hook_event);
    lua_setglobal(L, "hook_event");
    lua_pushcfunction(L, smlua_hook_on_sync_table_change);
    lua_setglobal(L, "hook_on_sync_table_change");
    lua_pushcfunction(L, smlua_hook_mod_menu_text);
    lua_setglobal(L, "hook_mod_menu_text");
    lua_pushcfunction(L, smlua_hook_mod_menu_button);
    lua_setglobal(L, "hook_mod_menu_button");
    lua_pushcfunction(L, smlua_hook_mod_menu_checkbox);
    lua_setglobal(L, "hook_mod_menu_checkbox");
    lua_pushcfunction(L, smlua_hook_mod_menu_slider);
    lua_setglobal(L, "hook_mod_menu_slider");
    lua_pushcfunction(L, smlua_hook_mod_menu_inputbox);
    lua_setglobal(L, "hook_mod_menu_inputbox");
    lua_pushcfunction(L, smlua_hook_mario_action);
    lua_setglobal(L, "hook_mario_action");
    lua_pushcfunction(L, smlua_hook_behavior);
    lua_setglobal(L, "hook_behavior");

    smlua_set_global_integer(L, "HOOK_UPDATE", HOOK_UPDATE);
    smlua_set_global_integer(L, "HOOK_MARIO_UPDATE", HOOK_MARIO_UPDATE);
    smlua_set_global_integer(L, "HOOK_BEFORE_MARIO_UPDATE", HOOK_BEFORE_MARIO_UPDATE);
    smlua_set_global_integer(L, "HOOK_ON_SET_MARIO_ACTION", HOOK_ON_SET_MARIO_ACTION);
    smlua_set_global_integer(L, "HOOK_BEFORE_PHYS_STEP", HOOK_BEFORE_PHYS_STEP);
    smlua_set_global_integer(L, "HOOK_ALLOW_PVP_ATTACK", HOOK_ALLOW_PVP_ATTACK);
    smlua_set_global_integer(L, "HOOK_ON_PVP_ATTACK", HOOK_ON_PVP_ATTACK);
    smlua_set_global_integer(L, "HOOK_ON_PLAYER_CONNECTED", HOOK_ON_PLAYER_CONNECTED);
    smlua_set_global_integer(L, "HOOK_ON_PLAYER_DISCONNECTED", HOOK_ON_PLAYER_DISCONNECTED);
    smlua_set_global_integer(L, "HOOK_ON_HUD_RENDER", HOOK_ON_HUD_RENDER);
    smlua_set_global_integer(L, "HOOK_ALLOW_INTERACT", HOOK_ALLOW_INTERACT);
    smlua_set_global_integer(L, "HOOK_ON_INTERACT", HOOK_ON_INTERACT);
    smlua_set_global_integer(L, "HOOK_ON_LEVEL_INIT", HOOK_ON_LEVEL_INIT);
    smlua_set_global_integer(L, "HOOK_ON_WARP", HOOK_ON_WARP);
    smlua_set_global_integer(L, "HOOK_ON_SYNC_VALID", HOOK_ON_SYNC_VALID);
    smlua_set_global_integer(L, "HOOK_ON_OBJECT_UNLOAD", HOOK_ON_OBJECT_UNLOAD);
    smlua_set_global_integer(L, "HOOK_ON_SYNC_OBJECT_UNLOAD", HOOK_ON_SYNC_OBJECT_UNLOAD);
    smlua_set_global_integer(L, "HOOK_ON_PAUSE_EXIT", HOOK_ON_PAUSE_EXIT);
    smlua_set_global_integer(L, "HOOK_GET_STAR_COLLECTION_DIALOG", HOOK_GET_STAR_COLLECTION_DIALOG);
    smlua_set_global_integer(L, "HOOK_ON_SET_CAMERA_MODE", HOOK_ON_SET_CAMERA_MODE);
    smlua_set_global_integer(L, "HOOK_ON_OBJECT_RENDER", HOOK_ON_OBJECT_RENDER);
    smlua_set_global_integer(L, "HOOK_ON_DEATH", HOOK_ON_DEATH);
    smlua_set_global_integer(L, "HOOK_ON_PACKET_RECEIVE", HOOK_ON_PACKET_RECEIVE);
    smlua_set_global_integer(L, "HOOK_USE_ACT_SELECT", HOOK_USE_ACT_SELECT);
    smlua_set_global_integer(L, "HOOK_ON_CHANGE_CAMERA_ANGLE", HOOK_ON_CHANGE_CAMERA_ANGLE);
    smlua_set_global_integer(L, "HOOK_ON_SCREEN_TRANSITION", HOOK_ON_SCREEN_TRANSITION);
    smlua_set_global_integer(L, "HOOK_ALLOW_HAZARD_SURFACE", HOOK_ALLOW_HAZARD_SURFACE);
    smlua_set_global_integer(L, "HOOK_ON_CHAT_MESSAGE", HOOK_ON_CHAT_MESSAGE);
    smlua_set_global_integer(L, "HOOK_OBJECT_SET_MODEL", HOOK_OBJECT_SET_MODEL);
    smlua_set_global_integer(L, "HOOK_CHARACTER_SOUND", HOOK_CHARACTER_SOUND);
    smlua_set_global_integer(L, "HOOK_BEFORE_SET_MARIO_ACTION", HOOK_BEFORE_SET_MARIO_ACTION);
    smlua_set_global_integer(L, "HOOK_JOINED_GAME", HOOK_JOINED_GAME);
    smlua_set_global_integer(L, "HOOK_ON_OBJECT_ANIM_UPDATE", HOOK_ON_OBJECT_ANIM_UPDATE);
    smlua_set_global_integer(L, "HOOK_ON_DIALOG", HOOK_ON_DIALOG);
    smlua_set_global_integer(L, "HOOK_ON_EXIT", HOOK_ON_EXIT);
    smlua_set_global_integer(L, "HOOK_DIALOG_SOUND", HOOK_DIALOG_SOUND);
    smlua_set_global_integer(L, "HOOK_ON_HUD_RENDER_BEHIND", HOOK_ON_HUD_RENDER_BEHIND);
    smlua_set_global_integer(L, "HOOK_ON_COLLIDE_LEVEL_BOUNDS", HOOK_ON_COLLIDE_LEVEL_BOUNDS);
    smlua_set_global_integer(L, "HOOK_MIRROR_MARIO_RENDER", HOOK_MIRROR_MARIO_RENDER);
    smlua_set_global_integer(L, "HOOK_MARIO_OVERRIDE_PHYS_STEP_DEFACTO_SPEED", HOOK_MARIO_OVERRIDE_PHYS_STEP_DEFACTO_SPEED);
    smlua_set_global_integer(L, "HOOK_ON_OBJECT_LOAD", HOOK_ON_OBJECT_LOAD);
    smlua_set_global_integer(L, "HOOK_ON_PLAY_SOUND", HOOK_ON_PLAY_SOUND);
    smlua_set_global_integer(L, "HOOK_ON_SEQ_LOAD", HOOK_ON_SEQ_LOAD);
    smlua_set_global_integer(L, "HOOK_ON_ATTACK_OBJECT", HOOK_ON_ATTACK_OBJECT);
    smlua_set_global_integer(L, "HOOK_ON_LANGUAGE_CHANGED", HOOK_ON_LANGUAGE_CHANGED);
    smlua_set_global_integer(L, "HOOK_ON_MODS_LOADED", HOOK_ON_MODS_LOADED);
    smlua_set_global_integer(L, "HOOK_ON_NAMETAGS_RENDER", HOOK_ON_NAMETAGS_RENDER);
    smlua_set_global_integer(L, "HOOK_ON_DJUI_THEME_CHANGED", HOOK_ON_DJUI_THEME_CHANGED);
    smlua_set_global_integer(L, "HOOK_ON_GEO_PROCESS", HOOK_ON_GEO_PROCESS);
    smlua_set_global_integer(L, "HOOK_BEFORE_GEO_PROCESS", HOOK_BEFORE_GEO_PROCESS);
    smlua_set_global_integer(L, "HOOK_ON_GEO_PROCESS_CHILDREN", HOOK_ON_GEO_PROCESS_CHILDREN);
    smlua_set_global_integer(L, "HOOK_MARIO_OVERRIDE_GEOMETRY_INPUTS", HOOK_MARIO_OVERRIDE_GEOMETRY_INPUTS);
    smlua_set_global_integer(L, "HOOK_ON_INTERACTIONS", HOOK_ON_INTERACTIONS);
    smlua_set_global_integer(L, "HOOK_ALLOW_FORCE_WATER_ACTION", HOOK_ALLOW_FORCE_WATER_ACTION);
    smlua_set_global_integer(L, "HOOK_BEFORE_WARP", HOOK_BEFORE_WARP);
    smlua_set_global_integer(L, "HOOK_ON_INSTANT_WARP", HOOK_ON_INSTANT_WARP);
    smlua_set_global_integer(L, "HOOK_MARIO_OVERRIDE_FLOOR_CLASS", HOOK_MARIO_OVERRIDE_FLOOR_CLASS);
    smlua_set_global_integer(L, "HOOK_ON_ADD_SURFACE", HOOK_ON_ADD_SURFACE);
    smlua_set_global_integer(L, "HOOK_ON_CLEAR_AREAS", HOOK_ON_CLEAR_AREAS);
    smlua_set_global_integer(L, "HOOK_ON_PACKET_BYTESTRING_RECEIVE", HOOK_ON_PACKET_BYTESTRING_RECEIVE);
}

u32 gLuaMarioActionIndex[8] = { 0 };
struct LuaHookedModMenuElement gHookedModMenuElements[MAX_HOOKED_MOD_MENU_ELEMENTS] = { 0 };
int gHookedModMenuElementsCount = 0;

bool smlua_call_chat_command_hook(char *command) {
    (void)command;
    return false;
}

void smlua_display_chat_commands(void) {
}

static char **smlua_alloc_empty_string_array(void) {
    char **list = calloc(1, sizeof(char *));
    return list;
}

char **smlua_get_chat_player_list(void) {
    return smlua_alloc_empty_string_array();
}

char **smlua_get_chat_maincommands_list(void) {
    return smlua_alloc_empty_string_array();
}

char **smlua_get_chat_subcommands_list(const char *maincommand) {
    (void)maincommand;
    return smlua_alloc_empty_string_array();
}

bool smlua_maincommand_exists(const char *maincommand) {
    (void)maincommand;
    return false;
}

bool smlua_subcommand_exists(const char *maincommand, const char *subcommand) {
    (void)maincommand;
    (void)subcommand;
    return false;
}

void smlua_call_mod_menu_element_hook(struct LuaHookedModMenuElement *hooked, int index) {
    (void)hooked;
    (void)index;
}
