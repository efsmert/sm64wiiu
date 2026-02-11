#include <stdbool.h>
#include <string.h>

#include <lauxlib.h>

#include "sm64.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"
#include "smlua_cobject.h"

static const char *SMLUA_COBJECT_METATABLE = "SM64.CObject";
static const char *SMLUA_VEC3F_METATABLE = "SM64.Vec3fRef";
static const char *SMLUA_VEC3S_METATABLE = "SM64.Vec3sRef";
static const char *SMLUA_CONTROLLER_METATABLE = "SM64.ControllerRef";
static const char *SMLUA_OBJECT_HEADER_METATABLE = "SM64.ObjectHeaderRef";
static const char *SMLUA_OBJECT_GFX_METATABLE = "SM64.ObjectGfxRef";
static const char *SMLUA_OBJECT_ANIMINFO_METATABLE = "SM64.ObjectAnimInfoRef";
static const char *SMLUA_OBJECT_SHAREDCHILD_METATABLE = "SM64.ObjectSharedChildRef";
static const char *SMLUA_CUSTOM_OBJECT_FIELD_REGISTRY = "SM64.CustomObjectFields";

typedef struct SmluaVec3fRef {
    f32 *pointer;
} SmluaVec3fRef;

typedef struct SmluaVec3sRef {
    s16 *pointer;
} SmluaVec3sRef;

typedef struct SmluaControllerRef {
    struct Controller *pointer;
} SmluaControllerRef;

typedef struct SmluaObjectHeaderRef {
    struct Object *object;
} SmluaObjectHeaderRef;

typedef struct SmluaObjectGfxRef {
    struct Object *object;
} SmluaObjectGfxRef;

typedef struct SmluaObjectAnimInfoRef {
    struct Object *object;
} SmluaObjectAnimInfoRef;

typedef struct SmluaObjectSharedChildRef {
    struct Object *object;
} SmluaObjectSharedChildRef;

#define SMLUA_SHARED_CHILD_HOOK_PROCESS_MAX 1024
struct SmluaSharedChildHookProcess {
    struct Object *object;
    s32 hook_process;
};

static struct SmluaSharedChildHookProcess sSharedChildHookProcess[SMLUA_SHARED_CHILD_HOOK_PROCESS_MAX];

// Returns a stable human-readable type name used in Lua-facing diagnostics.
static const char *smlua_cobject_type_name(uint16_t type) {
    switch (type) {
        case SMLUA_COBJECT_MARIO_STATE:
            return "MarioState";
        case SMLUA_COBJECT_OBJECT:
            return "Object";
        case SMLUA_COBJECT_AREA:
            return "Area";
        case SMLUA_COBJECT_CAMERA:
            return "Camera";
        case SMLUA_COBJECT_LAKITU_STATE:
            return "LakituState";
        default:
            return "Unknown";
    }
}

// Resolves vector component key into component index (supports x/y/z, 0-2, 1-3).
static int smlua_vec_component_index(lua_State *L, int key_index) {
    if (lua_type(L, key_index) == LUA_TSTRING) {
        const char *key = lua_tostring(L, key_index);
        if (strcmp(key, "x") == 0) {
            return 0;
        }
        if (strcmp(key, "y") == 0) {
            return 1;
        }
        if (strcmp(key, "z") == 0) {
            return 2;
        }
        return -1;
    }

    if (lua_type(L, key_index) == LUA_TNUMBER) {
        int idx = (int)lua_tointeger(L, key_index);
        if (idx >= 0 && idx <= 2) {
            return idx;
        }
        if (idx >= 1 && idx <= 3) {
            return idx - 1;
        }
    }

    return -1;
}

// Lua metamethod for Vec3f proxy userdata.
static int smlua_vec3f_index(lua_State *L) {
    const SmluaVec3fRef *vec = luaL_checkudata(L, 1, SMLUA_VEC3F_METATABLE);
    int idx = smlua_vec_component_index(L, 2);
    if (idx >= 0) {
        lua_pushnumber(L, vec->pointer[idx]);
        return 1;
    }

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "type") == 0) {
            lua_pushstring(L, "Vec3f");
            return 1;
        }
        if (strcmp(key, "pointer") == 0) {
            lua_pushlightuserdata(L, vec->pointer);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for Vec3f proxy writes.
static int smlua_vec3f_newindex(lua_State *L) {
    SmluaVec3fRef *vec = luaL_checkudata(L, 1, SMLUA_VEC3F_METATABLE);
    int idx = smlua_vec_component_index(L, 2);
    if (idx < 0) {
        return luaL_error(L, "invalid Vec3f component");
    }
    vec->pointer[idx] = (f32)luaL_checknumber(L, 3);
    return 0;
}

// Lua metamethod for Vec3s proxy userdata.
static int smlua_vec3s_index(lua_State *L) {
    const SmluaVec3sRef *vec = luaL_checkudata(L, 1, SMLUA_VEC3S_METATABLE);
    int idx = smlua_vec_component_index(L, 2);
    if (idx >= 0) {
        lua_pushinteger(L, vec->pointer[idx]);
        return 1;
    }

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "type") == 0) {
            lua_pushstring(L, "Vec3s");
            return 1;
        }
        if (strcmp(key, "pointer") == 0) {
            lua_pushlightuserdata(L, vec->pointer);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for Vec3s proxy writes.
static int smlua_vec3s_newindex(lua_State *L) {
    SmluaVec3sRef *vec = luaL_checkudata(L, 1, SMLUA_VEC3S_METATABLE);
    int idx = smlua_vec_component_index(L, 2);
    if (idx < 0) {
        return luaL_error(L, "invalid Vec3s component");
    }
    vec->pointer[idx] = (s16)luaL_checkinteger(L, 3);
    return 0;
}

// Pushes a Vec3f proxy userdata for direct Lua read/write access.
static void smlua_push_vec3f(lua_State *L, f32 *pointer) {
    if (pointer == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaVec3fRef *vec = lua_newuserdata(L, sizeof(SmluaVec3fRef));
    vec->pointer = pointer;
    luaL_getmetatable(L, SMLUA_VEC3F_METATABLE);
    lua_setmetatable(L, -2);
}

// Pushes a Vec3s proxy userdata for direct Lua read/write access.
static void smlua_push_vec3s(lua_State *L, s16 *pointer) {
    if (pointer == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaVec3sRef *vec = lua_newuserdata(L, sizeof(SmluaVec3sRef));
    vec->pointer = pointer;
    luaL_getmetatable(L, SMLUA_VEC3S_METATABLE);
    lua_setmetatable(L, -2);
}

// Lua metamethod for Controller proxy userdata reads.
static int smlua_controller_index(lua_State *L) {
    const SmluaControllerRef *ref = luaL_checkudata(L, 1, SMLUA_CONTROLLER_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "Controller");
        return 1;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, ref->pointer);
        return 1;
    }
    if (strcmp(key, "rawStickX") == 0) {
        lua_pushinteger(L, ref->pointer->rawStickX);
        return 1;
    }
    if (strcmp(key, "rawStickY") == 0) {
        lua_pushinteger(L, ref->pointer->rawStickY);
        return 1;
    }
    if (strcmp(key, "stickX") == 0) {
        lua_pushnumber(L, ref->pointer->stickX);
        return 1;
    }
    if (strcmp(key, "stickY") == 0) {
        lua_pushnumber(L, ref->pointer->stickY);
        return 1;
    }
    if (strcmp(key, "stickMag") == 0) {
        lua_pushnumber(L, ref->pointer->stickMag);
        return 1;
    }
    if (strcmp(key, "buttonDown") == 0) {
        lua_pushinteger(L, ref->pointer->buttonDown);
        return 1;
    }
    if (strcmp(key, "buttonPressed") == 0) {
        lua_pushinteger(L, ref->pointer->buttonPressed);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for Controller proxy writes used by gameplay mods.
static int smlua_controller_newindex(lua_State *L) {
    SmluaControllerRef *ref = luaL_checkudata(L, 1, SMLUA_CONTROLLER_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "rawStickX") == 0) {
        ref->pointer->rawStickX = (s16)luaL_checkinteger(L, 3);
        return 0;
    }
    if (strcmp(key, "rawStickY") == 0) {
        ref->pointer->rawStickY = (s16)luaL_checkinteger(L, 3);
        return 0;
    }
    if (strcmp(key, "stickX") == 0) {
        ref->pointer->stickX = (f32)luaL_checknumber(L, 3);
        return 0;
    }
    if (strcmp(key, "stickY") == 0) {
        ref->pointer->stickY = (f32)luaL_checknumber(L, 3);
        return 0;
    }
    if (strcmp(key, "stickMag") == 0) {
        ref->pointer->stickMag = (f32)luaL_checknumber(L, 3);
        return 0;
    }
    if (strcmp(key, "buttonDown") == 0) {
        ref->pointer->buttonDown = (u16)luaL_checkinteger(L, 3);
        return 0;
    }
    if (strcmp(key, "buttonPressed") == 0) {
        ref->pointer->buttonPressed = (u16)luaL_checkinteger(L, 3);
        return 0;
    }

    return luaL_error(L, "unknown Controller field '%s'", key);
}

// Pushes a Controller proxy userdata for direct Lua read/write access.
static void smlua_push_controller(lua_State *L, struct Controller *pointer) {
    if (pointer == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaControllerRef *ref = lua_newuserdata(L, sizeof(SmluaControllerRef));
    ref->pointer = pointer;
    luaL_getmetatable(L, SMLUA_CONTROLLER_METATABLE);
    lua_setmetatable(L, -2);
}

// Returns the synthetic hookProcess flag associated with an object's sharedChild proxy.
static s32 smlua_get_shared_child_hook_process(struct Object *object) {
    for (u32 i = 0; i < ARRAY_COUNT(sSharedChildHookProcess); i++) {
        if (sSharedChildHookProcess[i].object == object) {
            return sSharedChildHookProcess[i].hook_process;
        }
    }
    return 0;
}

// Sets the synthetic hookProcess flag associated with an object's sharedChild proxy.
static void smlua_set_shared_child_hook_process(struct Object *object, s32 value) {
    struct SmluaSharedChildHookProcess *empty = NULL;

    for (u32 i = 0; i < ARRAY_COUNT(sSharedChildHookProcess); i++) {
        struct SmluaSharedChildHookProcess *entry = &sSharedChildHookProcess[i];
        if (entry->object == object) {
            entry->hook_process = value;
            return;
        }
        if (empty == NULL && entry->object == NULL) {
            empty = entry;
        }
    }

    if (empty != NULL) {
        empty->object = object;
        empty->hook_process = value;
    }
}

// Pushes an Object header proxy userdata.
static void smlua_push_object_header(lua_State *L, struct Object *object) {
    if (object == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaObjectHeaderRef *ref = lua_newuserdata(L, sizeof(SmluaObjectHeaderRef));
    ref->object = object;
    luaL_getmetatable(L, SMLUA_OBJECT_HEADER_METATABLE);
    lua_setmetatable(L, -2);
}

// Pushes an Object gfx proxy userdata.
static void smlua_push_object_gfx(lua_State *L, struct Object *object) {
    if (object == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaObjectGfxRef *ref = lua_newuserdata(L, sizeof(SmluaObjectGfxRef));
    ref->object = object;
    luaL_getmetatable(L, SMLUA_OBJECT_GFX_METATABLE);
    lua_setmetatable(L, -2);
}

// Pushes an Object animInfo proxy userdata.
static void smlua_push_object_animinfo(lua_State *L, struct Object *object) {
    if (object == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaObjectAnimInfoRef *ref = lua_newuserdata(L, sizeof(SmluaObjectAnimInfoRef));
    ref->object = object;
    luaL_getmetatable(L, SMLUA_OBJECT_ANIMINFO_METATABLE);
    lua_setmetatable(L, -2);
}

// Pushes an Object sharedChild proxy userdata.
static void smlua_push_object_shared_child(lua_State *L, struct Object *object) {
    if (object == NULL) {
        lua_pushnil(L);
        return;
    }
    SmluaObjectSharedChildRef *ref = lua_newuserdata(L, sizeof(SmluaObjectSharedChildRef));
    ref->object = object;
    luaL_getmetatable(L, SMLUA_OBJECT_SHAREDCHILD_METATABLE);
    lua_setmetatable(L, -2);
}

// Lua metamethod for object header proxy reads.
static int smlua_object_header_index(lua_State *L) {
    const SmluaObjectHeaderRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_HEADER_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "gfx") == 0) {
        smlua_push_object_gfx(L, ref->object);
        return 1;
    }
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "ObjectHeader");
        return 1;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, ref->object);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for object gfx proxy reads.
static int smlua_object_gfx_index(lua_State *L) {
    const SmluaObjectGfxRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_GFX_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "cameraToObject") == 0) {
        smlua_push_vec3f(L, ref->object->header.gfx.cameraToObject);
        return 1;
    }
    if (strcmp(key, "animInfo") == 0) {
        smlua_push_object_animinfo(L, ref->object);
        return 1;
    }
    if (strcmp(key, "angle") == 0) {
        smlua_push_vec3s(L, ref->object->header.gfx.angle);
        return 1;
    }
    if (strcmp(key, "sharedChild") == 0) {
        smlua_push_object_shared_child(L, ref->object);
        return 1;
    }
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "ObjectGfx");
        return 1;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, ref->object);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for object animInfo proxy reads.
static int smlua_object_animinfo_index(lua_State *L) {
    const SmluaObjectAnimInfoRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_ANIMINFO_METATABLE);
    const char *key = luaL_checkstring(L, 2);
    struct AnimInfo *anim_info = &ref->object->header.gfx.animInfo;

    if (strcmp(key, "animID") == 0) {
        lua_pushinteger(L, anim_info->animID);
        return 1;
    }
    if (strcmp(key, "animFrame") == 0) {
        lua_pushinteger(L, anim_info->animFrame);
        return 1;
    }
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "AnimInfo");
        return 1;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, anim_info);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for object animInfo proxy writes.
static int smlua_object_animinfo_newindex(lua_State *L) {
    SmluaObjectAnimInfoRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_ANIMINFO_METATABLE);
    const char *key = luaL_checkstring(L, 2);
    struct AnimInfo *anim_info = &ref->object->header.gfx.animInfo;

    if (strcmp(key, "animID") == 0) {
        anim_info->animID = (s16)luaL_checkinteger(L, 3);
        return 0;
    }
    if (strcmp(key, "animFrame") == 0) {
        anim_info->animFrame = (s16)luaL_checkinteger(L, 3);
        return 0;
    }

    return luaL_error(L, "unknown AnimInfo field '%s'", key);
}

// Lua metamethod for object sharedChild proxy reads.
static int smlua_object_shared_child_index(lua_State *L) {
    const SmluaObjectSharedChildRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_SHAREDCHILD_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "hookProcess") == 0) {
        lua_pushinteger(L, smlua_get_shared_child_hook_process(ref->object));
        return 1;
    }
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "SharedChild");
        return 1;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, ref->object->header.gfx.sharedChild);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod for object sharedChild proxy writes.
static int smlua_object_shared_child_newindex(lua_State *L) {
    SmluaObjectSharedChildRef *ref = luaL_checkudata(L, 1, SMLUA_OBJECT_SHAREDCHILD_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "hookProcess") == 0) {
        smlua_set_shared_child_hook_process(ref->object, (s32)luaL_checkinteger(L, 3));
        return 0;
    }

    return luaL_error(L, "unknown SharedChild field '%s'", key);
}

// Converts nil or Object cobject userdata into a native Object pointer.
static struct Object *smlua_to_object_or_nil(lua_State *L, int index) {
    if (lua_isnil(L, index)) {
        return NULL;
    }

    const SmluaCObject *cobj = luaL_checkudata(L, index, SMLUA_COBJECT_METATABLE);
    if (cobj->type != SMLUA_COBJECT_OBJECT) {
        luaL_error(L, "expected Object cobject");
    }
    return (struct Object *)cobj->pointer;
}

// Reads Vec3f components from Lua table {x,y,z} or array [1..3]/[0..2].
static bool smlua_read_vec3f_table(lua_State *L, int index, f32 out[3]) {
    int abs = lua_absindex(L, index);
    const char *keys[] = { "x", "y", "z" };
    for (int i = 0; i < 3; i++) {
        lua_getfield(L, abs, keys[i]);
        if (lua_isnumber(L, -1)) {
            out[i] = (f32)lua_tonumber(L, -1);
            lua_pop(L, 1);
            continue;
        }
        lua_pop(L, 1);

        lua_pushinteger(L, i + 1);
        lua_gettable(L, abs);
        if (lua_isnumber(L, -1)) {
            out[i] = (f32)lua_tonumber(L, -1);
            lua_pop(L, 1);
            continue;
        }
        lua_pop(L, 1);

        lua_pushinteger(L, i);
        lua_gettable(L, abs);
        if (lua_isnumber(L, -1)) {
            out[i] = (f32)lua_tonumber(L, -1);
            lua_pop(L, 1);
            continue;
        }
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// Reads Vec3s components from Lua table {x,y,z} or array [1..3]/[0..2].
static bool smlua_read_vec3s_table(lua_State *L, int index, s16 out[3]) {
    f32 temp[3];
    if (!smlua_read_vec3f_table(L, index, temp)) {
        return false;
    }
    out[0] = (s16)temp[0];
    out[1] = (s16)temp[1];
    out[2] = (s16)temp[2];
    return true;
}

// Pushes a known MarioState field into Lua and returns true on handled key.
static bool smlua_push_mario_field(lua_State *L, struct MarioState *m, const char *key) {
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "MarioState");
        return true;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, m);
        return true;
    }
    if (strcmp(key, "is_null") == 0) {
        lua_pushboolean(L, m == NULL ? 1 : 0);
        return true;
    }
    if (strcmp(key, "playerIndex") == 0) {
        lua_pushinteger(L, (lua_Integer)(m - gMarioStates));
        return true;
    }
    if (strcmp(key, "action") == 0) {
        lua_pushinteger(L, m->action);
        return true;
    }
    if (strcmp(key, "prevAction") == 0) {
        lua_pushinteger(L, m->prevAction);
        return true;
    }
    if (strcmp(key, "actionArg") == 0) {
        lua_pushinteger(L, m->actionArg);
        return true;
    }
    if (strcmp(key, "actionState") == 0) {
        lua_pushinteger(L, m->actionState);
        return true;
    }
    if (strcmp(key, "actionTimer") == 0) {
        lua_pushinteger(L, m->actionTimer);
        return true;
    }
    if (strcmp(key, "flags") == 0) {
        lua_pushinteger(L, m->flags);
        return true;
    }
    if (strcmp(key, "particleFlags") == 0) {
        lua_pushinteger(L, m->particleFlags);
        return true;
    }
    if (strcmp(key, "input") == 0) {
        lua_pushinteger(L, m->input);
        return true;
    }
    if (strcmp(key, "invincTimer") == 0) {
        lua_pushinteger(L, m->invincTimer);
        return true;
    }
    if (strcmp(key, "intendedMag") == 0) {
        lua_pushnumber(L, m->intendedMag);
        return true;
    }
    if (strcmp(key, "intendedYaw") == 0) {
        lua_pushinteger(L, m->intendedYaw);
        return true;
    }
    if (strcmp(key, "forwardVel") == 0) {
        lua_pushnumber(L, m->forwardVel);
        return true;
    }
    if (strcmp(key, "health") == 0) {
        lua_pushinteger(L, m->health);
        return true;
    }
    if (strcmp(key, "hurtCounter") == 0) {
        lua_pushinteger(L, m->hurtCounter);
        return true;
    }
    if (strcmp(key, "healCounter") == 0) {
        lua_pushinteger(L, m->healCounter);
        return true;
    }
    if (strcmp(key, "numLives") == 0) {
        lua_pushinteger(L, m->numLives);
        return true;
    }
    if (strcmp(key, "numCoins") == 0) {
        lua_pushinteger(L, m->numCoins);
        return true;
    }
    if (strcmp(key, "numStars") == 0) {
        lua_pushinteger(L, m->numStars);
        return true;
    }
    if (strcmp(key, "floorHeight") == 0) {
        lua_pushnumber(L, m->floorHeight);
        return true;
    }
    if (strcmp(key, "waterLevel") == 0) {
        lua_pushinteger(L, m->waterLevel);
        return true;
    }
    if (strcmp(key, "peakHeight") == 0) {
        lua_pushnumber(L, m->peakHeight);
        return true;
    }
    if (strcmp(key, "pos") == 0) {
        smlua_push_vec3f(L, m->pos);
        return true;
    }
    if (strcmp(key, "vel") == 0) {
        smlua_push_vec3f(L, m->vel);
        return true;
    }
    if (strcmp(key, "faceAngle") == 0) {
        smlua_push_vec3s(L, m->faceAngle);
        return true;
    }
    if (strcmp(key, "angleVel") == 0) {
        smlua_push_vec3s(L, m->angleVel);
        return true;
    }
    if (strcmp(key, "controller") == 0) {
        smlua_push_controller(L, m->controller);
        return true;
    }
    if (strcmp(key, "area") == 0) {
        smlua_push_area(L, m->area);
        return true;
    }
    if (strcmp(key, "marioObj") == 0) {
        smlua_push_object(L, m->marioObj);
        return true;
    }
    if (strcmp(key, "interactObj") == 0) {
        smlua_push_object(L, m->interactObj);
        return true;
    }
    if (strcmp(key, "heldObj") == 0) {
        smlua_push_object(L, m->heldObj);
        return true;
    }
    if (strcmp(key, "usedObj") == 0) {
        smlua_push_object(L, m->usedObj);
        return true;
    }
    if (strcmp(key, "riddenObj") == 0) {
        smlua_push_object(L, m->riddenObj);
        return true;
    }

    return false;
}

// Pushes a known Area field into Lua and returns true on handled key.
static bool smlua_push_area_field(lua_State *L, struct Area *a, const char *key) {
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "Area");
        return true;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, a);
        return true;
    }
    if (strcmp(key, "is_null") == 0) {
        lua_pushboolean(L, a == NULL ? 1 : 0);
        return true;
    }
    if (strcmp(key, "index") == 0) {
        lua_pushinteger(L, a->index);
        return true;
    }
    if (strcmp(key, "flags") == 0) {
        lua_pushinteger(L, a->flags);
        return true;
    }
    if (strcmp(key, "terrainType") == 0) {
        lua_pushinteger(L, a->terrainType);
        return true;
    }
    if (strcmp(key, "musicParam") == 0) {
        lua_pushinteger(L, a->musicParam);
        return true;
    }
    if (strcmp(key, "musicParam2") == 0) {
        lua_pushinteger(L, a->musicParam2);
        return true;
    }
    if (strcmp(key, "camera") == 0) {
        smlua_push_camera(L, a->camera);
        return true;
    }

    return false;
}

// Pushes a known Camera field into Lua and returns true on handled key.
static bool smlua_push_camera_field(lua_State *L, struct Camera *c, const char *key) {
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "Camera");
        return true;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, c);
        return true;
    }
    if (strcmp(key, "is_null") == 0) {
        lua_pushboolean(L, c == NULL ? 1 : 0);
        return true;
    }
    if (strcmp(key, "mode") == 0) {
        lua_pushinteger(L, c->mode);
        return true;
    }
    if (strcmp(key, "defMode") == 0) {
        lua_pushinteger(L, c->defMode);
        return true;
    }
    if (strcmp(key, "yaw") == 0) {
        lua_pushinteger(L, c->yaw);
        return true;
    }
    if (strcmp(key, "cutscene") == 0) {
        lua_pushinteger(L, c->cutscene);
        return true;
    }
    if (strcmp(key, "nextYaw") == 0) {
        lua_pushinteger(L, c->nextYaw);
        return true;
    }
    if (strcmp(key, "areaCenX") == 0) {
        lua_pushnumber(L, c->areaCenX);
        return true;
    }
    if (strcmp(key, "areaCenY") == 0) {
        lua_pushnumber(L, c->areaCenY);
        return true;
    }
    if (strcmp(key, "areaCenZ") == 0) {
        lua_pushnumber(L, c->areaCenZ);
        return true;
    }
    if (strcmp(key, "pos") == 0) {
        smlua_push_vec3f(L, c->pos);
        return true;
    }
    if (strcmp(key, "focus") == 0) {
        smlua_push_vec3f(L, c->focus);
        return true;
    }

    return false;
}

// Pushes a known LakituState field into Lua and returns true on handled key.
static bool smlua_push_lakitu_state_field(lua_State *L, struct LakituState *s, const char *key) {
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "LakituState");
        return true;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, s);
        return true;
    }
    if (strcmp(key, "is_null") == 0) {
        lua_pushboolean(L, s == NULL ? 1 : 0);
        return true;
    }
    if (strcmp(key, "mode") == 0) {
        lua_pushinteger(L, s->mode);
        return true;
    }
    if (strcmp(key, "defMode") == 0) {
        lua_pushinteger(L, s->defMode);
        return true;
    }
    if (strcmp(key, "yaw") == 0) {
        lua_pushinteger(L, s->yaw);
        return true;
    }
    if (strcmp(key, "nextYaw") == 0) {
        lua_pushinteger(L, s->nextYaw);
        return true;
    }
    if (strcmp(key, "roll") == 0) {
        lua_pushinteger(L, s->roll);
        return true;
    }
    if (strcmp(key, "curFocus") == 0) {
        smlua_push_vec3f(L, s->curFocus);
        return true;
    }
    if (strcmp(key, "curPos") == 0) {
        smlua_push_vec3f(L, s->curPos);
        return true;
    }
    if (strcmp(key, "goalFocus") == 0) {
        smlua_push_vec3f(L, s->goalFocus);
        return true;
    }
    if (strcmp(key, "goalPos") == 0) {
        smlua_push_vec3f(L, s->goalPos);
        return true;
    }
    if (strcmp(key, "focus") == 0) {
        smlua_push_vec3f(L, s->focus);
        return true;
    }
    if (strcmp(key, "pos") == 0) {
        smlua_push_vec3f(L, s->pos);
        return true;
    }

    return false;
}

// Pushes a known Object field into Lua and returns true on handled key.
static bool smlua_push_object_field(lua_State *L, struct Object *o, const char *key) {
    if (strcmp(key, "type") == 0) {
        lua_pushstring(L, "Object");
        return true;
    }
    if (strcmp(key, "pointer") == 0) {
        lua_pushlightuserdata(L, o);
        return true;
    }
    if (strcmp(key, "is_null") == 0) {
        lua_pushboolean(L, o == NULL ? 1 : 0);
        return true;
    }
    if (strcmp(key, "header") == 0) {
        smlua_push_object_header(L, o);
        return true;
    }
    if (strcmp(key, "oPosX") == 0) {
        lua_pushnumber(L, o->oPosX);
        return true;
    }
    if (strcmp(key, "oPosY") == 0) {
        lua_pushnumber(L, o->oPosY);
        return true;
    }
    if (strcmp(key, "oPosZ") == 0) {
        lua_pushnumber(L, o->oPosZ);
        return true;
    }
    if (strcmp(key, "oVelX") == 0) {
        lua_pushnumber(L, o->oVelX);
        return true;
    }
    if (strcmp(key, "oVelY") == 0) {
        lua_pushnumber(L, o->oVelY);
        return true;
    }
    if (strcmp(key, "oVelZ") == 0) {
        lua_pushnumber(L, o->oVelZ);
        return true;
    }
    if (strcmp(key, "oForwardVel") == 0) {
        lua_pushnumber(L, o->oForwardVel);
        return true;
    }
    if (strcmp(key, "oMoveAnglePitch") == 0) {
        lua_pushinteger(L, o->oMoveAnglePitch);
        return true;
    }
    if (strcmp(key, "oMoveAngleYaw") == 0) {
        lua_pushinteger(L, o->oMoveAngleYaw);
        return true;
    }
    if (strcmp(key, "oMoveAngleRoll") == 0) {
        lua_pushinteger(L, o->oMoveAngleRoll);
        return true;
    }
    if (strcmp(key, "oFaceAnglePitch") == 0) {
        lua_pushinteger(L, o->oFaceAnglePitch);
        return true;
    }
    if (strcmp(key, "oFaceAngleYaw") == 0) {
        lua_pushinteger(L, o->oFaceAngleYaw);
        return true;
    }
    if (strcmp(key, "oFaceAngleRoll") == 0) {
        lua_pushinteger(L, o->oFaceAngleRoll);
        return true;
    }
    if (strcmp(key, "oAction") == 0) {
        lua_pushinteger(L, o->oAction);
        return true;
    }
    if (strcmp(key, "oSubAction") == 0) {
        lua_pushinteger(L, o->oSubAction);
        return true;
    }
    if (strcmp(key, "oTimer") == 0) {
        lua_pushinteger(L, o->oTimer);
        return true;
    }
    if (strcmp(key, "oBehParams") == 0) {
        lua_pushinteger(L, o->oBehParams);
        return true;
    }
    if (strcmp(key, "oBehParams2ndByte") == 0) {
        lua_pushinteger(L, o->oBehParams2ndByte);
        return true;
    }
    if (strcmp(key, "oInteractionSubtype") == 0) {
        lua_pushinteger(L, o->oInteractionSubtype);
        return true;
    }
    if (strcmp(key, "oInteractStatus") == 0) {
        lua_pushinteger(L, o->oInteractStatus);
        return true;
    }
    if (strcmp(key, "oInteractType") == 0) {
        lua_pushinteger(L, o->oInteractType);
        return true;
    }
    if (strcmp(key, "oDistanceToMario") == 0) {
        lua_pushnumber(L, o->oDistanceToMario);
        return true;
    }
    if (strcmp(key, "oAngleToMario") == 0) {
        lua_pushinteger(L, o->oAngleToMario);
        return true;
    }
    if (strcmp(key, "oHomeX") == 0) {
        lua_pushnumber(L, o->oHomeX);
        return true;
    }
    if (strcmp(key, "oHomeY") == 0) {
        lua_pushnumber(L, o->oHomeY);
        return true;
    }
    if (strcmp(key, "oHomeZ") == 0) {
        lua_pushnumber(L, o->oHomeZ);
        return true;
    }
    if (strcmp(key, "oFloorHeight") == 0) {
        lua_pushnumber(L, o->oFloorHeight);
        return true;
    }
    if (strcmp(key, "oOpacity") == 0) {
        lua_pushinteger(L, o->oOpacity);
        return true;
    }
    if (strcmp(key, "oDamageOrCoinValue") == 0) {
        lua_pushinteger(L, o->oDamageOrCoinValue);
        return true;
    }
    if (strcmp(key, "oHealth") == 0) {
        lua_pushinteger(L, o->oHealth);
        return true;
    }
    if (strcmp(key, "oAnimState") == 0) {
        lua_pushinteger(L, o->oAnimState);
        return true;
    }
    if (strcmp(key, "oKoopaMovementType") == 0) {
        lua_pushinteger(L, o->oKoopaMovementType);
        return true;
    }
    if (strcmp(key, "oFloorType") == 0) {
        lua_pushinteger(L, o->oFloorType);
        return true;
    }
    if (strcmp(key, "activeFlags") == 0) {
        lua_pushinteger(L, o->activeFlags);
        return true;
    }
    if (strcmp(key, "parentObj") == 0) {
        smlua_push_object(L, o->parentObj);
        return true;
    }
    if (strcmp(key, "behavior") == 0) {
        lua_pushlightuserdata(L, (void *)o->behavior);
        return true;
    }

    return false;
}

// Writes a known MarioState field from Lua and returns true on handled key.
static bool smlua_set_mario_field(lua_State *L, struct MarioState *m, const char *key) {
    if (strcmp(key, "action") == 0) {
        m->action = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "actionArg") == 0) {
        m->actionArg = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "actionState") == 0) {
        m->actionState = (u16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "actionTimer") == 0) {
        m->actionTimer = (u16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "flags") == 0) {
        m->flags = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "particleFlags") == 0) {
        m->particleFlags = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "input") == 0) {
        m->input = (u16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "invincTimer") == 0) {
        m->invincTimer = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "intendedMag") == 0) {
        m->intendedMag = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "intendedYaw") == 0) {
        m->intendedYaw = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "forwardVel") == 0) {
        m->forwardVel = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "health") == 0) {
        m->health = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "hurtCounter") == 0) {
        m->hurtCounter = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "healCounter") == 0) {
        m->healCounter = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "numLives") == 0) {
        m->numLives = (s8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "numCoins") == 0) {
        m->numCoins = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "numStars") == 0) {
        m->numStars = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "floorHeight") == 0) {
        m->floorHeight = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "waterLevel") == 0) {
        m->waterLevel = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "peakHeight") == 0) {
        m->peakHeight = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "interactObj") == 0) {
        m->interactObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "heldObj") == 0) {
        m->heldObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "usedObj") == 0) {
        m->usedObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "riddenObj") == 0) {
        m->riddenObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "marioObj") == 0) {
        m->marioObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "pos") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected pos table with x/y/z");
        }
        m->pos[0] = vec[0];
        m->pos[1] = vec[1];
        m->pos[2] = vec[2];
        return true;
    }
    if (strcmp(key, "vel") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected vel table with x/y/z");
        }
        m->vel[0] = vec[0];
        m->vel[1] = vec[1];
        m->vel[2] = vec[2];
        return true;
    }
    if (strcmp(key, "faceAngle") == 0 && lua_istable(L, 3)) {
        s16 vec[3];
        if (!smlua_read_vec3s_table(L, 3, vec)) {
            return luaL_error(L, "expected faceAngle table with x/y/z");
        }
        m->faceAngle[0] = vec[0];
        m->faceAngle[1] = vec[1];
        m->faceAngle[2] = vec[2];
        return true;
    }
    if (strcmp(key, "angleVel") == 0 && lua_istable(L, 3)) {
        s16 vec[3];
        if (!smlua_read_vec3s_table(L, 3, vec)) {
            return luaL_error(L, "expected angleVel table with x/y/z");
        }
        m->angleVel[0] = vec[0];
        m->angleVel[1] = vec[1];
        m->angleVel[2] = vec[2];
        return true;
    }

    return false;
}

// Writes a known Area field from Lua and returns true on handled key.
static bool smlua_set_area_field(lua_State *L, struct Area *a, const char *key) {
    if (strcmp(key, "index") == 0) {
        a->index = (s8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "flags") == 0) {
        a->flags = (s8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "terrainType") == 0) {
        a->terrainType = (u16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "musicParam") == 0) {
        a->musicParam = (u16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "musicParam2") == 0) {
        a->musicParam2 = (u16)luaL_checkinteger(L, 3);
        return true;
    }

    return false;
}

// Writes a known Camera field from Lua and returns true on handled key.
static bool smlua_set_camera_field(lua_State *L, struct Camera *c, const char *key) {
    if (strcmp(key, "mode") == 0) {
        c->mode = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "defMode") == 0) {
        c->defMode = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "yaw") == 0) {
        c->yaw = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "cutscene") == 0) {
        c->cutscene = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "nextYaw") == 0) {
        c->nextYaw = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "areaCenX") == 0) {
        c->areaCenX = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "areaCenY") == 0) {
        c->areaCenY = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "areaCenZ") == 0) {
        c->areaCenZ = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "pos") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected pos table with x/y/z");
        }
        c->pos[0] = vec[0];
        c->pos[1] = vec[1];
        c->pos[2] = vec[2];
        return true;
    }
    if (strcmp(key, "focus") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected focus table with x/y/z");
        }
        c->focus[0] = vec[0];
        c->focus[1] = vec[1];
        c->focus[2] = vec[2];
        return true;
    }

    return false;
}

// Writes a known LakituState field from Lua and returns true on handled key.
static bool smlua_set_lakitu_state_field(lua_State *L, struct LakituState *s, const char *key) {
    if (strcmp(key, "mode") == 0) {
        s->mode = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "defMode") == 0) {
        s->defMode = (u8)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "yaw") == 0) {
        s->yaw = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "nextYaw") == 0) {
        s->nextYaw = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "roll") == 0) {
        s->roll = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "curFocus") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected curFocus table with x/y/z");
        }
        s->curFocus[0] = vec[0];
        s->curFocus[1] = vec[1];
        s->curFocus[2] = vec[2];
        return true;
    }
    if (strcmp(key, "curPos") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected curPos table with x/y/z");
        }
        s->curPos[0] = vec[0];
        s->curPos[1] = vec[1];
        s->curPos[2] = vec[2];
        return true;
    }
    if (strcmp(key, "goalFocus") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected goalFocus table with x/y/z");
        }
        s->goalFocus[0] = vec[0];
        s->goalFocus[1] = vec[1];
        s->goalFocus[2] = vec[2];
        return true;
    }
    if (strcmp(key, "goalPos") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected goalPos table with x/y/z");
        }
        s->goalPos[0] = vec[0];
        s->goalPos[1] = vec[1];
        s->goalPos[2] = vec[2];
        return true;
    }
    if (strcmp(key, "pos") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected pos table with x/y/z");
        }
        s->pos[0] = vec[0];
        s->pos[1] = vec[1];
        s->pos[2] = vec[2];
        return true;
    }
    if (strcmp(key, "focus") == 0 && lua_istable(L, 3)) {
        f32 vec[3];
        if (!smlua_read_vec3f_table(L, 3, vec)) {
            return luaL_error(L, "expected focus table with x/y/z");
        }
        s->focus[0] = vec[0];
        s->focus[1] = vec[1];
        s->focus[2] = vec[2];
        return true;
    }

    return false;
}

// Writes a known Object field from Lua and returns true on handled key.
static bool smlua_set_object_field(lua_State *L, struct Object *o, const char *key) {
    if (strcmp(key, "oPosX") == 0) {
        o->oPosX = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oPosY") == 0) {
        o->oPosY = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oPosZ") == 0) {
        o->oPosZ = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oVelX") == 0) {
        o->oVelX = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oVelY") == 0) {
        o->oVelY = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oVelZ") == 0) {
        o->oVelZ = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oForwardVel") == 0) {
        o->oForwardVel = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oMoveAnglePitch") == 0) {
        o->oMoveAnglePitch = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oMoveAngleYaw") == 0) {
        o->oMoveAngleYaw = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oMoveAngleRoll") == 0) {
        o->oMoveAngleRoll = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oFaceAnglePitch") == 0) {
        o->oFaceAnglePitch = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oFaceAngleYaw") == 0) {
        o->oFaceAngleYaw = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oFaceAngleRoll") == 0) {
        o->oFaceAngleRoll = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oAction") == 0) {
        o->oAction = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oSubAction") == 0) {
        o->oSubAction = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oTimer") == 0) {
        o->oTimer = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oBehParams") == 0) {
        o->oBehParams = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oBehParams2ndByte") == 0) {
        o->oBehParams2ndByte = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oInteractionSubtype") == 0) {
        o->oInteractionSubtype = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oInteractStatus") == 0) {
        o->oInteractStatus = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oInteractType") == 0) {
        o->oInteractType = (u32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oDistanceToMario") == 0) {
        o->oDistanceToMario = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oAngleToMario") == 0) {
        o->oAngleToMario = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oHomeX") == 0) {
        o->oHomeX = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oHomeY") == 0) {
        o->oHomeY = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oHomeZ") == 0) {
        o->oHomeZ = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oFloorHeight") == 0) {
        o->oFloorHeight = (f32)luaL_checknumber(L, 3);
        return true;
    }
    if (strcmp(key, "oOpacity") == 0) {
        o->oOpacity = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oDamageOrCoinValue") == 0) {
        o->oDamageOrCoinValue = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oHealth") == 0) {
        o->oHealth = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oAnimState") == 0) {
        o->oAnimState = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oKoopaMovementType") == 0) {
        o->oKoopaMovementType = (s32)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "oFloorType") == 0) {
        o->oFloorType = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "activeFlags") == 0) {
        o->activeFlags = (s16)luaL_checkinteger(L, 3);
        return true;
    }
    if (strcmp(key, "parentObj") == 0) {
        o->parentObj = smlua_to_object_or_nil(L, 3);
        return true;
    }
    if (strcmp(key, "behavior") == 0) {
        o->behavior = (const BehaviorScript *)lua_touserdata(L, 3);
        return true;
    }

    return false;
}

// Gets (or creates) registry-side table used for Lua-defined custom object fields.
static void smlua_get_custom_object_field_store(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, SMLUA_CUSTOM_OBJECT_FIELD_REGISTRY);
    if (lua_istable(L, -1)) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, SMLUA_CUSTOM_OBJECT_FIELD_REGISTRY);
}

// Looks up custom object field and leaves value on stack when present.
static bool smlua_get_custom_object_field(lua_State *L, struct Object *o, const char *key) {
    bool found = false;

    smlua_get_custom_object_field_store(L);                     // [store]
    lua_pushlightuserdata(L, o);
    lua_gettable(L, -2);                                        // [store][objFields?]
    if (lua_istable(L, -1)) {
        lua_pushstring(L, key);
        lua_gettable(L, -2);                                    // [store][objFields][value]
        found = !lua_isnil(L, -1);
        lua_remove(L, -2);                                      // [store][value]
        lua_remove(L, -2);                                      // [value]
        return found;
    }

    lua_pop(L, 2);                                              // []
    return false;
}

// Writes custom object field in registry-side storage.
static void smlua_set_custom_object_field(lua_State *L, struct Object *o, const char *key, int valueIndex) {
    int valueAbs = lua_absindex(L, valueIndex);

    smlua_get_custom_object_field_store(L);                     // [store]
    lua_pushlightuserdata(L, o);
    lua_gettable(L, -2);                                        // [store][objFields?]
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);                                        // [store][objFields]
        lua_pushlightuserdata(L, o);
        lua_pushvalue(L, -2);
        lua_settable(L, -4);                                    // [store][objFields]
    }

    lua_pushstring(L, key);
    lua_pushvalue(L, valueAbs);
    lua_settable(L, -3);                                        // [store][objFields]
    lua_pop(L, 2);                                              // []
}

// Lua metamethod: field reads for typed cobject userdata.
static int smlua_cobject_index(lua_State *L) {
    const SmluaCObject *cobj = luaL_checkudata(L, 1, SMLUA_COBJECT_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (cobj->pointer == NULL) {
        lua_pushnil(L);
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_MARIO_STATE
        && smlua_push_mario_field(L, (struct MarioState *)cobj->pointer, key)) {
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_AREA
        && smlua_push_area_field(L, (struct Area *)cobj->pointer, key)) {
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_CAMERA
        && smlua_push_camera_field(L, (struct Camera *)cobj->pointer, key)) {
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_LAKITU_STATE
        && smlua_push_lakitu_state_field(L, (struct LakituState *)cobj->pointer, key)) {
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_OBJECT
        && smlua_push_object_field(L, (struct Object *)cobj->pointer, key)) {
        return 1;
    }

    if (cobj->type == SMLUA_COBJECT_OBJECT
        && smlua_get_custom_object_field(L, (struct Object *)cobj->pointer, key)) {
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Lua metamethod: field writes for typed cobject userdata.
static int smlua_cobject_newindex(lua_State *L) {
    SmluaCObject *cobj = luaL_checkudata(L, 1, SMLUA_COBJECT_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (cobj->pointer == NULL) {
        return luaL_error(L, "cannot write fields on null cobject");
    }

    if (cobj->type == SMLUA_COBJECT_MARIO_STATE
        && smlua_set_mario_field(L, (struct MarioState *)cobj->pointer, key)) {
        return 0;
    }

    if (cobj->type == SMLUA_COBJECT_AREA
        && smlua_set_area_field(L, (struct Area *)cobj->pointer, key)) {
        return 0;
    }

    if (cobj->type == SMLUA_COBJECT_CAMERA
        && smlua_set_camera_field(L, (struct Camera *)cobj->pointer, key)) {
        return 0;
    }

    if (cobj->type == SMLUA_COBJECT_LAKITU_STATE
        && smlua_set_lakitu_state_field(L, (struct LakituState *)cobj->pointer, key)) {
        return 0;
    }

    if (cobj->type == SMLUA_COBJECT_OBJECT
        && smlua_set_object_field(L, (struct Object *)cobj->pointer, key)) {
        return 0;
    }

    if (cobj->type == SMLUA_COBJECT_OBJECT) {
        // Co-op DX mods attach custom per-object fields declared via define_custom_obj_fields().
        smlua_set_custom_object_field(L, (struct Object *)cobj->pointer, key, 3);
        return 0;
    }

    return luaL_error(L, "unknown or read-only cobject field '%s'", key);
}

// Lua metamethod: compares two wrappers by both type and underlying pointer.
static int smlua_cobject_eq(lua_State *L) {
    const SmluaCObject *a = luaL_checkudata(L, 1, SMLUA_COBJECT_METATABLE);
    const SmluaCObject *b = luaL_checkudata(L, 2, SMLUA_COBJECT_METATABLE);
    lua_pushboolean(L, a->type == b->type && a->pointer == b->pointer);
    return 1;
}

// Lua metamethod: debug-friendly representation for logs and REPL output.
static int smlua_cobject_tostring(lua_State *L) {
    const SmluaCObject *cobj = luaL_checkudata(L, 1, SMLUA_COBJECT_METATABLE);
    lua_pushfstring(L, "CObject<%s>(%p)", smlua_cobject_type_name(cobj->type), cobj->pointer);
    return 1;
}

// Pushes a typed cobject wrapper to Lua, or nil when pointer is null.
static void smlua_push_cobject(lua_State *L, uint16_t type, const void *pointer) {
    if (pointer == NULL) {
        lua_pushnil(L);
        return;
    }

    SmluaCObject *cobj = lua_newuserdata(L, sizeof(SmluaCObject));
    cobj->pointer = pointer;
    cobj->type = type;

    luaL_getmetatable(L, SMLUA_COBJECT_METATABLE);
    lua_setmetatable(L, -2);
}

// Initializes metatables for cobjects and vector reference proxies.
void smlua_bind_cobject(lua_State *L) {
    if (L == NULL) {
        return;
    }

    if (luaL_newmetatable(L, SMLUA_COBJECT_METATABLE)) {
        lua_pushcfunction(L, smlua_cobject_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_cobject_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, smlua_cobject_eq);
        lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, smlua_cobject_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_VEC3F_METATABLE)) {
        lua_pushcfunction(L, smlua_vec3f_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_vec3f_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_VEC3S_METATABLE)) {
        lua_pushcfunction(L, smlua_vec3s_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_vec3s_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_CONTROLLER_METATABLE)) {
        lua_pushcfunction(L, smlua_controller_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_controller_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_OBJECT_HEADER_METATABLE)) {
        lua_pushcfunction(L, smlua_object_header_index);
        lua_setfield(L, -2, "__index");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_OBJECT_GFX_METATABLE)) {
        lua_pushcfunction(L, smlua_object_gfx_index);
        lua_setfield(L, -2, "__index");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_OBJECT_ANIMINFO_METATABLE)) {
        lua_pushcfunction(L, smlua_object_animinfo_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_object_animinfo_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_OBJECT_SHAREDCHILD_METATABLE)) {
        lua_pushcfunction(L, smlua_object_shared_child_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_object_shared_child_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);
}

// Exposes core global cobjects needed by many Co-op DX Lua scripts.
void smlua_cobject_init_globals(lua_State *L) {
    if (L == NULL) {
        return;
    }

    memset(sSharedChildHookProcess, 0, sizeof(sSharedChildHookProcess));

    lua_newtable(L);
    lua_pushinteger(L, 0);
    smlua_push_mario_state(L, &gMarioStates[0]);
    lua_settable(L, -3);
    lua_setglobal(L, "gMarioStates");

    smlua_push_mario_state(L, gMarioState);
    lua_setglobal(L, "gMarioState");

    smlua_push_object(L, gMarioObject);
    lua_setglobal(L, "gMarioObject");

    smlua_push_object(L, gCurrentObject);
    lua_setglobal(L, "gCurrentObject");

    smlua_push_cobject(L, SMLUA_COBJECT_LAKITU_STATE, &gLakituState);
    lua_setglobal(L, "gLakituState");
}

// Refreshes dynamic global pointers that can change after Lua startup.
void smlua_cobject_update_globals(lua_State *L) {
    if (L == NULL) {
        return;
    }
    smlua_push_object(L, gMarioObject);
    lua_setglobal(L, "gMarioObject");
    smlua_push_object(L, gCurrentObject);
    lua_setglobal(L, "gCurrentObject");
}

// Wraps a MarioState pointer as a typed cobject for Lua hook callbacks.
void smlua_push_mario_state(lua_State *L, const void *mario_state) {
    smlua_push_cobject(L, SMLUA_COBJECT_MARIO_STATE, mario_state);
}

// Wraps an Object pointer as a typed cobject for Lua hook callbacks.
void smlua_push_object(lua_State *L, const void *object) {
    smlua_push_cobject(L, SMLUA_COBJECT_OBJECT, object);
}

// Wraps an Area pointer as a typed cobject for Lua access.
void smlua_push_area(lua_State *L, const void *area) {
    smlua_push_cobject(L, SMLUA_COBJECT_AREA, area);
}

// Wraps a Camera pointer as a typed cobject for Lua access.
void smlua_push_camera(lua_State *L, const void *camera) {
    smlua_push_cobject(L, SMLUA_COBJECT_CAMERA, camera);
}
