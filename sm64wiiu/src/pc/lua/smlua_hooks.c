#include <stdio.h>
#include <string.h>

#include "sm64.h"
#include "behavior_data.h"
#include "game/mario.h"
#include "game/object_list_processor.h"

#include "smlua_hooks.h"
#include "smlua_cobject.h"
#include <lauxlib.h>

#define MAX_HOOKED_REFERENCES 64
#define MAX_SYNC_TABLE_CHANGE_HOOKS 64
#define MAX_MARIO_ACTION_HOOKS 128
#define MAX_BEHAVIOR_HOOKS 64
#define SMLUA_CALLBACK_INSTRUCTION_BUDGET 5000000

struct LuaHookedEvent {
    int references[MAX_HOOKED_REFERENCES];
    int count;
};

static lua_State *sHookState = NULL;
static struct LuaHookedEvent sHookedEvents[HOOK_MAX];
static int sNextModMenuHandle = 1;

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

static struct LuaMarioActionHook sMarioActionHooks[MAX_MARIO_ACTION_HOOKS];
static int sMarioActionHookCount = 0;
static struct LuaBehaviorHook sBehaviorHooks[MAX_BEHAVIOR_HOOKS];
static int sBehaviorHookCount = 0;
static int sObjectSetModelDispatchDepth = 0;

typedef void (*SmluaHookPushArgsFn)(lua_State *L, const void *ctx);
typedef void (*SmluaHookReadResultsFn)(lua_State *L, void *ctx);

// Aborts long-running Lua callbacks so one mod cannot hard-freeze frame/update loops.
static void smlua_callback_budget_hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    luaL_error(L, "callback exceeded instruction budget");
}

// Runs a Lua callback under an instruction budget guard.
static int smlua_pcall_with_budget(lua_State *L, int nargs, int nresults) {
    int status;
    lua_sethook(L, smlua_callback_budget_hook, LUA_MASKCOUNT, SMLUA_CALLBACK_INSTRUCTION_BUDGET);
    status = lua_pcall(L, nargs, nresults, 0);
    lua_sethook(L, NULL, 0, 0);
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
        printf("lua: hook '%d' exceeded max references\n", hook_type);
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
        printf("lua: sync-table hook exceeded max references\n");
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
            return NULL;
    }
}

// Registers a callback for a specific action ID allocated by Lua.
static int smlua_hook_mario_action(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isinteger(L, 1) || !lua_isfunction(L, 2)) {
        return 0;
    }
    if (sMarioActionHookCount >= MAX_MARIO_ACTION_HOOKS) {
        printf("lua: mario action hook exceeded max references\n");
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
    if (lua_gettop(L) < 5 || !lua_isinteger(L, 1) || !lua_isinteger(L, 2) || !lua_isboolean(L, 3)) {
        return 0;
    }
    if (sBehaviorHookCount >= MAX_BEHAVIOR_HOOKS) {
        printf("lua: behavior hook exceeded max references\n");
        return 0;
    }

    s32 behaviorId = (s32)lua_tointeger(L, 1);
    if (smlua_behavior_from_id(behaviorId) == NULL) {
        return 0;
    }

    struct LuaBehaviorHook *hook = &sBehaviorHooks[sBehaviorHookCount];
    memset(hook, 0, sizeof(*hook));
    hook->behaviorId = behaviorId;
    hook->objList = (int)lua_tointeger(L, 2);
    hook->sync = lua_toboolean(L, 3) != 0;
    hook->initRef = LUA_NOREF;
    hook->loopRef = LUA_NOREF;

    if (lua_isfunction(L, 4)) {
        lua_pushvalue(L, 4);
        hook->initRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        hook->loopRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    hook->active = hook->initRef != LUA_NOREF || hook->loopRef != LUA_NOREF;
    if (!hook->active) {
        if (hook->initRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, hook->initRef);
        }
        if (hook->loopRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, hook->loopRef);
        }
        return 0;
    }

    sBehaviorHookCount++;
    return 1;
}

// Shared dispatcher for hook callbacks with optional args and return values.
static bool smlua_dispatch_hook_callbacks(enum LuaHookedEventType hook_type, int arg_count, int result_count,
                                          SmluaHookPushArgsFn push_args, const void *arg_ctx,
                                          SmluaHookReadResultsFn read_results, void *result_ctx) {
    if (sHookState == NULL || hook_type < 0 || hook_type >= HOOK_MAX) {
        return false;
    }

    struct LuaHookedEvent *hook = &sHookedEvents[hook_type];
    bool called = false;

    for (int i = 0; i < hook->count; i++) {
        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, hook->references[i]);
        if (!lua_isfunction(sHookState, -1)) {
            lua_pop(sHookState, 1);
            continue;
        }

        if (push_args != NULL) {
            push_args(sHookState, arg_ctx);
        }

        if (smlua_pcall_with_budget(sHookState, arg_count, result_count) != LUA_OK) {
            const char *error = lua_tostring(sHookState, -1);
            printf("lua: hook %d failed: %s\n", hook_type, error != NULL ? error : "<unknown>");
            lua_pop(sHookState, 1);
            continue;
        }

        if (read_results != NULL) {
            read_results(sHookState, result_ctx);
        }

        if (result_count > 0) {
            lua_pop(sHookState, result_count);
        }

        called = true;
    }

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
bool smlua_call_event_hooks(enum LuaHookedEventType hook_type) {
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
    struct MarioState *m = (struct MarioState *)mario_state;
    bool called = false;

    if (sHookState == NULL || m == NULL) {
        return false;
    }

    for (int i = 0; i < sMarioActionHookCount; i++) {
        struct LuaMarioActionHook *hook = &sMarioActionHooks[i];
        if (!hook->active || hook->action != m->action) {
            continue;
        }

        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, hook->callbackRef);
        if (!lua_isfunction(sHookState, -1)) {
            lua_pop(sHookState, 1);
            continue;
        }

        smlua_push_mario_state(sHookState, m);
        if (smlua_pcall_with_budget(sHookState, 1, 1) != LUA_OK) {
            const char *error = lua_tostring(sHookState, -1);
            printf("lua: mario action hook failed: %s\n", error != NULL ? error : "<unknown>");
            lua_pop(sHookState, 1);
            continue;
        }

        if (in_loop != NULL) {
            if (lua_isnumber(sHookState, -1)) {
                *in_loop = (int)lua_tointeger(sHookState, -1);
            } else if (lua_isboolean(sHookState, -1)) {
                *in_loop = lua_toboolean(sHookState, -1) ? 1 : 0;
            }
        }

        lua_pop(sHookState, 1);
        called = true;
    }

    return called;
}

// Executes hook_behavior callbacks by iterating active objects and matching behavior pointers.
void smlua_call_behavior_hooks(void) {
    if (sHookState == NULL || gObjectLists == NULL) {
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
                        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, hook->initRef);
                        if (lua_isfunction(sHookState, -1)) {
                            smlua_push_object(sHookState, obj);
                            if (smlua_pcall_with_budget(sHookState, 1, 0) != LUA_OK) {
                                const char *error = lua_tostring(sHookState, -1);
                                printf("lua: behavior init hook failed: %s\n",
                                       error != NULL ? error : "<unknown>");
                                lua_pop(sHookState, 1);
                            }
                        } else {
                            lua_pop(sHookState, 1);
                        }
                    }

                    if (hook->loopRef != LUA_NOREF) {
                        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, hook->loopRef);
                        if (lua_isfunction(sHookState, -1)) {
                            smlua_push_object(sHookState, obj);
                            if (smlua_pcall_with_budget(sHookState, 1, 0) != LUA_OK) {
                                const char *error = lua_tostring(sHookState, -1);
                                printf("lua: behavior loop hook failed: %s\n",
                                       error != NULL ? error : "<unknown>");
                                lua_pop(sHookState, 1);
                            }
                        } else {
                            lua_pop(sHookState, 1);
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
    if (sHookState == NULL) {
        return;
    }

    for (int i = 0; i < sSyncTableChangeHookCount; i++) {
        struct LuaSyncTableChangeHook *watch = &sSyncTableChangeHooks[i];
        if (!watch->active) {
            continue;
        }

        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, watch->tableRef);
        if (!lua_istable(sHookState, -1)) {
            lua_pop(sHookState, 1);
            continue;
        }

        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, watch->keyRef);
        lua_gettable(sHookState, -2);
        int currentValueIndex = lua_gettop(sHookState);

        lua_rawgeti(sHookState, LUA_REGISTRYINDEX, watch->prevValueRef);
        int previousValueIndex = lua_gettop(sHookState);

        bool changed = !lua_compare(sHookState, currentValueIndex, previousValueIndex, LUA_OPEQ);
        if (changed) {
            lua_rawgeti(sHookState, LUA_REGISTRYINDEX, watch->funcRef);
            if (lua_isfunction(sHookState, -1)) {
                lua_rawgeti(sHookState, LUA_REGISTRYINDEX, watch->tagRef);
                lua_pushvalue(sHookState, previousValueIndex);
                lua_pushvalue(sHookState, currentValueIndex);

                if (smlua_pcall_with_budget(sHookState, 3, 0) != LUA_OK) {
                    const char *error = lua_tostring(sHookState, -1);
                    printf("lua: sync-table hook failed: %s\n", error != NULL ? error : "<unknown>");
                    lua_pop(sHookState, 1);
                }
            } else {
                lua_pop(sHookState, 1);
            }

            smlua_unref_registry_ref(sHookState, &watch->prevValueRef);
            lua_pushvalue(sHookState, currentValueIndex);
            watch->prevValueRef = luaL_ref(sHookState, LUA_REGISTRYINDEX);
        }

        lua_pop(sHookState, 3);
    }
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
    memset(sHookedEvents, 0, sizeof(sHookedEvents));
    memset(sSyncTableChangeHooks, 0, sizeof(sSyncTableChangeHooks));
    memset(sMarioActionHooks, 0, sizeof(sMarioActionHooks));
    memset(sBehaviorHooks, 0, sizeof(sBehaviorHooks));
    sSyncTableChangeHookCount = 0;
    sMarioActionHookCount = 0;
    sBehaviorHookCount = 0;
}

// Registers hook API and event constants in Lua.
void smlua_bind_hooks(lua_State *L) {
    sHookState = L;
    smlua_clear_hooks(L);

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
