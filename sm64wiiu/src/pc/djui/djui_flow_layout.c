#include "djui_flow_layout.h"

#include <stdlib.h>

static bool djui_flow_layout_render(struct DjuiBase *base) {
    struct DjuiFlowLayout *flow = (struct DjuiFlowLayout *)base;
    struct DjuiBaseChild *child = NULL;
    f32 cursor = 0.0f;
    f32 margin = 0.0f;

    if (flow == NULL) {
        return false;
    }

    margin = (flow->margin.type == DJUI_SVT_RELATIVE) ? (base->comp.height * flow->margin.value)
                                                       : flow->margin.value;
    cursor = margin;
    child = base->child;

    while (child != NULL) {
        struct DjuiBase *childBase = child->base;
        if (childBase != NULL) {
            if (flow->direction == DJUI_FLOW_DIR_DOWN) {
                djui_base_set_location_type(childBase, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
                djui_base_set_location(childBase, 0.0f, base->comp.height - cursor - childBase->height.value);
                cursor += childBase->height.value + margin;
            } else if (flow->direction == DJUI_FLOW_DIR_UP) {
                djui_base_set_location_type(childBase, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
                djui_base_set_location(childBase, 0.0f, cursor);
                cursor += childBase->height.value + margin;
            } else if (flow->direction == DJUI_FLOW_DIR_RIGHT) {
                djui_base_set_location_type(childBase, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
                djui_base_set_location(childBase, cursor, 0.0f);
                cursor += childBase->width.value + margin;
            } else if (flow->direction == DJUI_FLOW_DIR_LEFT) {
                djui_base_set_location_type(childBase, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
                djui_base_set_location(childBase, base->comp.width - cursor - childBase->width.value, 0.0f);
                cursor += childBase->width.value + margin;
            }
        }
        child = child->next;
    }

    return true;
}

static void djui_flow_layout_destroy(struct DjuiBase *base) {
    free((struct DjuiFlowLayout *)base);
}

struct DjuiFlowLayout *djui_flow_layout_create(struct DjuiBase *parent) {
    struct DjuiFlowLayout *flow = calloc(1, sizeof(struct DjuiFlowLayout));
    if (flow == NULL) {
        return NULL;
    }

    djui_base_init(parent, &flow->base, djui_flow_layout_render, djui_flow_layout_destroy);
    flow->margin.type = DJUI_SVT_ABSOLUTE;
    flow->margin.value = 8.0f;
    flow->direction = DJUI_FLOW_DIR_DOWN;
    return flow;
}

void djui_flow_layout_set_margin(struct DjuiFlowLayout *flow, f32 margin) {
    if (flow == NULL) {
        return;
    }
    flow->margin.value = margin;
}

void djui_flow_layout_set_margin_type(struct DjuiFlowLayout *flow, enum DjuiScreenValueType type) {
    if (flow == NULL) {
        return;
    }
    flow->margin.type = type;
}

void djui_flow_layout_set_flow_direction(struct DjuiFlowLayout *flow, enum DjuiFlowDirection direction) {
    if (flow == NULL) {
        return;
    }
    flow->direction = direction;
}
