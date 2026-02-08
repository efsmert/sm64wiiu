#include "djui_base.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/segment2.h"
#include "djui_gfx.h"

void create_dl_scale_matrix(s8 pushOp, f32 x, f32 y, f32 z);

static void djui_base_default_cursor_hover_location(struct DjuiBase *base, f32 *x, f32 *y) {
    *x = base->elem.x + (base->elem.width * 0.75f);
    *y = base->elem.y + (base->elem.height * 0.75f);
}

static void djui_base_add_child(struct DjuiBase *parent, struct DjuiBase *base) {
    struct DjuiBaseChild *baseChild = NULL;
    struct DjuiBaseChild *tail = NULL;

    if (parent == NULL || base == NULL) {
        return;
    }

    baseChild = calloc(1, sizeof(struct DjuiBaseChild));
    if (baseChild == NULL) {
        return;
    }

    baseChild->base = base;
    baseChild->next = NULL;

    if (parent->child == NULL || parent->addChildrenToHead) {
        baseChild->next = parent->child;
        parent->child = baseChild;
        return;
    }

    tail = parent->child;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = baseChild;
}

static void djui_base_clip(struct DjuiBase *base) {
    struct DjuiBase *parent = NULL;
    struct DjuiBaseRect *comp = NULL;
    struct DjuiBaseRect *clip = NULL;

    if (base == NULL) {
        return;
    }

    parent = base->parent;
    comp = &base->comp;
    clip = &base->clip;

    clip->x = comp->x;
    clip->y = comp->y;
    clip->width = comp->width;
    clip->height = comp->height;

    if (parent == NULL) {
        return;
    }

    clip->x = fmaxf(clip->x, parent->clip.x);
    clip->y = fmaxf(clip->y, parent->clip.y);

    clip->width = (comp->x + comp->width) - clip->x;
    clip->height = (comp->y + comp->height) - clip->y;

    clip->width = fminf(clip->width, (parent->clip.x + parent->clip.width) - clip->x);
    clip->height = fminf(clip->height, (parent->clip.y + parent->clip.height) - clip->y);
}

static void djui_base_add_padding(struct DjuiBase *base) {
    struct DjuiBaseRect *comp = NULL;
    f32 tPad = 0.0f;
    f32 rPad = 0.0f;
    f32 bPad = 0.0f;
    f32 lPad = 0.0f;

    if (base == NULL) {
        return;
    }

    comp = &base->comp;

    tPad = (base->padding.top.type == DJUI_SVT_RELATIVE) ? (comp->height * base->padding.top.value) : base->padding.top.value;
    rPad = (base->padding.right.type == DJUI_SVT_RELATIVE) ? (comp->width * base->padding.right.value) : base->padding.right.value;
    bPad = (base->padding.bottom.type == DJUI_SVT_RELATIVE) ? (comp->height * base->padding.bottom.value) : base->padding.bottom.value;
    lPad = (base->padding.left.type == DJUI_SVT_RELATIVE) ? (comp->width * base->padding.left.value) : base->padding.left.value;

    comp->x += lPad;
    comp->y += tPad;
    comp->width -= (lPad + rPad);
    comp->height -= (tPad + bPad);
}

static f32 djui_base_render_border_piece(struct DjuiBase *base, f32 x1, f32 y1, f32 x2, f32 y2, bool isXBorder) {
    struct DjuiBaseRect *clip = NULL;
    f32 width = 0.0f;
    f32 height = 0.0f;

    if (base == NULL) {
        return 0.0f;
    }

    clip = &base->clip;

    x1 = fmaxf(x1, clip->x);
    y1 = fmaxf(y1, clip->y);
    x2 = fminf(x2, clip->x + clip->width);
    y2 = fminf(y2, clip->y + clip->height);

    if (x2 <= x1 || y2 <= y1) {
        return 0.0f;
    }

    width = x2 - x1;
    height = y2 - y1;

    create_dl_translation_matrix(MENU_MTX_PUSH, x1, y1, 0.0f);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, width / 130.0f, height / 80.0f, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, base->borderColor.r, base->borderColor.g, base->borderColor.b, base->borderColor.a);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);

    return isXBorder ? fmaxf(width, 0.0f) : fmaxf(height, 0.0f);
}

static void djui_base_render_border(struct DjuiBase *base) {
    struct DjuiBaseRect *comp = NULL;
    struct DjuiBaseRect *clip = NULL;
    struct DjuiBaseRect savedComp;
    f32 xBorderWidth = 0.0f;
    f32 yBorderWidth = 0.0f;
    f32 addClip = 0.0f;

    if (base == NULL) {
        return;
    }

    comp = &base->comp;
    clip = &base->clip;
    savedComp = *comp;

    xBorderWidth = (base->borderWidth.type == DJUI_SVT_RELATIVE) ? (savedComp.width * base->borderWidth.value)
                                                                  : base->borderWidth.value;
    yBorderWidth = (base->borderWidth.type == DJUI_SVT_RELATIVE) ? (savedComp.height * base->borderWidth.value)
                                                                  : base->borderWidth.value;

    xBorderWidth = fminf(xBorderWidth, savedComp.width * 0.5f);
    yBorderWidth = fminf(yBorderWidth, savedComp.height * 0.5f);

    comp->x += xBorderWidth;
    comp->y += yBorderWidth;
    comp->width -= xBorderWidth * 2.0f;
    comp->height -= yBorderWidth * 2.0f;

    if (comp->width <= 0.0f || comp->height <= 0.0f) {
        return;
    }

    addClip = djui_base_render_border_piece(base, savedComp.x, savedComp.y, savedComp.x + savedComp.width,
                                            savedComp.y + yBorderWidth, false);
    clip->y += addClip;
    clip->height -= addClip;

    addClip = djui_base_render_border_piece(base, savedComp.x, savedComp.y + savedComp.height - yBorderWidth,
                                            savedComp.x + savedComp.width, savedComp.y + savedComp.height, false);
    clip->height -= addClip;

    addClip = djui_base_render_border_piece(base, savedComp.x, savedComp.y,
                                            savedComp.x + xBorderWidth, savedComp.y + savedComp.height, true);
    clip->x += addClip;
    clip->width -= addClip;

    addClip = djui_base_render_border_piece(base, savedComp.x + savedComp.width - xBorderWidth, savedComp.y,
                                            savedComp.x + savedComp.width, savedComp.y + savedComp.height, true);
    clip->width -= addClip;
}

void djui_base_set_visible(struct DjuiBase *base, bool visible) {
    if (base != NULL) {
        base->visible = visible;
    }
}

void djui_base_set_enabled(struct DjuiBase *base, bool enabled) {
    if (base == NULL) {
        return;
    }
    base->enabled = enabled;
    if (base->interactable != NULL && base->interactable->on_enabled_change != NULL) {
        base->interactable->on_enabled_change(base);
    }
    if (base->interactable != NULL && base->interactable->update_style != NULL) {
        base->interactable->update_style(base);
    }
}

void djui_base_set_location(struct DjuiBase *base, f32 x, f32 y) {
    if (base == NULL) {
        return;
    }
    base->x.value = x;
    base->y.value = y;
}

void djui_base_set_location_type(struct DjuiBase *base, enum DjuiScreenValueType xType, enum DjuiScreenValueType yType) {
    if (base == NULL) {
        return;
    }
    base->x.type = xType;
    base->y.type = yType;
}

void djui_base_set_size(struct DjuiBase *base, f32 width, f32 height) {
    if (base == NULL) {
        return;
    }
    base->width.value = width;
    base->height.value = height;
}

void djui_base_set_size_type(struct DjuiBase *base, enum DjuiScreenValueType widthType, enum DjuiScreenValueType heightType) {
    if (base == NULL) {
        return;
    }
    base->width.type = widthType;
    base->height.type = heightType;
}

void djui_base_set_color(struct DjuiBase *base, u8 r, u8 g, u8 b, u8 a) {
    if (base == NULL) {
        return;
    }
    base->color.r = r;
    base->color.g = g;
    base->color.b = b;
    base->color.a = a;
}

void djui_base_set_border_width(struct DjuiBase *base, f32 width) {
    if (base != NULL) {
        base->borderWidth.value = width;
    }
}

void djui_base_set_border_width_type(struct DjuiBase *base, enum DjuiScreenValueType widthType) {
    if (base != NULL) {
        base->borderWidth.type = widthType;
    }
}

void djui_base_set_border_color(struct DjuiBase *base, u8 r, u8 g, u8 b, u8 a) {
    if (base == NULL) {
        return;
    }
    base->borderColor.r = r;
    base->borderColor.g = g;
    base->borderColor.b = b;
    base->borderColor.a = a;
}

void djui_base_set_padding(struct DjuiBase *base, f32 top, f32 right, f32 bottom, f32 left) {
    if (base == NULL) {
        return;
    }
    base->padding.top.value = top;
    base->padding.right.value = right;
    base->padding.bottom.value = bottom;
    base->padding.left.value = left;
}

void djui_base_set_padding_type(struct DjuiBase *base, enum DjuiScreenValueType topType, enum DjuiScreenValueType rightType,
                                enum DjuiScreenValueType bottomType, enum DjuiScreenValueType leftType) {
    if (base == NULL) {
        return;
    }
    base->padding.top.type = topType;
    base->padding.right.type = rightType;
    base->padding.bottom.type = bottomType;
    base->padding.left.type = leftType;
}

void djui_base_set_alignment(struct DjuiBase *base, enum DjuiHAlign hAlign, enum DjuiVAlign vAlign) {
    if (base == NULL) {
        return;
    }
    base->hAlign = hAlign;
    base->vAlign = vAlign;
}

void djui_base_set_gradient(struct DjuiBase *base, bool gradient) {
    if (base != NULL) {
        base->gradient = gradient;
    }
}

void djui_base_compute(struct DjuiBase *base) {
    struct DjuiBase *parent = NULL;
    struct DjuiBaseRect *comp = NULL;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;

    if (base == NULL) {
        return;
    }

    parent = base->parent;
    comp = &base->comp;

    x = (parent != NULL && base->x.type == DJUI_SVT_RELATIVE) ? parent->comp.width * base->x.value : base->x.value;
    y = (parent != NULL && base->y.type == DJUI_SVT_RELATIVE) ? parent->comp.height * base->y.value : base->y.value;
    width = (parent != NULL && base->width.type == DJUI_SVT_RELATIVE) ? parent->comp.width * base->width.value : base->width.value;
    height = (parent != NULL && base->height.type == DJUI_SVT_RELATIVE) ? parent->comp.height * base->height.value : base->height.value;

    if (base->width.type == DJUI_SVT_ASPECT_RATIO) {
        width = height * base->width.value;
    }
    if (base->height.type == DJUI_SVT_ASPECT_RATIO) {
        height = width * base->height.value;
    }

    if (parent != NULL) {
        if (base->hAlign == DJUI_HALIGN_CENTER) {
            x += (parent->comp.width - width) * 0.5f;
        } else if (base->hAlign == DJUI_HALIGN_RIGHT) {
            x = parent->comp.width - width - x;
        }

        if (base->vAlign == DJUI_VALIGN_CENTER) {
            y += (parent->comp.height - height) * 0.5f;
        } else if (base->vAlign == DJUI_VALIGN_BOTTOM) {
            y = parent->comp.height - height - y;
        }

        x += parent->comp.x;
        y += parent->comp.y;
    }

    comp->x = x;
    comp->y = y;
    comp->width = width;
    comp->height = height;

    base->elem = *comp;
    djui_base_clip(base);
}

void djui_base_compute_tree(struct DjuiBase *base) {
    if (base == NULL) {
        return;
    }
    if (base->parent != NULL) {
        djui_base_compute_tree(base->parent);
    }
    djui_base_compute(base);
}

bool djui_base_render(struct DjuiBase *base) {
    struct DjuiBaseChild *child = NULL;

    if (base == NULL || !base->visible) {
        return false;
    }

    if (base->on_render_pre != NULL) {
        bool skipRender = false;
        base->on_render_pre(base, &skipRender);
        if (skipRender) {
            return false;
        }
    }

    djui_base_compute(base);
    if (base->comp.width <= 0.0f || base->comp.height <= 0.0f) {
        return false;
    }

    if (base->clip.width <= 0.0f || base->clip.height <= 0.0f) {
        return false;
    }

    if (base->borderWidth.value > 0.0f && base->borderColor.a > 0) {
        djui_base_render_border(base);
    }

    if (base->comp.width <= 0.0f || base->comp.height <= 0.0f) {
        return false;
    }

    if (base->clip.width <= 0.0f || base->clip.height <= 0.0f) {
        return false;
    }

    if (base->render != NULL && !base->render(base)) {
        return false;
    }

    djui_base_add_padding(base);

    child = base->child;
    bool hasChildRendered = false;
    while (child != NULL) {
        struct DjuiBase *childBase = child->base;
        struct DjuiBaseChild *nextChild = child->next;
        bool childRendered = djui_base_render(childBase);
        if (base->abandonAfterChildRenderFail && !childRendered && hasChildRendered) {
            break;
        }
        hasChildRendered = hasChildRendered || childRendered;
        if (base->on_child_render != NULL) {
            base->on_child_render(base, childBase);
        }
        child = nextChild;
    }

    return true;
}

void djui_base_destroy_children(struct DjuiBase *base) {
    struct DjuiBaseChild *child = NULL;
    if (base == NULL) {
        return;
    }

    child = base->child;
    while (child != NULL) {
        struct DjuiBaseChild *next = child->next;
        if (child->base != NULL) {
            child->base->parent = NULL;
            djui_base_destroy(child->base);
        }
        free(child);
        child = next;
    }
    base->child = NULL;
}

void djui_base_destroy_one_child(struct DjuiBase *base) {
    struct DjuiBaseChild *prev = NULL;
    struct DjuiBaseChild *child = NULL;

    if (base == NULL) {
        return;
    }

    child = base->child;
    while (child != NULL && child->next != NULL) {
        prev = child;
        child = child->next;
    }

    if (child == NULL) {
        return;
    }

    if (child->base != NULL) {
        child->base->parent = NULL;
        djui_base_destroy(child->base);
    }
    free(child);

    if (prev == NULL) {
        base->child = NULL;
    } else {
        prev->next = NULL;
    }
}

void djui_base_destroy(struct DjuiBase *base) {
    struct DjuiBaseChild *iter = NULL;
    struct DjuiBaseChild *prev = NULL;

    if (base == NULL) {
        return;
    }

    if (base->parent != NULL) {
        iter = base->parent->child;
        while (iter != NULL) {
            if (iter->base == base) {
                if (prev == NULL) {
                    base->parent->child = iter->next;
                } else {
                    prev->next = iter->next;
                }
                free(iter);
                break;
            }
            prev = iter;
            iter = iter->next;
        }
        base->parent = NULL;
    }

    djui_base_destroy_children(base);

    if (base->interactable != NULL) {
        free(base->interactable);
        base->interactable = NULL;
    }

    if (base == gDjuiHovered) {
        gDjuiHovered = NULL;
    }
    if (base == gDjuiCursorDownOn) {
        gDjuiCursorDownOn = NULL;
    }
    if (base == gInteractableFocus) {
        gInteractableFocus = NULL;
    }
    if (base == gInteractableBinding) {
        gInteractableBinding = NULL;
    }
    if (base == gInteractableMouseDown) {
        gInteractableMouseDown = NULL;
    }

    if (base->destroy != NULL) {
        base->destroy(base);
    }
}

void djui_base_init(struct DjuiBase *parent, struct DjuiBase *base, bool (*render)(struct DjuiBase *),
                    void (*destroy)(struct DjuiBase *)) {
    if (base == NULL) {
        return;
    }

    memset(base, 0, sizeof(struct DjuiBase));
    base->parent = parent;
    base->visible = true;
    base->enabled = true;
    base->x.type = DJUI_SVT_ABSOLUTE;
    base->y.type = DJUI_SVT_ABSOLUTE;
    base->width.type = DJUI_SVT_ABSOLUTE;
    base->height.type = DJUI_SVT_ABSOLUTE;
    base->width.value = 64.0f;
    base->height.value = 64.0f;
    base->color.r = 255;
    base->color.g = 255;
    base->color.b = 255;
    base->color.a = 255;
    base->borderWidth.type = DJUI_SVT_ABSOLUTE;
    base->borderWidth.value = 0.0f;
    base->hAlign = DJUI_HALIGN_LEFT;
    base->vAlign = DJUI_VALIGN_TOP;
    base->get_cursor_hover_location = djui_base_default_cursor_hover_location;
    base->render = render;
    base->destroy = destroy;

    djui_base_add_child(parent, base);
}
