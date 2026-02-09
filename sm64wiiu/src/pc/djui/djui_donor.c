#include "djui_donor.h"

#include <stdbool.h>

#include "sm64.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/print.h"
#include "game/segment2.h"
#include "djui_cursor.h"
#include "djui_interactable.h"
#include "djui_language.h"
#include "djui_panel_main.h"
#include "djui_panel.h"
#include "djui_root.h"
#include "djui_theme.h"
#include "djui_unicode.h"
#include "pc/configfile.h"

extern bool gDjuiInMainMenu;
extern bool gDjuiDisabled;

static bool sDonorInitialized = false;
static u16 sLastButtons = 0;
static bool sDonorMenuWarpPending = true;

#define DJUI_MENU_DEST_LEVEL LEVEL_CASTLE_GROUNDS
#define DJUI_MENU_DEST_AREA 1
#define DJUI_MENU_DEST_NODE 0x0A

// level_update.c does not expose this in a public header yet.
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);

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
    sDonorInitialized = true;
    sLastButtons = 0;
    sDonorMenuWarpPending = true;
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
    sDonorInitialized = false;
    sLastButtons = 0;
    sDonorMenuWarpPending = true;
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
    bool needsMenuLevelWarp = false;

    if (!sDonorInitialized || !gDjuiInMainMenu || gMarioState == NULL) {
        return;
    }

    if (gWarpTransition.isActive) {
        return;
    }

    needsMenuLevelWarp = (gCurrLevelNum != DJUI_MENU_DEST_LEVEL
                          || gCurrAreaIndex != DJUI_MENU_DEST_AREA);

    if (needsMenuLevelWarp) {
        sDonorMenuWarpPending = true;
    }

    // Do not force a warp when we're already in the menu destination level/area.
    // This avoids a visible startup camera jump before the menu pose is applied.
    if (sDonorMenuWarpPending) {
        if (needsMenuLevelWarp) {
            initiate_warp(DJUI_MENU_DEST_LEVEL, DJUI_MENU_DEST_AREA, DJUI_MENU_DEST_NODE, 0);
            return;
        }
        sDonorMenuWarpPending = false;
    }

    set_mario_action(gMarioState, ACT_IDLE, 0);

    gMarioState->vel[0] = 0.0f;
    gMarioState->vel[1] = 0.0f;
    gMarioState->vel[2] = 0.0f;
    gMarioState->forwardVel = 0.0f;
    gMarioState->input = 0;
    gMarioState->intendedMag = 0.0f;
    gMarioState->health = 0x880;

    gMarioState->pos[0] = -1328.0f;
    gMarioState->pos[1] = 260.0f;
    gMarioState->pos[2] = 4664.0f;
    gMarioState->faceAngle[1] = 0;

    gLakituState.curPos[0] = -1328.0f;
    gLakituState.curPos[1] = 390.0f;
    gLakituState.curPos[2] = 6064.0f;
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
    if (gDjuiRoot != NULL) {
        djui_base_render(&gDjuiRoot->base);
    }
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
    djui_donor_build_main_panel();
}

void djui_donor_close_main_menu(void) {
    djui_panel_shutdown();
    gInteractableOverridePad = false;
    gDjuiPanelMainCreated = false;
    gDjuiInMainMenu = false;
    sDonorMenuWarpPending = true;
}
