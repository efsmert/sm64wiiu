#include "djui_three_panel.h"

#include <stdlib.h>

static struct DjuiBase *djui_three_panel_child_at(struct DjuiThreePanel *threePanel, s32 index) {
    struct DjuiBaseChild *child = NULL;
    s32 i = 0;

    if (threePanel == NULL) {
        return NULL;
    }

    child = threePanel->base.child;
    while (child != NULL) {
        if (i == index) {
            return child->base;
        }
        child = child->next;
        i++;
    }

    return NULL;
}

struct DjuiBase *djui_three_panel_get_header(struct DjuiThreePanel *threePanel) {
    return djui_three_panel_child_at(threePanel, 0);
}

struct DjuiBase *djui_three_panel_get_body(struct DjuiThreePanel *threePanel) {
    return djui_three_panel_child_at(threePanel, 1);
}

struct DjuiBase *djui_three_panel_get_footer(struct DjuiThreePanel *threePanel) {
    return djui_three_panel_child_at(threePanel, 2);
}

void djui_three_panel_set_min_header_size_type(struct DjuiThreePanel *threePanel, enum DjuiScreenValueType minHeaderSizeType) {
    if (threePanel != NULL) {
        threePanel->minHeaderSize.type = minHeaderSizeType;
    }
}

void djui_three_panel_set_min_header_size(struct DjuiThreePanel *threePanel, f32 minHeaderSize) {
    if (threePanel != NULL) {
        threePanel->minHeaderSize.value = minHeaderSize;
    }
}

void djui_three_panel_set_body_size_type(struct DjuiThreePanel *threePanel, enum DjuiScreenValueType bodySizeType) {
    if (threePanel != NULL) {
        threePanel->bodySize.type = bodySizeType;
    }
}

void djui_three_panel_set_body_size(struct DjuiThreePanel *threePanel, f32 bodySize) {
    if (threePanel != NULL) {
        threePanel->bodySize.value = bodySize;
    }
}

void djui_three_panel_set_min_footer_size_type(struct DjuiThreePanel *threePanel, enum DjuiScreenValueType minFooterSizeType) {
    if (threePanel != NULL) {
        threePanel->minFooterSize.type = minFooterSizeType;
    }
}

void djui_three_panel_set_min_footer_size(struct DjuiThreePanel *threePanel, f32 minFooterSize) {
    if (threePanel != NULL) {
        threePanel->minFooterSize.value = minFooterSize;
    }
}

void djui_three_panel_recalculate_body_size(struct DjuiThreePanel *threePanel) {
    struct DjuiBase *bodyBase = NULL;
    struct DjuiFlowLayout *body = NULL;
    struct DjuiBaseChild *child = NULL;
    f32 bodyHeight = 0.0f;

    if (threePanel == NULL || threePanel->bodySize.value != 0.0f) {
        return;
    }

    bodyBase = djui_three_panel_get_body(threePanel);
    if (bodyBase == NULL) {
        return;
    }

    body = (struct DjuiFlowLayout *)bodyBase;
    if (body->margin.type != DJUI_SVT_ABSOLUTE) {
        return;
    }
    bodyHeight = body->margin.value;
    child = body->base.child;
    while (child != NULL) {
        if (child->base->height.type != DJUI_SVT_ABSOLUTE) {
            return;
        }
        bodyHeight += body->margin.value;
        bodyHeight += child->base->height.value;
        child = child->next;
    }
    if (bodyHeight < 0.0f) {
        bodyHeight = 0.0f;
    }
    djui_three_panel_set_body_size(threePanel, bodyHeight);
}

static bool djui_three_panel_render(struct DjuiBase *base) {
    struct DjuiThreePanel *threePanel = (struct DjuiThreePanel *)base;
    struct DjuiBase *header = djui_three_panel_get_header(threePanel);
    struct DjuiBase *body = djui_three_panel_get_body(threePanel);
    struct DjuiBase *footer = djui_three_panel_get_footer(threePanel);
    struct DjuiBaseRect *parentComp = NULL;
    f32 tPad = 0.0f;
    f32 bPad = 0.0f;
    f32 myHeight = 0.0f;
    f32 minHeaderSize = 0.0f;
    f32 minFooterSize = 0.0f;
    f32 largestMinSize = 0.0f;
    f32 headerSize = 0.0f;
    f32 bodySize = 0.0f;
    f32 footerSize = 0.0f;

    if (body == NULL) {
        return false;
    }

    parentComp = &base->comp;
    tPad = (base->padding.top.type == DJUI_SVT_RELATIVE) ? parentComp->height * base->padding.top.value
                                                          : base->padding.top.value;
    bPad = (base->padding.bottom.type == DJUI_SVT_RELATIVE) ? parentComp->height * base->padding.bottom.value
                                                             : base->padding.bottom.value;

    myHeight = base->comp.height - tPad - bPad;
    minHeaderSize = (threePanel->minHeaderSize.type == DJUI_SVT_RELATIVE) ? base->comp.height * threePanel->minHeaderSize.value
                                                                           : threePanel->minHeaderSize.value;
    minFooterSize = (threePanel->minFooterSize.type == DJUI_SVT_RELATIVE) ? base->comp.height * threePanel->minFooterSize.value
                                                                           : threePanel->minFooterSize.value;
    largestMinSize = (minHeaderSize > minFooterSize) ? minHeaderSize : minFooterSize;

    headerSize = minHeaderSize;
    bodySize = (threePanel->bodySize.type == DJUI_SVT_RELATIVE) ? base->comp.height * threePanel->bodySize.value
                                                                 : threePanel->bodySize.value;
    footerSize = minFooterSize;

    if (minHeaderSize + minFooterSize >= myHeight) {
        bodySize = 0.0f;
        headerSize = myHeight * (minHeaderSize / (minHeaderSize + minFooterSize));
        footerSize = myHeight * (minFooterSize / (minHeaderSize + minFooterSize));
    } else if (minHeaderSize + bodySize + minFooterSize >= myHeight) {
        bodySize = myHeight - headerSize - footerSize;
    } else if (largestMinSize * 2.0f + bodySize >= myHeight) {
        if (minHeaderSize > minFooterSize) {
            footerSize = myHeight - headerSize - bodySize;
        } else {
            headerSize = myHeight - footerSize - bodySize;
        }
    } else {
        headerSize = (myHeight - bodySize) * 0.5f;
        footerSize = (myHeight - bodySize) * 0.5f;
    }

    if (header != NULL) {
        djui_base_set_location_type(header, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        djui_base_set_location(header, 0.0f, myHeight - headerSize);
        djui_base_set_size_type(header, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(header, 1.0f, headerSize);
    }

    djui_base_set_location_type(body, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
    djui_base_set_location(body, 0.0f, footerSize);
    djui_base_set_size_type(body, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(body, 1.0f, bodySize);

    if (footer != NULL) {
        djui_base_set_location_type(footer, DJUI_SVT_ABSOLUTE, DJUI_SVT_ABSOLUTE);
        djui_base_set_location(footer, 0.0f, 0.0f);
        djui_base_set_size_type(footer, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
        djui_base_set_size(footer, 1.0f, footerSize);
    }

    return djui_rect_render(base);
}

static void djui_three_panel_destroy(struct DjuiBase *base) {
    struct DjuiThreePanel *threePanel = (struct DjuiThreePanel *)base;
    free(threePanel);
}

struct DjuiThreePanel *djui_three_panel_create(struct DjuiBase *parent, f32 minHeaderSize, f32 bodySize, f32 minFooterSize) {
    struct DjuiThreePanel *threePanel = calloc(1, sizeof(struct DjuiThreePanel));

    if (threePanel == NULL) {
        return NULL;
    }

    djui_base_init(parent, &threePanel->base, djui_three_panel_render, djui_three_panel_destroy);
    djui_three_panel_set_min_header_size_type(threePanel, DJUI_SVT_ABSOLUTE);
    djui_three_panel_set_body_size_type(threePanel, DJUI_SVT_ABSOLUTE);
    djui_three_panel_set_min_footer_size_type(threePanel, DJUI_SVT_ABSOLUTE);
    djui_three_panel_set_min_header_size(threePanel, minHeaderSize);
    djui_three_panel_set_body_size(threePanel, bodySize);
    djui_three_panel_set_min_footer_size(threePanel, minFooterSize);

    return threePanel;
}
