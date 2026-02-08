#include "djui_rect.h"

#include <stdlib.h>

#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/segment2.h"

void create_dl_scale_matrix(s8 pushOp, f32 x, f32 y, f32 z);

bool djui_rect_render(struct DjuiBase *base) {
    struct DjuiBaseRect *clip = NULL;
    f32 sx = 0.0f;
    f32 sy = 0.0f;

    if (base == NULL) {
        return false;
    }

    clip = &base->clip;
    if (base->color.a == 0) {
        return true;
    }
    if (clip->width <= 0.0f || clip->height <= 0.0f) {
        return false;
    }

    sx = clip->width / 130.0f;
    sy = clip->height / 80.0f;

    create_dl_translation_matrix(MENU_MTX_PUSH, clip->x, clip->y, 0.0f);
    create_dl_scale_matrix(MENU_MTX_NOPUSH, sx, sy, 1.0f);
    gDPSetEnvColor(gDisplayListHead++, base->color.r, base->color.g, base->color.b, base->color.a);
    gSPDisplayList(gDisplayListHead++, dl_draw_text_bg_box);
    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
    return true;
}

static void djui_rect_destroy(struct DjuiBase *base) {
    struct DjuiRect *rect = (struct DjuiRect *)base;
    free(rect);
}

struct DjuiRect *djui_rect_create(struct DjuiBase *parent) {
    struct DjuiRect *rect = calloc(1, sizeof(struct DjuiRect));
    if (rect == NULL) {
        return NULL;
    }

    djui_base_init(parent, &rect->base, djui_rect_render, djui_rect_destroy);
    return rect;
}

struct DjuiRect *djui_rect_container_create(struct DjuiBase *parent, f32 height) {
    struct DjuiRect *rect = djui_rect_create(parent);
    if (rect == NULL) {
        return NULL;
    }

    djui_base_set_size_type(&rect->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&rect->base, 1.0f, height);
    djui_base_set_color(&rect->base, 0, 0, 0, 0);
    return rect;
}
