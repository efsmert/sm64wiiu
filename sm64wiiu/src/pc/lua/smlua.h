#ifndef SM64_PC_SMLUA_H
#define SM64_PC_SMLUA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
// The devkitPPC/libstdc++ headers do not expose LLONG_MAX through <limits.h>
// in C++ mode, but Lua's luaconf.h uses it as a feature probe. Ensure it's
// defined before including Lua headers.
#include <climits>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifdef __cplusplus
extern "C" {
#endif

// DynOS/Lua compatibility surface (subset of Co-op DX smlua_utils).
// These are used by the DynOS gfx parser on ports.
#ifndef LOT_GFX
#define LOT_GFX 100
#endif
#ifndef LOT_VTX
#define LOT_VTX 101
#endif
#ifndef LVT_TEXTURE_P
#define LVT_TEXTURE_P 200
#endif

extern bool gSmLuaConvertSuccess;
int64_t smlua_to_integer(lua_State *L, uint32_t index);
const char *smlua_to_string(lua_State *L, uint32_t index);
void *smlua_to_cobject(lua_State *L, uint32_t index, uint16_t expected_type);
void *smlua_to_cpointer(lua_State *L, uint32_t index, uint16_t expected_type);

struct SmluaLightingState {
    float lighting_dir[3];
    uint8_t lighting_color[3];
    uint8_t lighting_ambient_color[3];
    uint8_t vertex_color[3];
    uint8_t fog_color[3];
    float fog_intensity;
    bool has_lighting_dir;
    bool has_lighting_color;
    bool has_lighting_ambient_color;
    bool has_vertex_color;
    bool has_fog_color;
    bool has_fog_intensity;
};

void smlua_init(void);
void smlua_update(void);
void smlua_shutdown(void);
struct lua_State *smlua_get_state(void);
void smlua_render_mod_overlay(void);
void smlua_get_lighting_state(struct SmluaLightingState *out_state);
int16_t smlua_get_override_far(int16_t default_far);
float smlua_get_override_fov(float default_fov);
int8_t smlua_get_override_skybox(int8_t default_background);
void smlua_get_skybox_color(uint8_t out_color[3]);

#ifdef __cplusplus
}
#endif

#endif
