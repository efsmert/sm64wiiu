#pragma once

#include "djui_base.h"

struct DjuiRect {
    struct DjuiBase base;
};

struct DjuiRect *djui_rect_create(struct DjuiBase *parent);
struct DjuiRect *djui_rect_container_create(struct DjuiBase *parent, f32 height);
bool djui_rect_render(struct DjuiBase *base);

