#include <stdbool.h>
#include <ultra64.h>

#include "controller_api.h"
#include "controller_keyboard.h"

#ifdef TARGET_WEB
#include "controller_emscripten_keyboard.h"
#endif

#include "../configfile.h"

static int keyboard_buttons_down;
static u32 keyboard_lastkey = VK_INVALID;

static int keyboard_mapping[13][2];

static int keyboard_map_scancode(int scancode) {
    int ret = 0;
    for (size_t i = 0; i < sizeof(keyboard_mapping) / sizeof(keyboard_mapping[0]); i++) {
        if (keyboard_mapping[i][0] == scancode) {
            ret |= keyboard_mapping[i][1];
        }
    }
    return ret;
}

bool keyboard_on_key_down(int scancode) {
    int mapped = keyboard_map_scancode(scancode);
    keyboard_buttons_down |= mapped;
    keyboard_lastkey = (u32)scancode;
    return mapped != 0;
}

bool keyboard_on_key_up(int scancode) {
    int mapped = keyboard_map_scancode(scancode);
    keyboard_buttons_down &= ~mapped;
    if (keyboard_lastkey == (u32)scancode) {
        keyboard_lastkey = VK_INVALID;
    }
    return mapped != 0;
}

void keyboard_on_all_keys_up(void) {
    keyboard_buttons_down = 0;
}

static void set_keyboard_mapping(int index, int mask, int scancode) {
    keyboard_mapping[index][0] = scancode;
    keyboard_mapping[index][1] = mask;
}

static void keyboard_bindkeys(void) {
    int i = 0;

    set_keyboard_mapping(i++, STICK_UP,     configKeyStickUp[0]);
    set_keyboard_mapping(i++, STICK_LEFT,   configKeyStickLeft[0]);
    set_keyboard_mapping(i++, STICK_DOWN,   configKeyStickDown[0]);
    set_keyboard_mapping(i++, STICK_RIGHT,  configKeyStickRight[0]);
    set_keyboard_mapping(i++, A_BUTTON,     configKeyA[0]);
    set_keyboard_mapping(i++, B_BUTTON,     configKeyB[0]);
    set_keyboard_mapping(i++, Z_TRIG,       configKeyZ[0]);
    set_keyboard_mapping(i++, U_CBUTTONS,   configKeyCUp[0]);
    set_keyboard_mapping(i++, L_CBUTTONS,   configKeyCLeft[0]);
    set_keyboard_mapping(i++, D_CBUTTONS,   configKeyCDown[0]);
    set_keyboard_mapping(i++, R_CBUTTONS,   configKeyCRight[0]);
    set_keyboard_mapping(i++, R_TRIG,       configKeyR[0]);
    set_keyboard_mapping(i++, START_BUTTON, configKeyStart[0]);
}

static void keyboard_init(void) {
    keyboard_bindkeys();

#ifdef TARGET_WEB
    controller_emscripten_keyboard_init();
#endif
}

static void keyboard_read(OSContPad *pad) {
    pad->button |= keyboard_buttons_down;
    if ((keyboard_buttons_down & STICK_XMASK) == STICK_LEFT) {
        pad->stick_x = -128;
    }
    if ((keyboard_buttons_down & STICK_XMASK) == STICK_RIGHT) {
        pad->stick_x = 127;
    }
    if ((keyboard_buttons_down & STICK_YMASK) == STICK_DOWN) {
        pad->stick_y = -128;
    }
    if ((keyboard_buttons_down & STICK_YMASK) == STICK_UP) {
        pad->stick_y = 127;
    }
}

static u32 keyboard_rawkey(void) {
    u32 key = keyboard_lastkey;
    keyboard_lastkey = VK_INVALID;
    return key;
}

static void keyboard_shutdown(void) {
}

struct ControllerAPI controller_keyboard = {
    .vkbase = VK_BASE_KEYBOARD,
    .init = keyboard_init,
    .read = keyboard_read,
    .rawkey = keyboard_rawkey,
    .rumble_play = NULL,
    .rumble_stop = NULL,
    .reconfig = keyboard_bindkeys,
    .shutdown = keyboard_shutdown,
};
