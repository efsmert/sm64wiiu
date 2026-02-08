#pragma once

#include "djui_panel.h"
#include "djui_text.h"

enum DjuiRainbowColor {
    DJUI_RAINBOW_COLOR_RED = 0,
    DJUI_RAINBOW_COLOR_GREEN = 1,
    DJUI_RAINBOW_COLOR_BLUE = 2,
    DJUI_RAINBOW_COLOR_YELLOW = 3,
};

char *djui_menu_get_rainbow_string_color(enum DjuiRainbowColor color);
void djui_panel_menu_back(struct DjuiBase *base);
struct DjuiThreePanel *djui_panel_menu_create(char *headerText, bool forcedLeftSide);

