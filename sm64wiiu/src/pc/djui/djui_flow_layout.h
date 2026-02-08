#pragma once

#include "djui_base.h"

struct DjuiFlowLayout {
    struct DjuiBase base;
    struct DjuiScreenValue margin;
    enum DjuiFlowDirection direction;
};

struct DjuiFlowLayout *djui_flow_layout_create(struct DjuiBase *parent);
void djui_flow_layout_set_margin(struct DjuiFlowLayout *flow, f32 margin);
void djui_flow_layout_set_margin_type(struct DjuiFlowLayout *flow, enum DjuiScreenValueType type);
void djui_flow_layout_set_flow_direction(struct DjuiFlowLayout *flow, enum DjuiFlowDirection direction);

