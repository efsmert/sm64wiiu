#include "djui_interactable.h"

#include <stdlib.h>

#include "audio/external.h"
#include "game/game_init.h"
#include "sm64.h"
#include "sounds.h"
#include "djui_cursor.h"
#include "djui_panel.h"
#include "djui_root.h"

#define CALL_CB(base, fn) if ((base) != NULL && (base)->interactable != NULL && (base)->interactable->fn != NULL) { (base)->interactable->fn(base); }
#define CALL_CB_PARAM(base, fn, p) if ((base) != NULL && (base)->interactable != NULL && (base)->interactable->fn != NULL) { (base)->interactable->fn(base, p); }

enum PadHoldDirection {
    PAD_HOLD_DIR_NONE,
    PAD_HOLD_DIR_UP,
    PAD_HOLD_DIR_DOWN,
    PAD_HOLD_DIR_LEFT,
    PAD_HOLD_DIR_RIGHT,
};

struct DjuiBase *gDjuiHovered = NULL;
struct DjuiBase *gDjuiCursorDownOn = NULL;
struct DjuiBase *gInteractableFocus = NULL;
struct DjuiBase *gInteractableBinding = NULL;
struct DjuiBase *gInteractableMouseDown = NULL;
bool gInteractableOverridePad = false;
OSContPad gInteractablePad = { 0 };

static bool sIgnoreInteractableUntilCursorReleased = false;
static u16 sLastPadButtons = 0;
static enum PadHoldDirection sLastPadHoldDirection = PAD_HOLD_DIR_NONE;
static s32 sPadHoldFrames = 0;

static void djui_interactable_update_style(struct DjuiBase *base) {
    CALL_CB(base, update_style);
}

static void djui_interactable_on_click(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_click);
}

static void djui_interactable_on_hover(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_hover);
}

static void djui_interactable_on_hover_end(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_hover_end);
}

static void djui_interactable_on_cursor_down_begin(struct DjuiBase *base, bool inputCursor) {
    if (gDjuiHovered != NULL) {
        djui_interactable_on_hover_end(gDjuiHovered);
        gDjuiHovered = NULL;
    }

    gDjuiCursorDownOn = base;
    djui_interactable_update_style(base);
    CALL_CB_PARAM(base, on_cursor_down_begin, inputCursor);
}

static void djui_interactable_on_cursor_down(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_cursor_down);
}

static void djui_interactable_on_cursor_down_end(struct DjuiBase *base) {
    if (gDjuiCursorDownOn == base) {
        gDjuiCursorDownOn = NULL;
    }

    djui_interactable_update_style(base);
    CALL_CB(base, on_cursor_down_end);

    if (djui_cursor_inside_base(base)) {
        djui_interactable_on_click(base);
        sIgnoreInteractableUntilCursorReleased = true;
    }
}

static void djui_interactable_on_focus_begin(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_focus_begin);
}

static void djui_interactable_on_focus(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_focus);
}

static void djui_interactable_on_focus_end(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_focus_end);
}

static void djui_interactable_on_bind(struct DjuiBase *base) {
    djui_interactable_update_style(base);
    CALL_CB(base, on_bind);
}

static void djui_interactable_cursor_update_active(struct DjuiBase *base) {
    static struct DjuiBase *insideParent = NULL;
    struct DjuiBaseChild *child = NULL;

    if (base == NULL || !base->visible || !base->enabled) {
        return;
    }

    if (!djui_cursor_inside_base(base)) {
        return;
    }

    if (base->interactable != NULL && base->interactable->enabled) {
        gDjuiHovered = base;
        insideParent = base;
    } else if (insideParent == NULL) {
        gDjuiHovered = NULL;
    }

    child = base->child;
    while (child != NULL) {
        djui_interactable_cursor_update_active(child->base);
        child = child->next;
    }

    if (insideParent == base) {
        insideParent = NULL;
    }
}

bool djui_interactable_is_binding(void) {
    return gInteractableBinding != NULL;
}

void djui_interactable_set_binding(struct DjuiBase *base) {
    gInteractableBinding = base;
    djui_cursor_set_visible(base == NULL);
    if (base == NULL) {
        sIgnoreInteractableUntilCursorReleased = true;
    }
}

void djui_interactable_set_input_focus(struct DjuiBase *base) {
    if (gInteractableFocus == base) {
        return;
    }

    djui_interactable_on_focus_end(gInteractableFocus);
    gInteractableFocus = base;
    djui_interactable_on_focus_begin(base);
}

bool djui_interactable_is_input_focus(struct DjuiBase *base) {
    return gInteractableFocus == base;
}

void djui_interactable_ignore_until_release(void) {
    sIgnoreInteractableUntilCursorReleased = true;
    gInteractableMouseDown = NULL;
    gDjuiCursorDownOn = NULL;
}

static void djui_interactable_update_pad(void) {
    enum PadHoldDirection padHoldDirection = PAD_HOLD_DIR_NONE;
    bool validPadHold = false;

    gInteractablePad.button = 0;
    gInteractablePad.stick_x = 0;
    gInteractablePad.stick_y = 0;

    if (gPlayer1Controller == NULL) {
        sLastPadHoldDirection = PAD_HOLD_DIR_NONE;
        sPadHoldFrames = 0;
        return;
    }

    gInteractablePad.button = gPlayer1Controller->buttonDown;
    gInteractablePad.stick_x = (s8)gPlayer1Controller->rawStickX;
    gInteractablePad.stick_y = (s8)gPlayer1Controller->rawStickY;

    if (gInteractablePad.button & (U_JPAD | U_CBUTTONS)) {
        padHoldDirection = PAD_HOLD_DIR_UP;
    } else if (gInteractablePad.button & (D_JPAD | D_CBUTTONS)) {
        padHoldDirection = PAD_HOLD_DIR_DOWN;
    } else if (gInteractablePad.button & (L_JPAD | L_CBUTTONS)) {
        padHoldDirection = PAD_HOLD_DIR_LEFT;
    } else if (gInteractablePad.button & (R_JPAD | R_CBUTTONS)) {
        padHoldDirection = PAD_HOLD_DIR_RIGHT;
    } else if (gInteractablePad.stick_y > 60) {
        padHoldDirection = PAD_HOLD_DIR_UP;
    } else if (gInteractablePad.stick_y < -60) {
        padHoldDirection = PAD_HOLD_DIR_DOWN;
    } else if (gInteractablePad.stick_x > 60) {
        padHoldDirection = PAD_HOLD_DIR_RIGHT;
    } else if (gInteractablePad.stick_x < -60) {
        padHoldDirection = PAD_HOLD_DIR_LEFT;
    }

    if (padHoldDirection == PAD_HOLD_DIR_NONE) {
        sPadHoldFrames = 0;
    } else if (padHoldDirection != sLastPadHoldDirection) {
        sPadHoldFrames = 0;
        validPadHold = true;
    } else {
        sPadHoldFrames++;
        if (sPadHoldFrames > 8 && ((sPadHoldFrames - 8) % 3) == 0) {
            validPadHold = true;
        }
    }

    if (validPadHold && gInteractableFocus == NULL) {
        switch (padHoldDirection) {
            case PAD_HOLD_DIR_UP:    djui_cursor_move(0, -1); break;
            case PAD_HOLD_DIR_DOWN:  djui_cursor_move(0, 1); break;
            case PAD_HOLD_DIR_LEFT:  djui_cursor_move(-1, 0); break;
            case PAD_HOLD_DIR_RIGHT: djui_cursor_move(1, 0); break;
            default: break;
        }
    }

    sLastPadHoldDirection = padHoldDirection;
}

void djui_interactable_update(void) {
    const u16 mainButtons = PAD_BUTTON_A | PAD_BUTTON_B;
    u16 padButtons = 0;
    bool aDown = false;
    bool lastADown = false;

    djui_interactable_update_pad();
    padButtons = gInteractablePad.button;

    if (sIgnoreInteractableUntilCursorReleased) {
        if (padButtons & (PAD_BUTTON_A | PAD_BUTTON_B)) {
            padButtons &= (u16)~(PAD_BUTTON_A | PAD_BUTTON_B);
        } else {
            sIgnoreInteractableUntilCursorReleased = false;
        }
    }

    if (gInteractableFocus != NULL) {
        if ((padButtons & mainButtons) && !(sLastPadButtons & mainButtons)) {
            djui_interactable_set_input_focus(NULL);
        } else {
            djui_interactable_on_focus(gInteractableFocus);
        }
    } else if ((padButtons & PAD_BUTTON_B) && !(sLastPadButtons & PAD_BUTTON_B)) {
        djui_panel_back();
        sIgnoreInteractableUntilCursorReleased = true;
        sLastPadButtons = padButtons;
        return;
    }

    if (gInteractableBinding != NULL) {
        djui_interactable_on_bind(gInteractableBinding);
        sLastPadButtons = padButtons;
        return;
    }

    aDown = (padButtons & PAD_BUTTON_A) != 0;
    lastADown = (sLastPadButtons & PAD_BUTTON_A) != 0;

    if (aDown) {
        if (!lastADown && gDjuiHovered != NULL) {
            gInteractableMouseDown = gDjuiHovered;
            gDjuiHovered = NULL;
            djui_interactable_on_cursor_down_begin(gInteractableMouseDown, true);
        } else {
            djui_interactable_on_cursor_down(gInteractableMouseDown);
        }
    } else {
        struct DjuiBase *lastHovered = gDjuiHovered;

        if (lastADown && gInteractableMouseDown != NULL) {
            djui_interactable_on_cursor_down_end(gInteractableMouseDown);
            gInteractableMouseDown = NULL;
        }

        gDjuiHovered = NULL;
        if (gDjuiRoot != NULL) {
            djui_interactable_cursor_update_active(&gDjuiRoot->base);
        }

        if (lastHovered != gDjuiHovered) {
            djui_interactable_on_hover_end(lastHovered);
            play_sound(SOUND_MENU_MESSAGE_NEXT_PAGE, gGlobalSoundSource);
        }
        djui_interactable_on_hover(gDjuiHovered);
    }

    sLastPadButtons = padButtons;
}

void djui_interactable_hook_hover(struct DjuiBase *base, void (*on_hover)(struct DjuiBase *),
                                  void (*on_hover_end)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_hover = on_hover;
    base->interactable->on_hover_end = on_hover_end;
}

void djui_interactable_hook_cursor_down(struct DjuiBase *base,
                                        void (*on_cursor_down_begin)(struct DjuiBase *, bool),
                                        void (*on_cursor_down)(struct DjuiBase *),
                                        void (*on_cursor_down_end)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_cursor_down_begin = on_cursor_down_begin;
    base->interactable->on_cursor_down = on_cursor_down;
    base->interactable->on_cursor_down_end = on_cursor_down_end;
}

void djui_interactable_hook_focus(struct DjuiBase *base, void (*on_focus_begin)(struct DjuiBase *),
                                  void (*on_focus)(struct DjuiBase *), void (*on_focus_end)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_focus_begin = on_focus_begin;
    base->interactable->on_focus = on_focus;
    base->interactable->on_focus_end = on_focus_end;
}

void djui_interactable_hook_click(struct DjuiBase *base, void (*on_click)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_click = on_click;
}

void djui_interactable_hook_value_change(struct DjuiBase *base, void (*on_value_change)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_value_change = on_value_change;
}

void djui_interactable_hook_bind(struct DjuiBase *base, void (*on_bind)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_bind = on_bind;
}

void djui_interactable_hook_enabled_change(struct DjuiBase *base, void (*on_enabled_change)(struct DjuiBase *)) {
    if (base == NULL || base->interactable == NULL) {
        return;
    }
    base->interactable->on_enabled_change = on_enabled_change;
}

void djui_interactable_create(struct DjuiBase *base, void (*update_style)(struct DjuiBase *)) {
    struct DjuiInteractable *interactable = NULL;

    if (base == NULL) {
        return;
    }

    if (base->interactable != NULL) {
        free(base->interactable);
        base->interactable = NULL;
    }

    interactable = calloc(1, sizeof(struct DjuiInteractable));
    if (interactable == NULL) {
        return;
    }

    interactable->enabled = true;
    interactable->update_style = update_style;
    base->interactable = interactable;
}
