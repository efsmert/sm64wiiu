#ifndef SM64_PC_SMLUA_H
#define SM64_PC_SMLUA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
