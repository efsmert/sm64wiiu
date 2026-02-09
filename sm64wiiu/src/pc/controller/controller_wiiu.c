#ifdef TARGET_WII_U

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <vpad/input.h>
#include <padscore/wpad.h>
#include <padscore/kpad.h>

#include "controller_api.h"
#include "../configfile.h"
#include "../pc_main.h"

#define VK_BASE_WIIU 0x2000

struct WiiUKeymap {
    uint32_t n64Button;
    uint32_t vpadButton;
    uint32_t classicButton;
    uint32_t proButton;
};

// Button shortcuts
#define VB(btn) VPAD_BUTTON_##btn
#define CB(btn) WPAD_CLASSIC_BUTTON_##btn
#define PB(btn) WPAD_PRO_BUTTON_##btn
#define PT(btn) WPAD_PRO_TRIGGER_##btn

// Stick emulation
#define SE(dir) VPAD_STICK_R_EMULATION_##dir, WPAD_CLASSIC_STICK_R_EMULATION_##dir, WPAD_PRO_STICK_R_EMULATION_##dir

struct WiiUKeymap map[] = {
    { B_BUTTON, VB(B) | VB(Y), CB(B) | CB(Y), PB(B) | PB(Y) },
    { A_BUTTON, VB(A) | VB(X), CB(A) | CB(X), PB(A) | PB(X) },
    { START_BUTTON, VB(PLUS), CB(PLUS), PB(PLUS) },
    { Z_TRIG, VB(L) | VB(ZL), CB(L) | CB(ZL), PT(L) | PT(ZL) },
    { L_TRIG, VB(MINUS), CB(MINUS), PB(MINUS) },
    { R_TRIG, VB(R) | VB(ZR), CB(R) | CB(ZR), PT(R) | PT(ZR) },
    { U_CBUTTONS, SE(UP) },
    { D_CBUTTONS, SE(DOWN) },
    { L_CBUTTONS, SE(LEFT) },
    { R_CBUTTONS, SE(RIGHT) }
};

size_t num_buttons = sizeof(map) / sizeof(map[0]);
KPADStatus last_kpad = {0};
int kpad_timeout = 10;

static s8 controller_wiiu_clamp_stick(s16 value) {
    if (value > 80) { value = 80; }
    if (value < -80) { value = -80; }
    return (s8)value;
}

static void controller_wiiu_apply_stick_config(OSContPad *pad) {
    s16 x;
    s16 y;
    s16 tmp;
    s32 deadzone;

    if (pad == NULL) {
        return;
    }

    x = pad->stick_x;
    y = pad->stick_y;

    if (configStick.rotateLeft) {
        tmp = x;
        x = -y;
        y = tmp;
    }
    if (configStick.rotateRight) {
        tmp = x;
        x = y;
        y = -tmp;
    }
    if (configStick.invertLeftX) { x = -x; }
    if (configStick.invertLeftY) { y = -y; }

    deadzone = (s32)((configStickDeadzone * 80U) / 100U);
    if ((x * x + y * y) < deadzone * deadzone) {
        x = 0;
        y = 0;
    }

    pad->stick_x = controller_wiiu_clamp_stick(x);
    pad->stick_y = controller_wiiu_clamp_stick(y);
}

// Applies runtime button layout changes from config to all Wii U controller types.
static void controller_wiiu_apply_config(void) {
    map[0] = (struct WiiUKeymap) { B_BUTTON, VB(B) | VB(Y), CB(B) | CB(Y), PB(B) | PB(Y) };
    map[1] = (struct WiiUKeymap) { A_BUTTON, VB(A) | VB(X), CB(A) | CB(X), PB(A) | PB(X) };

    if (configN64FaceButtons) {
        map[0] = (struct WiiUKeymap) { B_BUTTON, VB(Y) | VB(X), CB(Y) | CB(X), PB(Y) | PB(X) };
        map[1] = (struct WiiUKeymap) { A_BUTTON, VB(B) | VB(A), CB(B) | CB(A), PB(B) | PB(A) };
    }
}

static void controller_wiiu_init(void) {
    VPADInit();
    KPADInit();
    WPADEnableURCC(1);
    WPADEnableWiiRemote(1);
    controller_wiiu_apply_config();
}

static bool read_vpad(OSContPad *pad) {
    VPADStatus status;
    VPADReadError err;
    uint32_t v;

    VPADRead(VPAD_CHAN_0, &status, 1, &err);

    if (err != 0) {
        return false;
    }

    v = status.hold;

    for (size_t i = 0; i < num_buttons; i++) {
        if (v & map[i].vpadButton) {
            pad->button |= map[i].n64Button;
        }
    }

    if (v & VPAD_BUTTON_LEFT) pad->stick_x = -80;
    if (v & VPAD_BUTTON_RIGHT) pad->stick_x = 80;
    if (v & VPAD_BUTTON_DOWN) pad->stick_y = -80;
    if (v & VPAD_BUTTON_UP) pad->stick_y = 80;

    if (status.leftStick.x != 0) {
        pad->stick_x = (s8) round(status.leftStick.x * 80);
    }
    if (status.leftStick.y != 0) {
        pad->stick_y = (s8) round(status.leftStick.y * 80);
    }

    return true;
}

static bool read_wpad_channel(OSContPad* pad, int channel) {
    WPADExtensionType ext;
    int res = WPADProbe(channel, &ext);
    if (res != 0) {
        return false;
    }

    KPADStatus status;
    int err;
    int read = KPADReadEx(channel, &status, 1, &err);
    if (read == 0) {
        kpad_timeout--;

        if (kpad_timeout == 0) {
            WPADDisconnect(channel);
            memset(&last_kpad, 0, sizeof(KPADStatus));
            return false;
        }
        status = last_kpad;
    } else {
        kpad_timeout = 10;
        last_kpad = status;
    }

    uint32_t wm = status.hold;
    KPADVec2D stick;

    bool gamepadStickNotSet = pad->stick_x == 0 && pad->stick_y == 0;

    if (status.extensionType == WPAD_EXT_NUNCHUK || status.extensionType == WPAD_EXT_MPLUS_NUNCHUK) {
        uint32_t ext = status.nunchuk.hold;
        stick = status.nunchuk.stick;

        if (wm & WPAD_BUTTON_A) pad->button |= A_BUTTON;
        if (wm & WPAD_BUTTON_B) pad->button |= B_BUTTON;
        if (wm & WPAD_BUTTON_PLUS) pad->button |= START_BUTTON;
        if (wm & WPAD_BUTTON_UP) pad->button |= U_CBUTTONS;
        if (wm & WPAD_BUTTON_DOWN) pad->button |= D_CBUTTONS;
        if (wm & WPAD_BUTTON_LEFT) pad->button |= L_CBUTTONS;
        if (wm & WPAD_BUTTON_RIGHT) pad->button |= R_CBUTTONS;
        if (ext & WPAD_NUNCHUK_BUTTON_C) pad->button |= R_TRIG;
        if (ext & WPAD_NUNCHUK_BUTTON_Z) pad->button |= Z_TRIG;
    } else if (status.extensionType == WPAD_EXT_CLASSIC || status.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        uint32_t ext = status.classic.hold;
        stick = status.classic.leftStick;
        for (size_t i = 0; i < num_buttons; i++) {
            if (ext & map[i].classicButton) {
                pad->button |= map[i].n64Button;
            }
        }
        if (ext & WPAD_CLASSIC_BUTTON_LEFT) pad->stick_x = -80;
        if (ext & WPAD_CLASSIC_BUTTON_RIGHT) pad->stick_x = 80;
        if (ext & WPAD_CLASSIC_BUTTON_DOWN) pad->stick_y = -80;
        if (ext & WPAD_CLASSIC_BUTTON_UP) pad->stick_y = 80;
    } else if (status.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        uint32_t ext = status.pro.hold;
        stick = status.pro.leftStick;
        for (size_t i = 0; i < num_buttons; i++) {
            if (ext & map[i].proButton) {
                pad->button |= map[i].n64Button;
            }
        }
        if (ext & WPAD_PRO_BUTTON_LEFT) pad->stick_x = -80;
        if (ext & WPAD_PRO_BUTTON_RIGHT) pad->stick_x = 80;
        if (ext & WPAD_PRO_BUTTON_DOWN) pad->stick_y = -80;
        if (ext & WPAD_PRO_BUTTON_UP) pad->stick_y = 80;
    }

    // If we didn't already get stick input from the gamepad
    if (gamepadStickNotSet) {
        if (stick.x != 0) {
            pad->stick_x = (s8) round(stick.x * 80);
        }
        if (stick.y != 0) {
            pad->stick_y = (s8) round(stick.y * 80);
        }
    }

    return true;
}

static void controller_wiiu_read(OSContPad* pad) {
    bool hasInput = false;
    unsigned int selectedGamepad = 0;

    pad->stick_x = 0;
    pad->stick_y = 0;
    pad->button = 0;

    if (configDisableGamepads) {
        return;
    }

    if (!configBackgroundGamepad && wm_api != NULL && wm_api->has_focus != NULL && !wm_api->has_focus()) {
        return;
    }

    selectedGamepad = configGamepadNumber;

    // Wii U mapping: 0 = GamePad (VPAD), 1..4 = WPAD channels 0..3.
    if (selectedGamepad == 0) {
        hasInput = read_vpad(pad);
        if (!hasInput) {
            hasInput = read_wpad_channel(pad, 0);
        }
    } else {
        int wpadChannel = (int)(selectedGamepad - 1U);
        if (wpadChannel > 3) {
            wpadChannel = 0;
        }
        hasInput = read_wpad_channel(pad, wpadChannel);
    }

    if (hasInput) {
        controller_wiiu_apply_stick_config(pad);
    }
}

static u32 controller_wiiu_rawkey(void) {
    return VK_INVALID;
}

static void controller_wiiu_rumble_play(float str, float time) {
    (void)str;
    (void)time;
}

static void controller_wiiu_rumble_stop(void) {
}

static void controller_wiiu_shutdown(void) {
    KPADShutdown();
    VPADShutdown();
}

struct ControllerAPI controller_wiiu = {
    .vkbase = VK_BASE_WIIU,
    .init = controller_wiiu_init,
    .read = controller_wiiu_read,
    .rawkey = controller_wiiu_rawkey,
    .rumble_play = controller_wiiu_rumble_play,
    .rumble_stop = controller_wiiu_rumble_stop,
    .reconfig = controller_wiiu_apply_config,
    .shutdown = controller_wiiu_shutdown,
};

#endif
