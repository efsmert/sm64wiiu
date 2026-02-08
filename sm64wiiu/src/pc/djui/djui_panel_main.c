#include "djui_panel_main.h"

#include <stdio.h>
#include "djui.h"
#include "djui_button.h"
#include "djui_cursor.h"
#include "djui_interactable.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"

bool gDjuiPanelMainCreated = false;

static void djui_panel_simple_back(struct DjuiBase *base) {
    (void)base;
    djui_panel_back();
}

static void djui_panel_open_options(struct DjuiBase *caller) {
    struct DjuiThreePanel *panel = djui_panel_menu_create("OPTIONS", false);
    struct DjuiBase *body = NULL;

    if (panel == NULL) {
        return;
    }

    body = djui_three_panel_get_body(panel);
    if (body != NULL) {
        djui_button_create(body, "DISPLAY", DJUI_BUTTON_STYLE_NORMAL, NULL);
        djui_button_create(body, "CONTROLS", DJUI_BUTTON_STYLE_NORMAL, NULL);
        djui_button_create(body, "BACK", DJUI_BUTTON_STYLE_BACK, djui_panel_simple_back);
    }

    djui_panel_add(caller, panel, NULL);
}

static void djui_panel_open_join(struct DjuiBase *caller) {
    struct DjuiThreePanel *panel = djui_panel_menu_create("LOBBY", false);
    struct DjuiBase *body = NULL;

    if (panel == NULL) {
        return;
    }

    body = djui_three_panel_get_body(panel);
    if (body != NULL) {
        djui_button_create(body, "OFFLINE PRACTICE", DJUI_BUTTON_STYLE_NORMAL, NULL);
        djui_button_create(body, "LOCAL MOD TEST", DJUI_BUTTON_STYLE_NORMAL, NULL);
        djui_button_create(body, "BACK", DJUI_BUTTON_STYLE_BACK, djui_panel_simple_back);
    }

    djui_panel_add(caller, panel, NULL);
}

static void djui_panel_host_enter_game(struct DjuiBase *caller) {
    (void)caller;
    djui_close_main_menu();
}

static void djui_panel_open_host(struct DjuiBase *caller) {
    struct DjuiThreePanel *panel = djui_panel_menu_create("HOST", false);
    struct DjuiBase *body = NULL;

    if (panel == NULL) {
        return;
    }

    body = djui_three_panel_get_body(panel);
    if (body != NULL) {
        djui_button_create(body, "ENTER GAME", DJUI_BUTTON_STYLE_NORMAL, djui_panel_host_enter_game);
        djui_button_create(body, "MODS", DJUI_BUTTON_STYLE_NORMAL, NULL);
        djui_button_create(body, "BACK", DJUI_BUTTON_STYLE_BACK, djui_panel_simple_back);
    }

    djui_panel_add(caller, panel, NULL);
}

static void djui_panel_quit(struct DjuiBase *caller) {
    (void)caller;
    djui_close_main_menu();
}

void djui_panel_main_create(struct DjuiBase *caller) {
    struct DjuiThreePanel *panel = djui_panel_menu_create("SM64EX COOP\nMAIN MENU", false);
    struct DjuiBase *body = NULL;
    struct DjuiButton *button1 = NULL;

    if (panel == NULL) {
        return;
    }

    body = djui_three_panel_get_body(panel);
    if (body != NULL) {
        button1 = djui_button_create(body, "HOST", DJUI_BUTTON_STYLE_NORMAL, djui_panel_open_host);
        djui_button_create(body, "JOIN", DJUI_BUTTON_STYLE_NORMAL, djui_panel_open_join);
        djui_button_create(body, "OPTIONS", DJUI_BUTTON_STYLE_NORMAL, djui_panel_open_options);
        djui_button_create(body, "QUIT", DJUI_BUTTON_STYLE_BACK, djui_panel_quit);

        if (button1 != NULL) {
            djui_cursor_input_controlled_center(&button1->base);
        }
    }

    djui_panel_add(caller, panel, NULL);
    gInteractableOverridePad = true;
    gDjuiPanelMainCreated = true;
}
