#ifndef SM64_PC_SMLUA_COBJECT_H
#define SM64_PC_SMLUA_COBJECT_H

#include <stdint.h>

#include <lua.h>

enum SmluaCObjectType {
    SMLUA_COBJECT_MARIO_STATE = 1,
    SMLUA_COBJECT_OBJECT = 2,
    SMLUA_COBJECT_AREA = 3,
    SMLUA_COBJECT_CAMERA = 4,
    SMLUA_COBJECT_LAKITU_STATE = 5,
    SMLUA_COBJECT_MARIO_BODY_STATE = 6,
};

typedef struct SmluaCObject {
    const void *pointer;
    uint16_t type;
} SmluaCObject;

void smlua_bind_cobject(lua_State *L);
void smlua_cobject_init_globals(lua_State *L);
void smlua_cobject_update_globals(lua_State *L);
int smlua_define_custom_obj_fields(lua_State *L);
void smlua_push_mario_state(lua_State *L, const void *mario_state);
void smlua_push_mario_body_state(lua_State *L, const void *body_state);
void smlua_push_object(lua_State *L, const void *object);
void smlua_push_area(lua_State *L, const void *area);
void smlua_push_camera(lua_State *L, const void *camera);

#endif
