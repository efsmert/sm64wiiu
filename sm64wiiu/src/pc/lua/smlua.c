#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <PR/os_cont.h>

#include "sm64.h"
#include "behavior_data.h"
#include "gfx_dimensions.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/hud.h"
#include "game/ingame_menu.h"
#include "game/memory.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/print.h"
#include "game/rendering_graph_node.h"
#include "game/segment2.h"
#include "game/interaction.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/sound_init.h"
#include "audio/external.h"
#include "audio/load.h"
#include "include/object_constants.h"
#include "include/geo_commands.h"
#include "include/model_ids.h"
#include "include/level_table.h"
#include "include/seq_ids.h"
#include "include/sounds.h"
#include "smlua.h"
#include "smlua_cobject.h"
#include "smlua_hooks.h"
#include "../fs/fs.h"
#include "../mods/mods.h"
#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

static lua_State *sLuaState = NULL;
static const char *SMLUA_COBJECT_METATABLE = "SM64.CObject";
static const char *SMLUA_TEXINFO_METATABLE = "SM64.TextureInfo";
static const char *SMLUA_MODAUDIO_METATABLE = "SM64.ModAudio";
static const char *SMLUA_GRAPH_ROOT_METATABLE = "SM64.GraphNodeRoot";
static const char *sActiveScriptPath = NULL;

// Exposes the active Lua VM for subsystems that need a stable runtime pointer.
lua_State *smlua_get_state(void) {
    return sLuaState;
}

#ifndef BACKGROUND_CUSTOM
#define BACKGROUND_CUSTOM 10
#endif

// Dialog punctuation glyphs mirrored from in-game dialog charset map.
#define SMLUA_DIALOG_CHAR_APOSTROPHE 0x3E
#define SMLUA_DIALOG_CHAR_EXCLAMATION 0xF2
#define SMLUA_DIALOG_CHAR_QUESTION 0xF4
#define SMLUA_DIALOG_CHAR_DASH 0x9F
#define SMLUA_DIALOG_CHAR_LPAREN 0xE1
#define SMLUA_DIALOG_CHAR_RPAREN 0xE3
#define SMLUA_DIALOG_CHAR_AMPERSAND 0xE5
#define SMLUA_DIALOG_CHAR_COLON 0xE6
#define SMLUA_DIALOG_CHAR_STAR 0xFB

#define SMLUA_MOD_STORAGE_MAX_KEYS 256
#define SMLUA_MOD_STORAGE_MAX_KEY 127
#define SMLUA_MOD_STORAGE_MAX_VALUE 1023
#define SMLUA_MOD_STORAGE_LINE_SIZE (SMLUA_MOD_STORAGE_MAX_KEY + SMLUA_MOD_STORAGE_MAX_VALUE + 8)

// Known Co-op DX behavior IDs consumed by built-in scripts we currently ship.
#define SMLUA_BEHAVIOR_ID_ACT_SELECTOR 6
#define SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE 7
#define SMLUA_BEHAVIOR_ID_BEGINNING_PEACH 22
#define SMLUA_BEHAVIOR_ID_CELEBRATION_STAR 116
#define SMLUA_BEHAVIOR_ID_END_PEACH 156
#define SMLUA_BEHAVIOR_ID_END_TOAD 157
#define SMLUA_BEHAVIOR_ID_GOOMBA 199
#define SMLUA_BEHAVIOR_ID_KOOPA 246
#define SMLUA_BEHAVIOR_ID_MARIO 281
#define SMLUA_BEHAVIOR_ID_METAL_CAP 290
#define SMLUA_BEHAVIOR_ID_NORMAL_CAP 308
#define SMLUA_BEHAVIOR_ID_VANISH_CAP 486
#define SMLUA_BEHAVIOR_ID_WING_CAP 529
// Day-night-cycle compat aliases for custom behavior symbols.
#define SMLUA_BEHAVIOR_ID_DNC_SKYBOX 0x7F00
#define SMLUA_BEHAVIOR_ID_DNC_NO_SKYBOX 0x7F01

enum SmluaHudDisplayValue {
    SMLUA_HUD_DISPLAY_LIVES = 0,
    SMLUA_HUD_DISPLAY_COINS = 1,
    SMLUA_HUD_DISPLAY_STARS = 2,
    SMLUA_HUD_DISPLAY_WEDGES = 3,
    SMLUA_HUD_DISPLAY_KEYS = 4,
    SMLUA_HUD_DISPLAY_FLAGS = 5,
    SMLUA_HUD_DISPLAY_TIMER = 6,
    SMLUA_HUD_DISPLAY_CAMERA_STATUS = 7,
};

enum SmluaHudResolution {
    SMLUA_RESOLUTION_DJUI = 0,
    SMLUA_RESOLUTION_N64 = 1,
};

enum SmluaHudFont {
    SMLUA_FONT_NORMAL = 0,
    SMLUA_FONT_MENU = 1,
    SMLUA_FONT_HUD = 2,
};

struct SmluaModStorageEntry {
    char key[SMLUA_MOD_STORAGE_MAX_KEY + 1];
    char value[SMLUA_MOD_STORAGE_MAX_VALUE + 1];
};

struct SmluaHudColor {
    int r;
    int g;
    int b;
    int a;
};

static int sHudResolution = SMLUA_RESOLUTION_DJUI;
static int sHudFont = SMLUA_FONT_NORMAL;
static struct SmluaHudColor sHudColor = { 255, 255, 255, 255 };
static f32 sLuaLightingDir[3] = { 0.0f, 0.0f, 0.0f };
static u8 sLuaLightingColor[3] = { 255, 255, 255 };
static u8 sLuaLightingAmbientColor[3] = { 255, 255, 255 };
static u8 sLuaVertexColor[3] = { 255, 255, 255 };
static u8 sLuaFogColor[3] = { 255, 255, 255 };
static f32 sLuaFogIntensity = 1.0f;
static bool sLuaHasLightingDir = false;
static bool sLuaHasLightingColor = false;
static bool sLuaHasLightingAmbientColor = false;
static bool sLuaHasVertexColor = false;
static bool sLuaHasFogColor = false;
static bool sLuaHasFogIntensity = false;
static s32 sLuaOverrideFar = -1;
static f32 sLuaOverrideFov = 0.0f;
static s32 sLuaScriptOverrideSkybox = -1;
static s32 sLuaDncOverrideSkybox = -1;
static u8 sLuaScriptSkyboxColor[3] = { 255, 255, 255 };
static u8 sLuaDncSkyboxColor[3] = { 255, 255, 255 };
static u8 sLuaSequenceAlias[0x100];
static bool sLuaSequenceAliasValid[0x100];
static bool sLuaCameraFrozen = false;
static bool sLuaHudHidden = false;
static s32 sLuaHudSavedFlags = HUD_DISPLAY_DEFAULT;
static s8 sLuaHudFlash = 0;
static s32 sLuaActSelectHudMask = 0;

struct SmluaSequenceOverride {
    bool active;
    bool has_original;
    u8 *original_data;
    s32 original_len;
    u8 *patched_data;
    s32 patched_len;
};

static struct SmluaSequenceOverride sLuaSequenceOverrides[0x100];

static bool smlua_path_has_suffix(const char *path, const char *suffix);
static void smlua_run_file(const char *path);
static void smlua_hud_draw_text(const char *message, s32 x, s32 y);
extern char gSmluaConstants[];
static void smlua_script_budget_hook(lua_State *L, lua_Debug *ar);
static void smlua_update_budget_hook(lua_State *L, lua_Debug *ar);
static void smlua_bind_wiiu_mod_runtime_compat(lua_State *L);
static void smlua_extract_mod_relative_path(const char *script_path, char *out, size_t out_size);
static void smlua_format_mod_display_name(const char *relative_path, char *out, size_t out_size);

#define SMLUA_SCRIPT_LOAD_INSTRUCTION_BUDGET 10000000
#define SMLUA_UPDATE_INSTRUCTION_BUDGET 5000000

struct SmluaTextureInfo {
    char name[128];
    char path[SYS_MAX_PATH];
    s32 width;
    s32 height;
};

struct SmluaModAudio {
    char filepath[SYS_MAX_PATH];
    s32 id;
    bool is_stream;
    bool looping;
    f32 volume;
    f32 frequency;
    s32 loop_start;
    s32 loop_end;
};

struct SmluaGraphRootRef {
    struct GraphNodeRoot *root;
};

#define SMLUA_MAX_MOD_AUDIO 512
static struct SmluaModAudio sLuaAudioPool[SMLUA_MAX_MOD_AUDIO];
static s32 sLuaAudioPoolCount = 0;
static u32 sLuaUpdateCounter = 0;

#define SMLUA_MOD_OVERLAY_MAX_LINES 4
#define SMLUA_MOD_OVERLAY_LINE_MAX 48
#define SMLUA_MOD_OVERLAY_SHOW_FRAMES 900
static char sLuaModOverlayLines[SMLUA_MOD_OVERLAY_MAX_LINES][SMLUA_MOD_OVERLAY_LINE_MAX];
static s32 sLuaModOverlayLineCount = 0;
static s32 sLuaModOverlayFramesLeft = 0;

#define SMLUA_CUSTOM_MARKER_DNC_SKYBOX 0x444E4301u
#define SMLUA_CUSTOM_MARKER_DNC_NO_SKYBOX 0x444E4302u

// Co-op DX extended player-model IDs used by built-in character-select scripts.
#define SMLUA_E_MODEL_MARIO 1
#define SMLUA_E_MODEL_MARIOS_WINGED_METAL_CAP 36
#define SMLUA_E_MODEL_MARIOS_METAL_CAP 37
#define SMLUA_E_MODEL_MARIOS_WING_CAP 38
#define SMLUA_E_MODEL_MARIOS_CAP 39
#define SMLUA_E_MODEL_LUIGI 362
#define SMLUA_E_MODEL_LUIGIS_CAP 363
#define SMLUA_E_MODEL_LUIGIS_METAL_CAP 364
#define SMLUA_E_MODEL_LUIGIS_WING_CAP 365
#define SMLUA_E_MODEL_LUIGIS_WINGED_METAL_CAP 366
#define SMLUA_E_MODEL_TOAD_PLAYER 367
#define SMLUA_E_MODEL_TOADS_CAP 368
#define SMLUA_E_MODEL_TOADS_METAL_CAP 369
#define SMLUA_E_MODEL_TOADS_WING_CAP 370
#define SMLUA_E_MODEL_WALUIGI 371
#define SMLUA_E_MODEL_WALUIGIS_CAP 372
#define SMLUA_E_MODEL_WALUIGIS_METAL_CAP 373
#define SMLUA_E_MODEL_WALUIGIS_WING_CAP 374
#define SMLUA_E_MODEL_WALUIGIS_WINGED_METAL_CAP 375
#define SMLUA_E_MODEL_WARIO 376
#define SMLUA_E_MODEL_WARIOS_CAP 377
#define SMLUA_E_MODEL_WARIOS_METAL_CAP 378
#define SMLUA_E_MODEL_WARIOS_WING_CAP 379
#define SMLUA_E_MODEL_WARIOS_WINGED_METAL_CAP 380

// Synthetic IDs for shipped custom model names when DynOS model loading is unavailable.
#define SMLUA_E_MODEL_VL_TONE_LUIGI 500
#define SMLUA_E_MODEL_CJ_LUIGI 501
#define SMLUA_E_MODEL_DJOSLIN_TOAD 502
#define SMLUA_E_MODEL_FLUFFA_WARIO 503
#define SMLUA_E_MODEL_KEEB_WALUIGI 504
#define SMLUA_E_MODEL_FLUFFA_WALUIGI 505
#define SMLUA_E_MODEL_ERROR_MODEL 159

#define SMLUA_MAX_CUSTOM_ANIMS 128
#define SMLUA_MAX_MODEL_OVERRIDES 2048
#define SMLUA_MAX_DIALOG_OVERRIDES 512

struct SmluaCustomAnimation {
    bool active;
    char *name;
    struct Animation anim;
    s16 *values;
    u16 *index;
};

struct SmluaModelOverride {
    struct Object *obj;
    s32 logical_model_id;
};

struct SmluaDialogOverride {
    bool has_original;
    struct DialogEntry original;
    u8 *text;
};

static struct SmluaCustomAnimation sLuaCustomAnimations[SMLUA_MAX_CUSTOM_ANIMS];
static struct SmluaModelOverride sLuaModelOverrides[SMLUA_MAX_MODEL_OVERRIDES];
static struct SmluaDialogOverride sLuaDialogOverrides[SMLUA_MAX_DIALOG_OVERRIDES];
extern void add_glyph_texture(s8 glyphIndex);

// Logs Lua diagnostics. On Wii U/Cemu, only emit failures to avoid OSConsole stalls.
static void smlua_logf(const char *fmt, ...) {
    char msg[768];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

#ifdef TARGET_WII_U
    if (strstr(msg, "failed") != NULL || strstr(msg, "error") != NULL || strstr(msg, "fatal") != NULL) {
        WHBLogPrint(msg);
    }
#else
    printf("%s\n", msg);
#endif
}

// Converts one script path into a short HUD-safe mod label.
static void smlua_mod_overlay_make_label(char *out, size_t out_size, const char *script_path) {
    const char *name = script_path;
    const char *slash;
    const char *dot;
    size_t limit = 0;
    size_t pos = 0;

    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (name == NULL || name[0] == '\0') {
        snprintf(out, out_size, "UNKNOWN");
        return;
    }

    if (strncmp(name, "mods/", 5) == 0) {
        name += 5;
    }

    slash = strchr(name, '/');
    if (slash != NULL) {
        // Folder mods are represented by folder name (mods/foo/main.lua -> foo).
        limit = (size_t)(slash - name);
    } else {
        dot = strrchr(name, '.');
        limit = dot != NULL ? (size_t)(dot - name) : strlen(name);
    }
    if (limit == 0) {
        snprintf(out, out_size, "MOD");
        return;
    }

    for (size_t i = 0; i < limit && pos + 1 < out_size; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '.' || c == '/') {
            out[pos++] = (char)toupper((unsigned char)c);
        } else {
            out[pos++] = ' ';
        }
    }
    while (pos > 0 && out[pos - 1] == ' ') {
        pos--;
    }
    if (pos == 0) {
        snprintf(out, out_size, "MOD");
        return;
    }
    out[pos] = '\0';
}

// Rebuilds startup overlay lines showing currently active root scripts.
static void smlua_refresh_mod_overlay_lines(void) {
    size_t script_count = mods_get_active_script_count();
    size_t max_listed = (size_t)(SMLUA_MOD_OVERLAY_MAX_LINES - 1);
    size_t listed = 0;

    memset(sLuaModOverlayLines, 0, sizeof(sLuaModOverlayLines));
    sLuaModOverlayLineCount = 0;

    snprintf(sLuaModOverlayLines[sLuaModOverlayLineCount++], SMLUA_MOD_OVERLAY_LINE_MAX, "MODS %u",
             (unsigned)script_count);

    if (script_count == 0) {
        snprintf(sLuaModOverlayLines[sLuaModOverlayLineCount++], SMLUA_MOD_OVERLAY_LINE_MAX, "NONE");
    } else {
        for (size_t i = 0; i < script_count && listed < max_listed; i++) {
            const char *script_path = mods_get_active_script_path(i);
            smlua_mod_overlay_make_label(
                sLuaModOverlayLines[sLuaModOverlayLineCount++],
                SMLUA_MOD_OVERLAY_LINE_MAX,
                script_path
            );
            listed++;
        }
        if (script_count > listed && sLuaModOverlayLineCount < SMLUA_MOD_OVERLAY_MAX_LINES) {
            snprintf(sLuaModOverlayLines[sLuaModOverlayLineCount++], SMLUA_MOD_OVERLAY_LINE_MAX, "MORE %u",
                     (unsigned)(script_count - listed));
        }
    }

    sLuaModOverlayFramesLeft = SMLUA_MOD_OVERLAY_SHOW_FRAMES;
}

// Draws startup overlay with loaded mod count and names for quick runtime verification.
static void smlua_draw_mod_overlay(void) {
    const s32 x = 12;
    const s32 y = 16;
    const s32 line_step = 12;

    if (sLuaModOverlayFramesLeft <= 0 || sLuaModOverlayLineCount <= 0) {
        return;
    }

    for (s32 i = 0; i < sLuaModOverlayLineCount; i++) {
        smlua_hud_draw_text(sLuaModOverlayLines[i], x, y + (i * line_step));
    }

    sLuaModOverlayFramesLeft--;
}

// Renders startup mod overlay from HUD render phase when display lists are active.
void smlua_render_mod_overlay(void) {
    smlua_draw_mod_overlay();
}
extern s8 char_to_glyph_index(char c);
extern void render_textrect(s32 x, s32 y, s32 pos);
extern void render_hud_tex_lut(s32 x, s32 y, u8 *texture);

// Exposes current Lua-scripted lighting/fog compatibility values for renderer integration.
void smlua_get_lighting_state(struct SmluaLightingState *out_state) {
    if (out_state == NULL) {
        return;
    }

    out_state->lighting_dir[0] = sLuaLightingDir[0];
    out_state->lighting_dir[1] = sLuaLightingDir[1];
    out_state->lighting_dir[2] = sLuaLightingDir[2];
    out_state->lighting_color[0] = sLuaLightingColor[0];
    out_state->lighting_color[1] = sLuaLightingColor[1];
    out_state->lighting_color[2] = sLuaLightingColor[2];
    out_state->lighting_ambient_color[0] = sLuaLightingAmbientColor[0];
    out_state->lighting_ambient_color[1] = sLuaLightingAmbientColor[1];
    out_state->lighting_ambient_color[2] = sLuaLightingAmbientColor[2];
    out_state->vertex_color[0] = sLuaVertexColor[0];
    out_state->vertex_color[1] = sLuaVertexColor[1];
    out_state->vertex_color[2] = sLuaVertexColor[2];
    out_state->fog_color[0] = sLuaFogColor[0];
    out_state->fog_color[1] = sLuaFogColor[1];
    out_state->fog_color[2] = sLuaFogColor[2];
    out_state->fog_intensity = sLuaFogIntensity;
    out_state->has_lighting_dir = sLuaHasLightingDir;
    out_state->has_lighting_color = sLuaHasLightingColor;
    out_state->has_lighting_ambient_color = sLuaHasLightingAmbientColor;
    out_state->has_vertex_color = sLuaHasVertexColor;
    out_state->has_fog_color = sLuaHasFogColor;
    out_state->has_fog_intensity = sLuaHasFogIntensity;
}

// Returns the active Lua far-clip override clamped to the projection range.
int16_t smlua_get_override_far(int16_t default_far) {
    if (sLuaOverrideFar <= 0) {
        return default_far;
    }
    if (sLuaOverrideFar > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)sLuaOverrideFar;
}

// Returns the active Lua FOV override when scripts request one.
float smlua_get_override_fov(float default_fov) {
    if (sLuaOverrideFov > 0.0f) {
        return sLuaOverrideFov;
    }
    return default_fov;
}

// Returns the active script/DNC skybox override while preserving vanilla fallback.
int8_t smlua_get_override_skybox(int8_t default_background) {
    s32 background = default_background;

    if (sLuaScriptOverrideSkybox >= 0) {
        background = sLuaScriptOverrideSkybox;
    } else if (sLuaDncOverrideSkybox >= 0) {
        background = sLuaDncOverrideSkybox;
    }

    if (background < INT8_MIN || background > INT8_MAX) {
        return default_background;
    }
    return (int8_t)background;
}

// Returns the effective skybox tint, combining explicit script color and DNC overlay tint.
void smlua_get_skybox_color(uint8_t out_color[3]) {
    if (out_color == NULL) {
        return;
    }

    for (int i = 0; i < 3; i++) {
        out_color[i] = (u8)(((u32)sLuaScriptSkyboxColor[i] * (u32)sLuaDncSkyboxColor[i] + 127u) / 255u);
    }
}

// Resets Lua-driven lighting/fog compatibility state to neutral renderer values.
static void smlua_reset_lighting_state(void) {
    sLuaLightingDir[0] = 0.0f;
    sLuaLightingDir[1] = 0.0f;
    sLuaLightingDir[2] = 0.0f;
    sLuaLightingColor[0] = 255;
    sLuaLightingColor[1] = 255;
    sLuaLightingColor[2] = 255;
    sLuaLightingAmbientColor[0] = 255;
    sLuaLightingAmbientColor[1] = 255;
    sLuaLightingAmbientColor[2] = 255;
    sLuaVertexColor[0] = 255;
    sLuaVertexColor[1] = 255;
    sLuaVertexColor[2] = 255;
    sLuaFogColor[0] = 255;
    sLuaFogColor[1] = 255;
    sLuaFogColor[2] = 255;
    sLuaFogIntensity = 1.0f;
    sLuaHasLightingDir = false;
    sLuaHasLightingColor = false;
    sLuaHasLightingAmbientColor = false;
    sLuaHasVertexColor = false;
    sLuaHasFogColor = false;
    sLuaHasFogIntensity = false;
    sLuaOverrideFar = -1;
    sLuaOverrideFov = 0.0f;
    sLuaScriptOverrideSkybox = -1;
    sLuaDncOverrideSkybox = -1;
    sLuaScriptSkyboxColor[0] = 255;
    sLuaScriptSkyboxColor[1] = 255;
    sLuaScriptSkyboxColor[2] = 255;
    sLuaDncSkyboxColor[0] = 255;
    sLuaDncSkyboxColor[1] = 255;
    sLuaDncSkyboxColor[2] = 255;
    sLuaCameraFrozen = false;
    sLuaHudHidden = false;
    sLuaHudSavedFlags = HUD_DISPLAY_DEFAULT;
}

// Resets script-defined sequence alias mappings used by audio replacement shims.
static void smlua_reset_sequence_aliases(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sLuaSequenceOverrides); i++) {
        struct SmluaSequenceOverride *override = &sLuaSequenceOverrides[i];
        if (override->active && override->has_original && gSeqFileHeader != NULL &&
            i < (u32)gSeqFileHeader->seqCount) {
            gSeqFileHeader->seqArray[i].offset = override->original_data;
            gSeqFileHeader->seqArray[i].len = override->original_len;
        }
        if (override->patched_data != NULL) {
            free(override->patched_data);
            override->patched_data = NULL;
        }
        override->active = false;
        override->has_original = false;
        override->original_data = NULL;
        override->original_len = 0;
        override->patched_len = 0;
    }

    memset(sLuaSequenceAlias, 0, sizeof(sLuaSequenceAlias));
    memset(sLuaSequenceAliasValid, 0, sizeof(sLuaSequenceAliasValid));
}

// Frees custom animation buffers registered through Lua animation utilities.
static void smlua_reset_custom_animations(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sLuaCustomAnimations); i++) {
        struct SmluaCustomAnimation *anim = &sLuaCustomAnimations[i];
        if (!anim->active) {
            continue;
        }
        free(anim->name);
        free(anim->values);
        free(anim->index);
        memset(anim, 0, sizeof(*anim));
    }
}

// Clears object logical-model tracking for extended model compatibility.
static void smlua_reset_model_overrides(void) {
    memset(sLuaModelOverrides, 0, sizeof(sLuaModelOverrides));
}

// Returns the dialog entry pointer for a valid ID in the current dialog table.
static struct DialogEntry *smlua_get_dialog_entry(s32 dialog_id) {
    void **dialog_table;
    struct DialogEntry *dialog;

    if (dialog_id < 0 || dialog_id >= SMLUA_MAX_DIALOG_OVERRIDES) {
        return NULL;
    }
    dialog_table = segmented_to_virtual(seg2_dialog_table);
    if (dialog_table == NULL) {
        return NULL;
    }
    dialog = segmented_to_virtual(dialog_table[dialog_id]);
    if (dialog == segmented_to_virtual(NULL)) {
        return NULL;
    }
    return dialog;
}

// Converts SM64 dialog glyph encoding into printable ASCII.
static char smlua_dialog_char_to_ascii(u8 ch) {
    if (ch <= 9) {
        return (char)('0' + ch);
    }
    if (ch >= 0x0A && ch <= 0x23) {
        return (char)('A' + (ch - 0x0A));
    }
    if (ch >= 0x24 && ch <= 0x3D) {
        return (char)('a' + (ch - 0x24));
    }

    switch (ch) {
        case DIALOG_CHAR_PERIOD:     return '.';
        case DIALOG_CHAR_COMMA:      return ',';
        case DIALOG_CHAR_SPACE:      return ' ';
        case DIALOG_CHAR_NEWLINE:    return '\n';
#if defined(VERSION_US) || defined(VERSION_EU)
        case DIALOG_CHAR_SLASH:      return '/';
#endif
        case SMLUA_DIALOG_CHAR_STAR: return '*';
        case SMLUA_DIALOG_CHAR_APOSTROPHE: return '\'';
        case SMLUA_DIALOG_CHAR_QUESTION: return '?';
        case SMLUA_DIALOG_CHAR_EXCLAMATION: return '!';
        case SMLUA_DIALOG_CHAR_AMPERSAND: return '&';
        case SMLUA_DIALOG_CHAR_COLON: return ':';
        case SMLUA_DIALOG_CHAR_DASH: return '-';
        case SMLUA_DIALOG_CHAR_LPAREN: return '(';
        case SMLUA_DIALOG_CHAR_RPAREN:return ')';
        default:                     return ' ';
    }
}

// Converts ASCII into SM64 dialog glyph encoding.
static u8 smlua_ascii_to_dialog_char(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (u8)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return (u8)(ch - 'A' + 0x0A);
    }
    if (ch >= 'a' && ch <= 'z') {
        return (u8)(ch - 'a' + 0x24);
    }

    switch (ch) {
        case '.': return DIALOG_CHAR_PERIOD;
        case ',': return DIALOG_CHAR_COMMA;
        case '\'': return SMLUA_DIALOG_CHAR_APOSTROPHE;
        case '?': return SMLUA_DIALOG_CHAR_QUESTION;
        case '!': return SMLUA_DIALOG_CHAR_EXCLAMATION;
        case '&': return SMLUA_DIALOG_CHAR_AMPERSAND;
        case ':': return SMLUA_DIALOG_CHAR_COLON;
        case '(': return SMLUA_DIALOG_CHAR_LPAREN;
        case ')': return SMLUA_DIALOG_CHAR_RPAREN;
        case '-': return SMLUA_DIALOG_CHAR_DASH;
        case '/':
#if defined(VERSION_US) || defined(VERSION_EU)
            return DIALOG_CHAR_SLASH;
#else
            return DIALOG_CHAR_SPACE;
#endif
        case '*': return SMLUA_DIALOG_CHAR_STAR;
        case '\n': return DIALOG_CHAR_NEWLINE;
        default: return DIALOG_CHAR_SPACE;
    }
}

// Converts a dialog glyph string into an ASCII string for Lua scripts.
static void smlua_dialog_to_ascii(const u8 *dialog_text, char *ascii_text, size_t ascii_size) {
    size_t write_pos = 0;

    if (ascii_text == NULL || ascii_size == 0) {
        return;
    }

    if (dialog_text == NULL) {
        ascii_text[0] = '\0';
        return;
    }

    while (*dialog_text != DIALOG_CHAR_TERMINATOR && write_pos + 1 < ascii_size) {
        ascii_text[write_pos++] = smlua_dialog_char_to_ascii(*dialog_text++);
    }
    ascii_text[write_pos] = '\0';
}

// Finds a registered custom animation entry by name.
static struct SmluaCustomAnimation *smlua_find_custom_animation(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    for (u32 i = 0; i < ARRAY_COUNT(sLuaCustomAnimations); i++) {
        struct SmluaCustomAnimation *entry = &sLuaCustomAnimations[i];
        if (!entry->active || entry->name == NULL) {
            continue;
        }
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

// Finds an unused custom animation slot.
static struct SmluaCustomAnimation *smlua_alloc_custom_animation_slot(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sLuaCustomAnimations); i++) {
        if (!sLuaCustomAnimations[i].active) {
            return &sLuaCustomAnimations[i];
        }
    }
    return NULL;
}

// Reads a Lua numeric array into a newly allocated s16 buffer.
static bool smlua_read_s16_table(lua_State *L, int index, s16 **out_data, u32 *out_len) {
    int abs = lua_absindex(L, index);
    size_t len;
    s16 *data;

    if (!lua_istable(L, abs) || out_data == NULL || out_len == NULL) {
        return false;
    }

    len = lua_rawlen(L, abs);
    if (len == 0 || len > UINT32_MAX) {
        return false;
    }

    data = malloc(len * sizeof(*data));
    if (data == NULL) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        lua_rawgeti(L, abs, (lua_Integer)(i + 1));
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            free(data);
            return false;
        }
        data[i] = (s16)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    *out_data = data;
    *out_len = (u32)len;
    return true;
}

// Reads a Lua numeric array into a newly allocated u16 buffer.
static bool smlua_read_u16_table(lua_State *L, int index, u16 **out_data, u32 *out_len) {
    int abs = lua_absindex(L, index);
    size_t len;
    u16 *data;

    if (!lua_istable(L, abs) || out_data == NULL || out_len == NULL) {
        return false;
    }

    len = lua_rawlen(L, abs);
    if (len == 0 || len > UINT32_MAX) {
        return false;
    }

    data = malloc(len * sizeof(*data));
    if (data == NULL) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        lua_rawgeti(L, abs, (lua_Integer)(i + 1));
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            free(data);
            return false;
        }
        data[i] = (u16)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    *out_data = data;
    *out_len = (u32)len;
    return true;
}

// Restores original dialog entries and frees Lua replacement text buffers.
static void smlua_reset_dialog_overrides(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sLuaDialogOverrides); i++) {
        struct SmluaDialogOverride *override = &sLuaDialogOverrides[i];
        if (override->has_original) {
            struct DialogEntry *dialog = smlua_get_dialog_entry((s32)i);
            if (dialog != NULL) {
                *dialog = override->original;
            }
        }
        free(override->text);
        memset(override, 0, sizeof(*override));
    }
}

// Resolves extended model IDs into vanilla graph-node fallback models on Wii U.
static bool smlua_resolve_model_fallback(s32 logical_model_id, s32 *out_fallback) {
    s32 fallback;
    switch (logical_model_id) {
        case SMLUA_E_MODEL_MARIO: fallback = MODEL_MARIO; break;
        case SMLUA_E_MODEL_MARIOS_CAP: fallback = MODEL_MARIOS_CAP; break;
        case SMLUA_E_MODEL_MARIOS_WING_CAP: fallback = MODEL_MARIOS_WING_CAP; break;
        case SMLUA_E_MODEL_MARIOS_METAL_CAP: fallback = MODEL_MARIOS_METAL_CAP; break;
        case SMLUA_E_MODEL_MARIOS_WINGED_METAL_CAP: fallback = MODEL_MARIOS_WINGED_METAL_CAP; break;
        case SMLUA_E_MODEL_LUIGI: fallback = MODEL_LUIGI; break;
        case SMLUA_E_MODEL_LUIGIS_CAP: fallback = MODEL_MARIOS_CAP; break;
        case SMLUA_E_MODEL_LUIGIS_WING_CAP: fallback = MODEL_MARIOS_WING_CAP; break;
        case SMLUA_E_MODEL_LUIGIS_METAL_CAP: fallback = MODEL_MARIOS_METAL_CAP; break;
        case SMLUA_E_MODEL_LUIGIS_WINGED_METAL_CAP: fallback = MODEL_MARIOS_WINGED_METAL_CAP; break;
        case SMLUA_E_MODEL_TOAD_PLAYER: fallback = MODEL_TOAD; break;
        case SMLUA_E_MODEL_TOADS_CAP: fallback = MODEL_MARIOS_CAP; break;
        case SMLUA_E_MODEL_TOADS_WING_CAP: fallback = MODEL_MARIOS_WING_CAP; break;
        case SMLUA_E_MODEL_TOADS_METAL_CAP: fallback = MODEL_MARIOS_METAL_CAP; break;
        case SMLUA_E_MODEL_WALUIGI: fallback = MODEL_MARIO; break;
        case SMLUA_E_MODEL_WALUIGIS_CAP: fallback = MODEL_MARIOS_CAP; break;
        case SMLUA_E_MODEL_WALUIGIS_WING_CAP: fallback = MODEL_MARIOS_WING_CAP; break;
        case SMLUA_E_MODEL_WALUIGIS_METAL_CAP: fallback = MODEL_MARIOS_METAL_CAP; break;
        case SMLUA_E_MODEL_WALUIGIS_WINGED_METAL_CAP: fallback = MODEL_MARIOS_WINGED_METAL_CAP; break;
        case SMLUA_E_MODEL_WARIO: fallback = MODEL_MARIO; break;
        case SMLUA_E_MODEL_WARIOS_CAP: fallback = MODEL_MARIOS_CAP; break;
        case SMLUA_E_MODEL_WARIOS_WING_CAP: fallback = MODEL_MARIOS_WING_CAP; break;
        case SMLUA_E_MODEL_WARIOS_METAL_CAP: fallback = MODEL_MARIOS_METAL_CAP; break;
        case SMLUA_E_MODEL_WARIOS_WINGED_METAL_CAP: fallback = MODEL_MARIOS_WINGED_METAL_CAP; break;
        case SMLUA_E_MODEL_VL_TONE_LUIGI: fallback = MODEL_LUIGI; break;
        case SMLUA_E_MODEL_CJ_LUIGI: fallback = MODEL_LUIGI; break;
        case SMLUA_E_MODEL_DJOSLIN_TOAD: fallback = MODEL_TOAD; break;
        case SMLUA_E_MODEL_FLUFFA_WARIO: fallback = MODEL_MARIO; break;
        case SMLUA_E_MODEL_KEEB_WALUIGI: fallback = MODEL_MARIO; break;
        case SMLUA_E_MODEL_FLUFFA_WALUIGI: fallback = MODEL_MARIO; break;
        default:
            if (logical_model_id >= 0 && logical_model_id < 0x100) {
                fallback = logical_model_id;
            } else {
                return false;
            }
            break;
    }

    if (fallback < 0 || fallback >= 0x100) {
        return false;
    }
    if (out_fallback != NULL) {
        *out_fallback = fallback;
    }
    return true;
}

// Finds or creates a tracking entry for object logical-model state.
static struct SmluaModelOverride *smlua_get_model_override_slot(struct Object *obj, bool create) {
    struct SmluaModelOverride *empty = NULL;

    if (obj == NULL) {
        return NULL;
    }

    for (u32 i = 0; i < ARRAY_COUNT(sLuaModelOverrides); i++) {
        struct SmluaModelOverride *entry = &sLuaModelOverrides[i];
        if (entry->obj == obj) {
            return entry;
        }
        if (empty == NULL && entry->obj == NULL) {
            empty = entry;
        }
    }

    return create ? empty : NULL;
}

// Sets (or clears) the logical model mapping for one object pointer.
static void smlua_set_object_logical_model(struct Object *obj, s32 logical_model_id) {
    struct SmluaModelOverride *entry = smlua_get_model_override_slot(obj, logical_model_id >= 0);
    if (entry == NULL) {
        return;
    }

    if (logical_model_id >= 0) {
        entry->obj = obj;
        entry->logical_model_id = logical_model_id;
    } else {
        entry->obj = NULL;
        entry->logical_model_id = -1;
    }
}

// Resolves the active logical model ID for an object, clearing stale caches.
static s32 smlua_get_object_logical_model(struct Object *obj) {
    struct SmluaModelOverride *entry;

    if (obj == NULL) {
        return -1;
    }

    entry = smlua_get_model_override_slot(obj, false);
    if (entry != NULL) {
        s32 fallback;
        if (smlua_resolve_model_fallback(entry->logical_model_id, &fallback) &&
            obj->header.gfx.sharedChild == gLoadedGraphNodes[fallback]) {
            return entry->logical_model_id;
        }
        entry->obj = NULL;
        entry->logical_model_id = -1;
    }

    for (s32 i = 0; i < 0x100; i++) {
        if (obj->header.gfx.sharedChild == gLoadedGraphNodes[i]) {
            return i;
        }
    }

    return -1;
}

// Binds an integer constant into Lua's global table.
static void smlua_set_global_integer(lua_State *L, const char *name, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setglobal(L, name);
}

// Binds a C function into Lua's global table.
static void smlua_set_global_function(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setglobal(L, name);
}

// Gets parent directory of a script path.
static bool smlua_get_script_directory(char *out_dir, size_t out_size, const char *script_path) {
    const char *last_sep;
    size_t len;

    if (out_dir == NULL || out_size == 0 || script_path == NULL) {
        return false;
    }
    last_sep = strrchr(script_path, '/');
    if (last_sep == NULL) {
        return false;
    }
    len = (size_t)(last_sep - script_path);
    if (len + 1 > out_size) {
        return false;
    }
    memcpy(out_dir, script_path, len);
    out_dir[len] = '\0';
    return true;
}

// Returns true when a virtual path exists in the mounted Wii U filesystem view.
static bool smlua_vfs_file_exists(const char *path) {
    fs_file_t *file;

    if (path == NULL || path[0] == '\0') {
        return false;
    }

    file = fs_open(path);
    if (file == NULL) {
        return false;
    }
    fs_close(file);
    return true;
}

// Resolves a mod-local asset path relative to the calling script and sibling active mods.
static bool smlua_resolve_mod_asset_path(char *out_path, size_t out_size, const char *script_path,
                                         const char *filename, const char *subdir, const char *ext) {
    char script_dir[SYS_MAX_PATH];
    char candidate[SYS_MAX_PATH];
    size_t script_count;

    if (out_path == NULL || out_size == 0 || filename == NULL || filename[0] == '\0') {
        return false;
    }

    if (script_path != NULL && smlua_get_script_directory(script_dir, sizeof(script_dir), script_path)) {
        if (subdir != NULL && ext != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s/%s%s", script_dir, subdir, filename, ext) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (subdir != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s/%s", script_dir, subdir, filename) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (ext != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s%s", script_dir, filename, ext) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (snprintf(candidate, sizeof(candidate), "%s/%s", script_dir, filename) > 0 &&
            smlua_vfs_file_exists(candidate)) {
            snprintf(out_path, out_size, "%s", candidate);
            return true;
        }
    }

    script_count = mods_get_active_script_count();
    for (size_t i = 0; i < script_count; i++) {
        const char *root_script = mods_get_active_script_path(i);
        if (root_script == NULL || !smlua_get_script_directory(script_dir, sizeof(script_dir), root_script)) {
            continue;
        }
        if (subdir != NULL && ext != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s/%s%s", script_dir, subdir, filename, ext) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (subdir != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s/%s", script_dir, subdir, filename) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (ext != NULL) {
            if (snprintf(candidate, sizeof(candidate), "%s/%s%s", script_dir, filename, ext) > 0 &&
                smlua_vfs_file_exists(candidate)) {
                snprintf(out_path, out_size, "%s", candidate);
                return true;
            }
        }
        if (snprintf(candidate, sizeof(candidate), "%s/%s", script_dir, filename) > 0 &&
            smlua_vfs_file_exists(candidate)) {
            snprintf(out_path, out_size, "%s", candidate);
            return true;
        }
    }

    return false;
}

// Reads the first PNG header embedded in a .tex blob to recover texture dimensions.
static bool smlua_read_texture_dimensions(const char *path, s32 *out_width, s32 *out_height) {
    static const u8 kPngSig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    u8 buffer[4096];
    fs_file_t *file;
    int64_t bytes_read;

    if (path == NULL || out_width == NULL || out_height == NULL) {
        return false;
    }

    file = fs_open(path);
    if (file == NULL) {
        return false;
    }

    bytes_read = fs_read(file, buffer, sizeof(buffer));
    fs_close(file);
    if (bytes_read < 32) {
        return false;
    }

    for (int64_t i = 0; i <= bytes_read - 24; i++) {
        if (memcmp(&buffer[i], kPngSig, sizeof(kPngSig)) == 0) {
            // PNG IHDR stores width/height as big-endian u32.
            *out_width = (s32)((buffer[i + 16] << 24) | (buffer[i + 17] << 16) |
                               (buffer[i + 18] << 8) | buffer[i + 19]);
            *out_height = (s32)((buffer[i + 20] << 24) | (buffer[i + 21] << 16) |
                                (buffer[i + 22] << 8) | buffer[i + 23]);
            if (*out_width > 0 && *out_height > 0) {
                return true;
            }
            return false;
        }
    }

    return false;
}

// Returns a stable ModAudio record allocated from Lua runtime pool.
static struct SmluaModAudio *smlua_alloc_mod_audio(const char *filepath, bool is_stream) {
    struct SmluaModAudio *audio;

    if (sLuaAudioPoolCount >= SMLUA_MAX_MOD_AUDIO) {
        return NULL;
    }

    audio = &sLuaAudioPool[sLuaAudioPoolCount++];
    memset(audio, 0, sizeof(*audio));
    audio->id = sLuaAudioPoolCount;
    audio->is_stream = is_stream;
    audio->volume = 1.0f;
    audio->frequency = 1.0f;
    if (filepath != NULL) {
        snprintf(audio->filepath, sizeof(audio->filepath), "%s", filepath);
    }
    return audio;
}

// Pushes TextureInfo userdata compatible with Co-op DX scripts.
static void smlua_push_texture_info(lua_State *L, const char *name, const char *path, s32 width, s32 height) {
    struct SmluaTextureInfo *tex = lua_newuserdata(L, sizeof(struct SmluaTextureInfo));
    memset(tex, 0, sizeof(*tex));
    if (name != NULL) {
        snprintf(tex->name, sizeof(tex->name), "%s", name);
    }
    if (path != NULL) {
        snprintf(tex->path, sizeof(tex->path), "%s", path);
    }
    tex->width = width;
    tex->height = height;
    luaL_getmetatable(L, SMLUA_TEXINFO_METATABLE);
    lua_setmetatable(L, -2);
}

// Pushes ModAudio userdata used by voice/music helper wrappers.
static void smlua_push_mod_audio(lua_State *L, struct SmluaModAudio *audio) {
    struct SmluaModAudio **userdata = lua_newuserdata(L, sizeof(struct SmluaModAudio *));
    *userdata = audio;
    luaL_getmetatable(L, SMLUA_MODAUDIO_METATABLE);
    lua_setmetatable(L, -2);
}

// Converts Lua userdata to ModAudio pointer.
static struct SmluaModAudio *smlua_check_mod_audio(lua_State *L, int index) {
    struct SmluaModAudio **userdata = luaL_testudata(L, index, SMLUA_MODAUDIO_METATABLE);
    return userdata != NULL ? *userdata : NULL;
}

// Lua __index for TextureInfo userdata.
static int smlua_texinfo_index(lua_State *L) {
    struct SmluaTextureInfo *tex = luaL_checkudata(L, 1, SMLUA_TEXINFO_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "name") == 0) {
        lua_pushstring(L, tex->name);
        return 1;
    }
    if (strcmp(key, "path") == 0 || strcmp(key, "filepath") == 0) {
        lua_pushstring(L, tex->path);
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, tex->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, tex->height);
        return 1;
    }
    if (strcmp(key, "_pointer") == 0) {
        lua_pushlightuserdata(L, tex);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// Lua __tostring for TextureInfo userdata.
static int smlua_texinfo_tostring(lua_State *L) {
    struct SmluaTextureInfo *tex = luaL_checkudata(L, 1, SMLUA_TEXINFO_METATABLE);
    lua_pushfstring(L, "TextureInfo<%s>(%dx%d)", tex->name, tex->width, tex->height);
    return 1;
}

// Lua __index for ModAudio userdata.
static int smlua_modaudio_index(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (audio == NULL) {
        lua_pushnil(L);
        return 1;
    }
    if (strcmp(key, "filepath") == 0 || strcmp(key, "path") == 0) {
        lua_pushstring(L, audio->filepath);
        return 1;
    }
    if (strcmp(key, "_pointer") == 0) {
        lua_pushlightuserdata(L, audio);
        return 1;
    }
    if (strcmp(key, "id") == 0) {
        lua_pushinteger(L, audio->id);
        return 1;
    }
    if (strcmp(key, "volume") == 0) {
        lua_pushnumber(L, audio->volume);
        return 1;
    }
    if (strcmp(key, "frequency") == 0) {
        lua_pushnumber(L, audio->frequency);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// Lua __newindex for ModAudio userdata.
static int smlua_modaudio_newindex(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    const char *key = luaL_checkstring(L, 2);
    if (audio == NULL) {
        return 0;
    }
    if (strcmp(key, "volume") == 0) {
        audio->volume = (f32)luaL_checknumber(L, 3);
        return 0;
    }
    if (strcmp(key, "frequency") == 0) {
        audio->frequency = (f32)luaL_checknumber(L, 3);
        return 0;
    }
    return 0;
}

// Pushes a GraphNodeRoot reference userdata for geo viewport hooks.
static void smlua_push_graph_root(lua_State *L, struct GraphNodeRoot *root) {
    struct SmluaGraphRootRef *ref = lua_newuserdata(L, sizeof(struct SmluaGraphRootRef));
    ref->root = root;
    luaL_getmetatable(L, SMLUA_GRAPH_ROOT_METATABLE);
    lua_setmetatable(L, -2);
}

// Lua __index for GraphNodeRoot proxy used by `geo_get_current_root()`.
static int smlua_graph_root_index(lua_State *L) {
    struct SmluaGraphRootRef *ref = luaL_checkudata(L, 1, SMLUA_GRAPH_ROOT_METATABLE);
    const char *key = luaL_checkstring(L, 2);

    if (ref->root == NULL) {
        lua_pushnil(L);
        return 1;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushinteger(L, ref->root->x);
        return 1;
    }
    if (strcmp(key, "y") == 0) {
        lua_pushinteger(L, ref->root->y);
        return 1;
    }
    if (strcmp(key, "width") == 0) {
        lua_pushinteger(L, ref->root->width);
        return 1;
    }
    if (strcmp(key, "height") == 0) {
        lua_pushinteger(L, ref->root->height);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// Lua __newindex for GraphNodeRoot proxy used by viewport override hooks.
static int smlua_graph_root_newindex(lua_State *L) {
    struct SmluaGraphRootRef *ref = luaL_checkudata(L, 1, SMLUA_GRAPH_ROOT_METATABLE);
    const char *key = luaL_checkstring(L, 2);
    s32 value = (s32)luaL_checkinteger(L, 3);

    if (ref->root == NULL) {
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        ref->root->x = (s16)value;
    } else if (strcmp(key, "y") == 0) {
        ref->root->y = (s16)value;
    } else if (strcmp(key, "width") == 0) {
        ref->root->width = (s16)value;
    } else if (strcmp(key, "height") == 0) {
        ref->root->height = (s16)value;
    }

    return 0;
}

// Registers metatables for lightweight userdata types used by compatibility APIs.
static void smlua_bind_compat_metatables(lua_State *L) {
    if (luaL_newmetatable(L, SMLUA_TEXINFO_METATABLE)) {
        lua_pushcfunction(L, smlua_texinfo_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_texinfo_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_MODAUDIO_METATABLE)) {
        lua_pushcfunction(L, smlua_modaudio_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_modaudio_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, SMLUA_GRAPH_ROOT_METATABLE)) {
        lua_pushcfunction(L, smlua_graph_root_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, smlua_graph_root_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);
}

// Creates table[key] = {} when missing and leaves the table at stack top.
static void smlua_ensure_table_entry(lua_State *L, int table_index, lua_Integer key) {
    int abs = lua_absindex(L, table_index);
    lua_pushinteger(L, key);
    lua_gettable(L, abs);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushinteger(L, key);
        lua_pushvalue(L, -2);
        lua_settable(L, abs);
    }
}

// Reads Lua MAX_PLAYERS with defensive bounds for single-player table shims.
static int smlua_get_lua_max_players(lua_State *L) {
    int max_players = 1;

    if (L == NULL) {
        return max_players;
    }

    lua_getglobal(L, "MAX_PLAYERS");
    if (lua_isinteger(L, -1)) {
        lua_Integer value = lua_tointeger(L, -1);
        // Co-op DX commonly uses up to 16 players; allow a modest upper bound
        // so future constants changes do not cause unbounded table growth.
        if (value >= 1 && value <= 32) {
            max_players = (int)value;
        }
    }
    lua_pop(L, 1);

    return max_players;
}

// Converts a Lua argument into MarioState userdata when possible.
static struct MarioState *smlua_to_mario_state_arg(lua_State *L, int index) {
    SmluaCObject *cobj = luaL_testudata(L, index, SMLUA_COBJECT_METATABLE);
    if (cobj != NULL && cobj->type == SMLUA_COBJECT_MARIO_STATE) {
        return (struct MarioState *)cobj->pointer;
    }
    return gMarioState;
}

// Sanitizes a script path into a filesystem-safe mod-storage filename token.
static void smlua_sanitize_filename_component(char *dst, size_t dst_size, const char *src) {
    size_t pos = 0;
    if (dst_size == 0) {
        return;
    }

    if (src == NULL || src[0] == '\0') {
        src = "global";
    }

    while (*src != '\0' && pos + 1 < dst_size) {
        char c = *src++;
        if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            dst[pos++] = c;
        } else {
            dst[pos++] = '_';
        }
    }

    if (pos == 0) {
        dst[pos++] = 'g';
    }
    dst[pos] = '\0';
}

// Computes a script-scoped writable storage file under the FS write path.
static const char *smlua_get_mod_storage_file_path(const char *script_path) {
    static char out_path[SYS_MAX_PATH];
    char safe_name[SYS_MAX_PATH];
    char relative_path[SYS_MAX_PATH];
    const char *full_path;

    smlua_sanitize_filename_component(safe_name, sizeof(safe_name), script_path);
    if (snprintf(relative_path, sizeof(relative_path), "sav/%s.sav", safe_name) < 0) {
        return NULL;
    }

    full_path = fs_get_write_path(relative_path);
    if (full_path == NULL) {
        return NULL;
    }

    snprintf(out_path, sizeof(out_path), "%s", full_path);
    return out_path;
}

// Resolves caller chunk path so mod storage writes stay scoped per Lua script.
static const char *smlua_get_caller_script_path(lua_State *L) {
    lua_Debug ar;
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "S", &ar) && ar.source != NULL && ar.source[0] == '@') {
        return ar.source + 1;
    }
    return sActiveScriptPath;
}

// Builds `<script_dir>/<relative>` into `out_path`.
static bool smlua_build_script_relative_path(char *out_path, size_t out_size,
                                             const char *script_path, const char *relative) {
    const char *last_sep;
    size_t base_len;

    if (out_path == NULL || out_size == 0 || script_path == NULL || relative == NULL) {
        return false;
    }

    last_sep = strrchr(script_path, '/');
    if (last_sep == NULL) {
        return false;
    }

    base_len = (size_t)(last_sep - script_path);
    if (base_len + 1 + strlen(relative) + 1 > out_size) {
        return false;
    }

    memcpy(out_path, script_path, base_len);
    out_path[base_len] = '/';
    strcpy(out_path + base_len + 1, relative);
    return true;
}

// Builds a best-effort script-local asset path without filesystem probing.
static bool smlua_build_script_asset_hint(char *out_path, size_t out_size,
                                          const char *script_path, const char *subdir,
                                          const char *filename) {
    const char *last_sep;
    size_t base_len;

    if (out_path == NULL || out_size == 0 || script_path == NULL || filename == NULL) {
        return false;
    }

    last_sep = strrchr(script_path, '/');
    if (last_sep == NULL) {
        return false;
    }

    base_len = (size_t)(last_sep - script_path);
    if (subdir != NULL && subdir[0] != '\0') {
        if (base_len + 1 + strlen(subdir) + 1 + strlen(filename) + 1 > out_size) {
            return false;
        }
        memcpy(out_path, script_path, base_len);
        out_path[base_len] = '/';
        strcpy(out_path + base_len + 1, subdir);
        strcat(out_path, "/");
        strcat(out_path, filename);
        return true;
    }

    if (base_len + 1 + strlen(filename) + 1 > out_size) {
        return false;
    }
    memcpy(out_path, script_path, base_len);
    out_path[base_len] = '/';
    strcpy(out_path + base_len + 1, filename);
    return true;
}

// Loads a full file into heap memory for sequence override patching.
static bool smlua_load_binary_file(const char *path, u8 **out_data, s32 *out_len) {
    fs_file_t *file;
    int64_t length;
    u8 *buffer;

    if (path == NULL || out_data == NULL || out_len == NULL) {
        return false;
    }

    file = fs_open(path);
    if (file == NULL) {
        return false;
    }

    length = fs_size(file);
    if (length <= 0 || length > INT32_MAX) {
        fs_close(file);
        return false;
    }

    buffer = malloc((size_t)length);
    if (buffer == NULL) {
        fs_close(file);
        return false;
    }

    if (fs_read(file, buffer, (uint64_t)length) != length) {
        free(buffer);
        fs_close(file);
        return false;
    }

    fs_close(file);
    *out_data = buffer;
    *out_len = (s32)length;
    return true;
}

// Applies a runtime sequence-data override for a target vanilla sequence slot.
static bool smlua_apply_sequence_override(u8 target_seq, u8 *data, s32 len) {
    struct SmluaSequenceOverride *override;

    if (data == NULL || len <= 0 || gSeqFileHeader == NULL || target_seq >= gSeqFileHeader->seqCount) {
        return false;
    }

    override = &sLuaSequenceOverrides[target_seq];
    if (!override->has_original) {
        override->original_data = gSeqFileHeader->seqArray[target_seq].offset;
        override->original_len = gSeqFileHeader->seqArray[target_seq].len;
        override->has_original = true;
    }

    if (override->patched_data != NULL) {
        free(override->patched_data);
        override->patched_data = NULL;
    }

    override->patched_data = data;
    override->patched_len = len;
    override->active = true;
    gSeqFileHeader->seqArray[target_seq].offset = data;
    gSeqFileHeader->seqArray[target_seq].len = len;
    return true;
}

// Clamps and rounds a Lua numeric color channel into an 8-bit render value.
static u8 smlua_to_color_channel(lua_State *L, int index) {
    int value = (int)(luaL_checknumber(L, index) + 0.5f);
    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }
    return (u8)value;
}

// Converts script string tokens to nearest vanilla HUD glyph equivalents.
static char smlua_hud_normalize_char(char c) {
    if (c == '@') {
        return '*';
    }
    return c;
}

// Draws text immediately using vanilla HUD glyph renderer for hook-layer ordering.
static void smlua_hud_draw_text(const char *message, s32 x, s32 y) {
    int i = 0;

    if (message == NULL || message[0] == '\0') {
        return;
    }

    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    gDPSetEnvColor(gDisplayListHead++, sHudColor.r, sHudColor.g, sHudColor.b, sHudColor.a);

    while (message[i] != '\0') {
        s8 glyph = char_to_glyph_index(smlua_hud_normalize_char(message[i]));
        if (glyph != GLYPH_SPACE) {
            add_glyph_texture(glyph);
            render_textrect(x, y, i);
        }
        i++;
    }

    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

// Ensures the save folder used by mod storage exists before file writes.
static bool smlua_ensure_mod_storage_dir(void) {
    const char *dir = fs_get_write_path("sav");
    if (dir == NULL) {
        return false;
    }
    if (fs_sys_dir_exists(dir)) {
        return true;
    }
    return fs_sys_mkdir(dir);
}

// Finds an entry by key in an in-memory storage set.
static ssize_t smlua_mod_storage_find_entry(const struct SmluaModStorageEntry *entries, size_t count,
                                            const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            return (ssize_t)i;
        }
    }
    return -1;
}

// Reads key/value pairs from a mod storage file into an in-memory table.
static bool smlua_mod_storage_read_entries(const char *filename, struct SmluaModStorageEntry *entries,
                                           size_t *count) {
    FILE *file;
    char line[SMLUA_MOD_STORAGE_LINE_SIZE];

    *count = 0;
    if (filename == NULL) {
        return false;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return true;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *separator;
        char *newline;
        const char *key;
        const char *value;
        size_t key_len;
        size_t value_len;
        ssize_t existing;

        newline = strpbrk(line, "\r\n");
        if (newline != NULL) {
            *newline = '\0';
        }

        separator = strchr(line, '=');
        if (separator == NULL) {
            continue;
        }
        *separator = '\0';

        key = line;
        value = separator + 1;
        key_len = strlen(key);
        value_len = strlen(value);
        if (key_len == 0 || key_len > SMLUA_MOD_STORAGE_MAX_KEY || value_len > SMLUA_MOD_STORAGE_MAX_VALUE) {
            continue;
        }

        existing = smlua_mod_storage_find_entry(entries, *count, key);
        if (existing >= 0) {
            snprintf(entries[existing].value, sizeof(entries[existing].value), "%s", value);
            continue;
        }
        if (*count >= SMLUA_MOD_STORAGE_MAX_KEYS) {
            continue;
        }

        snprintf(entries[*count].key, sizeof(entries[*count].key), "%s", key);
        snprintf(entries[*count].value, sizeof(entries[*count].value), "%s", value);
        (*count)++;
    }

    fclose(file);
    return true;
}

// Persists an in-memory storage set back to a mod storage file.
static bool smlua_mod_storage_write_entries(const char *filename, const struct SmluaModStorageEntry *entries,
                                            size_t count) {
    FILE *file;

    if (filename == NULL) {
        return false;
    }

    file = fopen(filename, "wb");
    if (file == NULL) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        if (fprintf(file, "%s=%s\n", entries[i].key, entries[i].value) < 0) {
            fclose(file);
            return false;
        }
    }

    fclose(file);
    return true;
}

// Co-op DX compat wrapper for mod_storage_load(key) using per-script save files.
static int smlua_func_mod_storage_load(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    const char *filename;
    struct SmluaModStorageEntry entries[SMLUA_MOD_STORAGE_MAX_KEYS];
    size_t count = 0;
    ssize_t index;

    if (key == NULL || key[0] == '\0' || strlen(key) > SMLUA_MOD_STORAGE_MAX_KEY) {
        lua_pushnil(L);
        return 1;
    }

    filename = smlua_get_mod_storage_file_path(script_path);
    if (filename == NULL || !smlua_mod_storage_read_entries(filename, entries, &count)) {
        lua_pushnil(L);
        return 1;
    }

    index = smlua_mod_storage_find_entry(entries, count, key);
    if (index < 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, entries[index].value);
    return 1;
}

// Co-op DX compat wrapper for mod_storage_save(key, value) with immediate flush.
static int smlua_func_mod_storage_save(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *value = luaL_checkstring(L, 2);
    const char *script_path = smlua_get_caller_script_path(L);
    const char *filename;
    struct SmluaModStorageEntry entries[SMLUA_MOD_STORAGE_MAX_KEYS];
    size_t count = 0;
    ssize_t index;
    bool success = false;

    if (key == NULL || value == NULL || key[0] == '\0' || strlen(key) > SMLUA_MOD_STORAGE_MAX_KEY ||
        strlen(value) > SMLUA_MOD_STORAGE_MAX_VALUE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!smlua_ensure_mod_storage_dir()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    filename = smlua_get_mod_storage_file_path(script_path);
    if (filename != NULL) {
        if (smlua_mod_storage_read_entries(filename, entries, &count)) {
            index = smlua_mod_storage_find_entry(entries, count, key);
            if (index < 0 && count < SMLUA_MOD_STORAGE_MAX_KEYS) {
                index = (ssize_t)count;
                count++;
            }
            if (index >= 0) {
                snprintf(entries[index].key, sizeof(entries[index].key), "%s", key);
                snprintf(entries[index].value, sizeof(entries[index].value), "%s", value);
                success = smlua_mod_storage_write_entries(filename, entries, count);
            }
        }
    }

    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

// Removes a key from script-scoped mod storage file.
static int smlua_func_mod_storage_remove(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    const char *filename = smlua_get_mod_storage_file_path(script_path);
    struct SmluaModStorageEntry entries[SMLUA_MOD_STORAGE_MAX_KEYS];
    size_t count = 0;
    ssize_t index;

    if (key == NULL || key[0] == '\0' || filename == NULL) {
        lua_pushboolean(L, 0);
        return 1;
    }
    if (!smlua_mod_storage_read_entries(filename, entries, &count)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    index = smlua_mod_storage_find_entry(entries, count, key);
    if (index < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    for (size_t i = (size_t)index; i + 1 < count; i++) {
        entries[i] = entries[i + 1];
    }
    count--;

    lua_pushboolean(L, smlua_mod_storage_write_entries(filename, entries, count) ? 1 : 0);
    return 1;
}

// Typed wrapper for Co-op DX `mod_storage_load_number`.
static int smlua_func_mod_storage_load_number(lua_State *L) {
    char *end_ptr = NULL;
    double value = 0.0;

    lua_settop(L, 1);
    smlua_func_mod_storage_load(L);
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        lua_pushnumber(L, 0.0);
        return 1;
    }

    value = strtod(lua_tostring(L, -1), &end_ptr);
    lua_pop(L, 1);
    if (end_ptr == NULL || *end_ptr != '\0') {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    lua_pushnumber(L, value);
    return 1;
}

// Typed wrapper for Co-op DX `mod_storage_load_bool`.
static int smlua_func_mod_storage_load_bool(lua_State *L) {
    lua_settop(L, 1);
    smlua_func_mod_storage_load(L);
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, strcmp(lua_tostring(L, -1), "true") == 0 ? 1 : 0);
    lua_remove(L, -2);
    return 1;
}

// Co-op DX helper used by day-night-cycle: defaults missing values to true.
static int smlua_func_mod_storage_load_bool_2(lua_State *L) {
    lua_settop(L, 1);
    smlua_func_mod_storage_load(L);
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_pushboolean(L, strcmp(lua_tostring(L, -1), "true") == 0 ? 1 : 0);
    lua_remove(L, -2);
    return 1;
}

// Typed wrapper for Co-op DX `mod_storage_save_number`.
static int smlua_func_mod_storage_save_number(lua_State *L) {
    luaL_checkstring(L, 1);
    double value = luaL_checknumber(L, 2);
    char value_str[64];

    if ((double)((s64)value) == value) {
        snprintf(value_str, sizeof(value_str), "%lld", (long long)value);
    } else {
        snprintf(value_str, sizeof(value_str), "%.6f", value);
    }

    lua_settop(L, 1);
    lua_pushstring(L, value_str);
    return smlua_func_mod_storage_save(L);
}

// Typed wrapper for Co-op DX `mod_storage_save_bool`.
static int smlua_func_mod_storage_save_bool(lua_State *L) {
    luaL_checkstring(L, 1);
    int value = lua_toboolean(L, 2);
    lua_settop(L, 1);
    lua_pushstring(L, value ? "true" : "false");
    return smlua_func_mod_storage_save(L);
}

// Returns a HUD value field by Co-op DX enum index for Lua script compatibility.
static int smlua_func_hud_get_value(lua_State *L) {
    s32 type = (s32)luaL_checkinteger(L, 1);
    s32 value = 0;

    switch (type) {
        case SMLUA_HUD_DISPLAY_LIVES:         value = gHudDisplay.lives;  break;
        case SMLUA_HUD_DISPLAY_COINS:         value = gHudDisplay.coins;  break;
        case SMLUA_HUD_DISPLAY_STARS:         value = gHudDisplay.stars;  break;
        case SMLUA_HUD_DISPLAY_WEDGES:        value = gHudDisplay.wedges; break;
        case SMLUA_HUD_DISPLAY_KEYS:          value = gHudDisplay.keys;   break;
        case SMLUA_HUD_DISPLAY_FLAGS:         value = gHudDisplay.flags;  break;
        case SMLUA_HUD_DISPLAY_TIMER:         value = gHudDisplay.timer;  break;
        case SMLUA_HUD_DISPLAY_CAMERA_STATUS: value = 0;                  break;
        default:                              value = 0;                  break;
    }

    lua_pushinteger(L, value);
    return 1;
}

// Writes a HUD value field by Co-op DX enum index for Lua script compatibility.
static int smlua_func_hud_set_value(lua_State *L) {
    s32 type = (s32)luaL_checkinteger(L, 1);
    s32 value = (s32)luaL_checkinteger(L, 2);

    switch (type) {
        case SMLUA_HUD_DISPLAY_LIVES:         gHudDisplay.lives = value;  break;
        case SMLUA_HUD_DISPLAY_COINS:         gHudDisplay.coins = value;  break;
        case SMLUA_HUD_DISPLAY_STARS:         gHudDisplay.stars = value;  break;
        case SMLUA_HUD_DISPLAY_WEDGES:        gHudDisplay.wedges = value; break;
        case SMLUA_HUD_DISPLAY_KEYS:          gHudDisplay.keys = value;   break;
        case SMLUA_HUD_DISPLAY_FLAGS:         gHudDisplay.flags = value;  break;
        case SMLUA_HUD_DISPLAY_TIMER:         gHudDisplay.timer = (u16)value; break;
        case SMLUA_HUD_DISPLAY_CAMERA_STATUS: break;
        default:                              break;
    }

    return 0;
}

// Returns whether the vanilla HUD is fully hidden.
static int smlua_func_hud_is_hidden(lua_State *L) {
    (void)L;
    lua_pushboolean(L, gHudDisplay.flags == HUD_DISPLAY_NONE ? 1 : 0);
    return 1;
}

// Returns false because the DJUI pause menu subsystem is not imported yet.
static int smlua_func_djui_hud_is_pause_menu_created(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

// Stores current HUD color state so mods can read/write without DJUI renderer.
static int smlua_func_djui_hud_set_color(lua_State *L) {
    sHudColor.r = smlua_to_color_channel(L, 1);
    sHudColor.g = smlua_to_color_channel(L, 2);
    sHudColor.b = smlua_to_color_channel(L, 3);
    sHudColor.a = smlua_to_color_channel(L, 4);
    return 0;
}

// Returns current HUD color as a DJUI-style table.
static int smlua_func_djui_hud_get_color(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, sHudColor.r);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, sHudColor.g);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, sHudColor.b);
    lua_setfield(L, -2, "b");
    lua_pushinteger(L, sHudColor.a);
    lua_setfield(L, -2, "a");
    return 1;
}

// Estimates DJUI text width using vanilla HUD glyph spacing.
static int smlua_func_djui_hud_measure_text(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    size_t len = (message == NULL) ? 0 : strlen(message);
    lua_pushnumber(L, (lua_Number)(len * 12));
    return 1;
}

// DJUI text shim mapped to vanilla HUD glyph renderer for immediate hook drawing.
static int smlua_func_djui_hud_print_text(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    f32 x = (f32)luaL_checknumber(L, 2);
    f32 y = (f32)luaL_checknumber(L, 3);
    f32 scale = (f32)luaL_optnumber(L, 4, 1.0f);

    // Vanilla HUD path is fixed-size; keep coarse readability by snapping coordinates.
    if (scale > 0.0f && scale < 1.0f) {
        x += 2.0f;
    }
    smlua_hud_draw_text(message, (s32)x, (s32)y);
    return 0;
}

// DJUI texture shim that currently supports HUD-star texture usage from built-in mods.
static int smlua_func_djui_hud_render_texture(lua_State *L) {
    f32 x = (f32)luaL_checknumber(L, 2);
    f32 y = (f32)luaL_checknumber(L, 3);
    (void)luaL_optnumber(L, 4, 1.0f);
    (void)luaL_optnumber(L, 5, 1.0f);

    u8 *(*hud_lut)[58] = segmented_to_virtual(&main_hud_lut);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    gDPSetEnvColor(gDisplayListHead++, sHudColor.r, sHudColor.g, sHudColor.b, sHudColor.a);
    render_hud_tex_lut((s32)x, (s32)y, (*hud_lut)[GLYPH_STAR]);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
    return 0;
}

// Tracks the logical DJUI resolution requested by Lua for width/height helpers.
static int smlua_func_djui_hud_set_resolution(lua_State *L) {
    sHudResolution = (int)luaL_checkinteger(L, 1);
    return 0;
}

// Tracks the logical DJUI font requested by Lua; rendering is currently stubbed.
static int smlua_func_djui_hud_set_font(lua_State *L) {
    sHudFont = (int)luaL_checkinteger(L, 1);
    (void)sHudFont;
    return 0;
}

// Returns the active HUD logical width in pixels for mods that do layout math.
static int smlua_func_djui_hud_get_screen_width(lua_State *L) {
    s32 width = SCREEN_WIDTH;
    if (sHudResolution == SMLUA_RESOLUTION_N64) {
        width = SCREEN_WIDTH;
    } else {
        width = GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(0) - GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(0);
    }
    lua_pushinteger(L, width);
    return 1;
}

// Returns the active HUD logical height in pixels for mods that do layout math.
static int smlua_func_djui_hud_get_screen_height(lua_State *L) {
    (void)L;
    lua_pushinteger(L, SCREEN_HEIGHT);
    return 1;
}

// Mouse wheel shim for mods that support desktop scroll gestures.
static int smlua_func_djui_hud_get_mouse_scroll_y(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

// Pushes a color table with RGBA keys for DJUI-style theme access.
static void smlua_push_rgba_table(lua_State *L, s32 r, s32 g, s32 b, s32 a) {
    lua_newtable(L);
    lua_pushinteger(L, r);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, g);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, b);
    lua_setfield(L, -2, "b");
    lua_pushinteger(L, a);
    lua_setfield(L, -2, "a");
}

// Returns a compact default theme table expected by character-select HUD scripts.
static int smlua_func_djui_menu_get_theme(lua_State *L) {
    lua_newtable(L);

    lua_newtable(L);
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "hudFontHeader");
    lua_setfield(L, -2, "panels");

    lua_newtable(L);
    smlua_push_rgba_table(L, 16, 16, 16, 128);
    lua_setfield(L, -2, "rectColor");
    smlua_push_rgba_table(L, 224, 224, 224, 255);
    lua_setfield(L, -2, "borderColor");
    lua_setfield(L, -2, "threePanels");

    return 1;
}

// Returns active menu font fallback used by scripts when DJUI menu is unavailable.
static int smlua_func_djui_menu_get_font(lua_State *L) {
    (void)L;
    lua_pushinteger(L, sHudFont);
    return 1;
}

// Returns formatted color tags for rainbow text helper effects.
static int smlua_func_djui_menu_get_rainbow_string_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    const char *tag = "\\#ffffff\\";

    switch (index) {
        case 0: tag = "\\#ff6060\\"; break;
        case 1: tag = "\\#60ff60\\"; break;
        case 2: tag = "\\#6090ff\\"; break;
        case 3: tag = "\\#ffd040\\"; break;
        default: break;
    }
    lua_pushstring(L, tag);
    return 1;
}

// Lightweight language lookup for built-in HUD labels when DJUI language system is absent.
static int smlua_func_djui_language_get(lua_State *L) {
    const char *section = luaL_checkstring(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (section != NULL && key != NULL) {
        if (strcmp(section, "PLAYER_LIST") == 0 && strcmp(key, "PLAYERS") == 0) {
            lua_pushstring(L, "PLAYERS");
            return 1;
        }
        if (strcmp(section, "MODLIST") == 0 && strcmp(key, "MODS") == 0) {
            lua_pushstring(L, "MODS");
            return 1;
        }
    }

    lua_pushstring(L, key != NULL ? key : "");
    return 1;
}

// Returns whether the local runtime is currently opening the DJUI player-list panel.
static int smlua_func_djui_attempting_to_open_playerlist(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

// Returns behavior script pointer for currently supported Co-op DX behavior IDs.
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
        case SMLUA_BEHAVIOR_ID_DNC_SKYBOX:
        case SMLUA_BEHAVIOR_ID_DNC_NO_SKYBOX:
            // Spawn as stable static objects; Lua loop/init callbacks are driven separately.
            return bhvStaticObject;
        default:
            return NULL;
    }
}

// Resolves custom Lua behavior IDs to marker values for lookup/spawn compatibility.
static u32 smlua_custom_marker_from_behavior_id(s32 behavior_id) {
    switch (behavior_id) {
        case SMLUA_BEHAVIOR_ID_DNC_SKYBOX:
            return SMLUA_CUSTOM_MARKER_DNC_SKYBOX;
        case SMLUA_BEHAVIOR_ID_DNC_NO_SKYBOX:
            return SMLUA_CUSTOM_MARKER_DNC_NO_SKYBOX;
        default:
            return 0;
    }
}

// Maps known behavior pointers to Co-op DX behavior IDs used by scripts.
static s32 smlua_behavior_id_from_ptr(const BehaviorScript *behavior) {
    if (behavior == bhvActSelector) {
        return SMLUA_BEHAVIOR_ID_ACT_SELECTOR;
    }
    if (behavior == bhvActSelectorStarType) {
        return SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE;
    }
    if (behavior == bhvKoopa) {
        return SMLUA_BEHAVIOR_ID_KOOPA;
    }
    if (behavior == bhvMario) {
        return SMLUA_BEHAVIOR_ID_MARIO;
    }
    if (behavior == bhvMetalCap) {
        return SMLUA_BEHAVIOR_ID_METAL_CAP;
    }
    if (behavior == bhvNormalCap) {
        return SMLUA_BEHAVIOR_ID_NORMAL_CAP;
    }
    if (behavior == bhvVanishCap) {
        return SMLUA_BEHAVIOR_ID_VANISH_CAP;
    }
    if (behavior == bhvWingCap) {
        return SMLUA_BEHAVIOR_ID_WING_CAP;
    }
    return -1;
}

// Maps known behavior IDs to human-readable names used by warning logs.
static const char *smlua_behavior_name_from_id(s32 behavior_id) {
    switch (behavior_id) {
        case SMLUA_BEHAVIOR_ID_ACT_SELECTOR:
            return "bhvActSelector";
        case SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE:
            return "bhvActSelectorStarType";
        case SMLUA_BEHAVIOR_ID_KOOPA:
            return "bhvKoopa";
        case SMLUA_BEHAVIOR_ID_MARIO:
            return "bhvMario";
        case SMLUA_BEHAVIOR_ID_METAL_CAP:
            return "bhvMetalCap";
        case SMLUA_BEHAVIOR_ID_NORMAL_CAP:
            return "bhvNormalCap";
        case SMLUA_BEHAVIOR_ID_VANISH_CAP:
            return "bhvVanishCap";
        case SMLUA_BEHAVIOR_ID_WING_CAP:
            return "bhvWingCap";
        case SMLUA_BEHAVIOR_ID_DNC_SKYBOX:
            return "bhvDNCSkybox";
        case SMLUA_BEHAVIOR_ID_DNC_NO_SKYBOX:
            return "bhvDNCNoSkybox";
        default:
            return "unknown";
    }
}

// Finds first active object tagged with a custom Lua marker.
static struct Object *smlua_obj_get_first_with_custom_marker(u32 marker) {
    u32 sanity_depth = 0;

    if (marker == 0 || gObjectLists == NULL) {
        return NULL;
    }

    for (u32 obj_list = 0; obj_list < NUM_OBJ_LISTS; obj_list++) {
        struct Object *head = (struct Object *)&gObjectLists[obj_list];
        struct Object *obj = (struct Object *)head->header.next;
        while (obj != head) {
            if (++sanity_depth > 20000) {
                return NULL;
            }
            if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED && obj->oUnk94 == marker) {
                return obj;
            }
            obj = (struct Object *)obj->header.next;
        }
    }

    return NULL;
}

// Finds the first active object with the specified behavior pointer.
static struct Object *smlua_obj_get_first_with_behavior(const BehaviorScript *behavior) {
    u32 sanity_depth = 0;
    u32 obj_list;
    struct Object *head;
    struct Object *obj;

    if (behavior == NULL || gObjectLists == NULL) {
        return NULL;
    }

    obj_list = get_object_list_from_behavior(behavior);
    if (obj_list >= NUM_OBJ_LISTS) {
        return NULL;
    }

    head = (struct Object *)&gObjectLists[obj_list];
    obj = (struct Object *)head->header.next;
    while (obj != head) {
        if (++sanity_depth > 10000) {
            break;
        }
        if (obj->behavior == behavior && obj->activeFlags != ACTIVE_FLAG_DEACTIVATED) {
            return obj;
        }
        obj = (struct Object *)obj->header.next;
    }

    return NULL;
}

// Co-op DX compat wrapper for object lookup by behavior ID enum value.
static int smlua_func_obj_get_first_with_behavior_id(lua_State *L) {
    s32 behavior_id;
    u32 custom_marker;
    const BehaviorScript *behavior;
    struct Object *obj;

    if (!lua_isinteger(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    behavior_id = (s32)lua_tointeger(L, 1);
    custom_marker = smlua_custom_marker_from_behavior_id(behavior_id);
    if (custom_marker != 0) {
        obj = smlua_obj_get_first_with_custom_marker(custom_marker);
        if (obj == NULL) {
            lua_pushnil(L);
        } else {
            smlua_push_object(L, obj);
        }
        return 1;
    }
    behavior = smlua_behavior_from_id(behavior_id);

    if (behavior == NULL) {
        lua_pushnil(L);
        return 1;
    }

    obj = smlua_obj_get_first_with_behavior(behavior);
    if (obj == NULL) {
        lua_pushnil(L);
        return 1;
    }

    smlua_push_object(L, obj);
    return 1;
}

// Stub for Co-op DX chat command registration while chat UI is not yet ported.
static int smlua_func_hook_chat_command(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

// Stub for Co-op DX chat command description updates while chat UI is unported.
static int smlua_func_update_chat_command_description(lua_State *L) {
    const char *command = luaL_optstring(L, 1, "");
    const char *description = luaL_optstring(L, 2, "");
    if (command == NULL || description == NULL || command[0] == '\0' || description[0] == '\0') {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

// Stub for Co-op DX network player descriptions in local-only single-player mode.
static int smlua_func_network_player_set_description(lua_State *L) {
    (void)L;
    return 0;
}

// Local-only network shim: single player runtime always owns authority.
static int smlua_func_network_is_server(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

// Local-only network shim: Wii U runtime currently supports one local player.
static int smlua_func_network_player_connected_count(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 1);
    return 1;
}

// Reports pause state for mods that need to freeze time-based logic.
static int smlua_func_network_check_singleplayer_pause(lua_State *L) {
    extern s16 gMenuMode;
    s32 paused = ((gMenuMode != -1) || (gCameraMovementFlags & CAM_MOVE_PAUSE_SCREEN) || sLuaCameraFrozen);
    (void)L;
    lua_pushboolean(L, paused ? 1 : 0);
    return 1;
}

// Exposes pause-state checks used by many Co-op DX gameplay scripts.
static int smlua_func_is_game_paused(lua_State *L) {
    return smlua_func_network_check_singleplayer_pause(L);
}

// Marks camera as frozen for script-side pause/menu logic.
static int smlua_func_camera_freeze(lua_State *L) {
    (void)L;
    sLuaCameraFrozen = true;
    return 0;
}

// Clears script-side camera freeze state.
static int smlua_func_camera_unfreeze(lua_State *L) {
    (void)L;
    sLuaCameraFrozen = false;
    return 0;
}

// Hides HUD elements while preserving current flags for restoration.
static int smlua_func_hud_hide(lua_State *L) {
    (void)L;
    if (!sLuaHudHidden) {
        sLuaHudSavedFlags = gHudDisplay.flags;
        sLuaHudHidden = true;
    }
    gHudDisplay.flags = HUD_DISPLAY_NONE;
    return 0;
}

// Restores HUD visibility after a script-side hide.
static int smlua_func_hud_show(lua_State *L) {
    (void)L;
    if (sLuaHudHidden) {
        gHudDisplay.flags = sLuaHudSavedFlags;
    } else if (gHudDisplay.flags == HUD_DISPLAY_NONE) {
        gHudDisplay.flags = HUD_DISPLAY_DEFAULT;
    }
    sLuaHudHidden = false;
    return 0;
}

// Forwards secondary music requests to the engine music sequencer.
static int smlua_func_play_secondary_music(lua_State *L) {
    u8 seq_id = (u8)luaL_checkinteger(L, 1);
    u8 bg_music_volume = (u8)luaL_checkinteger(L, 2);
    u8 volume = (u8)luaL_checkinteger(L, 3);
    u16 fade_timer = (u16)luaL_checkinteger(L, 4);
    play_secondary_music(seq_id, bg_music_volume, volume, fade_timer);
    return 0;
}

// Approximates Co-op DX stop_secondary_music behavior using env-player fadeout.
static int smlua_func_stop_secondary_music(lua_State *L) {
    u16 fade_timer = (u16)luaL_optinteger(L, 1, 0);
    seq_player_fade_out(SEQ_PLAYER_ENV, fade_timer);
    return 0;
}

// Ray collision shim returning a hit table compatible with char-select scripts.
static int smlua_func_collision_find_surface_on_ray(lua_State *L) {
    f32 x = (f32)luaL_checknumber(L, 1);
    f32 y = (f32)luaL_checknumber(L, 2);
    f32 z = (f32)luaL_checknumber(L, 3);
    f32 dir_x = (f32)luaL_checknumber(L, 4);
    f32 dir_y = (f32)luaL_checknumber(L, 5);
    f32 dir_z = (f32)luaL_checknumber(L, 6);
    f32 end_x = x + dir_x;
    f32 end_y = y + dir_y;
    f32 end_z = z + dir_z;
    f32 query_y = (y > end_y ? y : end_y) + 200.0f;
    struct Surface *floor = NULL;
    f32 floor_y = find_floor(end_x, query_y, end_z, &floor);

    if (floor == NULL) {
        floor_y = end_y;
    }

    lua_newtable(L);
    lua_newtable(L);
    lua_pushnumber(L, end_x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, floor_y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, end_z);
    lua_setfield(L, -2, "z");
    lua_setfield(L, -2, "hitPos");
    return 1;
}

// Returns minimal per-level info table used by voice reverb helpers.
static int smlua_func_smlua_level_util_get_info(lua_State *L) {
    s32 level_num = (s32)luaL_checkinteger(L, 1);
    if (level_num <= LEVEL_NONE || level_num >= LEVEL_COUNT) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushinteger(L, 8);
    lua_setfield(L, -2, "echoLevel1");
    lua_pushinteger(L, 8);
    lua_setfield(L, -2, "echoLevel2");
    lua_pushinteger(L, 8);
    lua_setfield(L, -2, "echoLevel3");
    return 1;
}

// Returns true for stock level numbers to keep vanilla-only mod logic functional.
static int smlua_func_level_is_vanilla_level(lua_State *L) {
    s32 level = (s32)luaL_checkinteger(L, 1);
    lua_pushboolean(L, (level > LEVEL_NONE && level < LEVEL_COUNT) ? 1 : 0);
    return 1;
}

// Returns a stable course/level label when full DJUI localization lookups are unavailable.
static int smlua_func_get_level_name(lua_State *L) {
    s32 course_num = (s32)luaL_optinteger(L, 1, 0);
    s32 level_num = (s32)luaL_optinteger(L, 2, 0);
    char label[64];

    if (course_num > 0) {
        snprintf(label, sizeof(label), "COURSE %d", course_num);
    } else if (level_num > 0) {
        snprintf(label, sizeof(label), "LEVEL %d", level_num);
    } else {
        snprintf(label, sizeof(label), "UNKNOWN");
    }

    lua_pushstring(L, label);
    return 1;
}

// Exposes local system date/time as a Lua table matching Co-op DX field names.
static int smlua_func_get_date_and_time(lua_State *L) {
    time_t current_time;
    struct tm *lt;

    time(&current_time);
    lt = localtime(&current_time);
    if (lt == NULL) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushinteger(L, lt->tm_year);
    lua_setfield(L, -2, "year");
    lua_pushinteger(L, lt->tm_mon);
    lua_setfield(L, -2, "month");
    lua_pushinteger(L, lt->tm_mday);
    lua_setfield(L, -2, "day");
    lua_pushinteger(L, lt->tm_hour);
    lua_setfield(L, -2, "hour");
    lua_pushinteger(L, lt->tm_min);
    lua_setfield(L, -2, "minute");
    lua_pushinteger(L, lt->tm_sec);
    lua_setfield(L, -2, "second");
    return 1;
}

// Approximates skybox id from current vanilla level when renderer metadata is unavailable.
static s32 smlua_guess_skybox_for_level(s16 level_num) {
    switch (level_num) {
        case LEVEL_DDD:            return BACKGROUND_OCEAN_SKY;
        case LEVEL_WDW:            return BACKGROUND_UNDERWATER_CITY;
        case LEVEL_THI:            return BACKGROUND_ABOVE_CLOUDS;
        case LEVEL_BBH:            return BACKGROUND_HAUNTED;
        case LEVEL_BITDW:          return BACKGROUND_GREEN_SKY;
        case LEVEL_CASTLE:
        case LEVEL_CASTLE_GROUNDS: return BACKGROUND_BELOW_CLOUDS;
        default:                   return BACKGROUND_CUSTOM;
    }
}

// Returns a compatibility skybox id for mods that branch on environment visuals.
static int smlua_func_get_skybox(lua_State *L) {
    (void)L;
    lua_pushinteger(L, smlua_guess_skybox_for_level(gCurrLevelNum));
    return 1;
}

// Returns the currently processed root geo node for viewport adjustments.
static int smlua_func_geo_get_current_root(lua_State *L) {
    (void)L;
    if (gCurGraphNodeRoot == NULL) {
        lua_pushnil(L);
        return 1;
    }
    smlua_push_graph_root(L, gCurGraphNodeRoot);
    return 1;
}

// Registers a Lua-provided animation table and stores it for later lookup by name.
static int smlua_func_smlua_anim_util_register_animation(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    s16 flags = (s16)luaL_checkinteger(L, 2);
    s16 anim_y_trans_divisor = (s16)luaL_checkinteger(L, 3);
    s16 start_frame = (s16)luaL_checkinteger(L, 4);
    s16 loop_start = (s16)luaL_checkinteger(L, 5);
    s16 loop_end = (s16)luaL_checkinteger(L, 6);
    s16 *values = NULL;
    u16 *index = NULL;
    u32 values_len = 0;
    u32 index_len = 0;
    struct SmluaCustomAnimation *entry;

    if (!smlua_read_s16_table(L, 7, &values, &values_len)) {
        return luaL_error(L, "smlua_anim_util_register_animation: invalid values table");
    }
    if (!smlua_read_u16_table(L, 8, &index, &index_len)) {
        free(values);
        return luaL_error(L, "smlua_anim_util_register_animation: invalid index table");
    }

    entry = smlua_find_custom_animation(name);
    if (entry == NULL) {
        entry = smlua_alloc_custom_animation_slot();
    }
    if (entry == NULL) {
        free(values);
        free(index);
        return luaL_error(L, "smlua_anim_util_register_animation: animation pool exhausted");
    }

    free(entry->name);
    free(entry->values);
    free(entry->index);
    memset(entry, 0, sizeof(*entry));

    entry->name = strdup(name);
    if (entry->name == NULL) {
        free(values);
        free(index);
        return luaL_error(L, "smlua_anim_util_register_animation: out of memory");
    }

    entry->active = true;
    entry->values = values;
    entry->index = index;
    entry->anim.flags = flags;
    entry->anim.animYTransDivisor = anim_y_trans_divisor;
    entry->anim.startFrame = start_frame;
    entry->anim.loopStart = loop_start;
    entry->anim.loopEnd = loop_end;
    entry->anim.unusedBoneCount = 0;
    entry->anim.values = entry->values;
    entry->anim.index = entry->index;
    entry->anim.length = 0;
    return 0;
}

// Applies a previously registered custom animation to an object.
static int smlua_func_smlua_anim_util_set_animation(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    const char *name = luaL_checkstring(L, 2);
    struct SmluaCustomAnimation *entry;

    if (cobj == NULL || cobj->type != SMLUA_COBJECT_OBJECT || cobj->pointer == NULL) {
        return 0;
    }

    entry = smlua_find_custom_animation(name);
    if (entry == NULL || !entry->active) {
        return 0;
    }

    ((struct Object *)cobj->pointer)->header.gfx.animInfo.curAnim = &entry->anim;
    return 0;
}

// Returns dialog metadata and text for a dialog ID as a Lua table.
static int smlua_func_smlua_text_utils_dialog_get(lua_State *L) {
    s32 dialog_id = (s32)luaL_checkinteger(L, 1);
    struct DialogEntry *dialog = smlua_get_dialog_entry(dialog_id);
    const u8 *dialog_str;
    char ascii[2048];

    if (dialog == NULL) {
        lua_pushnil(L);
        return 1;
    }

    dialog_str = segmented_to_virtual((void *)dialog->str);
    smlua_dialog_to_ascii(dialog_str, ascii, sizeof(ascii));

    lua_newtable(L);
    lua_pushinteger(L, dialog->unused);
    lua_setfield(L, -2, "unused");
    lua_pushinteger(L, dialog->linesPerBox);
    lua_setfield(L, -2, "linesPerBox");
    lua_pushinteger(L, dialog->leftOffset);
    lua_setfield(L, -2, "leftOffset");
    lua_pushinteger(L, dialog->width);
    lua_setfield(L, -2, "width");
    lua_pushstring(L, ascii);
    lua_setfield(L, -2, "text");
    return 1;
}

// Replaces dialog text/metadata for a dialog ID using ASCII Lua text.
static int smlua_func_smlua_text_utils_dialog_replace(lua_State *L) {
    s32 dialog_id = (s32)luaL_checkinteger(L, 1);
    u32 unused = (u32)luaL_checkinteger(L, 2);
    s8 lines_per_box = (s8)luaL_checkinteger(L, 3);
    s16 left_offset = (s16)luaL_checkinteger(L, 4);
    s16 width = (s16)luaL_checkinteger(L, 5);
    const char *text = luaL_optstring(L, 6, "");
    struct DialogEntry *dialog = smlua_get_dialog_entry(dialog_id);
    struct SmluaDialogOverride *override;
    size_t text_len;
    u8 *converted;

    if (dialog == NULL || text == NULL) {
        return 0;
    }

    override = &sLuaDialogOverrides[dialog_id];
    if (!override->has_original) {
        override->original = *dialog;
        override->has_original = true;
    }

    text_len = strlen(text);
    converted = malloc(text_len + 1);
    if (converted == NULL) {
        return 0;
    }

    for (size_t i = 0; i < text_len; i++) {
        converted[i] = smlua_ascii_to_dialog_char(text[i]);
    }
    converted[text_len] = DIALOG_CHAR_TERMINATOR;

    free(override->text);
    override->text = converted;

    dialog->unused = unused;
    dialog->linesPerBox = lines_per_box;
    dialog->leftOffset = left_offset;
    dialog->width = width;
    dialog->str = override->text;
    return 0;
}

// Applies script-provided dialog color override to current render state.
static int smlua_func_set_dialog_override_color(lua_State *L) {
    u8 bg_r = smlua_to_color_channel(L, 1);
    u8 bg_g = smlua_to_color_channel(L, 2);
    u8 bg_b = smlua_to_color_channel(L, 3);
    u8 bg_a = smlua_to_color_channel(L, 4);
    u8 text_r = smlua_to_color_channel(L, 5);
    u8 text_g = smlua_to_color_channel(L, 6);
    u8 text_b = smlua_to_color_channel(L, 7);
    u8 text_a = smlua_to_color_channel(L, 8);

    set_dialog_override_color(bg_r, bg_g, bg_b, bg_a, text_r, text_g, text_b, text_a);
    return 0;
}

// Clears script-provided dialog color override.
static int smlua_func_reset_dialog_override_color(lua_State *L) {
    (void)L;
    reset_dialog_override_color();
    return 0;
}

// Stores scripted lighting direction overrides (not yet bound into the renderer).
static int smlua_func_set_lighting_dir(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        sLuaLightingDir[index] = (f32)luaL_checknumber(L, 2);
        sLuaHasLightingDir = true;
    }
    return 0;
}

// Reads one channel of the current Lua lighting color override state.
static int smlua_func_get_lighting_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index < 0 || index >= 3) {
        lua_pushinteger(L, 255);
    } else {
        lua_pushinteger(L, sLuaLightingColor[index]);
    }
    return 1;
}

// Reads one channel of the current Lua ambient lighting override state.
static int smlua_func_get_lighting_color_ambient(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index < 0 || index >= 3) {
        lua_pushinteger(L, 255);
    } else {
        lua_pushinteger(L, sLuaLightingAmbientColor[index]);
    }
    return 1;
}

// Stores scripted lighting color and mirrors ambient color like Co-op DX.
static int smlua_func_set_lighting_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        u8 value = smlua_to_color_channel(L, 2);
        sLuaLightingColor[index] = value;
        sLuaLightingAmbientColor[index] = value;
        sLuaHasLightingColor = true;
        sLuaHasLightingAmbientColor = true;
    }
    return 0;
}

// Stores scripted ambient-light channel overrides.
static int smlua_func_set_lighting_color_ambient(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        sLuaLightingAmbientColor[index] = smlua_to_color_channel(L, 2);
        sLuaHasLightingAmbientColor = true;
    }
    return 0;
}

// Stores scripted vertex color channel overrides.
static int smlua_func_set_vertex_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        sLuaVertexColor[index] = smlua_to_color_channel(L, 2);
        sLuaHasVertexColor = true;
    }
    return 0;
}

// Reads one channel of the current Lua vertex color override state.
static int smlua_func_get_vertex_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index < 0 || index >= 3) {
        lua_pushinteger(L, 255);
    } else {
        lua_pushinteger(L, sLuaVertexColor[index]);
    }
    return 1;
}

// Stores scripted fog color channel overrides.
static int smlua_func_set_fog_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        sLuaFogColor[index] = smlua_to_color_channel(L, 2);
        sLuaHasFogColor = true;
    }
    return 0;
}

// Reads one channel of the current Lua fog color override state.
static int smlua_func_get_fog_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index < 0 || index >= 3) {
        lua_pushinteger(L, 255);
    } else {
        lua_pushinteger(L, sLuaFogColor[index]);
    }
    return 1;
}

// Stores scripted fog intensity overrides.
static int smlua_func_set_fog_intensity(lua_State *L) {
    sLuaFogIntensity = (f32)luaL_checknumber(L, 1);
    sLuaHasFogIntensity = true;
    return 0;
}

// Compatibility stub for Lighting Engine ambient integration not present on Wii U yet.
static int smlua_func_le_set_ambient_color(lua_State *L) {
    (void)L;
    return 0;
}

// Stores scripted far-clip override for custom skybox visibility.
static int smlua_func_set_override_far(lua_State *L) {
    s32 value = (s32)luaL_checkinteger(L, 1);
    sLuaOverrideFar = (value > 0) ? value : -1;
    return 0;
}

// Stores a manual projection FOV override used by character-select camera scripts.
static int smlua_func_set_override_fov(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    sLuaOverrideFov = (value > 0.0f) ? value : 0.0f;
    return 0;
}

// Stores a manual skybox override compatible with Co-op DX's set_override_skybox API.
static int smlua_func_set_override_skybox(lua_State *L) {
    sLuaScriptOverrideSkybox = (s32)luaL_checkinteger(L, 1);
    return 0;
}

// Returns the current effective skybox tint channel for compatibility scripts.
static int smlua_func_get_skybox_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    u8 color[3];

    smlua_get_skybox_color(color);
    if (index < 0 || index >= 3) {
        lua_pushinteger(L, 255);
    } else {
        lua_pushinteger(L, color[index]);
    }
    return 1;
}

// Stores a script-side skybox tint channel that multiplies with runtime DNC tint.
static int smlua_func_set_skybox_color(lua_State *L) {
    s32 index = (s32)luaL_checkinteger(L, 1);
    if (index >= 0 && index < 3) {
        sLuaScriptSkyboxColor[index] = smlua_to_color_channel(L, 2);
    }
    return 0;
}

// Maps known Co-op DX model names to Wii U model IDs.
static s32 smlua_model_name_to_id(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return MODEL_NONE;
    }

    if (strcmp(name, "dnc_skybox_geo") == 0) {
        // Day-night-cycle custom skybox geo is not imported yet; spawn as logic-only object.
        return MODEL_NONE;
    }
    if (strcmp(name, "vl_tone_luigi_geo") == 0) {
        return SMLUA_E_MODEL_VL_TONE_LUIGI;
    }
    if (strcmp(name, "cj_luigi_geo") == 0) {
        return SMLUA_E_MODEL_CJ_LUIGI;
    }
    if (strcmp(name, "djoslin_toad_geo") == 0) {
        return SMLUA_E_MODEL_DJOSLIN_TOAD;
    }
    if (strcmp(name, "fluffa_wario_geo") == 0) {
        return SMLUA_E_MODEL_FLUFFA_WARIO;
    }
    if (strcmp(name, "keeb_waluigi_geo") == 0) {
        return SMLUA_E_MODEL_KEEB_WALUIGI;
    }
    if (strcmp(name, "fluffa_waluigi_geo") == 0) {
        return SMLUA_E_MODEL_FLUFFA_WALUIGI;
    }
    if (strcmp(name, "custom_model_cap_normal_geo") == 0) {
        return SMLUA_E_MODEL_MARIOS_CAP;
    }
    if (strcmp(name, "custom_model_cap_wing_geo") == 0) {
        return SMLUA_E_MODEL_MARIOS_WING_CAP;
    }
    if (strcmp(name, "custom_model_cap_metal_geo") == 0) {
        return SMLUA_E_MODEL_MARIOS_METAL_CAP;
    }
    if (strcmp(name, "MODEL_NONE") == 0) {
        return MODEL_NONE;
    }
    if (strcmp(name, "MODEL_STAR") == 0) {
        return MODEL_STAR;
    }
    if (strcmp(name, "MODEL_TRANSPARENT_STAR") == 0) {
        return MODEL_TRANSPARENT_STAR;
    }

    return MODEL_NONE;
}

// Returns a Wii U model id for Co-op DX-style model-name lookups.
static int smlua_func_smlua_model_util_get_id(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    lua_pushinteger(L, smlua_model_name_to_id(name));
    return 1;
}

// Spawns a local-only object and runs optional Lua setup callback.
static int smlua_func_spawn_non_sync_object(lua_State *L) {
    s32 behavior_id = (s32)luaL_checkinteger(L, 1);
    s32 model_id = (s32)luaL_checkinteger(L, 2);
    s32 fallback_model_id = MODEL_NONE;
    f32 x = (f32)luaL_checknumber(L, 3);
    f32 y = (f32)luaL_checknumber(L, 4);
    f32 z = (f32)luaL_checknumber(L, 5);
    u32 custom_marker = smlua_custom_marker_from_behavior_id(behavior_id);
    const BehaviorScript *behavior = smlua_behavior_from_id(behavior_id);
    struct Object *parent = gMarioObject;
    struct Object *obj;

    if (behavior == NULL) {
        lua_pushnil(L);
        return 1;
    }
    if (parent == NULL) {
        parent = gMarioStates[0].marioObj;
    }
    if (parent == NULL) {
        lua_pushnil(L);
        return 1;
    }

    if (!smlua_resolve_model_fallback(model_id, &fallback_model_id)) {
        fallback_model_id = MODEL_NONE;
    }
    obj = spawn_object(parent, fallback_model_id, behavior);
    if (obj == NULL) {
        lua_pushnil(L);
        return 1;
    }
    smlua_set_object_logical_model(obj, model_id);

    obj->parentObj = obj;
    obj->oPosX = x;
    obj->oPosY = y;
    obj->oPosZ = z;
    obj->oHomeX = x;
    obj->oHomeY = y;
    obj->oHomeZ = z;
    if (custom_marker != 0) {
        obj->oUnk94 = custom_marker;
    }

    if (lua_isfunction(L, 6)) {
        lua_pushvalue(L, 6);
        smlua_push_object(L, obj);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *error = lua_tostring(L, -1);
            smlua_logf("lua: spawn_non_sync_object setup failed: %s", error != NULL ? error : "<unknown>");
            lua_pop(L, 1);
        }
    }

    smlua_push_object(L, obj);
    return 1;
}

// Compatibility bridge for Lua object scaling helper.
static int smlua_func_obj_scale(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL) {
        obj_scale((struct Object *)cobj->pointer, (f32)luaL_checknumber(L, 2));
    }
    return 0;
}

// Compatibility bridge for explicit object deletion helpers.
static int smlua_func_obj_mark_for_deletion(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL) {
        obj_mark_for_deletion((struct Object *)cobj->pointer);
    }
    return 0;
}

// Compatibility bridge for object-position copy helper used by behavior scripts.
static int smlua_func_vec3f_to_object_pos(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    Vec3f src = { 0.0f, 0.0f, 0.0f };

    if (cobj == NULL || cobj->type != SMLUA_COBJECT_OBJECT || cobj->pointer == NULL) {
        return 0;
    }
    if (!lua_istable(L, 2)) {
        return 0;
    }

    lua_getfield(L, 2, "x");
    src[0] = (f32)luaL_optnumber(L, -1, 0.0f);
    lua_pop(L, 1);
    lua_getfield(L, 2, "y");
    src[1] = (f32)luaL_optnumber(L, -1, 0.0f);
    lua_pop(L, 1);
    lua_getfield(L, 2, "z");
    src[2] = (f32)luaL_optnumber(L, -1, 0.0f);
    lua_pop(L, 1);

    vec3f_to_object_pos((struct Object *)cobj->pointer, src);
    return 0;
}

// Chat bridge currently logs to stdout until DJUI chatbox is ported.
static int smlua_func_djui_chat_message_create(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    smlua_logf("lua chat: %s", message != NULL ? message : "");
    return 0;
}

// Audio bridge for Lua scripts that trigger UI feedback sounds.
static int smlua_func_play_sound(lua_State *L) {
    s32 sound = (s32)luaL_checkinteger(L, 1);
    (void)lua_gettop(L);
    play_sound(sound, gGlobalSoundSource);
    return 0;
}

// Audio shim for pitch-scaled sound hooks (fallbacks to standard playback on Wii U).
static int smlua_func_play_sound_with_freq_scale(lua_State *L) {
    s32 sound = (s32)luaL_checkinteger(L, 1);
    f32 *pos = gGlobalSoundSource;
    Vec3f pos_table = { 0.0f, 0.0f, 0.0f };
    (void)luaL_optnumber(L, 3, 1.0f);

    if (lua_type(L, 2) == LUA_TUSERDATA) {
        lua_getfield(L, 2, "pointer");
        if (lua_islightuserdata(L, -1)) {
            pos = (f32 *)lua_touserdata(L, -1);
        }
        lua_pop(L, 1);
    } else if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "x");
        pos_table[0] = (f32)luaL_optnumber(L, -1, 0.0f);
        lua_pop(L, 1);
        lua_getfield(L, 2, "y");
        pos_table[1] = (f32)luaL_optnumber(L, -1, 0.0f);
        lua_pop(L, 1);
        lua_getfield(L, 2, "z");
        pos_table[2] = (f32)luaL_optnumber(L, -1, 0.0f);
        lua_pop(L, 1);
        pos = pos_table;
    }
    play_sound(sound, pos);
    return 0;
}

// Audio bridge for background volume fade requests.
static int smlua_func_fade_volume_scale(lua_State *L) {
    u8 player = (u8)luaL_checkinteger(L, 1);
    u8 target = (u8)luaL_checkinteger(L, 2);
    u16 duration = (u16)luaL_checkinteger(L, 3);
    fade_volume_scale(player, target, duration);
    return 0;
}

// Audio bridge for sequence change requests.
static int smlua_func_set_background_music(lua_State *L) {
    u16 player = (u16)luaL_checkinteger(L, 1);
    u16 seq_args = (u16)luaL_checkinteger(L, 2);
    s16 fade_timer = (s16)luaL_checkinteger(L, 3);

    // Co-op DX replacement tracks can use remapped sequence IDs; route them to
    // fallback vanilla IDs on this runtime until full sequence override support lands.
    u8 seq_id = (u8)(seq_args & 0xFF);
    if (sLuaSequenceAliasValid[seq_id]) {
        seq_args = (u16)((seq_args & 0xFF00) | sLuaSequenceAlias[seq_id]);
    } else if (seq_id >= SEQ_COUNT) {
        seq_args = (u16)((seq_args & 0xFF00) | (seq_id % SEQ_COUNT));
    }

    set_background_music(player, seq_args, fade_timer);
    return 0;
}

// Records sequence-id aliasing so scripts can safely reference replacement IDs.
static int smlua_func_smlua_audio_utils_replace_sequence(lua_State *L) {
    s32 sequence_id = (s32)luaL_checkinteger(L, 1);
    s32 bank_id = (s32)luaL_optinteger(L, 2, 0);
    s32 default_volume = (s32)luaL_optinteger(L, 3, 0);
    const char *m64_name = luaL_optstring(L, 4, NULL);
    const char *script_path = smlua_get_caller_script_path(L);
    char relative_name[SYS_MAX_PATH];
    char candidate_path[SYS_MAX_PATH];
    u8 *sequence_data = NULL;
    s32 sequence_len = 0;

    (void)bank_id;
    (void)default_volume;

    if (sequence_id < 0 || sequence_id > 0xFF) {
        return 0;
    }

    // The shipped day-night scripts register replacement IDs as SEQ_COUNT + base.
    // We map those IDs back to base sequence slots for compatibility.
    s32 fallback_id = sequence_id;
    if (fallback_id >= SEQ_COUNT) {
        fallback_id -= SEQ_COUNT;
    }
    if (fallback_id < 0 || fallback_id >= SEQ_COUNT) {
        fallback_id = fallback_id % SEQ_COUNT;
    }
    if (fallback_id < 0) {
        fallback_id += SEQ_COUNT;
    }

    sLuaSequenceAlias[(u8)sequence_id] = (u8)fallback_id;
    sLuaSequenceAliasValid[(u8)sequence_id] = true;

#ifdef TARGET_WII_U
    // Wii U stability guard: keep sequence ID compatibility but skip runtime
    // .m64 patch injection. Some custom tracks have caused startup instability
    // in Cemu and on-device while Lua mods are initializing.
    (void)bank_id;
    (void)default_volume;
    (void)m64_name;
    (void)script_path;
    return 0;
#endif

    if (m64_name == NULL || m64_name[0] == '\0' || script_path == NULL) {
        return 0;
    }

    if (smlua_path_has_suffix(m64_name, ".m64")) {
        snprintf(relative_name, sizeof(relative_name), "sound/%s", m64_name);
    } else {
        snprintf(relative_name, sizeof(relative_name), "sound/%s.m64", m64_name);
    }
    if (smlua_build_script_relative_path(candidate_path, sizeof(candidate_path), script_path, relative_name) &&
        smlua_load_binary_file(candidate_path, &sequence_data, &sequence_len)) {
        if (smlua_apply_sequence_override((u8)fallback_id, sequence_data, sequence_len)) {
            return 0;
        }
        free(sequence_data);
        sequence_data = NULL;
    }

    // Fallback for mods that pass a path-like token instead of bare sequence names.
    if (smlua_path_has_suffix(m64_name, ".m64")) {
        snprintf(relative_name, sizeof(relative_name), "%s", m64_name);
    } else {
        snprintf(relative_name, sizeof(relative_name), "%s.m64", m64_name);
    }
    if (smlua_build_script_relative_path(candidate_path, sizeof(candidate_path), script_path, relative_name) &&
        smlua_load_binary_file(candidate_path, &sequence_data, &sequence_len)) {
        if (smlua_apply_sequence_override((u8)fallback_id, sequence_data, sequence_len)) {
            return 0;
        }
        free(sequence_data);
        sequence_data = NULL;
    }

    smlua_logf("lua: sequence override file not found for '%s'", m64_name);
    return 0;
}

// Mod-menu update helper stub for checkbox rows.
static int smlua_func_update_mod_menu_element_checkbox(lua_State *L) {
    (void)L;
    return 0;
}

// Mod-menu update helper stub for slider rows.
static int smlua_func_update_mod_menu_element_slider(lua_State *L) {
    (void)L;
    return 0;
}

// Mod-menu update helper stub for input rows.
static int smlua_func_update_mod_menu_element_inputbox(lua_State *L) {
    (void)L;
    return 0;
}

// Mod-menu update helper stub for item labels.
static int smlua_func_update_mod_menu_element_name(lua_State *L) {
    (void)L;
    return 0;
}

// Returns epoch-like timer used by Co-op DX Lua constants bootstrap.
static int smlua_func_get_time(lua_State *L) {
    (void)L;
    lua_pushinteger(L, (lua_Integer)time(NULL));
    return 1;
}

// Creates a shallow copy of a Lua table for Co-op DX table utility parity.
static int smlua_func_table_copy(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_newtable(L);

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        // Duplicate key so lua_settable consumes key/value and keeps iteration key.
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, 2);
    }
    return 1;
}

// Copies a Lua value deeply, preserving table sharing/cycles via cache table.
static void smlua_table_deepcopy_value(lua_State *L, int idx, int cache_idx);

// Copies a Lua table deeply, including metatables, with cycle handling.
static void smlua_table_deepcopy_table(lua_State *L, int table_idx, int cache_idx) {
    table_idx = lua_absindex(L, table_idx);
    cache_idx = lua_absindex(L, cache_idx);

    lua_pushvalue(L, table_idx);
    lua_rawget(L, cache_idx);
    if (!lua_isnil(L, -1)) {
        return;
    }
    lua_pop(L, 1);

    lua_newtable(L);
    int new_table_idx = lua_gettop(L);
    lua_pushvalue(L, table_idx);
    lua_pushvalue(L, new_table_idx);
    lua_rawset(L, cache_idx);

    lua_pushnil(L);
    while (lua_next(L, table_idx) != 0) {
        int key_idx = lua_absindex(L, -2);
        int value_idx = lua_absindex(L, -1);
        smlua_table_deepcopy_value(L, key_idx, cache_idx);
        smlua_table_deepcopy_value(L, value_idx, cache_idx);
        lua_settable(L, new_table_idx);
        lua_pop(L, 1);
    }

    if (lua_getmetatable(L, table_idx)) {
        smlua_table_deepcopy_value(L, -1, cache_idx);
        lua_setmetatable(L, new_table_idx);
        lua_pop(L, 1);
    }
}

// Copies primitive values directly and tables through deep-copy traversal.
static void smlua_table_deepcopy_value(lua_State *L, int idx, int cache_idx) {
    idx = lua_absindex(L, idx);
    if (lua_type(L, idx) == LUA_TTABLE) {
        smlua_table_deepcopy_table(L, idx, cache_idx);
    } else {
        lua_pushvalue(L, idx);
    }
}

// Creates a deep copy of a Lua table for Co-op DX table utility parity.
static int smlua_func_table_deepcopy(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_newtable(L);
    int cache_idx = lua_gettop(L);
    smlua_table_deepcopy_table(L, 1, cache_idx);
    lua_remove(L, cache_idx);
    return 1;
}

// Applies sync-table writes without triggering recursive __newindex calls.
static int smlua_func_set_sync_table_field(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, 1);
    return 0;
}

// Removes \#RRGGBB color tags used in DJUI/Co-op DX formatted strings.
static int smlua_func_get_uncolored_string(lua_State *L) {
    const char *input = luaL_checkstring(L, 1);
    size_t input_len = strlen(input);
    char *output = malloc(input_len + 1);
    size_t in_i = 0;
    size_t out_i = 0;

    if (output == NULL) {
        lua_pushstring(L, input);
        return 1;
    }

    while (in_i < input_len) {
        if (in_i + 7 < input_len && input[in_i] == '\\' && input[in_i + 1] == '#' &&
            isxdigit((unsigned char)input[in_i + 2]) && isxdigit((unsigned char)input[in_i + 3]) &&
            isxdigit((unsigned char)input[in_i + 4]) && isxdigit((unsigned char)input[in_i + 5]) &&
            isxdigit((unsigned char)input[in_i + 6]) && isxdigit((unsigned char)input[in_i + 7])) {
            in_i += 8;
            continue;
        }
        output[out_i++] = input[in_i++];
    }
    output[out_i] = '\0';
    lua_pushstring(L, output);
    free(output);
    return 1;
}

// Display-list mutation shim is unimplemented until DynOS command parsing is ported.
static int smlua_func_gfx_set_command(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 0);
    return 1;
}

// Prints Lua-facing console messages to stdout for debug parity.
static int smlua_func_log_to_console(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    int level = (int)luaL_optinteger(L, 2, 0);
    smlua_logf("lua[%d]: %s", level, message != NULL ? message : "");
    return 0;
}

// Popup compatibility shim routed to console/chat logging.
static int smlua_func_djui_popup_create(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    int lines = (int)luaL_optinteger(L, 2, 0);
    smlua_logf("lua: popup(%d): %s", lines, message != NULL ? message : "");
    return 0;
}

// Resolves mod-relative file existence checks used by startup validators.
static int smlua_func_mod_file_exists(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    char resolved[SYS_MAX_PATH];
    bool exists = false;

    if (filename != NULL) {
        if (smlua_resolve_mod_asset_path(resolved, sizeof(resolved), script_path, filename, NULL, NULL)) {
            exists = true;
        } else if (smlua_resolve_mod_asset_path(resolved, sizeof(resolved), script_path, filename, "textures", NULL)) {
            exists = true;
        } else if (smlua_resolve_mod_asset_path(resolved, sizeof(resolved), script_path, filename, "sound", NULL)) {
            exists = true;
        }
    }

    lua_pushboolean(L, exists ? 1 : 0);
    return 1;
}

// Loads texture metadata from mod asset paths and returns TextureInfo userdata.
static int smlua_func_get_texture_info(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    char path[SYS_MAX_PATH] = { 0 };
    s32 width = 32;
    s32 height = 32;

    if (name != NULL) {
        if (!smlua_resolve_mod_asset_path(path, sizeof(path), script_path, name, "textures", ".tex")) {
            (void)smlua_resolve_mod_asset_path(path, sizeof(path), script_path, name, NULL, NULL);
        }

        if (path[0] != '\0') {
            (void)smlua_read_texture_dimensions(path, &width, &height);
        }
    }

    smlua_push_texture_info(L, name != NULL ? name : "texture", path, width, height);
    return 1;
}

// Adds a stream-style ModAudio handle for menu/background tracks.
static int smlua_func_audio_stream_load(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    char path[SYS_MAX_PATH] = { 0 };
    struct SmluaModAudio *audio;

    // Audio stream APIs are currently metadata-only shims on Wii U, so avoid
    // expensive startup-time fs probing for every declared sound file.
    if (!smlua_build_script_asset_hint(path, sizeof(path), script_path, "sound", filename)) {
        if (!smlua_build_script_asset_hint(path, sizeof(path), script_path, NULL, filename)) {
            snprintf(path, sizeof(path), "%s", filename);
        }
    }

    audio = smlua_alloc_mod_audio(path, true);
    if (audio == NULL) {
        lua_pushnil(L);
        return 1;
    }

    smlua_push_mod_audio(L, audio);
    return 1;
}

// Stream playback compatibility shim (metadata only on Wii U minimal runtime).
static int smlua_func_audio_stream_play(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        audio->looping = lua_toboolean(L, 2) != 0;
        audio->volume = (f32)luaL_optnumber(L, 3, audio->volume);
    }
    return 0;
}

// Stops a stream handle.
static int smlua_func_audio_stream_stop(lua_State *L) {
    (void)smlua_check_mod_audio(L, 1);
    return 0;
}

// Updates stream looping flag.
static int smlua_func_audio_stream_set_looping(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        audio->looping = lua_toboolean(L, 2) != 0;
    }
    return 0;
}

// Stores stream loop points for compatibility with Co-op DX APIs.
static int smlua_func_audio_stream_set_loop_points(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        // Co-op DX accepts numeric loop points (including float expressions).
        audio->loop_start = (s32)luaL_optnumber(L, 2, 0.0);
        audio->loop_end = (s32)luaL_optnumber(L, 3, 0.0);
    }
    return 0;
}

// Stores stream volume gain.
static int smlua_func_audio_stream_set_volume(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        audio->volume = (f32)luaL_checknumber(L, 2);
    }
    return 0;
}

// Stores stream playback frequency multiplier.
static int smlua_func_audio_stream_set_frequency(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        audio->frequency = (f32)luaL_checknumber(L, 2);
    }
    return 0;
}

// Adds a sample-style ModAudio handle for character voices/effects.
static int smlua_func_audio_sample_load(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    const char *script_path = smlua_get_caller_script_path(L);
    char path[SYS_MAX_PATH] = { 0 };
    struct SmluaModAudio *audio;

    // Audio sample APIs are currently metadata-only shims on Wii U, so avoid
    // expensive startup-time fs probing for every declared sound file.
    if (!smlua_build_script_asset_hint(path, sizeof(path), script_path, "sound", filename)) {
        if (!smlua_build_script_asset_hint(path, sizeof(path), script_path, NULL, filename)) {
            snprintf(path, sizeof(path), "%s", filename);
        }
    }

    audio = smlua_alloc_mod_audio(path, false);
    if (audio == NULL) {
        lua_pushnil(L);
        return 1;
    }

    smlua_push_mod_audio(L, audio);
    return 1;
}

// Sample playback shim for compatibility with Lua voice tables.
static int smlua_func_audio_sample_play(lua_State *L) {
    struct SmluaModAudio *audio = smlua_check_mod_audio(L, 1);
    if (audio != NULL) {
        audio->volume = (f32)luaL_optnumber(L, 3, audio->volume);
    }
    return 0;
}

// Sample stop shim for compatibility with Lua voice tables.
static int smlua_func_audio_sample_stop(lua_State *L) {
    (void)smlua_check_mod_audio(L, 1);
    return 0;
}

// Single-player compatibility shim for network object initialization.
static int smlua_func_network_init_object(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player compatibility shim for explicit network object sends.
static int smlua_func_network_send_object(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player compatibility shim for targeted packet sends.
static int smlua_func_network_send_to(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player compatibility shim for bytestring packet sends.
static int smlua_func_network_send_bytestring(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player compatibility shim for targeted bytestring packet sends.
static int smlua_func_network_send_bytestring_to(lua_State *L) {
    (void)L;
    return 0;
}

// Exclamation-box content override shim (runtime mutation not ported yet).
static int smlua_func_set_exclamation_box_contents(lua_State *L) {
    (void)L;
    return 0;
}

// Exclamation-box content query shim returning an empty override set.
static int smlua_func_get_exclamation_box_contents(lua_State *L) {
    lua_newtable(L);
    return 1;
}

// Texture override shim while dynamic texture replacement is not ported.
static int smlua_func_texture_override_set(lua_State *L) {
    (void)L;
    return 0;
}

// Texture override reset shim while dynamic texture replacement is not ported.
static int smlua_func_texture_override_reset(lua_State *L) {
    (void)L;
    return 0;
}

// Level-script parse shim; full dynos level parser integration is pending.
static int smlua_func_level_script_parse(lua_State *L) {
    (void)L;
    return 0;
}

// Scroll-target registration shim; dynos scrolling hooks are not ported yet.
static int smlua_func_add_scroll_target(lua_State *L) {
    (void)L;
    return 0;
}

// Graph-node cast shim that preserves the incoming value for compatibility.
static int smlua_func_cast_graph_node(lua_State *L) {
    lua_pushvalue(L, 1);
    return 1;
}

// Single-player reset helper shim; Wii U route is still pending full parity.
static int smlua_func_reset_level(lua_State *L) {
    (void)L;
    return 0;
}

// Re-runs spawn-side setup after Lua-triggered warp state changes.
static int smlua_func_init_mario_after_warp(lua_State *L) {
    extern void init_mario_after_warp(void);
    (void)L;
    init_mario_after_warp();
    return 0;
}

// Draws a rectangle in the HUD layer (currently no-op in compatibility mode).
static int smlua_func_djui_hud_render_rect(lua_State *L) {
    (void)L;
    return 0;
}

// Sets HUD rotation state (currently no-op in compatibility mode).
static int smlua_func_djui_hud_set_rotation(lua_State *L) {
    (void)L;
    return 0;
}

// Returns currently selected HUD font id.
static int smlua_func_djui_hud_get_font(lua_State *L) {
    lua_pushinteger(L, sHudFont);
    return 1;
}

// Applies scissor clip region (currently no-op in compatibility mode).
static int smlua_func_djui_hud_set_scissor(lua_State *L) {
    (void)L;
    return 0;
}

// Resets scissor clip region (currently no-op in compatibility mode).
static int smlua_func_djui_hud_reset_scissor(lua_State *L) {
    (void)L;
    return 0;
}

// Interpolated text shim delegating to immediate text draw at current position.
static int smlua_func_djui_hud_print_text_interpolated(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    f32 x = (f32)luaL_checknumber(L, 5);
    f32 y = (f32)luaL_checknumber(L, 6);
    f32 scale = (f32)luaL_optnumber(L, 7, 1.0f);
    if (scale > 0.0f && scale < 1.0f) {
        x += 2.0f;
    }
    smlua_hud_draw_text(message, (s32)x, (s32)y);
    return 0;
}

// Interpolated texture shim delegating to immediate texture draw at current transform.
static int smlua_func_djui_hud_render_texture_interpolated(lua_State *L) {
    f32 x = (f32)luaL_checknumber(L, 6);
    f32 y = (f32)luaL_checknumber(L, 7);
    (void)luaL_optnumber(L, 8, 1.0f);
    (void)luaL_optnumber(L, 9, 1.0f);

    u8 *(*hud_lut)[58] = segmented_to_virtual(&main_hud_lut);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    gDPSetEnvColor(gDisplayListHead++, sHudColor.r, sHudColor.g, sHudColor.b, sHudColor.a);
    render_hud_tex_lut((s32)x, (s32)y, (*hud_lut)[GLYPH_STAR]);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
    return 0;
}

// Tile-render shim currently mapped to single-texture draw.
static int smlua_func_djui_hud_render_texture_tile(lua_State *L) {
    f32 x = (f32)luaL_checknumber(L, 2);
    f32 y = (f32)luaL_checknumber(L, 3);
    (void)luaL_optnumber(L, 4, 1.0f);
    (void)luaL_optnumber(L, 5, 1.0f);

    u8 *(*hud_lut)[58] = segmented_to_virtual(&main_hud_lut);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    gDPSetEnvColor(gDisplayListHead++, sHudColor.r, sHudColor.g, sHudColor.b, sHudColor.a);
    render_hud_tex_lut((s32)x, (s32)y, (*hud_lut)[GLYPH_STAR]);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
    return 0;
}

// Interpolated tile-render shim currently mapped to current-texture draw.
static int smlua_func_djui_hud_render_texture_tile_interpolated(lua_State *L) {
    f32 x = (f32)luaL_checknumber(L, 6);
    f32 y = (f32)luaL_checknumber(L, 7);
    (void)luaL_optnumber(L, 8, 1.0f);
    (void)luaL_optnumber(L, 9, 1.0f);

    u8 *(*hud_lut)[58] = segmented_to_virtual(&main_hud_lut);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    gDPSetEnvColor(gDisplayListHead++, sHudColor.r, sHudColor.g, sHudColor.b, sHudColor.a);
    render_hud_tex_lut((s32)x, (s32)y, (*hud_lut)[GLYPH_STAR]);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
    return 0;
}

// FOV coefficient shim for nametag scale calculations.
static int smlua_func_djui_hud_get_fov_coeff(lua_State *L) {
    (void)L;
    lua_pushnumber(L, 1.0);
    return 1;
}

// Reads x/y/z from Lua vec-like userdata or table.
static bool smlua_read_vec3_like(lua_State *L, int index, f32 out[3]) {
    int abs = lua_absindex(L, index);
    const char *keys[3] = { "x", "y", "z" };

    if (!(lua_istable(L, abs) || lua_isuserdata(L, abs))) {
        return false;
    }

    for (int i = 0; i < 3; i++) {
        lua_getfield(L, abs, keys[i]);
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        out[i] = (f32)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    return true;
}

// Writes x/y/z into Lua vec-like userdata or table.
static void smlua_write_vec3_like(lua_State *L, int index, const f32 in[3]) {
    int abs = lua_absindex(L, index);
    if (!(lua_istable(L, abs) || lua_isuserdata(L, abs))) {
        return;
    }
    lua_pushnumber(L, in[0]);
    lua_setfield(L, abs, "x");
    lua_pushnumber(L, in[1]);
    lua_setfield(L, abs, "y");
    lua_pushnumber(L, in[2]);
    lua_setfield(L, abs, "z");
}

// Projects world position into HUD space with a conservative fallback transform.
static int smlua_func_djui_hud_world_pos_to_screen_pos(lua_State *L) {
    f32 in[3] = { 0.0f, 0.0f, 0.0f };
    f32 out[3];
    if (!smlua_read_vec3_like(L, 1, in)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    out[0] = (f32)(SCREEN_WIDTH / 2) + in[0] * 0.10f;
    out[1] = (f32)(SCREEN_HEIGHT / 2) - in[1] * 0.10f;
    out[2] = -300.0f - fabsf(in[2]) * 0.01f;
    smlua_write_vec3_like(L, 2, out);
    lua_pushboolean(L, 1);
    return 1;
}

// Copies Vec3 values between vec-like userdata/tables.
static int smlua_func_vec3f_copy(lua_State *L) {
    f32 src[3];
    if (!smlua_read_vec3_like(L, 2, src)) {
        return 0;
    }
    smlua_write_vec3_like(L, 1, src);
    return 0;
}

// Allocates a mutable Vec3f table initialized to zero.
static int smlua_func_gvec3f_zero(lua_State *L) {
    (void)L;
    lua_newtable(L);
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "z");
    return 1;
}

// Returns true when player slot has an active Mario object and valid action.
static int smlua_func_is_player_active(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    bool active = m != NULL && m->marioObj != NULL && m->action != ACT_DISAPPEARED;
    lua_pushboolean(L, active ? 1 : 0);
    return 1;
}

static u8 sLuaPaletteDefault[8][3] = {
    { 255, 0, 0 }, { 0, 0, 255 }, { 255, 255, 255 }, { 120, 70, 20 },
    { 120, 70, 20 }, { 255, 200, 160 }, { 255, 0, 0 }, { 255, 255, 255 },
};
static u8 sLuaPaletteOverride[8][3];
static bool sLuaPaletteOverrideValid[8];

// Reads network player index from a Lua table or integer argument.
static int smlua_get_network_player_index(lua_State *L, int arg) {
    int index = 0;
    if (lua_isinteger(L, arg)) {
        index = (int)lua_tointeger(L, arg);
    } else if (lua_istable(L, arg)) {
        lua_getfield(L, arg, "globalIndex");
        if (lua_isinteger(L, -1)) {
            index = (int)lua_tointeger(L, -1);
        }
        lua_pop(L, 1);
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= 1) {
        index = 0;
    }
    return index;
}

// Returns palette color table for requested player part.
static int smlua_func_network_player_get_palette_color(lua_State *L) {
    (void)smlua_get_network_player_index(L, 1);
    int part = (int)luaL_checkinteger(L, 2);
    if (part < 0 || part >= 8) {
        part = 0;
    }
    lua_newtable(L);
    lua_pushinteger(L, sLuaPaletteDefault[part][0]);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, sLuaPaletteDefault[part][1]);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, sLuaPaletteDefault[part][2]);
    lua_setfield(L, -2, "b");
    return 1;
}

// Returns override palette color table (or default when unset).
static int smlua_func_network_player_get_override_palette_color(lua_State *L) {
    (void)smlua_get_network_player_index(L, 1);
    int part = (int)luaL_checkinteger(L, 2);
    if (part < 0 || part >= 8) {
        part = 0;
    }
    lua_newtable(L);
    lua_pushinteger(L, sLuaPaletteOverrideValid[part] ? sLuaPaletteOverride[part][0] : sLuaPaletteDefault[part][0]);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, sLuaPaletteOverrideValid[part] ? sLuaPaletteOverride[part][1] : sLuaPaletteDefault[part][1]);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, sLuaPaletteOverrideValid[part] ? sLuaPaletteOverride[part][2] : sLuaPaletteDefault[part][2]);
    lua_setfield(L, -2, "b");
    return 1;
}

// Sets one override palette part.
static int smlua_func_network_player_set_override_palette_color(lua_State *L) {
    (void)smlua_get_network_player_index(L, 1);
    int part = (int)luaL_checkinteger(L, 2);
    if (part < 0 || part >= 8) {
        return 0;
    }
    sLuaPaletteOverride[part][0] = smlua_to_color_channel(L, 3);
    sLuaPaletteOverride[part][1] = smlua_to_color_channel(L, 4);
    sLuaPaletteOverride[part][2] = smlua_to_color_channel(L, 5);
    sLuaPaletteOverrideValid[part] = true;
    return 0;
}

// Clears all override palette colors for player.
static int smlua_func_network_player_reset_override_palette(lua_State *L) {
    (void)L;
    memset(sLuaPaletteOverrideValid, 0, sizeof(sLuaPaletteOverrideValid));
    return 0;
}

// Applies full override palette from Lua table keyed by part index.
static int smlua_func_network_player_set_full_override_palette(lua_State *L) {
    (void)smlua_get_network_player_index(L, 1);
    if (!lua_istable(L, 2)) {
        return 0;
    }
    for (int part = 0; part < 8; part++) {
        lua_pushinteger(L, part);
        lua_gettable(L, 2);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "r");
            lua_getfield(L, -2, "g");
            lua_getfield(L, -3, "b");
            if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                sLuaPaletteOverride[part][0] = (u8)lua_tointeger(L, -3);
                sLuaPaletteOverride[part][1] = (u8)lua_tointeger(L, -2);
                sLuaPaletteOverride[part][2] = (u8)lua_tointeger(L, -1);
                sLuaPaletteOverrideValid[part] = true;
            }
            lua_pop(L, 3);
        }
        lua_pop(L, 1);
    }
    return 0;
}

// Single-player shim: local/global index are identical.
static int smlua_func_network_local_index_from_global(lua_State *L) {
    (void)luaL_optinteger(L, 1, 0);
    lua_pushinteger(L, 0);
    return 1;
}

// Single-player shim: global index is always local player.
static int smlua_func_network_global_index_from_local(lua_State *L) {
    (void)luaL_optinteger(L, 1, 0);
    lua_pushinteger(L, 0);
    return 1;
}

// Single-player shim for moderator privilege checks.
static int smlua_func_network_is_moderator(lua_State *L) {
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

// Single-player shim for custom packet sends.
static int smlua_func_network_send(lua_State *L) {
    (void)L;
    return 0;
}

// Compatibility no-op for Co-op DX object custom-field declarations.
static int smlua_func_define_custom_obj_fields(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player custom action allocator used by hook_mario_action mods.
static u32 sLuaCustomActionNextIndex[7] = { 0 };
static int smlua_func_allocate_mario_action(lua_State *L) {
    u32 act_flags = (u32)luaL_checkinteger(L, 1);
    u32 group_bits = act_flags & ACT_GROUP_MASK;
    u32 group_index;
    u32 action_index;

    if (group_bits == 0) {
        if (act_flags & ACT_FLAG_MOVING) {
            group_bits = ACT_GROUP_MOVING;
        } else if (act_flags & ACT_FLAG_AIR) {
            group_bits = ACT_GROUP_AIRBORNE;
        } else if (act_flags & ACT_FLAG_SWIMMING) {
            group_bits = ACT_GROUP_SUBMERGED;
        } else {
            group_bits = ACT_GROUP_STATIONARY;
        }
    }

    group_index = (group_bits >> 6);
    if (group_index >= ARRAY_COUNT(sLuaCustomActionNextIndex)) {
        group_index = 0;
        group_bits = ACT_GROUP_STATIONARY;
    }

    if (sLuaCustomActionNextIndex[group_index] == 0) {
        sLuaCustomActionNextIndex[group_index] = 0x3F;
    }
    action_index = sLuaCustomActionNextIndex[group_index]--;
    action_index &= 0x3F;

    lua_pushinteger(L, (lua_Integer)((act_flags & ~ACT_ID_MASK & ~ACT_GROUP_MASK) | group_bits | action_index));
    return 1;
}

// Returns behavior ID from behavior-script pointer argument.
static int smlua_func_get_id_from_behavior(lua_State *L) {
    const BehaviorScript *behavior = (const BehaviorScript *)lua_touserdata(L, 1);
    lua_pushinteger(L, smlua_behavior_id_from_ptr(behavior));
    return 1;
}

// Returns human-readable behavior name for diagnostics.
static int smlua_func_get_behavior_name_from_id(lua_State *L) {
    s32 behavior_id = (s32)luaL_checkinteger(L, 1);
    lua_pushstring(L, smlua_behavior_name_from_id(behavior_id));
    return 1;
}

// Returns whether object currently uses behavior ID.
static int smlua_func_obj_has_behavior_id(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    s32 behavior_id = (s32)luaL_checkinteger(L, 2);
    const BehaviorScript *behavior = smlua_behavior_from_id(behavior_id);
    bool match = false;

    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL && behavior != NULL) {
        struct Object *obj = (struct Object *)cobj->pointer;
        match = obj->behavior == behavior;
    }

    lua_pushinteger(L, match ? 1 : 0);
    return 1;
}

// Returns whether object currently uses model ID.
static int smlua_func_obj_has_model_extended(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    s32 model_id = (s32)luaL_checkinteger(L, 2);
    bool match = false;

    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL) {
        struct Object *obj = (struct Object *)cobj->pointer;
        s32 current = smlua_get_object_logical_model(obj);
        s32 fallback_requested;
        match = (current == model_id);
        if (!match && current >= 0 && smlua_resolve_model_fallback(model_id, &fallback_requested)) {
            if (current == fallback_requested) {
                match = true;
            } else if (obj->header.gfx.sharedChild == gLoadedGraphNodes[fallback_requested]) {
                match = true;
            }
        }
    }

    lua_pushinteger(L, match ? 1 : 0);
    return 1;
}

// Gets object model ID by matching loaded graph-node pointer.
static int smlua_func_obj_get_model_id_extended(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    s32 model_id = -1;

    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL) {
        struct Object *obj = (struct Object *)cobj->pointer;
        model_id = smlua_get_object_logical_model(obj);
    }

    lua_pushinteger(L, model_id);
    return 1;
}

// Sets object model from extended model ID when available.
static int smlua_func_obj_set_model_extended(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    s32 model_id = (s32)luaL_checkinteger(L, 2);
    s32 fallback_model_id = MODEL_NONE;

    if (cobj != NULL && cobj->type == SMLUA_COBJECT_OBJECT && cobj->pointer != NULL &&
        smlua_resolve_model_fallback(model_id, &fallback_model_id)) {
        struct Object *obj = (struct Object *)cobj->pointer;
        obj->header.gfx.sharedChild = gLoadedGraphNodes[fallback_model_id];
        smlua_set_object_logical_model(obj, model_id);
        smlua_call_event_hooks_object_set_model(obj, model_id);
    }

    return 0;
}

// Returns first active object in object list index.
static int smlua_func_obj_get_first(lua_State *L) {
    int list = (int)luaL_checkinteger(L, 1);
    if (list < 0 || list >= NUM_OBJ_LISTS || gObjectLists == NULL) {
        lua_pushnil(L);
        return 1;
    }

    struct Object *head = (struct Object *)&gObjectLists[list];
    struct Object *obj = (struct Object *)head->header.next;
    while (obj != head) {
        if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED) {
            smlua_push_object(L, obj);
            return 1;
        }
        obj = (struct Object *)obj->header.next;
    }

    lua_pushnil(L);
    return 1;
}

// Returns next active object in same list for object iteration loops.
static int smlua_func_obj_get_next(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    struct Object *target;
    struct Object *obj;
    u32 depth = 0;

    if (cobj == NULL || cobj->type != SMLUA_COBJECT_OBJECT || cobj->pointer == NULL || gObjectLists == NULL) {
        lua_pushnil(L);
        return 1;
    }
    target = (struct Object *)cobj->pointer;
    obj = (struct Object *)target->header.next;

    // Walk the intrusive object-list ring directly to avoid O(n^2) scans in
    // mod code that repeatedly calls obj_get_next() during startup/model sync.
    while (obj != NULL && obj != target && depth++ < 20000) {
        if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED) {
            smlua_push_object(L, obj);
            return 1;
        }
        obj = (struct Object *)obj->header.next;
    }

    lua_pushnil(L);
    return 1;
}

// Returns next active object that shares behavior with current object.
static int smlua_func_obj_get_next_with_same_behavior_id(lua_State *L) {
    SmluaCObject *cobj = luaL_testudata(L, 1, SMLUA_COBJECT_METATABLE);
    struct Object *target;
    struct Object *obj;
    u32 depth = 0;

    if (cobj == NULL || cobj->type != SMLUA_COBJECT_OBJECT || cobj->pointer == NULL || gObjectLists == NULL) {
        lua_pushnil(L);
        return 1;
    }
    target = (struct Object *)cobj->pointer;
    obj = (struct Object *)target->header.next;

    // Same ring walk as obj_get_next(), but filtered by behavior to match
    // Co-op DX Lua iteration semantics without repeated full-list scans.
    while (obj != NULL && obj != target && depth++ < 20000) {
        if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED && obj->behavior == target->behavior) {
            smlua_push_object(L, obj);
            return 1;
        }
        obj = (struct Object *)obj->header.next;
    }

    lua_pushnil(L);
    return 1;
}

// Converts an active root script path into the relative mod id used by Lua mods.
static void smlua_extract_mod_relative_path(const char *script_path, char *out, size_t out_size) {
    const char *start;
    size_t len;

    if (out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (script_path == NULL || script_path[0] == '\0') {
        return;
    }

    start = script_path;
    if (strncmp(start, "mods/", 5) == 0) {
        start += 5;
    }

    len = strlen(start);
    if (len >= 9 && strcmp(start + len - 9, "/main.lua") == 0) {
        len -= 9;
    } else if (len >= 10 && strcmp(start + len - 10, "/main.luac") == 0) {
        len -= 10;
    } else if (len >= 4 && strcmp(start + len - 4, ".lua") == 0) {
        len -= 4;
    } else if (len >= 5 && strcmp(start + len - 5, ".luac") == 0) {
        len -= 5;
    }

    if (len == 0) {
        return;
    }
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, start, len);
    out[len] = '\0';
}

// Formats a readable mod name from a relative mod id.
static void smlua_format_mod_display_name(const char *relative_path, char *out, size_t out_size) {
    bool capitalize = true;
    size_t pos = 0;

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (relative_path == NULL || relative_path[0] == '\0') {
        snprintf(out, out_size, "Unknown Mod");
        return;
    }

    for (size_t i = 0; relative_path[i] != '\0' && pos + 1 < out_size; i++) {
        char c = relative_path[i];
        if (c == '-' || c == '_' || c == '/') {
            out[pos++] = ' ';
            capitalize = true;
            continue;
        }
        if (capitalize && c >= 'a' && c <= 'z') {
            out[pos++] = (char)(c - ('a' - 'A'));
        } else {
            out[pos++] = c;
        }
        capitalize = false;
    }
    out[pos] = '\0';
}

// Applies known Co-op DX mod metadata names/categories for built-in Wii U bundles.
static void smlua_apply_known_mod_metadata(const char *relative_path, char *name, size_t name_size,
                                           const char **category) {
    if (relative_path == NULL || name == NULL || category == NULL) {
        return;
    }

    if (strcmp(relative_path, "character-select-coop") == 0) {
        snprintf(name, name_size, "Character Select");
        *category = "cs";
        return;
    }
    if (strcmp(relative_path, "char-select-the-originals") == 0) {
        snprintf(name, name_size, "[CS] The Originals");
        *category = "cs";
        return;
    }
    if (strcmp(relative_path, "day-night-cycle") == 0) {
        snprintf(name, name_size, "Day Night Cycle");
        return;
    }
    if (strcmp(relative_path, "faster-swimming") == 0) {
        snprintf(name, name_size, "Faster Swimming");
        return;
    }
    if (strcmp(relative_path, "personal-starcount-ex") == 0) {
        snprintf(name, name_size, "Personal Starcount");
        return;
    }
    if (strcmp(relative_path, "cheats") == 0) {
        snprintf(name, name_size, "Cheats");
        return;
    }
}

// Initializes lightweight globals expected by built-in mods (`gTextures.star`, etc).
static void smlua_bind_minimal_globals(lua_State *L) {
    lua_newtable(L);
    smlua_push_texture_info(L, "star", "", 16, 16);
    lua_setfield(L, -2, "star");
    smlua_push_texture_info(L, "mario_head", "", 16, 16);
    lua_setfield(L, -2, "mario_head");
    smlua_push_texture_info(L, "luigi_head", "", 16, 16);
    lua_setfield(L, -2, "luigi_head");
    smlua_push_texture_info(L, "toad_head", "", 16, 16);
    lua_setfield(L, -2, "toad_head");
    smlua_push_texture_info(L, "waluigi_head", "", 16, 16);
    lua_setfield(L, -2, "waluigi_head");
    smlua_push_texture_info(L, "wario_head", "", 16, 16);
    lua_setfield(L, -2, "wario_head");
    lua_setglobal(L, "gTextures");

    lua_newtable(L);
    size_t script_count = mods_get_active_script_count();
    for (size_t i = 0; i < script_count; i++) {
        const char *script_path = mods_get_active_script_path(i);
        char relative_path[128];
        char display_name[128];
        const char *category = "";

        smlua_extract_mod_relative_path(script_path, relative_path, sizeof(relative_path));
        if (relative_path[0] == '\0') {
            continue;
        }
        smlua_format_mod_display_name(relative_path, display_name, sizeof(display_name));
        smlua_apply_known_mod_metadata(relative_path, display_name, sizeof(display_name), &category);

        if (strncmp(relative_path, "character-select", 16) == 0 ||
            strncmp(relative_path, "char-select-", 12) == 0) {
            category = "cs";
        }

        lua_newtable(L);
        lua_pushstring(L, display_name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, relative_path);
        lua_setfield(L, -2, "relativePath");
        lua_pushstring(L, category);
        lua_setfield(L, -2, "category");
        lua_rawseti(L, -2, (lua_Integer)i);
    }
    lua_setglobal(L, "gActiveMods");
}

// Single-player shim for Co-op DX `get_global_timer()`.
static int smlua_func_get_global_timer(lua_State *L) {
    (void)L;
    lua_pushinteger(L, (lua_Integer)gGlobalTimer);
    return 1;
}

// Single-player shim for Co-op DX `set_mario_action(m, action, arg)`.
static int smlua_func_set_mario_action(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    u32 action = (u32)luaL_checkinteger(L, 2);
    u32 action_arg = (u32)luaL_optinteger(L, 3, 0);
    lua_pushinteger(L, (lua_Integer)set_mario_action(m, action, action_arg));
    return 1;
}

// Local implementation for Co-op DX `approach_s32`.
static int smlua_func_approach_s32(lua_State *L) {
    s32 current = (s32)luaL_checkinteger(L, 1);
    s32 target = (s32)luaL_checkinteger(L, 2);
    s32 inc = (s32)luaL_checkinteger(L, 3);
    s32 dec = (s32)luaL_checkinteger(L, 4);

    if (current < target) {
        current += inc;
        if (current > target) {
            current = target;
        }
    } else if (current > target) {
        current -= dec;
        if (current < target) {
            current = target;
        }
    }

    lua_pushinteger(L, current);
    return 1;
}

// Co-op DX math helper `approach_f32(current, target, inc, dec)`.
static int smlua_func_approach_f32(lua_State *L) {
    f32 current = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 inc = (f32)luaL_checknumber(L, 3);
    f32 dec = (f32)luaL_checknumber(L, 4);
    lua_pushnumber(L, approach_f32(current, target, inc, dec));
    return 1;
}

// Co-op DX helper that returns (changed, newValue) for asymptotic approach.
static int smlua_func_approach_f32_asymptotic_bool(lua_State *L) {
    f32 current = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 multiplier = (f32)luaL_checknumber(L, 3);
    lua_pushinteger(L, approach_f32_asymptotic_bool(&current, target, multiplier));
    lua_pushnumber(L, current);
    return 2;
}

// Co-op DX helper `approach_f32_asymptotic(current, target, multiplier)`.
static int smlua_func_approach_f32_asymptotic(lua_State *L) {
    f32 current = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 multiplier = (f32)luaL_checknumber(L, 3);
    lua_pushnumber(L, approach_f32_asymptotic(current, target, multiplier));
    return 1;
}

// Co-op DX helper that returns (changed, newValue) while preserving vanilla camera semantics.
static int smlua_func_set_or_approach_f32_asymptotic(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    f32 goal = (f32)luaL_checknumber(L, 2);
    f32 scale = (f32)luaL_checknumber(L, 3);
    lua_pushinteger(L, set_or_approach_f32_asymptotic(&value, goal, scale));
    lua_pushnumber(L, value);
    return 2;
}

// Co-op DX helper returning (changed, newValue) for signed linear approach.
static int smlua_func_approach_f32_signed(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 increment = (f32)luaL_checknumber(L, 3);
    lua_pushinteger(L, approach_f32_signed(&value, target, increment));
    lua_pushnumber(L, value);
    return 2;
}

// Co-op DX helper `approach_f32_symmetric(value, target, increment)`.
static int smlua_func_approach_f32_symmetric(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 increment = (f32)luaL_checknumber(L, 3);
    lua_pushnumber(L, approach_f32_symmetric(value, target, increment));
    return 1;
}

// Co-op DX helper `abs_angle_diff(x0, x1)`.
static int smlua_func_abs_angle_diff(lua_State *L) {
    s16 x0 = (s16)luaL_checkinteger(L, 1);
    s16 x1 = (s16)luaL_checkinteger(L, 2);
    lua_pushinteger(L, abs_angle_diff(x0, x1));
    return 1;
}

// Co-op DX helper `absf_2(value)`.
static int smlua_func_absf_2(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    lua_pushnumber(L, value < 0.0f ? -value : value);
    return 1;
}

// Co-op DX helper that returns (changed, newValue) for asymptotic s16 approach.
static int smlua_func_approach_s16_asymptotic_bool(lua_State *L) {
    s16 current = (s16)luaL_checkinteger(L, 1);
    s16 target = (s16)luaL_checkinteger(L, 2);
    s16 divisor = (s16)luaL_checkinteger(L, 3);
    lua_pushinteger(L, approach_s16_asymptotic_bool(&current, target, divisor));
    lua_pushinteger(L, current);
    return 2;
}

// Co-op DX helper `approach_s16_asymptotic(current, target, divisor)`.
static int smlua_func_approach_s16_asymptotic(lua_State *L) {
    s16 current = (s16)luaL_checkinteger(L, 1);
    s16 target = (s16)luaL_checkinteger(L, 2);
    s16 divisor = (s16)luaL_checkinteger(L, 3);
    lua_pushinteger(L, approach_s16_asymptotic(current, target, divisor));
    return 1;
}

// Co-op DX helper `approach_s16_symmetric(current, target, increment)`.
static int smlua_func_approach_s16_symmetric(lua_State *L) {
    s16 current = (s16)luaL_checkinteger(L, 1);
    s16 target = (s16)luaL_checkinteger(L, 2);
    s16 increment = (s16)luaL_checkinteger(L, 3);
    lua_pushinteger(L, approach_s16_symmetric(current, target, increment));
    return 1;
}

// Co-op DX helper that returns (changed, newValue) for camera s16 approach.
static int smlua_func_camera_approach_s16_symmetric_bool(lua_State *L) {
    s16 current = (s16)luaL_checkinteger(L, 1);
    s16 target = (s16)luaL_checkinteger(L, 2);
    s16 increment = (s16)luaL_checkinteger(L, 3);
    lua_pushinteger(L, camera_approach_s16_symmetric_bool(&current, target, increment));
    lua_pushinteger(L, current);
    return 2;
}

// Co-op DX helper that returns (changed, newValue) for set-or-approach camera s16.
static int smlua_func_set_or_approach_s16_symmetric(lua_State *L) {
    s16 current = (s16)luaL_checkinteger(L, 1);
    s16 target = (s16)luaL_checkinteger(L, 2);
    s16 increment = (s16)luaL_checkinteger(L, 3);
    lua_pushinteger(L, set_or_approach_s16_symmetric(&current, target, increment));
    lua_pushinteger(L, current);
    return 2;
}

// Co-op DX helper that returns adjusted value after applying object-style quadratic drag.
static int smlua_func_apply_drag_to_value(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    f32 drag_strength = (f32)luaL_checknumber(L, 2);
    if (value != 0.0f) {
        f32 decel = value * value * (drag_strength * 0.0001f);
        if (value > 0.0f) {
            value -= decel;
            if (value < 0.001f) {
                value = 0.0f;
            }
        } else {
            value += decel;
            if (value > -0.001f) {
                value = 0.0f;
            }
        }
    }
    lua_pushnumber(L, value);
    return 1;
}

// Co-op DX helper `adjust_sound_for_speed(m)`.
static int smlua_func_adjust_sound_for_speed(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        adjust_sound_for_speed(m);
    }
    return 0;
}

// Co-op DX helper `add_tree_leaf_particles(m)`.
static int smlua_func_add_tree_leaf_particles(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void add_tree_leaf_particles(struct MarioState *m);
        add_tree_leaf_particles(m);
    }
    return 0;
}

// Co-op DX helper `align_with_floor(m)`.
static int smlua_func_align_with_floor(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void align_with_floor(struct MarioState *m);
        align_with_floor(m);
    }
    return 0;
}

// Co-op DX helper `analog_stick_held_back(m)`.
static int smlua_func_analog_stick_held_back(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m == NULL) {
        lua_pushinteger(L, 0);
        return 1;
    }
    extern s32 analog_stick_held_back(struct MarioState *m);
    lua_pushinteger(L, analog_stick_held_back(m));
    return 1;
}

// Co-op DX helper `anim_and_audio_for_walk(m)`.
static int smlua_func_anim_and_audio_for_walk(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void anim_and_audio_for_walk(struct MarioState *m);
        anim_and_audio_for_walk(m);
    }
    return 0;
}

// Co-op DX helper `anim_and_audio_for_hold_walk(m)`.
static int smlua_func_anim_and_audio_for_hold_walk(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void anim_and_audio_for_hold_walk(struct MarioState *m);
        anim_and_audio_for_hold_walk(m);
    }
    return 0;
}

// Co-op DX helper `anim_and_audio_for_heavy_walk(m)`.
static int smlua_func_anim_and_audio_for_heavy_walk(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void anim_and_audio_for_heavy_walk(struct MarioState *m);
        anim_and_audio_for_heavy_walk(m);
    }
    return 0;
}

// Co-op DX helper `animated_stationary_ground_step(m, animation, endAction)`.
static int smlua_func_animated_stationary_ground_step(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    s32 animation = (s32)luaL_checkinteger(L, 2);
    u32 end_action = (u32)luaL_checkinteger(L, 3);
    if (m != NULL) {
        extern void animated_stationary_ground_step(struct MarioState *m, s32 animation, u32 endAction);
        animated_stationary_ground_step(m, animation, end_action);
    }
    return 0;
}

// Co-op DX helper mirrored for Lua value semantics (returns `changed, newValue`).
static s32 smlua_approach_f32_ptr_compat(f32 *value, f32 target, f32 delta) {
    if (*value > target) {
        delta = -delta;
    }
    *value += delta;
    if ((*value - target) * delta >= 0.0f) {
        *value = target;
        return TRUE;
    }
    return FALSE;
}

// Co-op DX helper `approach_f32_ptr(value, target, delta)`.
static int smlua_func_approach_f32_ptr(lua_State *L) {
    f32 value = (f32)luaL_checknumber(L, 1);
    f32 target = (f32)luaL_checknumber(L, 2);
    f32 delta = (f32)luaL_checknumber(L, 3);
    lua_pushinteger(L, smlua_approach_f32_ptr_compat(&value, target, delta));
    lua_pushnumber(L, value);
    return 2;
}

// Co-op DX helper `approach_vec3f_asymptotic(current, target, xMul, yMul, zMul)`.
static int smlua_func_approach_vec3f_asymptotic(lua_State *L) {
    Vec3f current = { 0.0f, 0.0f, 0.0f };
    Vec3f target = { 0.0f, 0.0f, 0.0f };
    f32 x_mul = (f32)luaL_checknumber(L, 3);
    f32 y_mul = (f32)luaL_checknumber(L, 4);
    f32 z_mul = (f32)luaL_checknumber(L, 5);

    if (!smlua_read_vec3_like(L, 1, current) || !smlua_read_vec3_like(L, 2, target)) {
        return 0;
    }

    approach_vec3f_asymptotic(current, target, x_mul, y_mul, z_mul);
    smlua_write_vec3_like(L, 1, current);

    lua_newtable(L);
    lua_pushnumber(L, current[0]);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, current[1]);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, current[2]);
    lua_setfield(L, -2, "z");
    return 1;
}

// Co-op DX helper `set_or_approach_vec3f_asymptotic(dst, goal, xMul, yMul, zMul)`.
static int smlua_func_set_or_approach_vec3f_asymptotic(lua_State *L) {
    Vec3f dst = { 0.0f, 0.0f, 0.0f };
    Vec3f goal = { 0.0f, 0.0f, 0.0f };
    f32 x_mul = (f32)luaL_checknumber(L, 3);
    f32 y_mul = (f32)luaL_checknumber(L, 4);
    f32 z_mul = (f32)luaL_checknumber(L, 5);

    if (!smlua_read_vec3_like(L, 1, dst) || !smlua_read_vec3_like(L, 2, goal)) {
        return 0;
    }

    set_or_approach_vec3f_asymptotic(dst, goal, x_mul, y_mul, z_mul);
    smlua_write_vec3_like(L, 1, dst);

    lua_newtable(L);
    lua_pushnumber(L, dst[0]);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, dst[1]);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, dst[2]);
    lua_setfield(L, -2, "z");
    return 1;
}

// Co-op DX trigonometry helper `sins(sm64Angle)`.
static int smlua_func_sins(lua_State *L) {
    s16 angle = (s16)luaL_checkinteger(L, 1);
    lua_pushnumber(L, sins(angle));
    return 1;
}

// Co-op DX trigonometry helper `coss(sm64Angle)`.
static int smlua_func_coss(lua_State *L) {
    s16 angle = (s16)luaL_checkinteger(L, 1);
    lua_pushnumber(L, coss(angle));
    return 1;
}

// Co-op DX helper `atan2s(y, x)`.
static int smlua_func_atan2s(lua_State *L) {
    f32 y = (f32)luaL_checknumber(L, 1);
    f32 x = (f32)luaL_checknumber(L, 2);
    lua_pushinteger(L, atan2s(y, x));
    return 1;
}

// Co-op DX helper `atan2f(y, x)`.
static int smlua_func_atan2f(lua_State *L) {
    f32 y = (f32)luaL_checknumber(L, 1);
    f32 x = (f32)luaL_checknumber(L, 2);
    lua_pushnumber(L, atan2f(y, x));
    return 1;
}

// Co-op DX helper `apply_slope_accel(m)`.
static int smlua_func_apply_slope_accel(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    if (m != NULL) {
        extern void apply_slope_accel(struct MarioState *m);
        apply_slope_accel(m);
    }
    return 0;
}

// Co-op DX helper `apply_landing_accel(m, frictionFactor)`.
static int smlua_func_apply_landing_accel(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    f32 friction_factor = (f32)luaL_checknumber(L, 2);
    if (m == NULL) {
        lua_pushinteger(L, 0);
        return 1;
    }
    extern s32 apply_landing_accel(struct MarioState *m, f32 frictionFactor);
    lua_pushinteger(L, apply_landing_accel(m, friction_factor));
    return 1;
}

// Co-op DX helper `apply_slope_decel(m, decelCoef)`.
static int smlua_func_apply_slope_decel(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    f32 decel_coef = (f32)luaL_checknumber(L, 2);
    if (m == NULL) {
        lua_pushinteger(L, 0);
        return 1;
    }
    extern s32 apply_slope_decel(struct MarioState *m, f32 decelCoef);
    lua_pushinteger(L, apply_slope_decel(m, decel_coef));
    return 1;
}

// Co-op DX helper `arc_to_goal_pos(goal, pos, yVel, gravity)`.
static int smlua_func_arc_to_goal_pos(lua_State *L) {
    Vec3f goal = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    f32 y_vel = (f32)luaL_checknumber(L, 3);
    f32 gravity = (f32)luaL_checknumber(L, 4);

    if (!smlua_read_vec3_like(L, 1, goal) || !smlua_read_vec3_like(L, 2, pos)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    extern s32 arc_to_goal_pos(Vec3f goal, Vec3f pos, f32 yVel, f32 gravity);
    lua_pushinteger(L, arc_to_goal_pos(goal, pos, y_vel, gravity));
    return 1;
}

// Co-op DX helper `act_select_hud_hide(part)`.
static int smlua_func_act_select_hud_hide(lua_State *L) {
    s32 part = (s32)luaL_checkinteger(L, 1);
    sLuaActSelectHudMask |= part;
    return 0;
}

// Co-op DX helper `act_select_hud_show(part)`.
static int smlua_func_act_select_hud_show(lua_State *L) {
    s32 part = (s32)luaL_checkinteger(L, 1);
    sLuaActSelectHudMask &= ~part;
    return 0;
}

// Co-op DX helper `act_select_hud_is_hidden(part)`.
static int smlua_func_act_select_hud_is_hidden(lua_State *L) {
    s32 part = (s32)luaL_checkinteger(L, 1);
    lua_pushboolean(L, (sLuaActSelectHudMask & part) != 0);
    return 1;
}

// Compatibility stub for idle-cancel helper used by character-select scripts.
static int smlua_func_check_common_idle_cancels(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

// Compatibility stub for stationary ground-step helper used by character-select scripts.
static int smlua_func_stationary_ground_step(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

// Local compatibility for pause-menu flash flag queried by HUD scripts.
static int smlua_func_hud_get_flash(lua_State *L) {
    (void)L;
    lua_pushinteger(L, (lua_Integer)sLuaHudFlash);
    return 1;
}

// Local compatibility for pause-menu flash flag set by HUD scripts.
static int smlua_func_hud_set_flash(lua_State *L) {
    sLuaHudFlash = (s8)luaL_checkinteger(L, 1);
    return 0;
}

// Compatibility shim for scripts requesting gameplay unpause.
static int smlua_func_game_unpause(lua_State *L) {
    (void)L;
    return 0;
}

// Single-player compatibility shim: nearest player object is local Mario object.
static int smlua_func_nearest_player_to_object(lua_State *L) {
    (void)L;
    if (gMarioObject == NULL) {
        lua_pushnil(L);
        return 1;
    }
    smlua_push_object(L, gMarioObject);
    return 1;
}

// Character-select compatibility helper mapped to vanilla animation setter.
static int smlua_func_set_character_animation(lua_State *L) {
    struct MarioState *m = smlua_to_mario_state_arg(L, 1);
    s32 anim_id = (s32)luaL_checkinteger(L, 2);
    lua_pushinteger(L, set_mario_animation(m, anim_id));
    return 1;
}

// Character voice compatibility stub until full character audio routing is ported.
static int smlua_func_play_character_sound(lua_State *L) {
    (void)L;
    return 0;
}

// Returns zero in local-only mode where network area timing is absent.
static int smlua_func_get_network_area_timer(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 0);
    return 1;
}

// Provides default language for mods that request translated labels.
static int smlua_func_text_utils_get_language(lua_State *L) {
    (void)L;
    lua_pushstring(L, "English");
    return 1;
}

// Co-op DX math helper mirrored into Lua as `math.lerp`.
static int smlua_func_math_lerp(lua_State *L) {
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    lua_Number t = luaL_checknumber(L, 3);
    lua_pushnumber(L, a + (b - a) * t);
    return 1;
}

// Co-op DX math helper mirrored into Lua as `math.round`.
static int smlua_func_math_round(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    if (value >= 0.0) {
        lua_pushinteger(L, (lua_Integer)(value + 0.5));
    } else {
        lua_pushinteger(L, (lua_Integer)(value - 0.5));
    }
    return 1;
}

// Co-op DX math helper mirrored into Lua as `math.clamp`.
static int smlua_func_math_clamp(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    lua_Number min_value = luaL_checknumber(L, 2);
    lua_Number max_value = luaL_checknumber(L, 3);

    if (value < min_value) {
        value = min_value;
    }
    if (value > max_value) {
        value = max_value;
    }
    lua_pushnumber(L, value);
    return 1;
}

// Converts Lua number into signed 16-bit integer range.
static int smlua_func_math_s16(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    int16_t converted = (int16_t)(uint16_t)((int64_t)floor(value));
    lua_pushinteger(L, (lua_Integer)converted);
    return 1;
}

// Converts Lua number into signed 32-bit integer range.
static int smlua_func_math_s32(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    int32_t converted = (int32_t)(uint32_t)((int64_t)floor(value));
    lua_pushinteger(L, (lua_Integer)converted);
    return 1;
}

// Converts Lua number into unsigned 16-bit integer range.
static int smlua_func_math_u16(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    uint16_t converted = (uint16_t)((uint64_t)floor(value));
    lua_pushinteger(L, (lua_Integer)converted);
    return 1;
}

// Converts Lua number into unsigned 32-bit integer range.
static int smlua_func_math_u32(lua_State *L) {
    lua_Number value = luaL_checknumber(L, 1);
    uint32_t converted = (uint32_t)((uint64_t)floor(value));
    lua_pushinteger(L, (lua_Integer)converted);
    return 1;
}

// Installs global aliases expected by some Co-op DX scripts.
static void smlua_bind_global_math_aliases(lua_State *L) {
    lua_getglobal(L, "math");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "min");
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, "min");
        lua_pushvalue(L, -1);
        lua_setglobal(L, "minf");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "max");
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, "max");
        lua_pushvalue(L, -1);
        lua_setglobal(L, "maxf");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "clamp");
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, "clamp");
        lua_pushvalue(L, -1);
        lua_setglobal(L, "clampf");
    }
    lua_pop(L, 1);
    lua_pop(L, 1);
}

// Installs compatibility math helpers expected by Co-op DX Lua mods.
static void smlua_bind_math_helpers(lua_State *L) {
    lua_getglobal(L, "math");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_pushcfunction(L, smlua_func_math_lerp);
    lua_setfield(L, -2, "lerp");
    lua_pushcfunction(L, smlua_func_math_round);
    lua_setfield(L, -2, "round");
    lua_pushcfunction(L, smlua_func_math_clamp);
    lua_setfield(L, -2, "clamp");
    lua_pushcfunction(L, smlua_func_math_s16);
    lua_setfield(L, -2, "s16");
    lua_pushcfunction(L, smlua_func_math_s32);
    lua_setfield(L, -2, "s32");
    lua_pushcfunction(L, smlua_func_math_u16);
    lua_setfield(L, -2, "u16");
    lua_pushcfunction(L, smlua_func_math_u32);
    lua_setfield(L, -2, "u32");
    lua_pop(L, 1);

    smlua_bind_global_math_aliases(L);
}

// Binds small preamble symbols needed when full constants preamble is skipped on Wii U.
static void smlua_bind_wiiu_preamble_compat(lua_State *L) {
    lua_newtable(L);
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, 0.0f);
    lua_setfield(L, -2, "z");
    lua_setglobal(L, "gGlobalSoundSource");

    // Co-op DX defines FONT_TINY in constants preamble; keep parity when skipping that preamble.
    smlua_set_global_integer(L, "FONT_TINY", -1);
}

// Writes one boolean field into a Lua table only when that field is currently nil.
static void smlua_ensure_table_bool_field(lua_State *L, int table_index,
                                          const char *field, bool value) {
    int abs_index = lua_absindex(L, table_index);
    if (field == NULL) {
        return;
    }

    lua_getfield(L, abs_index, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, value ? 1 : 0);
        lua_setfield(L, abs_index, field);
        return;
    }
    lua_pop(L, 1);
}

// Ensures common Co-op DX globals exist before bundled companion scripts execute.
static void smlua_bind_wiiu_mod_runtime_compat(lua_State *L) {
    if (L == NULL) {
        return;
    }

    // Character Select helper scripts read these immediately during file load.
    lua_getglobal(L, "gServerSettings");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    smlua_ensure_table_bool_field(L, -1, "nametags", false);
    smlua_ensure_table_bool_field(L, -1, "enablePlayerList", false);
    smlua_ensure_table_bool_field(L, -1, "enablePlayersInLevelDisplay", false);
    lua_setglobal(L, "gServerSettings");

    lua_getglobal(L, "gNametagsSettings");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    smlua_ensure_table_bool_field(L, -1, "showHealth", false);
    smlua_ensure_table_bool_field(L, -1, "showSelfTag", false);
    lua_setglobal(L, "gNametagsSettings");

    // Fallback wrapper keeps voice.lua from failing if main.lua hasn't created this yet.
    lua_getglobal(L, "cs_hook_mario_update");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        if (luaL_dostring(L,
            "cs_hook_mario_update = function(func)\n"
            "  if type(func) == 'function' then\n"
            "    hook_event(HOOK_MARIO_UPDATE, func)\n"
            "  end\n"
            "end") != LUA_OK) {
            const char *error = lua_tostring(L, -1);
            smlua_logf("lua: failed to bind cs_hook_mario_update fallback: %s",
                       error != NULL ? error : "<unknown>");
            lua_pop(L, 1);
        }
        return;
    }
    lua_pop(L, 1);
}

// Builds/maintains Co-op DX-like single-player sync globals.
static void smlua_ensure_singleplayer_tables(lua_State *L) {
    int max_players = smlua_get_lua_max_players(L);

    lua_getglobal(L, "gPlayerSyncTable");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    for (int i = 0; i < max_players; i++) {
        smlua_ensure_table_entry(L, -1, (lua_Integer)i);
        lua_pop(L, 1);
    }
    lua_setglobal(L, "gPlayerSyncTable");

    lua_getglobal(L, "gGlobalSyncTable");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    lua_setglobal(L, "gGlobalSyncTable");

    lua_getglobal(L, "gNetworkPlayers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    for (int i = 0; i < max_players; i++) {
        smlua_ensure_table_entry(L, -1, (lua_Integer)i);
        lua_pop(L, 1);
    }
    lua_setglobal(L, "gNetworkPlayers");
}

// Refreshes values expected by local-only mods that read network player metadata.
static void smlua_update_singleplayer_network_snapshot(lua_State *L) {
    lua_getglobal(L, "gNetworkPlayers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    smlua_ensure_table_entry(L, -1, 0);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "connected");
    lua_pushinteger(L, gCurrCourseNum);
    lua_setfield(L, -2, "currCourseNum");
    lua_pushinteger(L, gCurrActNum);
    lua_setfield(L, -2, "currActNum");
    lua_pushinteger(L, gCurrLevelNum);
    lua_setfield(L, -2, "currLevelNum");
    lua_pushinteger(L, gCurrAreaIndex);
    lua_setfield(L, -2, "currAreaIndex");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "globalIndex");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "localIndex");
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "currAreaSyncValid");
    lua_pushinteger(L, MODEL_MARIO);
    lua_setfield(L, -2, "modelIndex");
    lua_pushinteger(L, 255);
    lua_setfield(L, -2, "fadeOpacity");

    lua_pop(L, 2);
}

// Exposes a compatibility subset of Co-op DX constants used by core built-in mods.
static void smlua_bind_minimal_constants(lua_State *L) {
    smlua_set_global_integer(L, "MAX_PLAYERS", 1);
    smlua_set_global_integer(L, "CT_MARIO", 0);
    smlua_set_global_integer(L, "CT_LUIGI", 1);
    smlua_set_global_integer(L, "CT_TOAD", 2);
    smlua_set_global_integer(L, "CT_WALUIGI", 3);
    smlua_set_global_integer(L, "CT_WARIO", 4);
    smlua_set_global_integer(L, "CT_MAX", 5);

    smlua_set_global_integer(L, "A_BUTTON", A_BUTTON);
    smlua_set_global_integer(L, "B_BUTTON", B_BUTTON);
    smlua_set_global_integer(L, "L_TRIG", L_TRIG);
    smlua_set_global_integer(L, "R_TRIG", R_TRIG);
    smlua_set_global_integer(L, "Z_TRIG", Z_TRIG);
    smlua_set_global_integer(L, "START_BUTTON", START_BUTTON);

    smlua_set_global_integer(L, "INTERACT_STAR_OR_KEY", INTERACT_STAR_OR_KEY);
    smlua_set_global_integer(L, "ACT_FLAG_SWIMMING", ACT_FLAG_SWIMMING);
    smlua_set_global_integer(L, "ACT_WATER_PLUNGE", ACT_WATER_PLUNGE);
    smlua_set_global_integer(L, "ACT_WATER_JUMP", ACT_WATER_JUMP);
    smlua_set_global_integer(L, "ACT_HOLD_WATER_JUMP", ACT_HOLD_WATER_JUMP);
    smlua_set_global_integer(L, "ACT_FORWARD_GROUND_KB", ACT_FORWARD_GROUND_KB);
    smlua_set_global_integer(L, "ACT_BACKWARD_GROUND_KB", ACT_BACKWARD_GROUND_KB);
    smlua_set_global_integer(L, "ACT_SOFT_FORWARD_GROUND_KB", ACT_SOFT_FORWARD_GROUND_KB);
    smlua_set_global_integer(L, "ACT_HARD_BACKWARD_GROUND_KB", ACT_HARD_BACKWARD_GROUND_KB);
    smlua_set_global_integer(L, "ACT_FORWARD_AIR_KB", ACT_FORWARD_AIR_KB);
    smlua_set_global_integer(L, "ACT_BACKWARD_AIR_KB", ACT_BACKWARD_AIR_KB);
    smlua_set_global_integer(L, "ACT_HARD_FORWARD_AIR_KB", ACT_HARD_FORWARD_AIR_KB);
    smlua_set_global_integer(L, "ACT_HARD_BACKWARD_AIR_KB", ACT_HARD_BACKWARD_AIR_KB);
    smlua_set_global_integer(L, "ACT_AIR_HIT_WALL", ACT_AIR_HIT_WALL);
    smlua_set_global_integer(L, "ACT_FREEFALL", ACT_FREEFALL);
    smlua_set_global_integer(L, "ACT_LONG_JUMP", ACT_LONG_JUMP);
    smlua_set_global_integer(L, "ACT_DOUBLE_JUMP_LAND", ACT_DOUBLE_JUMP_LAND);
    smlua_set_global_integer(L, "ACT_JUMP", ACT_JUMP);
    smlua_set_global_integer(L, "ACT_TRIPLE_JUMP", ACT_TRIPLE_JUMP);
    smlua_set_global_integer(L, "ACT_WALKING", ACT_WALKING);
    smlua_set_global_integer(L, "ACT_HOLD_WALKING", ACT_HOLD_WALKING);
    smlua_set_global_integer(L, "ACT_HOLD_HEAVY_WALKING", ACT_HOLD_HEAVY_WALKING);
    smlua_set_global_integer(L, "ACT_FINISH_TURNING_AROUND", ACT_FINISH_TURNING_AROUND);
    smlua_set_global_integer(L, "ACT_CRAWLING", ACT_CRAWLING);
    smlua_set_global_integer(L, "ACT_INTRO_CUTSCENE", ACT_INTRO_CUTSCENE);
    smlua_set_global_integer(L, "ACT_END_PEACH_CUTSCENE", ACT_END_PEACH_CUTSCENE);
    smlua_set_global_integer(L, "ACT_CREDITS_CUTSCENE", ACT_CREDITS_CUTSCENE);
    smlua_set_global_integer(L, "ACT_END_WAVING_CUTSCENE", ACT_END_WAVING_CUTSCENE);

    smlua_set_global_integer(L, "HUD_DISPLAY_LIVES", SMLUA_HUD_DISPLAY_LIVES);
    smlua_set_global_integer(L, "HUD_DISPLAY_COINS", SMLUA_HUD_DISPLAY_COINS);
    smlua_set_global_integer(L, "HUD_DISPLAY_STARS", SMLUA_HUD_DISPLAY_STARS);
    smlua_set_global_integer(L, "HUD_DISPLAY_WEDGES", SMLUA_HUD_DISPLAY_WEDGES);
    smlua_set_global_integer(L, "HUD_DISPLAY_KEYS", SMLUA_HUD_DISPLAY_KEYS);
    smlua_set_global_integer(L, "HUD_DISPLAY_FLAGS", SMLUA_HUD_DISPLAY_FLAGS);
    smlua_set_global_integer(L, "HUD_DISPLAY_TIMER", SMLUA_HUD_DISPLAY_TIMER);
    smlua_set_global_integer(L, "HUD_DISPLAY_CAMERA_STATUS", SMLUA_HUD_DISPLAY_CAMERA_STATUS);
    smlua_set_global_integer(L, "HUD_DISPLAY_FLAG_LIVES", HUD_DISPLAY_FLAG_LIVES);
    smlua_set_global_integer(L, "HUD_DISPLAY_FLAG_COIN_COUNT", HUD_DISPLAY_FLAG_COIN_COUNT);
    smlua_set_global_integer(L, "HUD_DISPLAY_FLAG_STAR_COUNT", HUD_DISPLAY_FLAG_STAR_COUNT);
    smlua_set_global_integer(L, "HUD_DISPLAY_FLAG_KEYS", HUD_DISPLAY_FLAG_KEYS);
    smlua_set_global_integer(L, "HUD_DISPLAY_DEFAULT", HUD_DISPLAY_DEFAULT);

    smlua_set_global_integer(L, "RESOLUTION_DJUI", SMLUA_RESOLUTION_DJUI);
    smlua_set_global_integer(L, "RESOLUTION_N64", SMLUA_RESOLUTION_N64);
    smlua_set_global_integer(L, "FONT_NORMAL", SMLUA_FONT_NORMAL);
    smlua_set_global_integer(L, "FONT_MENU", SMLUA_FONT_MENU);
    smlua_set_global_integer(L, "FONT_HUD", SMLUA_FONT_HUD);
    smlua_set_global_integer(L, "FONT_TINY", -1);

    smlua_set_global_integer(L, "E_MODEL_NONE", MODEL_NONE);
    smlua_set_global_integer(L, "E_MODEL_ERROR_MODEL", SMLUA_E_MODEL_ERROR_MODEL);
    smlua_set_global_integer(L, "E_MODEL_MARIO", SMLUA_E_MODEL_MARIO);
    smlua_set_global_integer(L, "E_MODEL_MARIOS_CAP", SMLUA_E_MODEL_MARIOS_CAP);
    smlua_set_global_integer(L, "E_MODEL_MARIOS_WING_CAP", SMLUA_E_MODEL_MARIOS_WING_CAP);
    smlua_set_global_integer(L, "E_MODEL_MARIOS_METAL_CAP", SMLUA_E_MODEL_MARIOS_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_MARIOS_WINGED_METAL_CAP", SMLUA_E_MODEL_MARIOS_WINGED_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_LUIGI", SMLUA_E_MODEL_LUIGI);
    smlua_set_global_integer(L, "E_MODEL_LUIGIS_CAP", SMLUA_E_MODEL_LUIGIS_CAP);
    smlua_set_global_integer(L, "E_MODEL_LUIGIS_WING_CAP", SMLUA_E_MODEL_LUIGIS_WING_CAP);
    smlua_set_global_integer(L, "E_MODEL_LUIGIS_METAL_CAP", SMLUA_E_MODEL_LUIGIS_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_LUIGIS_WINGED_METAL_CAP", SMLUA_E_MODEL_LUIGIS_WINGED_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_TOAD_PLAYER", SMLUA_E_MODEL_TOAD_PLAYER);
    smlua_set_global_integer(L, "E_MODEL_TOADS_CAP", SMLUA_E_MODEL_TOADS_CAP);
    smlua_set_global_integer(L, "E_MODEL_TOADS_WING_CAP", SMLUA_E_MODEL_TOADS_WING_CAP);
    smlua_set_global_integer(L, "E_MODEL_TOADS_METAL_CAP", SMLUA_E_MODEL_TOADS_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_WALUIGI", SMLUA_E_MODEL_WALUIGI);
    smlua_set_global_integer(L, "E_MODEL_WALUIGIS_CAP", SMLUA_E_MODEL_WALUIGIS_CAP);
    smlua_set_global_integer(L, "E_MODEL_WALUIGIS_WING_CAP", SMLUA_E_MODEL_WALUIGIS_WING_CAP);
    smlua_set_global_integer(L, "E_MODEL_WALUIGIS_METAL_CAP", SMLUA_E_MODEL_WALUIGIS_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_WALUIGIS_WINGED_METAL_CAP", SMLUA_E_MODEL_WALUIGIS_WINGED_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_WARIO", SMLUA_E_MODEL_WARIO);
    smlua_set_global_integer(L, "E_MODEL_WARIOS_CAP", SMLUA_E_MODEL_WARIOS_CAP);
    smlua_set_global_integer(L, "E_MODEL_WARIOS_WING_CAP", SMLUA_E_MODEL_WARIOS_WING_CAP);
    smlua_set_global_integer(L, "E_MODEL_WARIOS_METAL_CAP", SMLUA_E_MODEL_WARIOS_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_WARIOS_WINGED_METAL_CAP", SMLUA_E_MODEL_WARIOS_WINGED_METAL_CAP);
    smlua_set_global_integer(L, "E_MODEL_VL_TONE_LUIGI", SMLUA_E_MODEL_VL_TONE_LUIGI);
    smlua_set_global_integer(L, "E_MODEL_CJ_LUIGI", SMLUA_E_MODEL_CJ_LUIGI);
    smlua_set_global_integer(L, "E_MODEL_DJOSLIN_TOAD", SMLUA_E_MODEL_DJOSLIN_TOAD);
    smlua_set_global_integer(L, "E_MODEL_FLUFFA_WARIO", SMLUA_E_MODEL_FLUFFA_WARIO);
    smlua_set_global_integer(L, "E_MODEL_KEEB_WALUIGI", SMLUA_E_MODEL_KEEB_WALUIGI);
    smlua_set_global_integer(L, "E_MODEL_FLUFFA_WALUIGI", SMLUA_E_MODEL_FLUFFA_WALUIGI);
    smlua_set_global_integer(L, "E_MODEL_KOOPA_WITH_SHELL", MODEL_KOOPA_WITH_SHELL);
    smlua_set_global_integer(L, "E_MODEL_KOOPA_WITHOUT_SHELL", MODEL_KOOPA_WITHOUT_SHELL);

    smlua_set_global_integer(L, "id_bhvActSelector", SMLUA_BEHAVIOR_ID_ACT_SELECTOR);
    smlua_set_global_integer(L, "id_bhvActSelectorStarType", SMLUA_BEHAVIOR_ID_ACT_SELECTOR_STAR_TYPE);
    smlua_set_global_integer(L, "id_bhvBeginningPeach", SMLUA_BEHAVIOR_ID_BEGINNING_PEACH);
    smlua_set_global_integer(L, "id_bhvCelebrationStar", SMLUA_BEHAVIOR_ID_CELEBRATION_STAR);
    smlua_set_global_integer(L, "id_bhvEndPeach", SMLUA_BEHAVIOR_ID_END_PEACH);
    smlua_set_global_integer(L, "id_bhvEndToad", SMLUA_BEHAVIOR_ID_END_TOAD);
    smlua_set_global_integer(L, "id_bhvGoomba", SMLUA_BEHAVIOR_ID_GOOMBA);
    smlua_set_global_integer(L, "id_bhvAmbientLight", -1);
    smlua_set_global_integer(L, "id_bhvKoopa", SMLUA_BEHAVIOR_ID_KOOPA);
    smlua_set_global_integer(L, "id_bhvMario", SMLUA_BEHAVIOR_ID_MARIO);
    smlua_set_global_integer(L, "id_bhvMetalCap", SMLUA_BEHAVIOR_ID_METAL_CAP);
    smlua_set_global_integer(L, "id_bhvNormalCap", SMLUA_BEHAVIOR_ID_NORMAL_CAP);
    smlua_set_global_integer(L, "id_bhvVanishCap", SMLUA_BEHAVIOR_ID_VANISH_CAP);
    smlua_set_global_integer(L, "id_bhvWingCap", SMLUA_BEHAVIOR_ID_WING_CAP);
    smlua_set_global_integer(L, "bhvDNCSkybox", SMLUA_BEHAVIOR_ID_DNC_SKYBOX);
    smlua_set_global_integer(L, "bhvDNCNoSkybox", SMLUA_BEHAVIOR_ID_DNC_NO_SKYBOX);

    smlua_set_global_integer(L, "BACKGROUND_OCEAN_SKY", BACKGROUND_OCEAN_SKY);
    smlua_set_global_integer(L, "BACKGROUND_FLAMING_SKY", BACKGROUND_FLAMING_SKY);
    smlua_set_global_integer(L, "BACKGROUND_UNDERWATER_CITY", BACKGROUND_UNDERWATER_CITY);
    smlua_set_global_integer(L, "BACKGROUND_BELOW_CLOUDS", BACKGROUND_BELOW_CLOUDS);
    smlua_set_global_integer(L, "BACKGROUND_SNOW_MOUNTAINS", BACKGROUND_SNOW_MOUNTAINS);
    smlua_set_global_integer(L, "BACKGROUND_DESERT", BACKGROUND_DESERT);
    smlua_set_global_integer(L, "BACKGROUND_HAUNTED", BACKGROUND_HAUNTED);
    smlua_set_global_integer(L, "BACKGROUND_GREEN_SKY", BACKGROUND_GREEN_SKY);
    smlua_set_global_integer(L, "BACKGROUND_ABOVE_CLOUDS", BACKGROUND_ABOVE_CLOUDS);
    smlua_set_global_integer(L, "BACKGROUND_PURPLE_SKY", BACKGROUND_PURPLE_SKY);
    smlua_set_global_integer(L, "BACKGROUND_CUSTOM", BACKGROUND_CUSTOM);

    smlua_set_global_integer(L, "LEVEL_CASTLE", LEVEL_CASTLE);
    smlua_set_global_integer(L, "LEVEL_CASTLE_GROUNDS", LEVEL_CASTLE_GROUNDS);
    smlua_set_global_integer(L, "LEVEL_DDD", LEVEL_DDD);
    smlua_set_global_integer(L, "LEVEL_THI", LEVEL_THI);
    smlua_set_global_integer(L, "LEVEL_WDW", LEVEL_WDW);
    smlua_set_global_integer(L, "LEVEL_BOWSER_3", LEVEL_BOWSER_3);
    smlua_set_global_integer(L, "LEVEL_ENDING", LEVEL_ENDING);

    smlua_set_global_integer(L, "SOUND_MENU_CAMERA_BUZZ", SOUND_MENU_CAMERA_BUZZ);
    smlua_set_global_integer(L, "SEQ_PLAYER_LEVEL", SEQ_PLAYER_LEVEL);
    smlua_set_global_integer(L, "SEQ_LEVEL_GRASS", SEQ_LEVEL_GRASS);
    smlua_set_global_integer(L, "SEQ_LEVEL_INSIDE_CASTLE", SEQ_LEVEL_INSIDE_CASTLE);
    smlua_set_global_integer(L, "SEQ_LEVEL_WATER", SEQ_LEVEL_WATER);
    smlua_set_global_integer(L, "SEQ_LEVEL_HOT", SEQ_LEVEL_HOT);
    smlua_set_global_integer(L, "SEQ_LEVEL_SNOW", SEQ_LEVEL_SNOW);
    smlua_set_global_integer(L, "SEQ_LEVEL_SLIDE", SEQ_LEVEL_SLIDE);
    smlua_set_global_integer(L, "SEQ_LEVEL_UNDERGROUND", SEQ_LEVEL_UNDERGROUND);
    smlua_set_global_integer(L, "SEQ_COUNT", SEQ_COUNT);

    // This action does not exist in base sm64; keep mods source-compatible.
    smlua_set_global_integer(L, "ACT_BUBBLED", -1);
}

// Exposes a compatibility subset of Co-op DX global helper functions.
static void smlua_bind_minimal_functions(lua_State *L) {
    smlua_set_global_function(L, "table_copy", smlua_func_table_copy);
    smlua_set_global_function(L, "table_deepcopy", smlua_func_table_deepcopy);
    smlua_set_global_function(L, "_set_sync_table_field", smlua_func_set_sync_table_field);
    smlua_set_global_function(L, "get_time", smlua_func_get_time);
    smlua_set_global_function(L, "get_uncolored_string", smlua_func_get_uncolored_string);
    smlua_set_global_function(L, "get_global_timer", smlua_func_get_global_timer);
    smlua_set_global_function(L, "log_to_console", smlua_func_log_to_console);
    smlua_set_global_function(L, "djui_popup_create", smlua_func_djui_popup_create);
    smlua_set_global_function(L, "init_mario_after_warp", smlua_func_init_mario_after_warp);
    smlua_set_global_function(L, "reset_level", smlua_func_reset_level);
    smlua_set_global_function(L, "network_init_object", smlua_func_network_init_object);
    smlua_set_global_function(L, "network_send_object", smlua_func_network_send_object);
    smlua_set_global_function(L, "set_mario_action", smlua_func_set_mario_action);
    smlua_set_global_function(L, "allocate_mario_action", smlua_func_allocate_mario_action);
    smlua_set_global_function(L, "approach_s32", smlua_func_approach_s32);
    smlua_set_global_function(L, "approach_f32", smlua_func_approach_f32);
    smlua_set_global_function(L, "approach_f32_asymptotic_bool", smlua_func_approach_f32_asymptotic_bool);
    smlua_set_global_function(L, "approach_f32_asymptotic", smlua_func_approach_f32_asymptotic);
    smlua_set_global_function(L, "set_or_approach_f32_asymptotic", smlua_func_set_or_approach_f32_asymptotic);
    smlua_set_global_function(L, "approach_f32_signed", smlua_func_approach_f32_signed);
    smlua_set_global_function(L, "approach_f32_symmetric", smlua_func_approach_f32_symmetric);
    smlua_set_global_function(L, "abs_angle_diff", smlua_func_abs_angle_diff);
    smlua_set_global_function(L, "absf_2", smlua_func_absf_2);
    smlua_set_global_function(L, "approach_s16_asymptotic_bool", smlua_func_approach_s16_asymptotic_bool);
    smlua_set_global_function(L, "approach_s16_asymptotic", smlua_func_approach_s16_asymptotic);
    smlua_set_global_function(L, "approach_s16_symmetric", smlua_func_approach_s16_symmetric);
    smlua_set_global_function(L, "camera_approach_s16_symmetric_bool", smlua_func_camera_approach_s16_symmetric_bool);
    smlua_set_global_function(L, "set_or_approach_s16_symmetric", smlua_func_set_or_approach_s16_symmetric);
    smlua_set_global_function(L, "apply_drag_to_value", smlua_func_apply_drag_to_value);
    smlua_set_global_function(L, "adjust_sound_for_speed", smlua_func_adjust_sound_for_speed);
    smlua_set_global_function(L, "add_tree_leaf_particles", smlua_func_add_tree_leaf_particles);
    smlua_set_global_function(L, "align_with_floor", smlua_func_align_with_floor);
    smlua_set_global_function(L, "analog_stick_held_back", smlua_func_analog_stick_held_back);
    smlua_set_global_function(L, "anim_and_audio_for_walk", smlua_func_anim_and_audio_for_walk);
    smlua_set_global_function(L, "anim_and_audio_for_hold_walk", smlua_func_anim_and_audio_for_hold_walk);
    smlua_set_global_function(L, "anim_and_audio_for_heavy_walk", smlua_func_anim_and_audio_for_heavy_walk);
    smlua_set_global_function(L, "animated_stationary_ground_step", smlua_func_animated_stationary_ground_step);
    smlua_set_global_function(L, "approach_f32_ptr", smlua_func_approach_f32_ptr);
    smlua_set_global_function(L, "approach_vec3f_asymptotic", smlua_func_approach_vec3f_asymptotic);
    smlua_set_global_function(L, "set_or_approach_vec3f_asymptotic", smlua_func_set_or_approach_vec3f_asymptotic);
    smlua_set_global_function(L, "sins", smlua_func_sins);
    smlua_set_global_function(L, "coss", smlua_func_coss);
    smlua_set_global_function(L, "atan2s", smlua_func_atan2s);
    smlua_set_global_function(L, "atan2f", smlua_func_atan2f);
    smlua_set_global_function(L, "apply_slope_accel", smlua_func_apply_slope_accel);
    smlua_set_global_function(L, "apply_landing_accel", smlua_func_apply_landing_accel);
    smlua_set_global_function(L, "apply_slope_decel", smlua_func_apply_slope_decel);
    smlua_set_global_function(L, "arc_to_goal_pos", smlua_func_arc_to_goal_pos);
    smlua_set_global_function(L, "act_select_hud_hide", smlua_func_act_select_hud_hide);
    smlua_set_global_function(L, "act_select_hud_show", smlua_func_act_select_hud_show);
    smlua_set_global_function(L, "act_select_hud_is_hidden", smlua_func_act_select_hud_is_hidden);
    smlua_set_global_function(L, "check_common_idle_cancels", smlua_func_check_common_idle_cancels);
    smlua_set_global_function(L, "stationary_ground_step", smlua_func_stationary_ground_step);
    smlua_set_global_function(L, "hud_get_flash", smlua_func_hud_get_flash);
    smlua_set_global_function(L, "hud_set_flash", smlua_func_hud_set_flash);
    smlua_set_global_function(L, "game_unpause", smlua_func_game_unpause);
    smlua_set_global_function(L, "nearest_player_to_object", smlua_func_nearest_player_to_object);
    smlua_set_global_function(L, "set_character_animation", smlua_func_set_character_animation);
    smlua_set_global_function(L, "play_character_sound", smlua_func_play_character_sound);
    smlua_set_global_function(L, "get_network_area_timer", smlua_func_get_network_area_timer);
    smlua_set_global_function(L, "smlua_text_utils_get_language", smlua_func_text_utils_get_language);
    smlua_set_global_function(L, "mod_storage_load", smlua_func_mod_storage_load);
    smlua_set_global_function(L, "mod_storage_save", smlua_func_mod_storage_save);
    smlua_set_global_function(L, "mod_storage_remove", smlua_func_mod_storage_remove);
    smlua_set_global_function(L, "mod_storage_load_number", smlua_func_mod_storage_load_number);
    smlua_set_global_function(L, "mod_storage_load_bool", smlua_func_mod_storage_load_bool);
    smlua_set_global_function(L, "mod_storage_load_bool_2", smlua_func_mod_storage_load_bool_2);
    smlua_set_global_function(L, "mod_storage_save_number", smlua_func_mod_storage_save_number);
    smlua_set_global_function(L, "mod_storage_save_bool", smlua_func_mod_storage_save_bool);
    smlua_set_global_function(L, "mod_file_exists", smlua_func_mod_file_exists);
    smlua_set_global_function(L, "get_texture_info", smlua_func_get_texture_info);
    smlua_set_global_function(L, "texture_override_set", smlua_func_texture_override_set);
    smlua_set_global_function(L, "texture_override_reset", smlua_func_texture_override_reset);
    smlua_set_global_function(L, "level_script_parse", smlua_func_level_script_parse);
    smlua_set_global_function(L, "audio_stream_load", smlua_func_audio_stream_load);
    smlua_set_global_function(L, "audio_stream_play", smlua_func_audio_stream_play);
    smlua_set_global_function(L, "audio_stream_stop", smlua_func_audio_stream_stop);
    smlua_set_global_function(L, "audio_stream_set_looping", smlua_func_audio_stream_set_looping);
    smlua_set_global_function(L, "audio_stream_set_loop_points", smlua_func_audio_stream_set_loop_points);
    smlua_set_global_function(L, "audio_stream_set_volume", smlua_func_audio_stream_set_volume);
    smlua_set_global_function(L, "audio_stream_set_frequency", smlua_func_audio_stream_set_frequency);
    smlua_set_global_function(L, "audio_sample_load", smlua_func_audio_sample_load);
    smlua_set_global_function(L, "audio_sample_play", smlua_func_audio_sample_play);
    smlua_set_global_function(L, "audio_sample_stop", smlua_func_audio_sample_stop);
    smlua_set_global_function(L, "hud_get_value", smlua_func_hud_get_value);
    smlua_set_global_function(L, "hud_set_value", smlua_func_hud_set_value);
    smlua_set_global_function(L, "hud_is_hidden", smlua_func_hud_is_hidden);
    smlua_set_global_function(L, "djui_hud_is_pause_menu_created", smlua_func_djui_hud_is_pause_menu_created);
    smlua_set_global_function(L, "djui_hud_get_color", smlua_func_djui_hud_get_color);
    smlua_set_global_function(L, "djui_hud_measure_text", smlua_func_djui_hud_measure_text);
    smlua_set_global_function(L, "djui_hud_set_color", smlua_func_djui_hud_set_color);
    smlua_set_global_function(L, "djui_hud_print_text", smlua_func_djui_hud_print_text);
    smlua_set_global_function(L, "djui_hud_print_text_interpolated", smlua_func_djui_hud_print_text_interpolated);
    smlua_set_global_function(L, "djui_hud_render_rect", smlua_func_djui_hud_render_rect);
    smlua_set_global_function(L, "djui_hud_render_texture", smlua_func_djui_hud_render_texture);
    smlua_set_global_function(L, "djui_hud_render_texture_interpolated", smlua_func_djui_hud_render_texture_interpolated);
    smlua_set_global_function(L, "djui_hud_render_texture_tile", smlua_func_djui_hud_render_texture_tile);
    smlua_set_global_function(L, "djui_hud_render_texture_tile_interpolated", smlua_func_djui_hud_render_texture_tile_interpolated);
    smlua_set_global_function(L, "djui_hud_set_rotation", smlua_func_djui_hud_set_rotation);
    smlua_set_global_function(L, "djui_hud_set_resolution", smlua_func_djui_hud_set_resolution);
    smlua_set_global_function(L, "djui_hud_set_font", smlua_func_djui_hud_set_font);
    smlua_set_global_function(L, "djui_hud_get_font", smlua_func_djui_hud_get_font);
    smlua_set_global_function(L, "djui_hud_set_scissor", smlua_func_djui_hud_set_scissor);
    smlua_set_global_function(L, "djui_hud_reset_scissor", smlua_func_djui_hud_reset_scissor);
    smlua_set_global_function(L, "djui_hud_get_fov_coeff", smlua_func_djui_hud_get_fov_coeff);
    smlua_set_global_function(L, "djui_hud_world_pos_to_screen_pos", smlua_func_djui_hud_world_pos_to_screen_pos);
    smlua_set_global_function(L, "djui_hud_get_screen_width", smlua_func_djui_hud_get_screen_width);
    smlua_set_global_function(L, "djui_hud_get_screen_height", smlua_func_djui_hud_get_screen_height);
    smlua_set_global_function(L, "djui_hud_get_mouse_scroll_y", smlua_func_djui_hud_get_mouse_scroll_y);
    smlua_set_global_function(L, "djui_menu_get_theme", smlua_func_djui_menu_get_theme);
    smlua_set_global_function(L, "djui_menu_get_font", smlua_func_djui_menu_get_font);
    smlua_set_global_function(L, "djui_menu_get_rainbow_string_color", smlua_func_djui_menu_get_rainbow_string_color);
    smlua_set_global_function(L, "djui_language_get", smlua_func_djui_language_get);
    smlua_set_global_function(L, "djui_attempting_to_open_playerlist", smlua_func_djui_attempting_to_open_playerlist);
    smlua_set_global_function(L, "gVec3fZero", smlua_func_gvec3f_zero);
    smlua_set_global_function(L, "vec3f_copy", smlua_func_vec3f_copy);
    smlua_set_global_function(L, "is_player_active", smlua_func_is_player_active);
    smlua_set_global_function(L, "obj_get_first_with_behavior_id", smlua_func_obj_get_first_with_behavior_id);
    smlua_set_global_function(L, "obj_get_first", smlua_func_obj_get_first);
    smlua_set_global_function(L, "obj_get_next", smlua_func_obj_get_next);
    smlua_set_global_function(L, "obj_get_next_with_same_behavior_id", smlua_func_obj_get_next_with_same_behavior_id);
    smlua_set_global_function(L, "obj_has_behavior_id", smlua_func_obj_has_behavior_id);
    smlua_set_global_function(L, "obj_has_model_extended", smlua_func_obj_has_model_extended);
    smlua_set_global_function(L, "obj_get_model_id_extended", smlua_func_obj_get_model_id_extended);
    smlua_set_global_function(L, "obj_set_model_extended", smlua_func_obj_set_model_extended);
    smlua_set_global_function(L, "get_id_from_behavior", smlua_func_get_id_from_behavior);
    smlua_set_global_function(L, "get_behavior_name_from_id", smlua_func_get_behavior_name_from_id);
    smlua_set_global_function(L, "set_exclamation_box_contents", smlua_func_set_exclamation_box_contents);
    smlua_set_global_function(L, "get_exclamation_box_contents", smlua_func_get_exclamation_box_contents);
    smlua_set_global_function(L, "add_scroll_target", smlua_func_add_scroll_target);
    smlua_set_global_function(L, "cast_graph_node", smlua_func_cast_graph_node);
    smlua_set_global_function(L, "define_custom_obj_fields", smlua_func_define_custom_obj_fields);
    smlua_set_global_function(L, "gfx_set_command", smlua_func_gfx_set_command);
    smlua_set_global_function(L, "hook_chat_command", smlua_func_hook_chat_command);
    smlua_set_global_function(L, "update_chat_command_description", smlua_func_update_chat_command_description);
    smlua_set_global_function(L, "network_player_set_description", smlua_func_network_player_set_description);
    smlua_set_global_function(L, "network_is_server", smlua_func_network_is_server);
    smlua_set_global_function(L, "network_is_moderator", smlua_func_network_is_moderator);
    smlua_set_global_function(L, "network_player_connected_count", smlua_func_network_player_connected_count);
    smlua_set_global_function(L, "network_check_singleplayer_pause", smlua_func_network_check_singleplayer_pause);
    smlua_set_global_function(L, "is_game_paused", smlua_func_is_game_paused);
    smlua_set_global_function(L, "camera_freeze", smlua_func_camera_freeze);
    smlua_set_global_function(L, "camera_unfreeze", smlua_func_camera_unfreeze);
    smlua_set_global_function(L, "hud_hide", smlua_func_hud_hide);
    smlua_set_global_function(L, "hud_show", smlua_func_hud_show);
    smlua_set_global_function(L, "network_local_index_from_global", smlua_func_network_local_index_from_global);
    smlua_set_global_function(L, "network_global_index_from_local", smlua_func_network_global_index_from_local);
    smlua_set_global_function(L, "network_send", smlua_func_network_send);
    smlua_set_global_function(L, "network_send_to", smlua_func_network_send_to);
    smlua_set_global_function(L, "network_send_bytestring", smlua_func_network_send_bytestring);
    smlua_set_global_function(L, "network_send_bytestring_to", smlua_func_network_send_bytestring_to);
    smlua_set_global_function(L, "network_player_get_palette_color", smlua_func_network_player_get_palette_color);
    smlua_set_global_function(L, "network_player_get_override_palette_color", smlua_func_network_player_get_override_palette_color);
    smlua_set_global_function(L, "network_player_set_override_palette_color", smlua_func_network_player_set_override_palette_color);
    smlua_set_global_function(L, "network_player_set_full_override_palette", smlua_func_network_player_set_full_override_palette);
    smlua_set_global_function(L, "network_player_reset_override_palette", smlua_func_network_player_reset_override_palette);
    smlua_set_global_function(L, "smlua_level_util_get_info", smlua_func_smlua_level_util_get_info);
    smlua_set_global_function(L, "level_is_vanilla_level", smlua_func_level_is_vanilla_level);
    smlua_set_global_function(L, "get_level_name", smlua_func_get_level_name);
    smlua_set_global_function(L, "get_date_and_time", smlua_func_get_date_and_time);
    smlua_set_global_function(L, "get_skybox", smlua_func_get_skybox);
    smlua_set_global_function(L, "geo_get_current_root", smlua_func_geo_get_current_root);
    smlua_set_global_function(L, "set_lighting_dir", smlua_func_set_lighting_dir);
    smlua_set_global_function(L, "get_lighting_color", smlua_func_get_lighting_color);
    smlua_set_global_function(L, "set_lighting_color", smlua_func_set_lighting_color);
    smlua_set_global_function(L, "get_lighting_color_ambient", smlua_func_get_lighting_color_ambient);
    smlua_set_global_function(L, "set_lighting_color_ambient", smlua_func_set_lighting_color_ambient);
    smlua_set_global_function(L, "get_vertex_color", smlua_func_get_vertex_color);
    smlua_set_global_function(L, "set_vertex_color", smlua_func_set_vertex_color);
    smlua_set_global_function(L, "get_fog_color", smlua_func_get_fog_color);
    smlua_set_global_function(L, "set_fog_color", smlua_func_set_fog_color);
    smlua_set_global_function(L, "set_fog_intensity", smlua_func_set_fog_intensity);
    smlua_set_global_function(L, "le_set_ambient_color", smlua_func_le_set_ambient_color);
    smlua_set_global_function(L, "set_override_far", smlua_func_set_override_far);
    smlua_set_global_function(L, "set_override_fov", smlua_func_set_override_fov);
    smlua_set_global_function(L, "set_override_skybox", smlua_func_set_override_skybox);
    smlua_set_global_function(L, "get_skybox_color", smlua_func_get_skybox_color);
    smlua_set_global_function(L, "set_skybox_color", smlua_func_set_skybox_color);
    smlua_set_global_function(L, "smlua_model_util_get_id", smlua_func_smlua_model_util_get_id);
    smlua_set_global_function(L, "spawn_non_sync_object", smlua_func_spawn_non_sync_object);
    smlua_set_global_function(L, "obj_scale", smlua_func_obj_scale);
    smlua_set_global_function(L, "obj_mark_for_deletion", smlua_func_obj_mark_for_deletion);
    smlua_set_global_function(L, "vec3f_to_object_pos", smlua_func_vec3f_to_object_pos);
    smlua_set_global_function(L, "djui_chat_message_create", smlua_func_djui_chat_message_create);
    smlua_set_global_function(L, "play_sound", smlua_func_play_sound);
    smlua_set_global_function(L, "play_sound_with_freq_scale", smlua_func_play_sound_with_freq_scale);
    smlua_set_global_function(L, "fade_volume_scale", smlua_func_fade_volume_scale);
    smlua_set_global_function(L, "set_background_music", smlua_func_set_background_music);
    smlua_set_global_function(L, "play_secondary_music", smlua_func_play_secondary_music);
    smlua_set_global_function(L, "stop_secondary_music", smlua_func_stop_secondary_music);
    smlua_set_global_function(L, "smlua_audio_utils_replace_sequence", smlua_func_smlua_audio_utils_replace_sequence);
    smlua_set_global_function(L, "smlua_anim_util_register_animation", smlua_func_smlua_anim_util_register_animation);
    smlua_set_global_function(L, "smlua_anim_util_set_animation", smlua_func_smlua_anim_util_set_animation);
    smlua_set_global_function(L, "smlua_text_utils_dialog_get", smlua_func_smlua_text_utils_dialog_get);
    smlua_set_global_function(L, "smlua_text_utils_dialog_replace", smlua_func_smlua_text_utils_dialog_replace);
    smlua_set_global_function(L, "set_dialog_override_color", smlua_func_set_dialog_override_color);
    smlua_set_global_function(L, "reset_dialog_override_color", smlua_func_reset_dialog_override_color);
    smlua_set_global_function(L, "collision_find_surface_on_ray", smlua_func_collision_find_surface_on_ray);
    smlua_set_global_function(L, "update_mod_menu_element_checkbox", smlua_func_update_mod_menu_element_checkbox);
    smlua_set_global_function(L, "update_mod_menu_element_slider", smlua_func_update_mod_menu_element_slider);
    smlua_set_global_function(L, "update_mod_menu_element_inputbox", smlua_func_update_mod_menu_element_inputbox);
    smlua_set_global_function(L, "update_mod_menu_element_name", smlua_func_update_mod_menu_element_name);
}

// Returns true when path ends with suffix.
static bool smlua_path_has_suffix(const char *path, const char *suffix) {
    size_t path_len;
    size_t suffix_len;
    if (path == NULL || suffix == NULL) {
        return false;
    }
    path_len = strlen(path);
    suffix_len = strlen(suffix);
    if (path_len < suffix_len) {
        return false;
    }
    return strcmp(path + path_len - suffix_len, suffix) == 0;
}

// qsort comparator for lexicographic ordering of path strings.
static int smlua_compare_paths(const void *a, const void *b) {
    const char *const *path_a = (const char *const *)a;
    const char *const *path_b = (const char *const *)b;
    return strcmp(*path_a, *path_b);
}

struct SmluaCompanionFallback {
    const char *main_script;
    const char *const *companions;
    size_t companion_count;
};

static const char *sSmluaDayNightCycleCompanions[] = {
    "mods/day-night-cycle/a-utils.lua",
    "mods/day-night-cycle/b-constants.lua",
    "mods/day-night-cycle/b-time.lua",
    "mods/day-night-cycle/skybox.lua",
};

static const char *sSmluaCharacterSelectCoopCompanions[] = {
    "mods/character-select-coop/a-font-handler.lua",
    "mods/character-select-coop/a-supporters.lua",
    "mods/character-select-coop/a-utils.lua",
    "mods/character-select-coop/anims.lua",
    "mods/character-select-coop/dialog.lua",
    "mods/character-select-coop/hud.lua",
    "mods/character-select-coop/moveset.lua",
    "mods/character-select-coop/palettes.lua",
    "mods/character-select-coop/voice.lua",
    "mods/character-select-coop/z-api.lua",
};

static const struct SmluaCompanionFallback sSmluaCompanionFallbacks[] = {
    {
        "mods/day-night-cycle/main.lua",
        sSmluaDayNightCycleCompanions,
        sizeof(sSmluaDayNightCycleCompanions) / sizeof(sSmluaDayNightCycleCompanions[0]),
    },
    {
        "mods/character-select-coop/main.lua",
        sSmluaCharacterSelectCoopCompanions,
        sizeof(sSmluaCharacterSelectCoopCompanions) / sizeof(sSmluaCharacterSelectCoopCompanions[0]),
    },
};

// Loads known bundled companion scripts when /vol/content directory walking fails.
static int smlua_run_known_companion_fallbacks(const char *script_path) {
    for (size_t i = 0; i < sizeof(sSmluaCompanionFallbacks) / sizeof(sSmluaCompanionFallbacks[0]); i++) {
        const struct SmluaCompanionFallback *fallback = &sSmluaCompanionFallbacks[i];
        int loaded = 0;
        if (strcmp(script_path, fallback->main_script) != 0) {
            continue;
        }

        for (size_t j = 0; j < fallback->companion_count; j++) {
            const char *path = fallback->companions[j];
            fs_file_t *script = fs_open(path);
            if (script == NULL) {
                continue;
            }
            fs_close(script);
            smlua_run_file(path);
            loaded++;
        }

        return loaded;
    }

    return 0;
}

// Calls a Lua behavior callback with one Object cobject argument.
static void smlua_call_object_callback(lua_State *L, int func_index, const char *name, struct Object *obj) {
    int abs_func = lua_absindex(L, func_index);

    lua_pushvalue(L, abs_func);
    smlua_push_object(L, obj);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        smlua_logf("lua: %s failed: %s", name, error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
    }
}

// Returns true when skybox id is one of the built-in or DNC custom skyboxes (0..27).
static bool smlua_is_supported_skybox_id(s32 skybox) {
    return skybox >= BACKGROUND_OCEAN_SKY && skybox <= 27;
}

// Returns true when ID is one of DNC's custom sunrise skybox variants.
static bool smlua_is_dnc_sunrise_skybox(s32 skybox) {
    switch (skybox) {
        case 11:
        case 14:
        case 17:
        case 20:
        case 23:
        case 26:
            return true;
        default:
            return false;
    }
}

// Blends one skybox tint into the accumulator using linear alpha.
static void smlua_blend_skybox_tint(f32 *r, f32 *g, f32 *b, const u8 tint[3], f32 alpha) {
    if (alpha <= 0.0f) {
        return;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }

    *r = (*r * (1.0f - alpha)) + ((f32)tint[0] * alpha / 255.0f);
    *g = (*g * (1.0f - alpha)) + ((f32)tint[1] * alpha / 255.0f);
    *b = (*b * (1.0f - alpha)) + ((f32)tint[2] * alpha / 255.0f);
}

// Selects the active DNC skybox id and optional tint bridge for transition smoothing.
static void smlua_update_dnc_skybox_state(struct Object *day_obj, struct Object *sunset_obj, struct Object *night_obj,
                                          struct Object *fallback_obj) {
    static const u8 kNightTint[3] = { 108, 128, 184 };
    static const u8 kSunriseTint[3] = { 255, 200, 140 };
    static const u8 kSunsetTint[3] = { 255, 165, 120 };
    s32 base_background;
    s32 selected_background;
    f32 sunset_alpha = 0.0f;
    f32 night_alpha = 0.0f;
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;

    sLuaDncOverrideSkybox = -1;
    sLuaDncSkyboxColor[0] = 255;
    sLuaDncSkyboxColor[1] = 255;
    sLuaDncSkyboxColor[2] = 255;

    if (fallback_obj == NULL) {
        return;
    }

    base_background = (day_obj != NULL) ? day_obj->oAnimState : fallback_obj->oAnimState;
    selected_background = base_background;

    if (sunset_obj != NULL) {
        sunset_alpha = (f32)sunset_obj->oOpacity / 255.0f;
    }
    if (night_obj != NULL) {
        night_alpha = (f32)night_obj->oOpacity / 255.0f;
    }

    // Switch to custom sunrise/sunset/night textures when overlay opacity is visually dominant.
    if (night_obj != NULL && night_alpha >= 0.45f) {
        selected_background = night_obj->oAnimState;
    } else if (sunset_obj != NULL && sunset_alpha >= 0.45f) {
        selected_background = sunset_obj->oAnimState;
    }

    if (smlua_is_supported_skybox_id(selected_background)) {
        sLuaDncOverrideSkybox = selected_background;
    } else if (smlua_is_supported_skybox_id(base_background)) {
        sLuaDncOverrideSkybox = base_background;
    }

    // When using true custom skybox textures, keep raw texture colors (no extra tint multiply).
    if (sLuaDncOverrideSkybox >= BACKGROUND_CUSTOM) {
        return;
    }

    if (sunset_obj != NULL) {
        const u8 *sunset_tint = smlua_is_dnc_sunrise_skybox(sunset_obj->oAnimState) ? kSunriseTint : kSunsetTint;
        smlua_blend_skybox_tint(&r, &g, &b, sunset_tint, sunset_alpha);
    }
    if (night_obj != NULL) {
        smlua_blend_skybox_tint(&r, &g, &b, kNightTint, night_alpha);
    }

    sLuaDncSkyboxColor[0] = (u8)(255.0f * r + 0.5f);
    sLuaDncSkyboxColor[1] = (u8)(255.0f * g + 0.5f);
    sLuaDncSkyboxColor[2] = (u8)(255.0f * b + 0.5f);
}

// Runs Lua behavior init/loop callbacks for custom day-night skybox objects.
static void smlua_update_custom_dnc_objects(lua_State *L) {
    int init_func_index;
    int loop_func_index;
    bool has_init;
    bool has_loop;
    u32 sanity_depth = 0;
    struct Object *day_obj = NULL;
    struct Object *sunset_obj = NULL;
    struct Object *night_obj = NULL;
    struct Object *fallback_obj = NULL;

    if (L == NULL || gObjectLists == NULL) {
        smlua_update_dnc_skybox_state(NULL, NULL, NULL, NULL);
        return;
    }

    lua_getglobal(L, "bhv_dnc_skybox_init");
    init_func_index = lua_gettop(L);
    has_init = lua_isfunction(L, init_func_index);

    lua_getglobal(L, "bhv_dnc_skybox_loop");
    loop_func_index = lua_gettop(L);
    has_loop = lua_isfunction(L, loop_func_index);

    if (!has_init && !has_loop) {
        smlua_update_dnc_skybox_state(NULL, NULL, NULL, NULL);
        lua_pop(L, 2);
        return;
    }

    for (u32 obj_list = 0; obj_list < NUM_OBJ_LISTS; obj_list++) {
        struct Object *head = (struct Object *)&gObjectLists[obj_list];
        struct Object *obj = (struct Object *)head->header.next;
        while (obj != head) {
            struct Object *next = (struct Object *)obj->header.next;

            if (++sanity_depth > 20000) {
                smlua_update_dnc_skybox_state(NULL, NULL, NULL, NULL);
                lua_pop(L, 2);
                return;
            }

            if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED &&
                obj->oUnk94 == SMLUA_CUSTOM_MARKER_DNC_SKYBOX) {
                if (has_init && obj->oAction == 0) {
                    // Use oAction as a one-time init sentinel for Lua-backed objects.
                    smlua_call_object_callback(L, init_func_index, "bhv_dnc_skybox_init", obj);
                    if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED && obj->oAction == 0) {
                        obj->oAction = 1;
                    }
                }
                if (has_loop && obj->activeFlags != ACTIVE_FLAG_DEACTIVATED) {
                    smlua_call_object_callback(L, loop_func_index, "bhv_dnc_skybox_loop", obj);
                }
                if (obj->activeFlags != ACTIVE_FLAG_DEACTIVATED) {
                    if (fallback_obj == NULL) {
                        fallback_obj = obj;
                    }
                    switch (obj->oBehParams2ndByte) {
                        case 0:
                            day_obj = obj;
                            break;
                        case 1:
                            sunset_obj = obj;
                            break;
                        case 2:
                            night_obj = obj;
                            break;
                    }
                }
            }

            obj = next;
        }
    }

    smlua_update_dnc_skybox_state(day_obj, sunset_obj, night_obj, fallback_obj);
    lua_pop(L, 2);
}

// Aborts Lua chunks that exceed a startup instruction budget to prevent hard hangs.
static void smlua_script_budget_hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    luaL_error(L, "script exceeded startup instruction budget");
}

// Aborts per-frame global on_update handlers that exceed an instruction budget.
static void smlua_update_budget_hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    luaL_error(L, "on_update exceeded instruction budget");
}

// Loads and executes one Lua chunk, logging any compile/runtime failure.
static bool smlua_exec_lua_chunk(lua_State *L, const char *chunk, size_t len, const char *name) {
    if (L == NULL || chunk == NULL || len == 0) {
        return true;
    }

    if (luaL_loadbuffer(L, chunk, len, name) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        smlua_logf("lua: failed to compile chunk '%s': %s", name,
                   error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        smlua_logf("lua: failed to execute chunk '%s': %s", name,
                   error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// Executes a large block of assignment-only constants in bounded chunks.
static bool smlua_exec_constants_tail_chunked(lua_State *L, const char *tail) {
    static const size_t MAX_CHUNK_BYTES = 16384;
    static const int MAX_CHUNK_LINES = 192;
    const char *cursor = tail;
    bool ok = true;

    while (cursor != NULL && *cursor != '\0') {
        const char *chunk_start = cursor;
        const char *chunk_end = cursor;
        int line_count = 0;

        while (*chunk_end != '\0' && line_count < MAX_CHUNK_LINES &&
               (size_t)(chunk_end - chunk_start) < MAX_CHUNK_BYTES) {
            const char *newline = strchr(chunk_end, '\n');
            if (newline == NULL) {
                chunk_end += strlen(chunk_end);
                break;
            }
            chunk_end = newline + 1;
            line_count++;
        }

        if (chunk_end <= chunk_start) {
            break;
        }

        if (!smlua_exec_lua_chunk(L, chunk_start, (size_t)(chunk_end - chunk_start), "@smlua_constants:chunk")) {
            const char *line = chunk_start;
            while (line < chunk_end) {
                const char *newline = memchr(line, '\n', (size_t)(chunk_end - line));
                const char *line_end = (newline != NULL) ? newline + 1 : chunk_end;
                size_t line_len = (size_t)(line_end - line);
                if (line_len > 1 &&
                    !smlua_exec_lua_chunk(L, line, line_len, "@smlua_constants:line")) {
                    ok = false;
                }
                line = line_end;
            }
        }

        cursor = chunk_end;
    }
    return ok;
}

// Returns true when c is an ASCII whitespace character.
static bool smlua_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Returns true when token is a valid Lua identifier.
static bool smlua_is_valid_identifier(const char *begin, const char *end) {
    if (begin == NULL || end == NULL || begin >= end) {
        return false;
    }
    if (!(isalpha((unsigned char)*begin) || *begin == '_')) {
        return false;
    }
    for (const char *p = begin + 1; p < end; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return false;
        }
    }
    return true;
}

// Parses and binds one NAME=literal constant line from gSmluaConstants tail.
static bool smlua_bind_tail_constant_literal(lua_State *L, const char *line_begin, const char *line_end) {
    const char *eq;
    const char *name_begin;
    const char *name_end;
    const char *value_begin;
    const char *value_end;
    char name_buf[128];
    char value_buf[128];
    size_t name_len;
    size_t value_len;

    if (L == NULL || line_begin == NULL || line_end == NULL || line_begin >= line_end) {
        return false;
    }

    eq = memchr(line_begin, '=', (size_t)(line_end - line_begin));
    if (eq == NULL) {
        return false;
    }

    name_begin = line_begin;
    name_end = eq;
    while (name_begin < name_end && smlua_is_space(*name_begin)) {
        name_begin++;
    }
    while (name_end > name_begin && smlua_is_space(*(name_end - 1))) {
        name_end--;
    }
    if (!smlua_is_valid_identifier(name_begin, name_end)) {
        return false;
    }

    value_begin = eq + 1;
    value_end = line_end;
    while (value_begin < value_end && smlua_is_space(*value_begin)) {
        value_begin++;
    }
    while (value_end > value_begin && smlua_is_space(*(value_end - 1))) {
        value_end--;
    }
    if (value_begin >= value_end) {
        return false;
    }

    name_len = (size_t)(name_end - name_begin);
    if (name_len == 0 || name_len >= sizeof(name_buf)) {
        return false;
    }
    memcpy(name_buf, name_begin, name_len);
    name_buf[name_len] = '\0';

    if (*value_begin == '\'' && (value_end - value_begin) >= 2 && *(value_end - 1) == '\'') {
        const char *str_begin = value_begin + 1;
        size_t str_len = (size_t)((value_end - 1) - str_begin);
        if (str_len >= sizeof(value_buf)) {
            return false;
        }
        memcpy(value_buf, str_begin, str_len);
        value_buf[str_len] = '\0';
        lua_pushstring(L, value_buf);
        lua_setglobal(L, name_buf);
        return true;
    }

    value_len = (size_t)(value_end - value_begin);
    if (value_len == 0 || value_len >= sizeof(value_buf)) {
        return false;
    }
    memcpy(value_buf, value_begin, value_len);
    value_buf[value_len] = '\0';

    // Integers (decimal/hex with optional sign).
    {
        char *endptr = NULL;
        long long parsed = strtoll(value_buf, &endptr, 0);
        if (endptr != NULL && *endptr == '\0') {
            lua_pushinteger(L, (lua_Integer)parsed);
            lua_setglobal(L, name_buf);
            return true;
        }
    }

    // Floating-point literals.
    {
        char *endptr = NULL;
        double parsed = strtod(value_buf, &endptr);
        if (endptr != NULL && *endptr == '\0') {
            lua_pushnumber(L, (lua_Number)parsed);
            lua_setglobal(L, name_buf);
            return true;
        }
    }

    return false;
}

// Parses gSmluaConstants tail and binds only literal NAME=VALUE constants.
static void smlua_bind_literal_constants_tail(lua_State *L, const char *tail) {
    const char *cursor = tail;
    int applied = 0;
    int skipped = 0;

    if (L == NULL || tail == NULL) {
        return;
    }

    while (*cursor != '\0') {
        const char *line_begin = cursor;
        const char *line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
        }

        if (line_end > line_begin) {
            if (smlua_bind_tail_constant_literal(L, line_begin, line_end)) {
                applied++;
            } else {
                skipped++;
            }
        }

        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
    }

    smlua_logf("lua: literal constants bootstrap applied=%d skipped=%d", applied, skipped);
}

// Bootstraps gSmluaConstants on Wii U using a split strategy that avoids
// single huge parser chunks while preserving broad Lua compatibility.
static void smlua_bootstrap_constants_wiiu(lua_State *L) {
    static const char *kSplitMarker = "\nINSTANT_WARP_INDEX_START=0x00\n";
    const char *marker;
    size_t preamble_len = 0;
    const char *tail;
    bool chunked_ok = false;

    if (L == NULL) {
        return;
    }

    marker = strstr(gSmluaConstants, kSplitMarker);
    if (marker == NULL) {
        smlua_logf("lua: constants chunk marker missing, keeping minimal constants only");
        return;
    }

    // Execute the helper/preamble section as one bounded chunk so utility
    // functions and globals (VERSION_NUMBER, FONT_COUNT, etc.) are available.
    preamble_len = (size_t)(marker - gSmluaConstants);
    if (preamble_len > 0) {
        (void)smlua_exec_lua_chunk(L, gSmluaConstants, preamble_len, "@smlua_constants:preamble");
    }

    // Skip the marker's first newline so the constants tail starts with a valid assignment.
    tail = marker + 1;
    chunked_ok = smlua_exec_constants_tail_chunked(L, tail);
    if (!chunked_ok) {
        // Keep startup resilient when a subset of expression-based constants fail.
        smlua_bind_literal_constants_tail(L, tail);
    }
}

enum SmluaVfsLoadResult {
    SMLUA_VFS_LOAD_MISSING = 0,
    SMLUA_VFS_LOAD_OK = 1,
    SMLUA_VFS_LOAD_ERROR = 2,
};

// Attempts to compile a Lua chunk from the virtual filesystem for Wii U content paths.
static enum SmluaVfsLoadResult smlua_try_load_file_from_vfs(lua_State *L, const char *path) {
    uint64_t script_size = 0;
    void *script_data = NULL;
    char *script_text = NULL;
    int lua_status = LUA_OK;

    if (L == NULL || path == NULL) {
        return SMLUA_VFS_LOAD_MISSING;
    }

    script_data = fs_load_file(path, &script_size);
    if (script_data == NULL) {
        return SMLUA_VFS_LOAD_MISSING;
    }

    if (script_size > (uint64_t)(SIZE_MAX - 1)) {
        free(script_data);
        lua_pushfstring(L, "Lua script too large: %s", path);
        return SMLUA_VFS_LOAD_ERROR;
    }

    // Wii U/Cemu is sensitive to parser input lifetime and mode; use an explicit
    // NUL-terminated text buffer to keep script loading deterministic.
    script_text = malloc((size_t)script_size + 1);
    if (script_text == NULL) {
        free(script_data);
        lua_pushfstring(L, "Out of memory while loading script: %s", path);
        return SMLUA_VFS_LOAD_ERROR;
    }

    memcpy(script_text, script_data, (size_t)script_size);
    script_text[script_size] = '\0';
    free(script_data);

    lua_status = luaL_loadbufferx(L, script_text, (size_t)script_size, path, "t");
    free(script_text);
    if (lua_status != LUA_OK) {
        return SMLUA_VFS_LOAD_ERROR;
    }

    return SMLUA_VFS_LOAD_OK;
}

#define SMLUA_REQUIRE_REGISTRY_KEY "SM64.Require.loaded"
#define SMLUA_REQUIRE_FALLBACK_KEY "SM64.Require.fallback"
#define SMLUA_REQUIRE_LOADING_SENTINEL ((void *)-1)

// Returns true when normalized path stays inside the caller's mod root.
static bool smlua_path_is_within_root(const char *path, const char *root) {
    size_t root_len;
    if (path == NULL || root == NULL || root[0] == '\0') {
        return true;
    }
    root_len = strlen(root);
    return strncmp(path, root, root_len) == 0 &&
           (path[root_len] == '\0' || path[root_len] == '/');
}

// Derives mod root from script path (mods/<name>/...), or "mods" for single-file mods.
static bool smlua_get_mod_root_from_script(char *out_root, size_t out_size, const char *script_path) {
    const char *after_prefix;
    const char *first_slash;
    size_t root_len;

    if (out_root == NULL || out_size == 0 || script_path == NULL || strncmp(script_path, "mods/", 5) != 0) {
        return false;
    }

    after_prefix = script_path + 5;
    first_slash = strchr(after_prefix, '/');
    if (first_slash == NULL) {
        root_len = strlen("mods");
    } else {
        root_len = (size_t)(first_slash - script_path);
    }

    if (root_len + 1 > out_size) {
        return false;
    }
    memcpy(out_root, script_path, root_len);
    out_root[root_len] = '\0';
    return true;
}

// Normalizes "a/./b/../c" style paths and rejects paths that escape their root.
static bool smlua_normalize_virtual_path(char *out_path, size_t out_size, const char *path) {
    char work[SYS_MAX_PATH];
    char *segments[256];
    size_t segment_count = 0;
    char *token;
    size_t used = 0;

    int written;

    if (out_path == NULL || out_size == 0 || path == NULL || path[0] == '\0') {
        return false;
    }

    written = snprintf(work, sizeof(work), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(work)) {
        return false;
    }
    for (char *p = work; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }

    token = strtok(work, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            // Skip no-op segments.
        } else if (strcmp(token, "..") == 0) {
            if (segment_count == 0) {
                return false;
            }
            segment_count--;
        } else {
            if (segment_count >= (sizeof(segments) / sizeof(segments[0]))) {
                return false;
            }
            segments[segment_count++] = token;
        }
        token = strtok(NULL, "/");
    }

    if (segment_count == 0) {
        return false;
    }

    out_path[0] = '\0';
    for (size_t i = 0; i < segment_count; i++) {
        size_t len = strlen(segments[i]);
        if (used + len + (i > 0 ? 1 : 0) + 1 > out_size) {
            return false;
        }
        if (i > 0) {
            out_path[used++] = '/';
        }
        memcpy(out_path + used, segments[i], len);
        used += len;
        out_path[used] = '\0';
    }
    return true;
}

// Returns shared `loaded` table used to cache required module results by path.
static void smlua_require_get_loaded_table(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, SMLUA_REQUIRE_REGISTRY_KEY);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, SMLUA_REQUIRE_REGISTRY_KEY);
    }
}

// Resolves Lua require() names against the caller script directory inside mod scope.
static bool smlua_resolve_require_module_path(char *out_path, size_t out_size,
                                              const char *caller_script, const char *module_name) {
    char module_expr[SYS_MAX_PATH];
    char script_dir[SYS_MAX_PATH];
    char mod_root[SYS_MAX_PATH];
    char joined[SYS_MAX_PATH];
    char normalized[SYS_MAX_PATH];
    bool has_mod_root = false;
    bool has_slash = false;

    int written;

    if (out_path == NULL || out_size == 0 || caller_script == NULL || module_name == NULL) {
        return false;
    }
    if (!smlua_get_script_directory(script_dir, sizeof(script_dir), caller_script)) {
        return false;
    }
    written = snprintf(module_expr, sizeof(module_expr), "%s", module_name);
    if (written < 0 || (size_t)written >= sizeof(module_expr)) {
        return false;
    }
    if (module_expr[0] == '\0' || module_expr[0] == '/') {
        return false;
    }

    for (char *p = module_expr; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
        if (*p == '/') {
            has_slash = true;
        }
    }
    // Common Lua pattern: require("foo.bar") => foo/bar.lua.
    if (!has_slash) {
        for (char *p = module_expr; *p != '\0'; p++) {
            if (*p == '.') {
                *p = '/';
            }
        }
    }

    written = snprintf(joined, sizeof(joined), "%s/%s", script_dir, module_expr);
    if (written < 0 || (size_t)written >= sizeof(joined)) {
        return false;
    }
    if (!smlua_normalize_virtual_path(normalized, sizeof(normalized), joined)) {
        return false;
    }

    has_mod_root = smlua_get_mod_root_from_script(mod_root, sizeof(mod_root), caller_script);
    if (has_mod_root && !smlua_path_is_within_root(normalized, mod_root)) {
        return false;
    }

    if (smlua_path_has_suffix(normalized, ".lua") || smlua_path_has_suffix(normalized, ".luac")) {
        if (smlua_vfs_file_exists(normalized)) {
            snprintf(out_path, out_size, "%s", normalized);
            return true;
        }
    } else {
        char candidate[SYS_MAX_PATH];
        written = snprintf(candidate, sizeof(candidate), "%s.lua", normalized);
        if (written > 0 && (size_t)written < sizeof(candidate) &&
            (!has_mod_root || smlua_path_is_within_root(candidate, mod_root)) &&
            smlua_vfs_file_exists(candidate)) {
            snprintf(out_path, out_size, "%s", candidate);
            return true;
        }
        written = snprintf(candidate, sizeof(candidate), "%s.luac", normalized);
        if (written > 0 && (size_t)written < sizeof(candidate) &&
            (!has_mod_root || smlua_path_is_within_root(candidate, mod_root)) &&
            smlua_vfs_file_exists(candidate)) {
            snprintf(out_path, out_size, "%s", candidate);
            return true;
        }
    }

    return false;
}

// Lua require() compatibility for Wii U: scoped resolution + cache + recursion guard.
static int smlua_func_require(lua_State *L) {
    const char *module_name = luaL_checkstring(L, 1);
    const char *caller_script = smlua_get_caller_script_path(L);
    const char *previous_script = sActiveScriptPath;
    enum SmluaVfsLoadResult vfs_result = SMLUA_VFS_LOAD_MISSING;
    char resolved_path[SYS_MAX_PATH];
    char error_buffer[512];
    int loaded_index;
    int top_before_call;
    int result_ref = LUA_NOREF;

    if (!smlua_resolve_require_module_path(resolved_path, sizeof(resolved_path), caller_script, module_name)) {
        // Preserve stock Lua behavior for non-mod modules when available.
        lua_getfield(L, LUA_REGISTRYINDEX, SMLUA_REQUIRE_FALLBACK_KEY);
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            return 1;
        }
        lua_pop(L, 1);
        return luaL_error(L, "module '%s' not found from '%s'",
                          module_name != NULL ? module_name : "<null>",
                          caller_script != NULL ? caller_script : "<unknown>");
    }

    smlua_require_get_loaded_table(L);
    loaded_index = lua_gettop(L);

    lua_getfield(L, loaded_index, resolved_path);
    if (lua_touserdata(L, -1) == SMLUA_REQUIRE_LOADING_SENTINEL) {
        return luaL_error(L, "module '%s' is already loading", module_name);
    }
    if (!lua_isnil(L, -1)) {
        lua_remove(L, loaded_index);
        return 1;
    }
    lua_pop(L, 1);

    lua_pushlightuserdata(L, SMLUA_REQUIRE_LOADING_SENTINEL);
    lua_setfield(L, loaded_index, resolved_path);

    sActiveScriptPath = resolved_path;
    vfs_result = smlua_try_load_file_from_vfs(L, resolved_path);
    if (vfs_result == SMLUA_VFS_LOAD_MISSING && luaL_loadfile(L, resolved_path) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        snprintf(error_buffer, sizeof(error_buffer), "%s", error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setfield(L, loaded_index, resolved_path);
        sActiveScriptPath = previous_script;
        return luaL_error(L, "module '%s' failed to load: %s", module_name, error_buffer);
    }
    if (vfs_result == SMLUA_VFS_LOAD_ERROR) {
        const char *error = lua_tostring(L, -1);
        snprintf(error_buffer, sizeof(error_buffer), "%s", error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setfield(L, loaded_index, resolved_path);
        sActiveScriptPath = previous_script;
        return luaL_error(L, "module '%s' failed to load: %s", module_name, error_buffer);
    }

    top_before_call = lua_gettop(L) - 1;
    lua_sethook(L, smlua_script_budget_hook, LUA_MASKCOUNT, SMLUA_SCRIPT_LOAD_INSTRUCTION_BUDGET);
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        snprintf(error_buffer, sizeof(error_buffer), "%s", error != NULL ? error : "<unknown>");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setfield(L, loaded_index, resolved_path);
        lua_sethook(L, NULL, 0, 0);
        sActiveScriptPath = previous_script;
        return luaL_error(L, "module '%s' failed to run: %s", module_name, error_buffer);
    }
    lua_sethook(L, NULL, 0, 0);
    sActiveScriptPath = previous_script;

    if (lua_gettop(L) == top_before_call) {
        lua_pushboolean(L, 1);
    }
    if (lua_isnil(L, top_before_call + 1)) {
        lua_pushboolean(L, 1);
        lua_replace(L, top_before_call + 1);
    }

    lua_pushvalue(L, top_before_call + 1);
    result_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_rawgeti(L, LUA_REGISTRYINDEX, result_ref);
    lua_setfield(L, loaded_index, resolved_path);

    lua_settop(L, 0);
    lua_rawgeti(L, LUA_REGISTRYINDEX, result_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, result_ref);
    return 1;
}

// Installs custom require() so Lua mods can load sibling modules in VFS scope.
static void smlua_bind_require_system(lua_State *L) {
    if (L == NULL) {
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, SMLUA_REQUIRE_FALLBACK_KEY);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getglobal(L, "require");
        if (lua_isfunction(L, -1)) {
            lua_setfield(L, LUA_REGISTRYINDEX, SMLUA_REQUIRE_FALLBACK_KEY);
        } else {
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    lua_pushcfunction(L, smlua_func_require);
    lua_setglobal(L, "require");
}

// Runs one Lua file and prints a readable error without aborting the game.
static void smlua_run_file(const char *path) {
    const char *previous_script = sActiveScriptPath;
    enum SmluaVfsLoadResult vfs_result = SMLUA_VFS_LOAD_MISSING;
    bool log_main_script = false;

    if (path == NULL || sLuaState == NULL) {
        return;
    }

    log_main_script = strstr(path, "/main.lua") != NULL;
#ifdef TARGET_WII_U
    // Keep startup diagnostics visible without flooding OSConsole every helper script.
    if (log_main_script) {
        WHBLogPrintf("lua: begin main '%s'", path);
    }
#endif

    smlua_logf("lua: running '%s'", path);
    sActiveScriptPath = path;
    vfs_result = smlua_try_load_file_from_vfs(sLuaState, path);
    if (vfs_result == SMLUA_VFS_LOAD_MISSING && luaL_loadfile(sLuaState, path) != LUA_OK) {
        const char *error = lua_tostring(sLuaState, -1);
        smlua_logf("lua: failed to load '%s': %s", path, error != NULL ? error : "<unknown>");
#ifdef TARGET_WII_U
        if (log_main_script) {
            WHBLogPrintf("lua: failed load main '%s': %s", path, error != NULL ? error : "<unknown>");
        }
#endif
        lua_pop(sLuaState, 1);
        sActiveScriptPath = previous_script;
        return;
    }

    if (vfs_result == SMLUA_VFS_LOAD_ERROR) {
        const char *error = lua_tostring(sLuaState, -1);
        smlua_logf("lua: failed to load '%s': %s", path, error != NULL ? error : "<unknown>");
#ifdef TARGET_WII_U
        if (log_main_script) {
            WHBLogPrintf("lua: failed vfs main '%s': %s", path, error != NULL ? error : "<unknown>");
        }
#endif
        lua_pop(sLuaState, 1);
        sActiveScriptPath = previous_script;
        return;
    }

    lua_sethook(sLuaState, smlua_script_budget_hook, LUA_MASKCOUNT,
                SMLUA_SCRIPT_LOAD_INSTRUCTION_BUDGET);
    if (lua_pcall(sLuaState, 0, 0, 0) != LUA_OK) {
        const char *error = lua_tostring(sLuaState, -1);
        smlua_logf("lua: failed to run '%s': %s", path, error != NULL ? error : "<unknown>");
#ifdef TARGET_WII_U
        if (log_main_script) {
            WHBLogPrintf("lua: failed run main '%s': %s", path, error != NULL ? error : "<unknown>");
        }
#endif
        lua_pop(sLuaState, 1);
    } else {
        smlua_logf("lua: completed '%s'", path);
#ifdef TARGET_WII_U
        if (log_main_script) {
            WHBLogPrintf("lua: completed main '%s'", path);
        }
#endif
    }
    lua_sethook(sLuaState, NULL, 0, 0);
    sActiveScriptPath = previous_script;
}

// Loads sibling Lua helpers before `main.lua` so multi-file mods can initialize.
static void smlua_run_script_with_companions(const char *script_path) {
    static const char *MAIN_SUFFIX = "/main.lua";
    char base_path[SYS_MAX_PATH];
    fs_pathlist_t files;
    const char **companions;
    int companion_count = 0;
    int fallback_count = 0;
    size_t companion_cap = 0;
    size_t script_len;
    size_t suffix_len = 0;

    if (script_path == NULL) {
        return;
    }

#ifdef TARGET_WII_U
    // Wii U/Cemu path: prefer deterministic bundled fallback companion lists to
    // avoid intermittent VFS directory-walk stalls during startup.
    if (smlua_path_has_suffix(script_path, MAIN_SUFFIX)) {
        fallback_count = smlua_run_known_companion_fallbacks(script_path);
        if (fallback_count > 0) {
            WHBLogPrintf("lua: loaded %d helper scripts for '%s' (fallback)", fallback_count, script_path);
            smlua_run_file(script_path);
            return;
        }
    }
#endif

    if (!smlua_path_has_suffix(script_path, MAIN_SUFFIX)) {
        smlua_run_file(script_path);
        return;
    }

    script_len = strlen(script_path);
    suffix_len = strlen(MAIN_SUFFIX);
    if (script_len <= suffix_len || script_len - suffix_len >= sizeof(base_path)) {
        smlua_run_file(script_path);
        return;
    }

    memcpy(base_path, script_path, script_len - suffix_len);
    base_path[script_len - suffix_len] = '\0';

    files = fs_enumerate(base_path, false);
    companion_cap = (files.numpaths > 0) ? (size_t)files.numpaths : 1;
    companions = malloc(companion_cap * sizeof(const char *));
    if (companions == NULL) {
        fs_pathlist_free(&files);
        smlua_run_file(script_path);
        return;
    }

    for (int i = 0; i < files.numpaths; i++) {
        const char *path = files.paths[i];
        if (!smlua_path_has_suffix(path, ".lua")) {
            continue;
        }
        if (strcmp(path, script_path) == 0) {
            continue;
        }
        companions[companion_count++] = path;
    }

    smlua_logf("lua: companion scan '%s' found %d helper scripts", base_path, companion_count);
    if (companion_count > 0) {
        qsort(companions, (size_t)companion_count, sizeof(const char *), smlua_compare_paths);
        for (int i = 0; i < companion_count; i++) {
            smlua_run_file(companions[i]);
        }
    } else {
        fallback_count = smlua_run_known_companion_fallbacks(script_path);
        if (fallback_count > 0) {
            smlua_logf("lua: used %d fallback companion scripts for '%s'", fallback_count, script_path);
        }
    }

#ifdef TARGET_WII_U
    // Wii U/Cemu startup currently hits a black-screen stall in this cleanup path
    // right after companion helper execution. Keep boot reliable by deferring these
    // tiny init-time frees; this leaks a small amount once per process launch.
    (void)companions;
    (void)files;
#else
    free(companions);
    fs_pathlist_free(&files);
#endif
    smlua_run_file(script_path);
}

#ifdef TARGET_WII_U
// Logs hook/global snapshot so Cemu traces can confirm mods are truly active.
static void smlua_log_runtime_hook_snapshot(lua_State *L, const char *phase) {
    int version_number = -1;
    bool char_select_exists = false;
    int active_mods = 0;

    if (L == NULL) {
        return;
    }

    lua_getglobal(L, "VERSION_NUMBER");
    if (lua_isinteger(L, -1)) {
        version_number = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "charSelectExists");
    char_select_exists = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getglobal(L, "gActiveMods");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            active_mods++;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    WHBLogPrintf("lua: snapshot[%s] ver=%d charSelectExists=%d activeMods=%d hooks{mods_loaded=%d update=%d mario_update=%d before_phys=%d hud=%d hud_behind=%d}",
                 phase != NULL ? phase : "?", version_number, char_select_exists ? 1 : 0, active_mods,
                 smlua_get_event_hook_count(HOOK_ON_MODS_LOADED),
                 smlua_get_event_hook_count(HOOK_UPDATE),
                 smlua_get_event_hook_count(HOOK_MARIO_UPDATE),
                 smlua_get_event_hook_count(HOOK_BEFORE_PHYS_STEP),
                 smlua_get_event_hook_count(HOOK_ON_HUD_RENDER),
                 smlua_get_event_hook_count(HOOK_ON_HUD_RENDER_BEHIND));
}
#endif

// Creates the Lua VM and loads active built-in scripts from the mod set.
void smlua_init(void) {
    smlua_logf("lua: init begin");
    smlua_shutdown();
    smlua_reset_lighting_state();
    smlua_reset_sequence_aliases();
    smlua_reset_custom_animations();
    smlua_reset_model_overrides();
    smlua_reset_dialog_overrides();
    sLuaAudioPoolCount = 0;
    memset(sLuaAudioPool, 0, sizeof(sLuaAudioPool));
    memset(sLuaCustomActionNextIndex, 0, sizeof(sLuaCustomActionNextIndex));

    sLuaState = luaL_newstate();
    if (sLuaState == NULL) {
        smlua_logf("lua: failed to allocate Lua state");
        return;
    }

    luaL_openlibs(sLuaState);
    smlua_bind_require_system(sLuaState);
    smlua_bind_math_helpers(sLuaState);
    smlua_bind_compat_metatables(sLuaState);
    smlua_bind_cobject(sLuaState);
    smlua_cobject_init_globals(sLuaState);
    smlua_bind_hooks(sLuaState);
    smlua_bind_minimal_constants(sLuaState);
    smlua_bind_minimal_functions(sLuaState);
    smlua_bind_minimal_globals(sLuaState);
#ifdef TARGET_WII_U
    smlua_bind_wiiu_preamble_compat(sLuaState);
    // Use split bootstrap for broad Co-op DX constant coverage without loading
    // the full autogen blob in one parser call (which was unstable on Wii U/Cemu).
    smlua_bootstrap_constants_wiiu(sLuaState);
#else
    if (luaL_dostring(sLuaState, gSmluaConstants) != LUA_OK) {
        const char *error = lua_tostring(sLuaState, -1);
        smlua_logf("lua: constants bootstrap failed: %s", error != NULL ? error : "<unknown>");
        lua_pop(sLuaState, 1);
    }
#endif
    // Re-apply Wii U compatibility bindings after constants bootstrap script mutates globals.
    smlua_bind_require_system(sLuaState);
    smlua_bind_minimal_functions(sLuaState);
    smlua_bind_minimal_globals(sLuaState);
    smlua_set_global_function(sLuaState, "gVec3fZero", smlua_func_gvec3f_zero);
    smlua_set_global_function(sLuaState, "vec3f_copy", smlua_func_vec3f_copy);
    smlua_bind_wiiu_mod_runtime_compat(sLuaState);
    smlua_ensure_singleplayer_tables(sLuaState);
    smlua_update_singleplayer_network_snapshot(sLuaState);

    size_t script_count = mods_get_active_script_count();
    smlua_logf("lua: loading %u root scripts", (unsigned)script_count);
    for (size_t i = 0; i < script_count; i++) {
        const char *root_script = mods_get_active_script_path(i);
#ifdef TARGET_WII_U
        // Keep startup visibility for single-file scripts that do not emit
        // main-script begin/completed markers.
        WHBLogPrintf("lua: root[%u] '%s'", (unsigned)i,
                     root_script != NULL ? root_script : "<null>");
#endif
        smlua_run_script_with_companions(root_script);
    }

#ifdef TARGET_WII_U
    smlua_log_runtime_hook_snapshot(sLuaState, "pre_mods_loaded");
#endif
    smlua_call_event_hooks(HOOK_ON_MODS_LOADED);
#ifdef TARGET_WII_U
    smlua_log_runtime_hook_snapshot(sLuaState, "post_mods_loaded");
#endif
    smlua_refresh_mod_overlay_lines();

    smlua_logf("lua: initialized (%u scripts)", (unsigned)script_count);
}

// Executes optional global on_update() hook once per game frame.
void smlua_update(void) {
    if (sLuaState == NULL) {
        return;
    }

    if (sLuaUpdateCounter == 0) {
        smlua_logf("lua: first update tick");
    }
    sLuaUpdateCounter++;

    smlua_cobject_update_globals(sLuaState);
    smlua_ensure_singleplayer_tables(sLuaState);
    smlua_update_singleplayer_network_snapshot(sLuaState);
    smlua_call_event_hooks(HOOK_UPDATE);

    lua_getglobal(sLuaState, "on_update");
    if (lua_isfunction(sLuaState, -1)) {
        lua_sethook(sLuaState, smlua_update_budget_hook, LUA_MASKCOUNT,
                    SMLUA_UPDATE_INSTRUCTION_BUDGET);
        if (lua_pcall(sLuaState, 0, 0, 0) != LUA_OK) {
            const char *error = lua_tostring(sLuaState, -1);
            smlua_logf("lua: on_update failed: %s", error != NULL ? error : "<unknown>");
            lua_pop(sLuaState, 1);
        }
        lua_sethook(sLuaState, NULL, 0, 0);
    } else {
        lua_pop(sLuaState, 1);
    }

    smlua_call_behavior_hooks();
    smlua_update_custom_dnc_objects(sLuaState);
    smlua_poll_sync_table_change_hooks();
}

// Destroys the Lua VM and releases all script-managed resources.
void smlua_shutdown(void) {
    if (sLuaState != NULL) {
        smlua_call_event_hooks(HOOK_ON_EXIT);
        smlua_clear_hooks(sLuaState);
        lua_close(sLuaState);
        sLuaState = NULL;
    }
    smlua_reset_lighting_state();
    smlua_reset_sequence_aliases();
    smlua_reset_custom_animations();
    smlua_reset_model_overrides();
    smlua_reset_dialog_overrides();
    sLuaAudioPoolCount = 0;
    sLuaUpdateCounter = 0;
    memset(sLuaAudioPool, 0, sizeof(sLuaAudioPool));
    memset(sLuaCustomActionNextIndex, 0, sizeof(sLuaCustomActionNextIndex));
    sActiveScriptPath = NULL;
}
