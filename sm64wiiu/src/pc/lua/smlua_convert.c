#include <stdint.h>

#include "smlua.h"
#include "smlua_cobject.h"

bool gSmLuaConvertSuccess = true;

int64_t smlua_to_integer(lua_State *L, uint32_t index) {
    gSmLuaConvertSuccess = true;
    if (!lua_isnumber(L, (int)index)) {
        gSmLuaConvertSuccess = false;
        return 0;
    }
    return (int64_t)lua_tointeger(L, (int)index);
}

const char *smlua_to_string(lua_State *L, uint32_t index) {
    gSmLuaConvertSuccess = true;
    const char *str = lua_tostring(L, (int)index);
    if (str == NULL) {
        gSmLuaConvertSuccess = false;
        return "";
    }
    return str;
}

static void *smlua_to_pointer_fallback(lua_State *L, uint32_t index) {
    if (lua_islightuserdata(L, (int)index)) {
        return lua_touserdata(L, (int)index);
    }

    if (lua_isinteger(L, (int)index)) {
        uintptr_t value = (uintptr_t)lua_tointeger(L, (int)index);
        return (void *)value;
    }

    return NULL;
}

void *smlua_to_cobject(lua_State *L, uint32_t index, uint16_t expected_type) {
    gSmLuaConvertSuccess = true;

    void *fallback = smlua_to_pointer_fallback(L, index);
    if (fallback != NULL) {
        return fallback;
    }

    SmluaCObject *cobj = (SmluaCObject *)luaL_testudata(L, (int)index, "SM64.CObject");
    if (cobj == NULL || cobj->pointer == NULL) {
        gSmLuaConvertSuccess = false;
        return NULL;
    }

    if (expected_type != 0 && cobj->type != expected_type) {
        gSmLuaConvertSuccess = false;
        return NULL;
    }

    return (void *)cobj->pointer;
}

void *smlua_to_cpointer(lua_State *L, uint32_t index, uint16_t expected_type) {
    (void)expected_type;
    gSmLuaConvertSuccess = true;

    void *fallback = smlua_to_pointer_fallback(L, index);
    if (fallback != NULL) {
        return fallback;
    }

    SmluaCObject *cobj = (SmluaCObject *)luaL_testudata(L, (int)index, "SM64.CObject");
    if (cobj != NULL) {
        return (void *)cobj->pointer;
    }

    gSmLuaConvertSuccess = false;
    return NULL;
}

