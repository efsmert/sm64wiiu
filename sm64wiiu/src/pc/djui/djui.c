#include "djui.h"

#include "sm64.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/mario.h"

#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

bool gDjuiInMainMenu = false;
bool gDjuiDisabled = false;

static bool sDjuiInitialized = false;
static bool sDjuiRenderLogged = false;
static bool sDjuiMenuWarpPending = false;

#define DJUI_MENU_DEST_LEVEL LEVEL_CASTLE_GROUNDS
#define DJUI_MENU_DEST_AREA 1
#define DJUI_MENU_DEST_NODE 0x0A

// level_update.c does not expose this in a public header yet.
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);

// Phase-1 scaffold: lifecycle + render hook only.
void djui_init(void) {
    sDjuiInitialized = true;
    sDjuiRenderLogged = false;
#ifdef TARGET_WII_U
    WHBLogPrint("djui: init");
#endif
}

void djui_init_late(void) {
#ifdef TARGET_WII_U
    WHBLogPrint("djui: init_late");
#endif
}

void djui_shutdown(void) {
    sDjuiInitialized = false;
    gDjuiInMainMenu = false;
    sDjuiRenderLogged = false;
    sDjuiMenuWarpPending = false;
#ifdef TARGET_WII_U
    WHBLogPrint("djui: shutdown");
#endif
}

void djui_open_main_menu(void) {
    gDjuiInMainMenu = true;
    sDjuiMenuWarpPending = true;
}

void djui_close_main_menu(void) {
    gDjuiInMainMenu = false;
    sDjuiMenuWarpPending = false;
}

// Temporary debug toggle until full panel input routing is ported.
void djui_update(void) {
    if (!sDjuiInitialized || gPlayer1Controller == NULL) {
        return;
    }

    if ((gPlayer1Controller->buttonPressed & START_BUTTON)
        && (gPlayer1Controller->buttonDown & L_TRIG)
        && (gPlayer1Controller->buttonDown & R_TRIG)) {
        if (gDjuiInMainMenu) {
            djui_close_main_menu();
#ifdef TARGET_WII_U
            WHBLogPrint("djui: main menu disabled (L+R+START)");
#endif
        } else {
            djui_open_main_menu();
#ifdef TARGET_WII_U
            WHBLogPrint("djui: main menu enabled (L+R+START)");
#endif
        }
    }
}

// Donor-style menu-scene control (castle grounds) while DJUI main menu is active.
void djui_update_menu_level(void) {
    if (!sDjuiInitialized || !gDjuiInMainMenu || gMarioState == NULL) {
        return;
    }

    if (gWarpTransition.isActive) {
        return;
    }

    if (sDjuiMenuWarpPending
        || gCurrLevelNum != DJUI_MENU_DEST_LEVEL
        || gCurrAreaIndex != DJUI_MENU_DEST_AREA) {
        initiate_warp(DJUI_MENU_DEST_LEVEL, DJUI_MENU_DEST_AREA, DJUI_MENU_DEST_NODE, 0);
        sDjuiMenuWarpPending = false;
        return;
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

void djui_render(void) {
    if (!sDjuiInitialized || gDjuiDisabled) {
        return;
    }

#ifdef TARGET_WII_U
    if (!sDjuiRenderLogged) {
        WHBLogPrint("djui: render hook active");
        sDjuiRenderLogged = true;
    }
#endif
}
