#include "characters.h"

extern ALIGNED8 const u8 texture_hud_char_mario_head[];
extern ALIGNED8 const u8 texture_hud_char_luigi_head[];
extern ALIGNED8 const u8 texture_hud_char_toad_head[];
extern ALIGNED8 const u8 texture_hud_char_waluigi_head[];
extern ALIGNED8 const u8 texture_hud_char_wario_head[];

struct Character gCharacters[CT_MAX] = {
    { CT_MARIO, "Mario", { .texture = texture_hud_char_mario_head, .name = "texture_hud_char_mario_head", .width = 16, .height = 16, .format = G_IM_FMT_RGBA, .size = G_IM_SIZ_16b } },
    { CT_LUIGI, "Luigi", { .texture = texture_hud_char_luigi_head, .name = "texture_hud_char_luigi_head", .width = 16, .height = 16, .format = G_IM_FMT_RGBA, .size = G_IM_SIZ_16b } },
    { CT_TOAD, "Toad", { .texture = texture_hud_char_toad_head, .name = "texture_hud_char_toad_head", .width = 16, .height = 16, .format = G_IM_FMT_RGBA, .size = G_IM_SIZ_16b } },
    { CT_WALUIGI, "Waluigi", { .texture = texture_hud_char_waluigi_head, .name = "texture_hud_char_waluigi_head", .width = 16, .height = 16, .format = G_IM_FMT_RGBA, .size = G_IM_SIZ_16b } },
    { CT_WARIO, "Wario", { .texture = texture_hud_char_wario_head, .name = "texture_hud_char_wario_head", .width = 16, .height = 16, .format = G_IM_FMT_RGBA, .size = G_IM_SIZ_16b } },
};
