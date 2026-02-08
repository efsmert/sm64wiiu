#include "djui_cursor.h"

#include <math.h>

#include "djui_interactable.h"
#include "djui_root.h"

static bool sCursorVisible = true;
static struct DjuiBase *sInputControlledBase = NULL;

f32 gCursorX = 0.0f;
f32 gCursorY = 0.0f;

void djui_cursor_set_visible(bool visible) {
    sCursorVisible = visible;
}

bool djui_cursor_inside_base(struct DjuiBase *base) {
    if (base == NULL) {
        return false;
    }
    if (gCursorX < base->elem.x || gCursorX > base->elem.x + base->elem.width) {
        return false;
    }
    if (gCursorY < base->elem.y || gCursorY > base->elem.y + base->elem.height) {
        return false;
    }
    return true;
}

static void djui_cursor_base_hover_location(struct DjuiBase *base, f32 *x, f32 *y) {
    if (base == NULL || base->get_cursor_hover_location == NULL) {
        *x = gCursorX;
        *y = gCursorY;
        return;
    }
    base->get_cursor_hover_location(base, x, y);
}

void djui_cursor_input_controlled_center(struct DjuiBase *base) {
    if (base != NULL && base->interactable != NULL && !base->interactable->enabled) {
        return;
    }
    sInputControlledBase = base;
    djui_cursor_set_visible(base != NULL);
    if (base != NULL) {
        djui_cursor_base_hover_location(base, &gCursorX, &gCursorY);
    }
}

static f32 djui_cursor_base_distance(struct DjuiBase *base, f32 xScale, f32 yScale) {
    f32 x = 0.0f;
    f32 y = 0.0f;
    djui_cursor_base_hover_location(base, &x, &y);
    x -= gCursorX;
    y -= gCursorY;
    return sqrtf((x * x) * xScale + (y * y) * yScale);
}

static void djui_cursor_move_check(s8 xDir, s8 yDir, struct DjuiBase **pick, struct DjuiBase *base) {
    if (base == NULL || !base->visible) {
        return;
    }

    if (base->interactable != NULL && base->interactable->enabled) {
        f32 x1 = base->elem.x;
        f32 y1 = base->elem.y;
        f32 x2 = base->elem.x + base->elem.width;
        f32 y2 = base->elem.y + base->elem.height;
        bool xWithin = (gCursorX >= x1 && gCursorX <= x2);
        bool yWithin = (gCursorY >= y1 && gCursorY <= y2);
        bool valid = false;

        if (yDir > 0 && gCursorY < y1 && xWithin) {
            valid = true;
        } else if (yDir < 0 && gCursorY > y2 && xWithin) {
            valid = true;
        } else if (xDir > 0 && gCursorX < x1 && yWithin) {
            valid = true;
        } else if (xDir < 0 && gCursorX > x2 && yWithin) {
            valid = true;
        }

        if (valid) {
            if (*pick == NULL) {
                *pick = base;
            } else {
                f32 pickDist = djui_cursor_base_distance(*pick, xDir ? 1.0f : 1.2f, yDir ? 1.0f : 2.0f);
                f32 baseDist = djui_cursor_base_distance(base, xDir ? 1.0f : 1.2f, yDir ? 1.0f : 2.0f);
                if (baseDist < pickDist) {
                    *pick = base;
                }
            }
        }
    }

    for (struct DjuiBaseChild *child = base->child; child != NULL; child = child->next) {
        djui_cursor_move_check(xDir, yDir, pick, child->base);
    }
}

void djui_cursor_move(s8 xDir, s8 yDir) {
    struct DjuiBase *pick = NULL;
    if (gDjuiRoot == NULL || (xDir == 0 && yDir == 0)) {
        return;
    }
    djui_cursor_move_check(xDir, yDir, &pick, &gDjuiRoot->base);
    if (pick != NULL) {
        djui_cursor_input_controlled_center(pick);
    }
}

void djui_cursor_interp(void) {
    djui_cursor_update();
}

void djui_cursor_update(void) {
    if (!sCursorVisible) {
        return;
    }

    if (sInputControlledBase != NULL) {
        djui_cursor_base_hover_location(sInputControlledBase, &gCursorX, &gCursorY);
    }
}

void djui_cursor_create(void) {
    sInputControlledBase = NULL;
    gCursorX = 0.0f;
    gCursorY = 0.0f;
    sCursorVisible = true;
}
