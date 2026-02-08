#pragma once

#include <PR/ultratypes.h>

struct DjuiColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
};

enum DjuiScreenValueType {
    DJUI_SVT_ABSOLUTE = 0,
    DJUI_SVT_RELATIVE = 1,
    DJUI_SVT_ASPECT_RATIO = 2,
};

struct DjuiScreenValue {
    enum DjuiScreenValueType type;
    f32 value;
};

enum DjuiFlowDirection {
    DJUI_FLOW_DIR_DOWN = 0,
    DJUI_FLOW_DIR_UP = 1,
    DJUI_FLOW_DIR_RIGHT = 2,
    DJUI_FLOW_DIR_LEFT = 3,
};

enum DjuiHAlign {
    DJUI_HALIGN_LEFT = 0,
    DJUI_HALIGN_CENTER = 1,
    DJUI_HALIGN_RIGHT = 2,
};

enum DjuiVAlign {
    DJUI_VALIGN_TOP = 0,
    DJUI_VALIGN_CENTER = 1,
    DJUI_VALIGN_BOTTOM = 2,
};
