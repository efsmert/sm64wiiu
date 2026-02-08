#pragma once

#include "djui_base.h"

struct DjuiRoot {
    struct DjuiBase base;
};

extern struct DjuiRoot *gDjuiRoot;

struct DjuiRoot *djui_root_create(void);
