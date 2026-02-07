#ifndef SM64_PC_SMLUA_HOOKS_H
#define SM64_PC_SMLUA_HOOKS_H

#include <stdbool.h>

#include <lua.h>

// Co-op DX hook namespace. Not every hook is dispatched yet on Wii U, but the
// full enum keeps Lua mod scripts source-compatible with upstream names.
enum LuaHookedEventType {
    HOOK_UPDATE,
    HOOK_MARIO_UPDATE,
    HOOK_BEFORE_MARIO_UPDATE,
    HOOK_ON_SET_MARIO_ACTION,
    HOOK_BEFORE_PHYS_STEP,
    HOOK_ALLOW_PVP_ATTACK,
    HOOK_ON_PVP_ATTACK,
    HOOK_ON_PLAYER_CONNECTED,
    HOOK_ON_PLAYER_DISCONNECTED,
    HOOK_ON_HUD_RENDER,
    HOOK_ALLOW_INTERACT,
    HOOK_ON_INTERACT,
    HOOK_ON_LEVEL_INIT,
    HOOK_ON_WARP,
    HOOK_ON_SYNC_VALID,
    HOOK_ON_OBJECT_UNLOAD,
    HOOK_ON_SYNC_OBJECT_UNLOAD,
    HOOK_ON_PAUSE_EXIT,
    HOOK_GET_STAR_COLLECTION_DIALOG,
    HOOK_ON_SET_CAMERA_MODE,
    HOOK_ON_OBJECT_RENDER,
    HOOK_ON_DEATH,
    HOOK_ON_PACKET_RECEIVE,
    HOOK_USE_ACT_SELECT,
    HOOK_ON_CHANGE_CAMERA_ANGLE,
    HOOK_ON_SCREEN_TRANSITION,
    HOOK_ALLOW_HAZARD_SURFACE,
    HOOK_ON_CHAT_MESSAGE,
    HOOK_OBJECT_SET_MODEL,
    HOOK_CHARACTER_SOUND,
    HOOK_BEFORE_SET_MARIO_ACTION,
    HOOK_JOINED_GAME,
    HOOK_ON_OBJECT_ANIM_UPDATE,
    HOOK_ON_DIALOG,
    HOOK_ON_EXIT,
    HOOK_DIALOG_SOUND,
    HOOK_ON_HUD_RENDER_BEHIND,
    HOOK_ON_COLLIDE_LEVEL_BOUNDS,
    HOOK_MIRROR_MARIO_RENDER,
    HOOK_MARIO_OVERRIDE_PHYS_STEP_DEFACTO_SPEED,
    HOOK_ON_OBJECT_LOAD,
    HOOK_ON_PLAY_SOUND,
    HOOK_ON_SEQ_LOAD,
    HOOK_ON_ATTACK_OBJECT,
    HOOK_ON_LANGUAGE_CHANGED,
    HOOK_ON_MODS_LOADED,
    HOOK_ON_NAMETAGS_RENDER,
    HOOK_ON_DJUI_THEME_CHANGED,
    HOOK_ON_GEO_PROCESS,
    HOOK_BEFORE_GEO_PROCESS,
    HOOK_ON_GEO_PROCESS_CHILDREN,
    HOOK_MARIO_OVERRIDE_GEOMETRY_INPUTS,
    HOOK_ON_INTERACTIONS,
    HOOK_ALLOW_FORCE_WATER_ACTION,
    HOOK_BEFORE_WARP,
    HOOK_ON_INSTANT_WARP,
    HOOK_MARIO_OVERRIDE_FLOOR_CLASS,
    HOOK_ON_ADD_SURFACE,
    HOOK_ON_CLEAR_AREAS,
    HOOK_ON_PACKET_BYTESTRING_RECEIVE,
    HOOK_MAX,
};

void smlua_bind_hooks(lua_State *L);
void smlua_clear_hooks(lua_State *L);
bool smlua_call_event_hooks(enum LuaHookedEventType hook_type);
bool smlua_call_event_hooks_warp(enum LuaHookedEventType hook_type, int warp_type, int level_num,
                                 int area_idx, int node_id, unsigned int warp_arg);
bool smlua_call_event_hooks_interact(const void *mario_state, const void *object,
                                     unsigned int interact_type, bool interact_value);
bool smlua_call_event_hooks_allow_interact(const void *mario_state, const void *object,
                                           unsigned int interact_type, bool *allow_interact);
bool smlua_call_event_hooks_mario(enum LuaHookedEventType hook_type, const void *mario_state);
bool smlua_call_event_hooks_dialog(int dialog_id, bool *open_dialog_box,
                                   const char **dialog_text_override);
void smlua_call_event_hooks_before_set_mario_action(const void *mario_state, int next_action,
                                                    int action_arg);
void smlua_call_event_hooks_on_set_mario_action(const void *mario_state);
void smlua_call_event_hooks_object_set_model(const void *object, int model_id);
void smlua_poll_sync_table_change_hooks(void);
bool smlua_call_mario_action_hook(const void *mario_state, int *in_loop);
void smlua_call_behavior_hooks(void);

#endif
