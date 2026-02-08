#include "djui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "sm64.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/print.h"
#include "game/segment2.h"
#include "pc/lua/smlua.h"
#include "pc/mods/mods.h"

#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

bool gDjuiInMainMenu = true;
bool gDjuiDisabled = false;

enum DjuiMenuPage {
    DJUI_MENU_PAGE_MAIN = 0,
    DJUI_MENU_PAGE_LOBBIES = 1,
    DJUI_MENU_PAGE_LOBBY = 2,
    DJUI_MENU_PAGE_OPTIONS = 3,
    DJUI_MENU_PAGE_MODS = 4,
};

static bool sDjuiInitialized = false;
static bool sDjuiRenderLogged = false;
static bool sDjuiMenuWarpPending = true;
static enum DjuiMenuPage sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
static enum DjuiMenuPage sDjuiModsReturnPage = DJUI_MENU_PAGE_MAIN;
static s32 sDjuiMainSelection = 0;
static s32 sDjuiLobbyListSelection = 0;
static s32 sDjuiLobbySelection = 0;
static s32 sDjuiOptionsSelection = 0;
static s32 sDjuiSelectedLobby = 0;
static s32 sDjuiModsScroll = 0;
static s32 sDjuiModsSelection = 0;
static char sDjuiStatusText[64];
static u32 sDjuiStatusTimer = 0;
static s8 sDjuiNavStickDirY = 0;
static u8 sDjuiNavStickTimerY = 0;
static bool sDjuiRequireNeutralInput = true;
static bool sDjuiModsDirty = false;

#define DJUI_MENU_DEST_LEVEL LEVEL_CASTLE_GROUNDS
#define DJUI_MENU_DEST_AREA 1
#define DJUI_MENU_DEST_NODE 0x0A

#define DJUI_LOBBY_OPTION_COUNT 3
#define DJUI_MODS_VISIBLE_ROWS 8
#define DJUI_MAIN_TITLE_LINE1_Y (DJUI_MAIN_PANEL_TOP - 20)
#define DJUI_MAIN_TITLE_LINE2_Y (DJUI_MAIN_PANEL_TOP - 36)
#define DJUI_MAIN_TEXT_Y_START 126
#define DJUI_MAIN_ROWS_Y_STEP 20
#define DJUI_TEXT_GLYPH_HALF_HEIGHT 8.0f

#define DJUI_MAIN_PANEL_WIDTH 214
#define DJUI_MAIN_PANEL_HEIGHT 150
#define DJUI_MAIN_PANEL_LEFT ((320 - DJUI_MAIN_PANEL_WIDTH) / 2)
#define DJUI_MAIN_PANEL_TOP 190
#define DJUI_MAIN_PANEL_BOTTOM (DJUI_MAIN_PANEL_TOP - DJUI_MAIN_PANEL_HEIGHT)

#define DJUI_MAIN_BUTTON_WIDTH 186
#define DJUI_MAIN_BUTTON_HEIGHT 22
#define DJUI_MAIN_BUTTON_LEFT ((320 - DJUI_MAIN_BUTTON_WIDTH) / 2)

static const char *sDjuiMainOptions[] = {
    "HOST",
    "JOIN",
    "OPTIONS",
    "QUIT",
};

#define DJUI_MAIN_OPTION_COUNT ((s32)(sizeof(sDjuiMainOptions) / sizeof(sDjuiMainOptions[0])))

static const char *sDjuiOfflineLobbies[] = {
    "OFFLINE PRACTICE",
    "LOCAL MOD TEST",
    "PARITY SANDBOX",
};

#define DJUI_OFFLINE_LOBBY_COUNT ((s32)(sizeof(sDjuiOfflineLobbies) / sizeof(sDjuiOfflineLobbies[0])))

static const char *sDjuiOptionsItems[] = {
    "CAMERA",
    "CONTROLS",
    "DISPLAY",
    "SOUND",
    "MISC",
    "BACK",
};

#define DJUI_OPTIONS_OPTION_COUNT ((s32)(sizeof(sDjuiOptionsItems) / sizeof(sDjuiOptionsItems[0])))

// level_update.c does not expose this in a public header yet.
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);
void create_dl_scale_matrix(s8 pushOp, f32 x, f32 y, f32 z);

// Donor-adjacent box primitives already implemented by vanilla pause/menu code.
void shade_screen(void);

static s32 djui_active_mod_count(void) {
    return (s32)mods_get_active_script_count();
}

static s32 djui_available_mod_count(void) {
    return (s32)mods_get_available_script_count();
}

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

static void djui_make_mod_option_label(size_t index, char *out, size_t outSize) {
    char name[40];

    if (out == NULL || outSize == 0) {
        return;
    }

    djui_extract_mod_name(mods_get_available_script_path(index), name, sizeof(name));
    snprintf(out, outSize, "%s %s",
             mods_get_available_script_enabled(index) ? "ON" : "OFF",
             name);
}

static u8 djui_ascii_to_dialog_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return ASCII_TO_DIALOG(c);
    }
    if (c >= 'a' && c <= 'z') {
        return ASCII_TO_DIALOG(c);
    }
    if (c >= '0' && c <= '9') {
        return ASCII_TO_DIALOG(c);
    }
    if (c == ' ') {
        return DIALOG_CHAR_SPACE;
    }
    if (c == '.') {
        return DIALOG_CHAR_PERIOD;
    }
    if (c == '/') {
        return DIALOG_CHAR_SLASH;
    }

    return DIALOG_CHAR_SPACE;
}

static void djui_ascii_to_dialog_string(const char *ascii, u8 *out, size_t outSize) {
    size_t i = 0;

    if (out == NULL || outSize == 0) {
        return;
    }

    if (ascii == NULL) {
        out[0] = DIALOG_CHAR_TERMINATOR;
        return;
    }

    while (ascii[i] != '\0' && i + 1 < outSize) {
        out[i] = djui_ascii_to_dialog_char(ascii[i]);
        i++;
    }

    out[i] = DIALOG_CHAR_TERMINATOR;
}

static void djui_print_ascii(s16 x, s16 y, const char *ascii) {
    u8 dialog[128];
    djui_ascii_to_dialog_string(ascii, dialog, sizeof(dialog));
    print_generic_string(x, y, dialog);
}

static void djui_print_ascii_centered(s16 centerX, s16 y, const char *ascii) {
    u8 dialog[128];
    s16 x = centerX;

    djui_ascii_to_dialog_string(ascii, dialog, sizeof(dialog));
    x = get_str_x_pos_from_center(centerX, dialog, 10.0f);
    print_generic_string(x, y, dialog);
}

static void djui_print_option(s16 x, s16 y, bool selected, const char *label) {
    u8 dialog[128];
    size_t idx = 0;
    size_t i = 0;

    if (selected) {
        dialog[idx++] = DIALOG_CHAR_STAR_FILLED;
        dialog[idx++] = DIALOG_CHAR_SPACE;
    } else {
        dialog[idx++] = DIALOG_CHAR_SPACE;
        dialog[idx++] = DIALOG_CHAR_SPACE;
    }

    while (label[i] != '\0' && idx + 3 < sizeof(dialog)) {
        dialog[idx++] = djui_ascii_to_dialog_char(label[i]);
        i++;
    }

    if (selected && idx + 2 < sizeof(dialog)) {
        dialog[idx++] = DIALOG_CHAR_SPACE;
        dialog[idx++] = DIALOG_CHAR_STAR_FILLED;
    }

    dialog[idx] = DIALOG_CHAR_TERMINATOR;
    print_generic_string(x, y, dialog);
}

static void djui_draw_box_colored(s16 x, s16 y, f32 sx, f32 sy, u8 r, u8 g, u8 b, u8 a) {
    create_dl_translation_matrix(MENU_MTX_PUSH, x - 78, y - 32, 0);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, sx, sy, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, r, g, b, a);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

static void djui_draw_panel_box(s16 x, s16 y, f32 sx, f32 sy) {
    djui_draw_box_colored(x, y, sx, sy, 0, 0, 0, 125);
}

static void djui_draw_box_top_left_colored(s16 left, s16 top, s16 width, s16 height, u8 r, u8 g, u8 b, u8 a) {
    f32 sx = (f32)width / 130.0f;
    f32 sy = (f32)height / 80.0f;
    s16 anchorX = left + 78;
    s16 anchorY = top + 32;
    djui_draw_box_colored(anchorX, anchorY, sx, sy, r, g, b, a);
}

static void djui_draw_button_row_scaled(s16 x, s16 y, bool selected, f32 outerScaleX, f32 innerScaleX) {
    // Co-op DX-like button bars: all rows keep a visible border, selected row is brighter.
    if (selected) {
        djui_draw_box_colored(x, y, outerScaleX, 0.24f, 135, 162, 198, 230);
        djui_draw_box_colored(x, y, innerScaleX, 0.20f, 42, 64, 104, 220);
    } else {
        djui_draw_box_colored(x, y, outerScaleX, 0.24f, 82, 84, 96, 220);
        djui_draw_box_colored(x, y, innerScaleX, 0.20f, 18, 19, 24, 210);
    }
}

static void djui_draw_button_row(s16 x, s16 y, bool selected) {
    djui_draw_button_row_scaled(x, y, selected, 1.08f, 1.01f);
}

static s16 djui_main_text_y_for_index(s32 index) {
    return DJUI_MAIN_TEXT_Y_START - (s16)(index * DJUI_MAIN_ROWS_Y_STEP);
}

static s16 djui_main_row_top_from_text_bottom(s16 textY) {
    return (s16)((f32)textY + DJUI_TEXT_GLYPH_HALF_HEIGHT + ((f32)DJUI_MAIN_BUTTON_HEIGHT * 0.5f));
}

static void djui_print_center_button_label(s16 centerX, s16 y, bool selected, const char *label) {
    char line[64];
    (void)selected;
    snprintf(line, sizeof(line), "%s", label != NULL ? label : "");
    djui_print_ascii_centered(centerX, y, line);
}

static void djui_draw_panel_layout(void) {
    // Matches donor intent: dim castle scene with custom panels layered on top.
    gDialogTextAlpha = 255;
    shade_screen();

    switch (sDjuiMenuPage) {
        case DJUI_MENU_PAGE_MAIN:
            // Donor-style centered dark panel with a subtle inner fill.
            djui_draw_box_top_left_colored(DJUI_MAIN_PANEL_LEFT, DJUI_MAIN_PANEL_TOP,
                                           DJUI_MAIN_PANEL_WIDTH, DJUI_MAIN_PANEL_HEIGHT,
                                           0, 0, 0, 200);
            djui_draw_box_top_left_colored(DJUI_MAIN_PANEL_LEFT + 4, DJUI_MAIN_PANEL_TOP - 4,
                                           DJUI_MAIN_PANEL_WIDTH - 8, DJUI_MAIN_PANEL_HEIGHT - 8,
                                           0, 0, 0, 230);
            break;
        case DJUI_MENU_PAGE_LOBBIES:
            djui_draw_panel_box(102, 154, 1.2f, 0.8f);
            djui_draw_panel_box(246, 154, 1.2f, 0.8f);
            break;
        case DJUI_MENU_PAGE_LOBBY:
            djui_draw_panel_box(102, 154, 1.2f, 0.8f);
            djui_draw_panel_box(246, 154, 1.2f, 0.8f);
            break;
        case DJUI_MENU_PAGE_OPTIONS:
            djui_draw_panel_box(102, 154, 1.2f, 0.8f);
            djui_draw_panel_box(246, 154, 1.2f, 0.8f);
            break;
        case DJUI_MENU_PAGE_MODS:
            djui_draw_panel_box(102, 154, 1.2f, 0.8f);
            djui_draw_panel_box(246, 154, 1.2f, 0.8f);
            break;
    }
}

static void djui_draw_page_rows(void) {
    s16 y = 0;

    switch (sDjuiMenuPage) {
        case DJUI_MENU_PAGE_MAIN:
            for (s32 i = 0; i < DJUI_MAIN_OPTION_COUNT; i++) {
                s16 rowTop = djui_main_row_top_from_text_bottom(djui_main_text_y_for_index(i));
                bool selected = (sDjuiMainSelection == i);
                if (selected) {
                    djui_draw_box_top_left_colored(DJUI_MAIN_BUTTON_LEFT, rowTop,
                                                   DJUI_MAIN_BUTTON_WIDTH, DJUI_MAIN_BUTTON_HEIGHT,
                                                   0, 120, 215, 255);
                    djui_draw_box_top_left_colored(DJUI_MAIN_BUTTON_LEFT + 2, rowTop - 2,
                                                   DJUI_MAIN_BUTTON_WIDTH - 4, DJUI_MAIN_BUTTON_HEIGHT - 4,
                                                   80, 80, 80, 255);
                } else {
                    djui_draw_box_top_left_colored(DJUI_MAIN_BUTTON_LEFT, rowTop,
                                                   DJUI_MAIN_BUTTON_WIDTH, DJUI_MAIN_BUTTON_HEIGHT,
                                                   75, 75, 75, 255);
                    djui_draw_box_top_left_colored(DJUI_MAIN_BUTTON_LEFT + 2, rowTop - 2,
                                                   DJUI_MAIN_BUTTON_WIDTH - 4, DJUI_MAIN_BUTTON_HEIGHT - 4,
                                                   22, 22, 22, 255);
                }
            }
            break;
        case DJUI_MENU_PAGE_LOBBIES:
            y = 176;
            for (s32 i = 0; i < DJUI_OFFLINE_LOBBY_COUNT; i++) {
                djui_draw_button_row(102, y, sDjuiLobbyListSelection == i);
                y -= 18;
            }
            break;
        case DJUI_MENU_PAGE_LOBBY:
            y = 172;
            for (s32 i = 0; i < DJUI_LOBBY_OPTION_COUNT; i++) {
                djui_draw_button_row(102, y, sDjuiLobbySelection == i);
                y -= 18;
            }
            break;
        case DJUI_MENU_PAGE_OPTIONS:
            y = 178;
            for (s32 i = 0; i < DJUI_OPTIONS_OPTION_COUNT; i++) {
                djui_draw_button_row(102, y, sDjuiOptionsSelection == i);
                y -= 14;
            }
            break;
        case DJUI_MENU_PAGE_MODS: {
            s32 count = djui_available_mod_count();
            s32 start = sDjuiModsScroll;
            s32 end = start + DJUI_MODS_VISIBLE_ROWS;
            y = 182;
            if (end > count) {
                end = count;
            }
            for (s32 i = start; i < end; i++) {
                djui_draw_button_row(102, y, sDjuiModsSelection == i);
                y -= 14;
            }
            break;
        }
    }
}

static void djui_render_main_text(void) {
    char modsLine[48];
    s16 footerY = DJUI_MAIN_PANEL_BOTTOM - 4;

    djui_print_ascii_centered(160, DJUI_MAIN_TITLE_LINE1_Y, "SM64EX COOP");
    djui_print_ascii_centered(160, DJUI_MAIN_TITLE_LINE2_Y, "MAIN MENU");

    for (s32 i = 0; i < DJUI_MAIN_OPTION_COUNT; i++) {
        bool selected = sDjuiMainSelection == i;
        djui_print_center_button_label(160, djui_main_text_y_for_index(i), selected, sDjuiMainOptions[i]);
    }

    snprintf(modsLine, sizeof(modsLine), "ACTIVE MODS %d", djui_active_mod_count());
    djui_print_ascii_centered(160, footerY, modsLine);
    djui_print_ascii_centered(160, footerY - 16, "DPAD MOVE   A SELECT");
    djui_print_ascii_centered(160, footerY - 32, "START ENTER GAME");
}

static void djui_render_lobbies_text(void) {
    char modsLine[48];
    s16 y = 170;

    djui_print_ascii_centered(160, 212, "JOIN LOBBY");

    for (s32 i = 0; i < DJUI_OFFLINE_LOBBY_COUNT; i++) {
        bool selected = sDjuiLobbyListSelection == i;
        djui_print_option(38, y, selected, sDjuiOfflineLobbies[i]);
        y -= 18;
    }

    djui_print_ascii(188, 170, "SELECTED");
    djui_print_ascii(188, 154, sDjuiOfflineLobbies[sDjuiLobbyListSelection]);
    snprintf(modsLine, sizeof(modsLine), "LOBBY MODS %d", djui_active_mod_count());
    djui_print_ascii(188, 136, modsLine);
    djui_print_ascii(188, 118, "OFFLINE SAFE");

    djui_print_ascii_centered(160, 64, "A OPEN LOBBY");
    djui_print_ascii_centered(160, 50, "B BACK");
}

static void djui_render_lobby_text(void) {
    char modsLine[48];
    s16 y = 166;

    djui_print_ascii_centered(160, 212, "LOBBY");
    djui_print_ascii_centered(160, 196, sDjuiOfflineLobbies[sDjuiSelectedLobby]);

    for (s32 i = 0; i < DJUI_LOBBY_OPTION_COUNT; i++) {
        bool selected = sDjuiLobbySelection == i;
        const char *option = "ENTER GAME";
        if (i == 1) {
            option = "VIEW MODS";
        } else if (i == 2) {
            option = "BACK TO LOBBIES";
        }
        djui_print_option(38, y, selected, option);
        y -= 18;
    }

    djui_print_ascii(188, 170, "LOBBY INFO");
    djui_print_ascii(188, 154, "NETWORK WIRING");
    djui_print_ascii(188, 138, "COMES LATER");
    snprintf(modsLine, sizeof(modsLine), "ACTIVE MODS %d", djui_active_mod_count());
    djui_print_ascii(188, 120, modsLine);

    djui_print_ascii_centered(160, 64, "DPAD MOVE   A SELECT");
    djui_print_ascii_centered(160, 50, "B BACK");
}

static void djui_render_mods_text(void) {
    s32 count = djui_available_mod_count();
    s32 enabled = djui_active_mod_count();
    s32 start = sDjuiModsScroll;
    s32 end = start + DJUI_MODS_VISIBLE_ROWS;
    s32 y = 176;
    char modsLine[48];

    if (end > count) {
        end = count;
    }

    djui_print_ascii_centered(160, 212, "LOBBY MODS");
    snprintf(modsLine, sizeof(modsLine), "ACTIVE MODS %d", enabled);
    djui_print_ascii(38, 194, modsLine);

    if (count == 0) {
        djui_print_ascii(38, 164, "NO MODS FOUND");
    } else {
        for (s32 i = start; i < end; i++) {
            char label[56];
            djui_make_mod_option_label((size_t)i, label, sizeof(label));
            djui_print_option(38, y, i == sDjuiModsSelection, label);
            y -= 14;
        }
    }

    djui_print_ascii(188, 170, "MOD CONTROL");
    djui_print_ascii(188, 154, "A TOGGLE MOD");
    djui_print_ascii(188, 138, "START APPLY");
    snprintf(modsLine, sizeof(modsLine), "ENABLED %d/%d", enabled, count);
    djui_print_ascii(188, 122, modsLine);

    djui_print_ascii_centered(160, 64, "DPAD SELECT   A TOGGLE");
    djui_print_ascii_centered(160, 50, "B BACK   START APPLY");
}

static void djui_render_options_text(void) {
    const char *selected = sDjuiOptionsItems[sDjuiOptionsSelection];
    s16 y = 176;

    djui_print_ascii_centered(160, 212, "OPTIONS");

    for (s32 i = 0; i < DJUI_OPTIONS_OPTION_COUNT; i++) {
        djui_print_option(38, y, sDjuiOptionsSelection == i, sDjuiOptionsItems[i]);
        y -= 14;
    }

    djui_print_ascii(188, 170, "SELECTED");
    djui_print_ascii(188, 154, selected);
    if (sDjuiOptionsSelection == DJUI_OPTIONS_OPTION_COUNT - 1) {
        djui_print_ascii(188, 138, "RETURN MAIN MENU");
    } else {
        djui_print_ascii(188, 138, "PANEL PENDING");
        djui_print_ascii(188, 122, "DONOR PARITY");
    }

    djui_print_ascii_centered(160, 64, "A OPEN   B BACK");
    djui_print_ascii_centered(160, 50, "DPAD MOVE");
}

static void djui_render_overlay(void) {
    if (!gDjuiInMainMenu || gDjuiDisabled) {
        return;
    }

    create_dl_ortho_matrix();
    djui_draw_panel_layout();
    djui_draw_page_rows();
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);

    switch (sDjuiMenuPage) {
        case DJUI_MENU_PAGE_MAIN:
            djui_render_main_text();
            break;
        case DJUI_MENU_PAGE_LOBBIES:
            djui_render_lobbies_text();
            break;
        case DJUI_MENU_PAGE_LOBBY:
            djui_render_lobby_text();
            break;
        case DJUI_MENU_PAGE_OPTIONS:
            djui_render_options_text();
            break;
        case DJUI_MENU_PAGE_MODS:
            djui_render_mods_text();
            break;
    }

    if (sDjuiStatusTimer > 0 && sDjuiStatusText[0] != '\0') {
        djui_print_ascii_centered(160, 34, sDjuiStatusText);
        sDjuiStatusTimer--;
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

static bool djui_input_is_neutral(void) {
    if (gPlayer1Controller == NULL) {
        return true;
    }

    if (gPlayer1Controller->buttonDown != 0) {
        return false;
    }

    if (gPlayer1Controller->rawStickX > 20 || gPlayer1Controller->rawStickX < -20) {
        return false;
    }

    if (gPlayer1Controller->rawStickY > 20 || gPlayer1Controller->rawStickY < -20) {
        return false;
    }

    return true;
}

void djui_init(void) {
    sDjuiInitialized = true;
    sDjuiRenderLogged = false;
    gDjuiInMainMenu = true;
    sDjuiMenuWarpPending = true;
    sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
    sDjuiModsReturnPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbyListSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiOptionsSelection = 0;
    sDjuiSelectedLobby = 0;
    sDjuiModsScroll = 0;
    sDjuiModsSelection = 0;
    sDjuiNavStickDirY = 0;
    sDjuiNavStickTimerY = 0;
    sDjuiRequireNeutralInput = true;
    sDjuiModsDirty = false;
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
    sDjuiModsReturnPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbyListSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiOptionsSelection = 0;
    sDjuiSelectedLobby = 0;
    sDjuiModsScroll = 0;
    sDjuiModsSelection = 0;
    sDjuiNavStickDirY = 0;
    sDjuiNavStickTimerY = 0;
    sDjuiRequireNeutralInput = false;
    sDjuiModsDirty = false;
    djui_status_set(NULL);
#ifdef TARGET_WII_U
    WHBLogPrint("djui: shutdown");
#endif
}

void djui_open_main_menu(void) {
    gDjuiInMainMenu = true;
    sDjuiMenuWarpPending = true;
    sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
    sDjuiModsReturnPage = DJUI_MENU_PAGE_MAIN;
    sDjuiMainSelection = 0;
    sDjuiLobbyListSelection = 0;
    sDjuiLobbySelection = 0;
    sDjuiOptionsSelection = 0;
    sDjuiSelectedLobby = 0;
    sDjuiModsScroll = 0;
    sDjuiModsSelection = 0;
    sDjuiNavStickDirY = 0;
    sDjuiNavStickTimerY = 0;
    sDjuiRequireNeutralInput = true;
    djui_status_set(NULL);
}

void djui_close_main_menu(void) {
    if (sDjuiModsDirty) {
#ifdef TARGET_WII_U
        WHBLogPrint("djui: applying mod toggle set");
#endif
        smlua_init();
        sDjuiModsDirty = false;
    }

    gDjuiInMainMenu = false;
    sDjuiMenuWarpPending = false;
    sDjuiNavStickDirY = 0;
    sDjuiNavStickTimerY = 0;
    sDjuiRequireNeutralInput = false;
    djui_status_set(NULL);
}

static void djui_set_mod_selection(s32 newSelection) {
    s32 count = djui_available_mod_count();

    if (count <= 0) {
        sDjuiModsSelection = 0;
        sDjuiModsScroll = 0;
        return;
    }

    if (newSelection < 0) {
        newSelection = 0;
    }
    if (newSelection >= count) {
        newSelection = count - 1;
    }

    sDjuiModsSelection = newSelection;
    if (sDjuiModsSelection < sDjuiModsScroll) {
        sDjuiModsScroll = sDjuiModsSelection;
    }
    if (sDjuiModsSelection >= sDjuiModsScroll + DJUI_MODS_VISIBLE_ROWS) {
        sDjuiModsScroll = sDjuiModsSelection - DJUI_MODS_VISIBLE_ROWS + 1;
    }
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

static void djui_update_lobby_list_selection(s32 direction) {
    sDjuiLobbyListSelection += direction;
    if (sDjuiLobbyListSelection < 0) {
        sDjuiLobbyListSelection = DJUI_OFFLINE_LOBBY_COUNT - 1;
    }
    if (sDjuiLobbyListSelection >= DJUI_OFFLINE_LOBBY_COUNT) {
        sDjuiLobbyListSelection = 0;
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

static void djui_update_options_selection(s32 direction) {
    sDjuiOptionsSelection += direction;
    if (sDjuiOptionsSelection < 0) {
        sDjuiOptionsSelection = DJUI_OPTIONS_OPTION_COUNT - 1;
    }
    if (sDjuiOptionsSelection >= DJUI_OPTIONS_OPTION_COUNT) {
        sDjuiOptionsSelection = 0;
    }
}

static void djui_activate_main_selection(void) {
    switch (sDjuiMainSelection) {
        case 0:
            sDjuiSelectedLobby = 0;
            sDjuiLobbySelection = 0;
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBY;
            djui_status_set("HOST LOBBY: LOCAL OFFLINE");
            break;
        case 1:
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBIES;
            djui_status_set("JOIN FLOW: OFFLINE LIST");
            break;
        case 2:
            sDjuiOptionsSelection = 0;
            sDjuiMenuPage = DJUI_MENU_PAGE_OPTIONS;
            djui_status_set("OPTIONS PANEL");
            break;
        case 3:
            djui_close_main_menu();
            break;
    }
}

static void djui_activate_lobby_selection(void) {
    switch (sDjuiLobbySelection) {
        case 0:
            djui_close_main_menu();
            break;
        case 1:
            sDjuiModsReturnPage = DJUI_MENU_PAGE_LOBBY;
            sDjuiMenuPage = DJUI_MENU_PAGE_MODS;
            djui_set_mod_selection(0);
            break;
        case 2:
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBIES;
            break;
    }
}

static void djui_activate_options_selection(void) {
    switch (sDjuiOptionsSelection) {
        case 0:
            djui_status_set("CAMERA PANEL: PENDING");
            break;
        case 1:
            djui_status_set("CONTROLS PANEL: PENDING");
            break;
        case 2:
            djui_status_set("DISPLAY PANEL: PENDING");
            break;
        case 3:
            djui_status_set("SOUND PANEL: PENDING");
            break;
        case 4:
            djui_status_set("MISC PANEL: PENDING");
            break;
        case 5:
            sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
            break;
    }
}

static s8 djui_poll_vertical_nav_step(void) {
    s8 dir = 0;
    s8 rawY = 0;

    if (gPlayer1Controller == NULL) {
        return 0;
    }

    rawY = gPlayer1Controller->rawStickY;
    if (rawY > 60) {
        dir = -1;
    } else if (rawY < -60) {
        dir = 1;
    }

    if (dir == 0) {
        sDjuiNavStickDirY = 0;
        sDjuiNavStickTimerY = 0;
        return 0;
    }

    if (sDjuiNavStickDirY != dir) {
        sDjuiNavStickDirY = dir;
        sDjuiNavStickTimerY = 0;
        return dir;
    }

    sDjuiNavStickTimerY++;
    if (sDjuiNavStickTimerY >= 10) {
        sDjuiNavStickTimerY = 8;
        return dir;
    }

    return 0;
}

void djui_update(void) {
    u16 pressed = 0;
    s8 navStep = 0;

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
        sDjuiNavStickDirY = 0;
        sDjuiNavStickTimerY = 0;
        sDjuiRequireNeutralInput = false;
        return;
    }

    if (sDjuiRequireNeutralInput) {
        if (!djui_input_is_neutral()) {
            return;
        }
        sDjuiRequireNeutralInput = false;
        return;
    }

    if (pressed & START_BUTTON) {
        djui_close_main_menu();
        return;
    }

    if (pressed & B_BUTTON) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_close_main_menu();
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBIES) {
            sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBIES;
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_OPTIONS) {
            sDjuiMenuPage = DJUI_MENU_PAGE_MAIN;
        } else {
            sDjuiMenuPage = sDjuiModsReturnPage;
        }
        return;
    }

    navStep = djui_poll_vertical_nav_step();
    if (pressed & (U_JPAD | U_CBUTTONS)) {
        navStep = -1;
    } else if (pressed & (D_JPAD | D_CBUTTONS)) {
        navStep = 1;
    }

    if (navStep != 0) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_update_main_selection(navStep);
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBIES) {
            djui_update_lobby_list_selection(navStep);
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            djui_update_lobby_selection(navStep);
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_OPTIONS) {
            djui_update_options_selection(navStep);
        } else {
            djui_set_mod_selection(sDjuiModsSelection + navStep);
        }
    }

    if (pressed & A_BUTTON) {
        if (sDjuiMenuPage == DJUI_MENU_PAGE_MAIN) {
            djui_activate_main_selection();
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBIES) {
            sDjuiSelectedLobby = sDjuiLobbyListSelection;
            sDjuiLobbySelection = 0;
            sDjuiMenuPage = DJUI_MENU_PAGE_LOBBY;
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_LOBBY) {
            djui_activate_lobby_selection();
        } else if (sDjuiMenuPage == DJUI_MENU_PAGE_OPTIONS) {
            djui_activate_options_selection();
        } else {
            size_t mod_index = (size_t)sDjuiModsSelection;
            bool enabled = mods_get_available_script_enabled(mod_index);
            if (mods_set_available_script_enabled(mod_index, !enabled)) {
                sDjuiModsDirty = true;
                djui_status_set(enabled ? "MOD DISABLED: APPLY ON START"
                                        : "MOD ENABLED: APPLY ON START");
            }
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

    djui_render_overlay();
}
