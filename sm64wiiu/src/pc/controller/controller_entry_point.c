#include "macros.h"

#include "lib/src/libultra_internal.h"
#include "lib/src/osContInternal.h"

#include "controller_recorded_tas.h"
#include "controller_keyboard.h"

#if defined(_WIN32) || defined(_WIN64)
#include "controller_xinput.h"
#elif defined(TARGET_WII_U)
#include "controller_wiiu.h"
#else
#include "controller_sdl.h"
#endif

#ifdef __linux__
#include "controller_wup.h"
#endif

static struct ControllerAPI *controller_implementations[] = {
    &controller_recorded_tas,
#if defined(_WIN32) || defined(_WIN64)
    &controller_xinput,
#elif defined(TARGET_WII_U) || defined(__WIIU__)
    &controller_wiiu,
#else
    &controller_sdl,
#endif
#ifdef __linux__
    &controller_wup,
#endif
    &controller_keyboard,
};

s32 osContInit(UNUSED OSMesgQueue *mq, u8 *controllerBits, UNUSED OSContStatus *status) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->init) {
            controller_implementations[i]->init();
        }
    }
    *controllerBits = 1;
    return 0;
}

s32 osContStartReadData(UNUSED OSMesgQueue *mesg) {
    return 0;
}

void osContGetReadData(OSContPad *pad) {
    pad->button = 0;
    pad->stick_x = 0;
    pad->stick_y = 0;
    pad->errnum = 0;

    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->read) {
            controller_implementations[i]->read(pad);
        }
    }
}

u32 controller_get_raw_key(void) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->rawkey == NULL) {
            continue;
        }
        u32 vk = controller_implementations[i]->rawkey();
        if (vk != VK_INVALID) {
            return vk + controller_implementations[i]->vkbase;
        }
    }
    return VK_INVALID;
}

void controller_shutdown(void) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->shutdown) {
            controller_implementations[i]->shutdown();
        }
    }
}

void controller_reconfigure(void) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->reconfig) {
            controller_implementations[i]->reconfig();
        }
    }
}

void controller_rumble_play(float str, float time) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->rumble_play) {
            controller_implementations[i]->rumble_play(str, time);
        }
    }
}

void controller_rumble_stop(void) {
    for (size_t i = 0; i < sizeof(controller_implementations) / sizeof(struct ControllerAPI *); i++) {
        if (controller_implementations[i]->rumble_stop) {
            controller_implementations[i]->rumble_stop();
        }
    }
}
