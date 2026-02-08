#include "djui_panel_menu.h"

#include "djui_root.h"

static char *sRainbowColors[] = {
    "\\#ff3030\\",
    "\\#40e740\\",
    "\\#40b0ff\\",
    "\\#ffef40\\",
};

char *djui_menu_get_rainbow_string_color(enum DjuiRainbowColor color) {
    s32 index = (color >= 0 && color <= 3) ? (s32)color : 0;
    return sRainbowColors[index];
}

void djui_panel_menu_back(struct DjuiBase *base) {
    (void)base;
    djui_panel_back();
}

struct DjuiThreePanel *djui_panel_menu_create(char *headerText, bool forcedLeftSide) {
    struct DjuiThreePanel *panel = NULL;
    struct DjuiText *header = NULL;
    struct DjuiFlowLayout *body = NULL;

    (void)forcedLeftSide;

    if (gDjuiRoot == NULL) {
        return NULL;
    }

    panel = djui_three_panel_create(&gDjuiRoot->base, 38.0f, 0.0f, 0.0f);
    if (panel == NULL) {
        return NULL;
    }

    djui_base_set_size_type(&panel->base, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&panel->base, 214.0f, 150.0f);
    if (!forcedLeftSide) {
        djui_base_set_alignment(&panel->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_CENTER);
    }
    djui_base_set_location(&panel->base, 0.0f, 0.0f);
    djui_base_set_color(&panel->base, 0, 0, 0, 200);
    djui_base_set_border_width(&panel->base, 8.0f);
    djui_base_set_border_color(&panel->base, 75, 75, 75, 255);
    djui_base_set_padding(&panel->base, 16.0f, 16.0f, 16.0f, 16.0f);

    header = djui_text_create(&panel->base, (headerText == NULL) ? "" : headerText);
    if (header != NULL) {
        djui_base_set_size_type(&header->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(&header->base, 1.0f, 34.0f);
        djui_base_set_location(&header->base, 0.0f, -8.0f);
        djui_text_set_alignment(header, DJUI_HALIGN_CENTER, DJUI_VALIGN_BOTTOM);
        djui_base_set_color(&header->base, 255, 255, 255, 255);
    }

    body = djui_flow_layout_create(&panel->base);
    if (body != NULL) {
        djui_base_set_alignment(&body->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_TOP);
        djui_base_set_location(&body->base, 0.0f, 34.0f);
        djui_base_set_size_type(&body->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(&body->base, 0.86f, 0.0f);
        djui_base_set_color(&body->base, 0, 0, 0, 0);
        djui_flow_layout_set_margin(body, 0.0f);
        djui_flow_layout_set_flow_direction(body, DJUI_FLOW_DIR_DOWN);
    }

    return panel;
}
