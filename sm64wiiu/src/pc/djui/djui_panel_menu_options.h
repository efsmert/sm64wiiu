#pragma once
#include "djui.h"

#define STAGE_MUSIC 0
#define MAIN_MENU_SOUND_COUNT 14

struct MainMenuSounds {
    char* name;
    int sound;
};

extern struct MainMenuSounds gMainMenuSounds[];

void djui_panel_main_menu_create(struct DjuiBase* caller);
