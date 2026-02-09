#ifndef CHARACTERS_H
#define CHARACTERS_H

#include "types.h"

enum CharacterType {
    CT_MARIO,
    CT_LUIGI,
    CT_TOAD,
    CT_WALUIGI,
    CT_WARIO,
    CT_MAX,
};

struct Character {
    enum CharacterType type;
    char *name;
    struct TextureInfo hudHeadTexture;
};

#define CHAR_SOUND_OKEY_DOKEY 43
#define CHAR_SOUND_WAAAOOOW 15

extern struct Character gCharacters[CT_MAX];

#endif
