#include "djui_donor.h"

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "sm64.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/print.h"
#include "game/segment2.h"
#include "game/sound_init.h"
#include "djui_cursor.h"
#include "djui_ctx_display.h"
#include "djui_fps_display.h"
#include "djui_interactable.h"
#include "djui_language.h"
#include "djui_lua_profiler.h"
#include "djui_panel_main.h"
#include "djui_panel_menu_options.h"
#include "djui_panel.h"
#include "djui_root.h"
#include "djui_theme.h"
#include "djui_unicode.h"
#include "pc/configfile.h"
#include "seq_ids.h"

extern bool gDjuiInMainMenu;
extern bool gDjuiDisabled;

static bool sDonorInitialized = false;
static u16 sLastButtons = 0;
static bool sDonorMenuWarpPending = true;
static bool sDonorRandomSeeded = false;
static bool sDonorRandomLevelLatched = false;
static u8 sDonorRandomLevelIndex = 0;
static bool sDonorStaffRollTriggered = false;
static bool sDonorLastMenuRandom = false;
static bool sDonorLastMenuStaffRoll = false;
static unsigned int sDonorLastMenuLevel = 0;

#define WARP_TYPE_NOT_WARPING 0
#define DJUI_MENU_LEVEL_COUNT 18
#define DJUI_MENU_RANDOM_MIN 1
#define DJUI_MENU_RANDOM_MAX 17

struct DjuiMenuPreset {
    s16 level;
    s16 area;
    s16 node;
    f32 marioX;
    f32 marioY;
    f32 marioZ;
    f32 camX;
    f32 camY;
    f32 camZ;
    s16 yaw;
};

static const struct DjuiMenuPreset sDjuiMenuPresets[DJUI_MENU_LEVEL_COUNT] = {
    { LEVEL_CASTLE_GROUNDS, 1, 0x0A, -1328.0f,   260.0f,  4664.0f, -1328.0f,   390.0f,  6064.0f,      0 },
    { LEVEL_BOB,            1, 0x0A,  7008.0f,   864.0f,  1943.0f,  7909.0f,  1064.0f,  2843.0f,  0x2000 },
    { LEVEL_WF,             1, 0x0A, -2904.0f,  2560.0f,  -327.0f, -4504.0f,  2760.0f,  -777.0f, -15536 },
    { LEVEL_WMOTR,          1, 0x0A,  3548.0f, -2738.0f,  4663.0f,  3548.0f, -2438.0f,  6063.0f,      0 },
    { LEVEL_JRB,            1, 0x0A,  3639.0f,  1536.0f,  6202.0f,  5039.0f,  1736.0f,  6402.0f,      0 },
    { LEVEL_SSL,            1, 0x0A, -2048.0f,   256.0f,   961.0f, -2048.0f,   356.0f,  2461.0f,      0 },
    { LEVEL_TTM,            1, 0x0A,  2488.0f,  1460.0f,  2011.0f,  3488.0f,  1763.0f,  3411.0f,  0x1000 },
    { LEVEL_SL,             1, 0x0A,  5494.0f,  1024.0f,   443.0f,  6994.0f,  1124.0f,   443.0f,  0x4000 },
    { LEVEL_BBH,            1, 0x0A,   666.0f,  -204.0f,  5303.0f,   666.0f,  -204.0f,  6803.0f,      0 },
    { LEVEL_LLL,            1, 0x0A, -2376.0f,   638.0f,   956.0f, -3576.0f,   938.0f,  1576.0f, -0x2800 },
    { LEVEL_THI,            2, 0x0A, -1010.0f,   341.0f,  -324.0f, -2246.0f,   431.0f,  -324.0f, -0x4000 },
    { LEVEL_HMC,            1, 0x0A, -3600.0f, -4279.0f,  3616.0f, -6000.0f, -2938.0f,   600.0f, -0x6000 },
    { LEVEL_CCM,            1, 0x0A, -1127.0f, -3580.0f,  6162.0f, -1330.0f, -2830.0f,  9099.0f, -0x1000 },
    { LEVEL_RR,             1, 0x0A,  1418.0f,  3167.0f, -2349.0f, -1518.0f,  4567.0f, -4549.0f, -0x6000 },
    { LEVEL_BITDW,          1, 0x0A, -4507.0f,  1126.0f,  -285.0f, -2507.0f,  2126.0f,  -285.0f,      0 },
    { LEVEL_PSS,            1, 0x0A, -4729.0f, -3057.0f, -3025.0f, -2729.0f, -1557.0f, -5025.0f,  0x5000 },
    { LEVEL_TTC,            1, 0x0A,  -645.0f,     0.0f,  -750.0f,  2500.0f,   570.0f,  -240.0f,  0x2000 },
    { LEVEL_WDW,            1, 0x0A, -2684.0f,  3328.0f,  3000.0f, -4002.0f,  4000.0f,  4622.0f, -0x1C34 },
};

// level_update.c does not expose this in a public header yet.
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);

static u8 djui_donor_pick_random_level_index(void) {
    if (!sDonorRandomSeeded) {
        sDonorRandomSeeded = true;
        srand((unsigned int)time(NULL));
    }
    return (u8)((rand() % (DJUI_MENU_RANDOM_MAX - DJUI_MENU_RANDOM_MIN + 1)) + DJUI_MENU_RANDOM_MIN);
}

static u8 djui_donor_resolve_menu_level_index(void) {
    if (configMenuRandom) {
        if (!sDonorRandomLevelLatched) {
            sDonorRandomLevelIndex = djui_donor_pick_random_level_index();
            sDonorRandomLevelLatched = true;
        }
        return sDonorRandomLevelIndex;
    }

    sDonorRandomLevelLatched = false;
    if (configMenuLevel >= DJUI_MENU_LEVEL_COUNT) {
        configMenuLevel = 0;
    }
    return (u8)configMenuLevel;
}

static void djui_donor_apply_menu_music(const struct DjuiMenuPreset* preset) {
    int soundCount = MAIN_MENU_SOUND_COUNT;
    int soundIndex = (int)configMenuSound;

    if (soundIndex < 0 || soundIndex >= soundCount) {
        configMenuSound = 0;
        soundIndex = 0;
    }

    stop_cap_music();
    reset_volume();
    disable_background_sound();

    if (gMainMenuSounds[soundIndex].sound == STAGE_MUSIC) {
        if (configMenuStaffRoll) {
            configMenuSound = 0;
            soundIndex = 0;
        }
        if (preset->level == LEVEL_CASTLE_GROUNDS) {
            set_background_music(0, SEQ_MENU_FILE_SELECT, 0);
        } else if (gCurrentArea != NULL) {
            set_background_music(gCurrentArea->musicParam, gCurrentArea->musicParam2, 0);
        } else {
            set_background_music(0, SEQ_MENU_FILE_SELECT, 0);
        }
    } else {
        set_background_music(0, gMainMenuSounds[soundIndex].sound, 0);
    }
}

static void djui_donor_build_main_panel(void) {
    if (gDjuiRoot == NULL || djui_panel_is_active()) {
        return;
    }
    djui_panel_main_create(NULL);
}

void djui_donor_init(void) {
    if (sDonorInitialized) {
        return;
    }

    djui_unicode_init();
    if (!djui_language_init(configLanguage)) {
        djui_language_init("English");
    }
    djui_themes_init();

    djui_root_create();
    djui_cursor_create();
    djui_fps_display_create();
    djui_ctx_display_create();
    djui_lua_profiler_create();
    sDonorInitialized = true;
    sLastButtons = 0;
    sDonorMenuWarpPending = true;
    sDonorRandomLevelLatched = false;
    sDonorStaffRollTriggered = false;
    sDonorLastMenuRandom = configMenuRandom;
    sDonorLastMenuStaffRoll = configMenuStaffRoll;
    sDonorLastMenuLevel = configMenuLevel;
    gDjuiInMainMenu = true;
}

void djui_donor_init_late(void) {
    if (!sDonorInitialized) {
        return;
    }
    djui_donor_build_main_panel();
}

void djui_donor_shutdown(void) {
    djui_panel_shutdown();
    if (gDjuiRoot != NULL) {
        djui_base_destroy(&gDjuiRoot->base);
    }
    gInteractableOverridePad = false;
    gDjuiPanelMainCreated = false;
    djui_fps_display_destroy();
    djui_ctx_display_destroy();
    djui_lua_profiler_destroy();
    sDonorInitialized = false;
    sLastButtons = 0;
    sDonorMenuWarpPending = true;
    sDonorRandomLevelLatched = false;
    sDonorStaffRollTriggered = false;
}

void djui_donor_update(void) {
    u16 down = 0;
    u16 pressed = 0;

    if (!sDonorInitialized) {
        return;
    }

    gInteractableOverridePad = (gDjuiInMainMenu && djui_panel_is_active());

    if (gInteractableOverridePad) {
        down = gInteractablePad.button;
    } else if (gPlayer1Controller != NULL) {
        down = gPlayer1Controller->buttonDown;
    }

    if (!gDjuiInMainMenu) {
        sLastButtons = down;
        return;
    }

    pressed = (u16)(down & (u16)~sLastButtons);
    sLastButtons = down;

    if (pressed & START_BUTTON) {
        djui_donor_close_main_menu();
        return;
    }
}

void djui_donor_update_menu_level(void) {
    u8 levelIndex = 0;
    const struct DjuiMenuPreset* preset = NULL;
    bool needsMenuLevelWarp = false;
    bool levelMismatch = false;
    bool areaMismatch = false;

    if (!sDonorInitialized || !gDjuiInMainMenu || gMarioState == NULL) {
        return;
    }

    if (gWarpTransition.isActive) {
        return;
    }

    if (sDonorLastMenuRandom != configMenuRandom
        || sDonorLastMenuStaffRoll != configMenuStaffRoll
        || sDonorLastMenuLevel != configMenuLevel) {
        sDonorMenuWarpPending = true;
        sDonorRandomLevelLatched = false;
        sDonorStaffRollTriggered = false;
    }
    sDonorLastMenuRandom = configMenuRandom;
    sDonorLastMenuStaffRoll = configMenuStaffRoll;
    sDonorLastMenuLevel = configMenuLevel;

    // level_update.c keeps play-mode constants private; 0 is PLAY_MODE_NORMAL.
    if (sCurrPlayMode != 0 || gCurrentArea == NULL) {
        return;
    }

    levelIndex = djui_donor_resolve_menu_level_index();
    if (levelIndex >= DJUI_MENU_LEVEL_COUNT) {
        levelIndex = 0;
    }
    preset = &sDjuiMenuPresets[levelIndex];

    djui_donor_apply_menu_music(preset);

    if (configMenuStaffRoll) {
        if (!sDonorStaffRollTriggered) {
            warp_credits();
            level_trigger_warp(gMarioState, WARP_OP_CREDITS_NEXT);
            sDonorStaffRollTriggered = true;
        }
        return;
    }
    sDonorStaffRollTriggered = false;
    gCurrCreditsEntry = NULL;

    levelMismatch = (gCurrLevelNum != preset->level);
    areaMismatch = (gCurrAreaIndex != preset->area);
    needsMenuLevelWarp = levelMismatch || areaMismatch;

    if (needsMenuLevelWarp) {
        sDonorMenuWarpPending = true;
    }

    // Donor-like behavior: transition levels via level_update's deferred change path.
    // Reserve direct initiate_warp() only for same-level area switches (e.g. THI area 2).
    if (sDonorMenuWarpPending && needsMenuLevelWarp) {
        if (levelMismatch) {
            gChangeLevelTransition = preset->level;
            return;
        }

        if (areaMismatch
            && sWarpDest.type == WARP_TYPE_NOT_WARPING
            && sDelayedWarpOp == WARP_OP_NONE) {
            initiate_warp(preset->level, preset->area, preset->node, 0);
        }
        return;
    }
    sDonorMenuWarpPending = false;

    gMarioState->vel[0] = 0.0f;
    gMarioState->vel[1] = 0.0f;
    gMarioState->vel[2] = 0.0f;
    gMarioState->forwardVel = 0.0f;
    gMarioState->input = 0;
    gMarioState->intendedMag = 0.0f;
    gMarioState->health = 0x880;

    gMarioState->pos[0] = preset->marioX;
    gMarioState->pos[1] = preset->marioY;
    gMarioState->pos[2] = preset->marioZ;
    gMarioState->faceAngle[1] = preset->yaw;

    gLakituState.curFocus[0] = preset->marioX;
    gLakituState.curFocus[1] = preset->marioY;
    gLakituState.curFocus[2] = preset->marioZ;
    gLakituState.goalFocus[0] = preset->marioX;
    gLakituState.goalFocus[1] = preset->marioY;
    gLakituState.goalFocus[2] = preset->marioZ;
    gLakituState.focus[0] = preset->marioX;
    gLakituState.focus[1] = preset->marioY;
    gLakituState.focus[2] = preset->marioZ;
    gLakituState.curPos[0] = preset->camX;
    gLakituState.curPos[1] = preset->camY;
    gLakituState.curPos[2] = preset->camZ;
    gLakituState.goalPos[0] = preset->camX;
    gLakituState.goalPos[1] = preset->camY;
    gLakituState.goalPos[2] = preset->camZ;
    gLakituState.pos[0] = preset->camX;
    gLakituState.pos[1] = preset->camY;
    gLakituState.pos[2] = preset->camZ;
    gLakituState.focHSpeed = 0.0f;
    gLakituState.focVSpeed = 0.0f;
    gLakituState.posHSpeed = 0.0f;
    gLakituState.posVSpeed = 0.0f;
    gLakituState.yaw = gMarioState->faceAngle[1] + 0x8000;
    gLakituState.nextYaw = gMarioState->faceAngle[1] + 0x8000;

    if (gMarioState->controller != NULL) {
        gMarioState->controller->buttonDown = 0;
        gMarioState->controller->buttonPressed = 0;
        gMarioState->controller->rawStickX = 0;
        gMarioState->controller->rawStickY = 0;
        gMarioState->controller->stickX = 0;
        gMarioState->controller->stickY = 0;
        gMarioState->controller->stickMag = 0.0f;
    }
}

void djui_donor_render(void) {
    if (!sDonorInitialized || !gDjuiInMainMenu || gDjuiDisabled) {
        return;
    }

    create_dl_ortho_matrix();
    djui_gfx_displaylist_begin();
    djui_reset_hud_params();
    djui_panel_update();
    djui_lua_profiler_render();
    if (gDjuiRoot != NULL) {
        djui_base_render(&gDjuiRoot->base);
    }
    djui_fps_display_render();
    djui_ctx_display_render();
    djui_cursor_update();
    extern u8 gRenderingInterpolated;
    if (!gRenderingInterpolated) {
        djui_interactable_update();
    }
    djui_gfx_displaylist_end();
}

void djui_donor_open_main_menu(void) {
    gDjuiInMainMenu = true;
    sDonorMenuWarpPending = true;
    sDonorRandomLevelLatched = false;
    sDonorStaffRollTriggered = false;
    djui_donor_build_main_panel();
}

void djui_donor_close_main_menu(void) {
    djui_panel_shutdown();
    gInteractableOverridePad = false;
    gDjuiPanelMainCreated = false;
    gDjuiInMainMenu = false;
    sDonorMenuWarpPending = true;
    sDonorRandomLevelLatched = false;
    sDonorStaffRollTriggered = false;
}
