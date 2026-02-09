#include <string.h>

#include "player_palette.h"
#include "pc/configfile.h"

const struct PlayerPalette DEFAULT_MARIO_PALETTE = {
    .parts = {
        { 0x00, 0x00, 0xff },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0xff, 0xff },
        { 0x72, 0x1c, 0x0e },
        { 0x73, 0x06, 0x00 },
        { 0xfe, 0xc1, 0x79 },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0x00, 0x00 },
    },
};

struct PresetPalette gPresetPalettes[MAX_PRESET_PALETTES] = { 0 };
u16 gPresetPaletteCount = 0;

void player_palettes_reset(void) {
    memset(gPresetPalettes, 0, sizeof(gPresetPalettes));
    gPresetPaletteCount = 0;
}

void player_palettes_read(const char *palettePath, bool appendPalettes) {
    (void)palettePath;
    (void)appendPalettes;
}

void player_palette_export(char *name) {
    (void)name;
}

bool player_palette_delete(const char *palettesPath, char *name, bool appendPalettes) {
    (void)palettesPath;
    (void)name;
    (void)appendPalettes;
    return false;
}
