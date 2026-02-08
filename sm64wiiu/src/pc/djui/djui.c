#include "djui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "sm64.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/print.h"
#include "pc/mods/mods.h"

#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

bool gDjuiInMainMenu = true;
bool gDjuiDisabled = false;

enum DjuiMenuPage {
    DJUI_MENU_PAGE_MAIN = 0,
    DJUI_MENU_PAGE_LOBBY = 1,
    DJUI_MENU_PAGE_MODS = 2,
};

static bool sDjuiInitialized = false;
static bool sDjuiRenderLogged = false;
static bool sDjuiMenuWarpPending = true;
static enum DjuiMenuPage sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
static s32 sDjuiMainSelection = 0;
static s32 sDjuiLobbySelection = 0;
static s32 sDjuiModsScroll = 0;
static char sDjuiStatusText[48];
static u32 sDjuiStatusTimer = 0;

#define DJUI_MENU_DEST_LEVEL LEVEL_CASTLE_GROUNDS
#define DJUI_MENU_DEST_AREA 1
#define DJUI_MENU_DEST_NODE 0x0A

#define DJUI_MAIN_OPTION_COUNT 3
#define DJUI_LOBBY_OPTION_COUNT 2
#define DJUI_MODS_VISIBLE_ROWS 8

// level_update.c does not expose this in a public header yet.
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);

static void djui_status_set(const char *message) {
    if (message == NULL) {
        sDjuiStatusText[0] = '\0';
        sDjuiStatusTimer = 0;
        return;
    }
    strncpy(sDjuiStatusText, message, sizeof(sDjuiStatusText) - 1);
    sDjuiStatusText[sizeof(sDjuiStatusText) - 1] = '\0';
    sDjuiStatusTimer = 30 * 3;
}

static void djui_extract_mod_name(const char *path, char *out, size_t outSize) {
    const char *start = path;
    const char *dot = NULL;
    const char *slash = NULL;
    size_t len = 0;
    size_t i = 0;

    if (out == NULL || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (path == NULL || path[0] == '\0') {
        snprintf(out, outSize, "UNKNOWN");
        return;
    }

    if (strncmp(start, "mods/", 5) == 0) {
        start += 5;
    }

    slash = strchr(start, '/');
    if (slash != NULL && strcmp(slash + 1, "main.lua") == 0) {
        len = (size_t)(slash - start);
    } else {
        const char *leaf = strrchr(start, '/');
        if (leaf != NULL) {
            start = leaf + 1;
        }
        dot = strrchr(start, '.');
        len = (dot != NULL && dot > start) ? (size_t)(dot - start) : strlen(start);
    }

    if (len == 0) {
        snprintf(out, outSize, "MOD");
        return;
    }

    if (len >= outSize) {
        len = outSize - 1;
    }

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)start[i];
        if (isalnum(c)) {
            out[i] = (char)toupper(c);
        } else {
            out[i] = ' ';
        }
    }
    out[len] = '\0';
}

static void djui_print_option(s32 x, s32 y, bool selected, const char *label) {
    char line[48];
    snprintf(line, sizeof(line), "%s%s", selected ? "X " : "  ", label);
    print_text(x, y, line);
}

static void djui_render_main_overlay(void) {
    print_text_centered(160, 212, "SM64 COOP DX");
    print_text_centered(160, 196, "WII U MAIN MENU");

    djui_print_option(60, 166, sDjuiMainSelection == 0, "ENTER GAME");
    djui_print_option(60, 148, sDjuiMainSelection == 1, "LOBBY");
    djui_print_option(60, 130, sDjuiMainSelection == 2, "MODS");

    print_text_centered(160, 66, "DPAD MOVE   A SELECT");
    print_text_centered(160, 52, "START ENTER GAME");
}

static void djui_render_lobby_overlay(void) {
    print_text_centered(160, 212, "LOBBY");
    print_text_centered(160, 192, "NETWORK MENU NEXT PHASE");

    djui_print_option(60, 160, sDjuiLobbySelection == 0, "ENTER GAME");
    djui_print_option(60, 142, sDjuiLobbySelection == 1, "BACK");

    print_text_centered(160, 88, "HOST JOIN UI PORT FOLLOWS");
    print_text_centered(160, 70, "AFTER NETWORK PARITY");
    print_text_centered(160, 52, "B BACK");
}

static void djui_render_mods_overlay(void) {
    size_t count = mods_get_active_script_count();
    size_t start = (size_t)sDjuiModsScroll;
    size_t end = start + DJUI_MODS_VISIBLE_ROWS;
    s32 y = 178;

    print_text_centered(160, 212, "MOD LIST");
    print_text_fmt_int(46, 192, "ACTIVE MODS %d", (s32)count);

    if (count == 0) {
        print_text(46, 162, "NO MODS ACTIVE");
    } else {
        if (end > count) {
            end = count;
        }

        for (size_t i = start; i < end; i++) {
            char name[40];
            djui_extract_mod_name(mods_get_active_script_path(i), name, sizeof(name));
            print_text(46, y, name);
            y -= 14;
        }
    }

    print_text_centered(160, 52, "DPAD SCROLL   B BACK");
}

static void djui_queue_menu_overlay(void) {
    if (!gDjuiInMainMenu || gDjuiDisabled) {
        return;
    }

    switch (sDjuiMenuPage) {
        case DJUI_MENU_PAGE_MAIN:
            djui_render_main_overlay();
            break;
        case DJUI_MENU_PAGE_LOBBY:
            djui_render_lobby_overlay();
            break;
        case DJUI_MENU_PAGE_MODS:
            djui_render_mods_overlay();
            break;
    }

    if (sDjuiStatusTimer > 0 && sDjuiStatusText[0] != '\0') {
        print_text_centered(160, 34, sDjuiStatusText);
        sDjuiStatusTimer--;
    }
}

// Phase-1 scaffold: lifecycle + render hook only.
void djui_init(void) {
    sDjuiInitialized = true;
    sDjuiRenderLogged = false;
    gDjuiInMainMenu = true;
    sDjuiMenuWarpPending = true;
    sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiModsScroll = 0;
    djui_status_set(NULL);
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
    sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiModsScroll = 0;
    djui_status_set(NULL);
#ifdef TARGET_WII_U
    WHBLogPrint("djui: shutdown");
#endif
}

void djui_open_main_menu(void) {
    gDjuiInMainMenu = true;
    sDjuiMenuWarpPending = true;
    sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiModsScroll = 0;
    djui_status_set(NULL);
}

void djui_close_main_menu(void) {
    gDjuiInMainMenu = false;
    sDjuiMenuWarpPending = false;
    djui_status_set(NULL);
}

static void djui_update_main_selection(s32 direction) {
    sDjuiMainSelection += direction;
    if (sDjuiMainSelection < 0) {
        sDjuiMainSelection = DJUI_MAIN_OPTION_COUNT - 1;
    }
    if (sDjuiMainSelection >= DJUI_MAIN_OPTION_COUNT) {
        sDjuiMainSelection = 0;
    }
}

static void djui_update_lobby_selection(s32 direction) {
    sDjuiLobbySelection += direction;
    if (sDjuiLobbySelection < 0) {
        sDjuiLobbySelection = DJUI_LOBBY_OPTION_COUNT - 1;
    }
    if (sDjuiLobbySelection >= DJUI_LOBBY_OPTION_COUNT) {
        sDjuiLobbySelection = 0;
    }
}

static void djui_activate_main_selection(void) {
    switch (sDjuiMainSelection) {
        case 0:
            djui_close_main_menu();
            break;
        case 1:
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBY;
            sDjuiLobbySelection = 0;
            break;
        case 2:
            sDjuiMenuPage = DJUI_MENU_PAGE_MODS;
            sDjuiModsScroll = 0;
            break;
    }
}

static void djui_activate_lobby_selection(void) {
    switch (sDjuiLobbySelection) {
        case 0:
            djui_close_main_menu();
            break;
        case 1:
            sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
            break;
    }
}

static void djui_update_mods_scroll(s32 direction) {
    s32 maxScroll = (s32)mods_get_active_script_count() - DJUI_MODS_VISIBLE_ROWS;
    if (maxScroll < 0) {
        maxScroll = 0;
    }

    sDjuiModsScroll += direction;
    if (sDjuiModsScroll < 0) {
        sDjuiModsScroll = 0;
    }
    if (sDjuiModsScroll > maxScroll) {
        sDjuiModsScroll = maxScroll;
    }
}

// Main-menu input routing for the Wii U DJUI parity scaffold.
void djui_update(void) {
    u16 pressed = 0;

    if (!sDjuiInitialized || gPlayer1Controller == NULL) {
        return;
    }

    pressed = gPlayer1Controller->buttonPressed;

    if ((pressed & START_BUTTON)
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
        return;
    }

    if (!gDjuiInMainMenu) {
        return;
    }

    if (pressed & START_BUTTON) {
        djui_close_main_menu();
        return;
    }

    if (pressed & B_BUTTON) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_close_main_menu();
        } else {
            sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
            sDjuiModsScroll = 0;
        }
        return;
    }

    if (pressed & U_JPAD) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_update_main_selection(-1);
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            djui_update_lobby_selection(-1);
        } else {
            djui_update_mods_scroll(-1);
        }
    }

    if (pressed & D_JPAD) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_update_main_selection(+1);
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            djui_update_lobby_selection(+1);
        } else {
            djui_update_mods_scroll(+1);
        }
    }

    if (pressed & A_BUTTON) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_activate_main_selection();
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            djui_activate_lobby_selection();
        } else {
            djui_status_set("MOD LIST IS READ ONLY");
        }
    }

    djui_queue_menu_overlay();
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
