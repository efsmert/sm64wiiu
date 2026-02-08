#include "djui_panel.h"

#include <stdlib.h>

#include "audio/external.h"
#include "sounds.h"
#include "djui_cursor.h"
#include "djui_interactable.h"

static struct DjuiPanel *sPanelList = NULL;
static struct DjuiPanel *sPanelRemoving = NULL;
static f32 sMoveAmount = 0.0f;

bool gDjuiPanelDisableBack = false;

static f32 djui_panel_smooth_step(f32 value) {
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value >= 1.0f) {
        return 1.0f;
    }
    return value * value * (3.0f - 2.0f * value);
}

bool djui_panel_is_active(void) {
    return sPanelList != NULL;
}

static struct DjuiBase *djui_panel_find_first_interactable(struct DjuiBaseChild *child) {
    while (child != NULL) {
        if (child->base != NULL && child->base->interactable != NULL && child->base->interactable->enabled) {
            return child->base;
        }
        if (child->base != NULL) {
            struct DjuiBase *check = djui_panel_find_first_interactable(child->base->child);
            if (check != NULL) {
                return check;
            }
        }
        child = child->next;
    }

    return NULL;
}

struct DjuiPanel *djui_panel_add(struct DjuiBase *caller, struct DjuiThreePanel *threePanel, struct DjuiBase *defaultElementBase) {
    struct DjuiPanel *panel = NULL;
    struct DjuiBase *panelBase = NULL;
    bool firstPanel = (sPanelList == NULL);

    if (threePanel == NULL || sPanelRemoving != NULL) {
        return NULL;
    }

    panelBase = &threePanel->base;

    if (sPanelList != NULL) {
        sPanelList->defaultElementBase = caller;
    }

    if (sPanelList != NULL && sPanelList->parent != NULL) {
        djui_base_set_visible(sPanelList->parent->base, false);
    }

    djui_three_panel_recalculate_body_size(threePanel);

    panel = calloc(1, sizeof(struct DjuiPanel));
    if (panel == NULL) {
        return NULL;
    }

    panel->parent = sPanelList;
    panel->base = panelBase;
    panel->defaultElementBase = defaultElementBase;
    panel->on_back = threePanel->on_back;
    panel->on_panel_destroy = NULL;
    panel->temporary = threePanel->temporary;
    sPanelList = panel;

    if (panel->defaultElementBase == NULL) {
        panel->defaultElementBase = djui_panel_find_first_interactable(panel->base->child);
    }

    djui_cursor_input_controlled_center(NULL);
    djui_interactable_ignore_until_release();

    djui_base_set_location_type(panelBase, DJUI_SVT_ABSOLUTE, DJUI_SVT_RELATIVE);
    djui_base_set_location(panelBase, 0.0f, -1.0f);

    djui_base_set_enabled(panel->base, false);
    if (panel->parent != NULL) {
        djui_base_set_enabled(panel->parent->base, false);
    }

    sMoveAmount = 0.0f;

    if (firstPanel) {
        djui_base_set_location(panelBase, 0.0f, 0.0f);
        djui_cursor_input_controlled_center(panel->defaultElementBase);
        djui_base_set_enabled(panel->base, true);
    } else {
        play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
    }

    return panel;
}

void djui_panel_back(void) {
    if (sPanelRemoving != NULL || sPanelList == NULL || gDjuiPanelDisableBack) {
        return;
    }

    if (sPanelList->parent == NULL) {
        return;
    }

    if (sPanelList->on_back != NULL && sPanelList->on_back(sPanelList->base)) {
        return;
    }

    djui_cursor_input_controlled_center(NULL);
    djui_interactable_ignore_until_release();

    sPanelRemoving = sPanelList;
    djui_base_set_enabled(sPanelRemoving->base, false);

    sPanelList = sPanelList->parent;
    while (sPanelList != NULL && sPanelList->temporary) {
        sPanelList = sPanelList->parent;
    }

    if (sPanelList != NULL) {
        djui_base_set_visible(sPanelList->base, true);
    }

    sMoveAmount = 0.0f;
    play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
}

void djui_panel_update(void) {
    struct DjuiBase *activeBase = NULL;
    struct DjuiBase *parentBase = NULL;
    struct DjuiBase *removingBase = NULL;
    f32 t = 0.0f;

    if (sPanelList == NULL || sPanelList->base == NULL || sPanelList->base->elem.height == 0.0f) {
        return;
    }

    activeBase = sPanelList->base;
    parentBase = (sPanelList->parent == NULL) ? NULL : sPanelList->parent->base;
    removingBase = (sPanelRemoving == NULL) ? NULL : sPanelRemoving->base;

    if (sMoveAmount >= DJUI_PANEL_MOVE_MAX) {
        sMoveAmount = DJUI_PANEL_MOVE_MAX;
        djui_base_set_enabled(activeBase, true);
        activeBase->y.value = 0.0f;
        return;
    }

    sMoveAmount += DJUI_PANEL_MOVE_MAX / 10.0f;
    if (sMoveAmount >= DJUI_PANEL_MOVE_MAX) {
        struct DjuiPanel *panel = NULL;

        sMoveAmount = DJUI_PANEL_MOVE_MAX;
        if (parentBase != NULL) {
            djui_base_set_visible(parentBase, false);
        }

        djui_base_set_enabled(activeBase, true);
        activeBase->y.value = 0.0f;
        djui_cursor_input_controlled_center(sPanelList->defaultElementBase);

        if (removingBase != NULL) {
            panel = sPanelRemoving;
            sPanelRemoving = NULL;

            if (panel->on_panel_destroy != NULL) {
                panel->on_panel_destroy(NULL);
            }

            djui_base_destroy(removingBase);
            free(panel);
        }
        return;
    }

    t = djui_panel_smooth_step(sMoveAmount / DJUI_PANEL_MOVE_MAX);

    if (activeBase != NULL && removingBase != NULL) {
        activeBase->y.value = DJUI_PANEL_MOVE_MAX - DJUI_PANEL_MOVE_MAX * t;
        if (sPanelRemoving != NULL) {
            removingBase->y.value = activeBase->y.value - DJUI_PANEL_MOVE_MAX;
        }
    } else if (activeBase != NULL && parentBase != NULL) {
        activeBase->y.value = DJUI_PANEL_MOVE_MAX * t - DJUI_PANEL_MOVE_MAX;
        parentBase->y.value = activeBase->y.value + DJUI_PANEL_MOVE_MAX;
    }
}

void djui_panel_shutdown(void) {
    struct DjuiPanel *panel = sPanelList;

    while (panel != NULL) {
        struct DjuiPanel *next = panel->parent;

        if (panel->on_panel_destroy != NULL) {
            panel->on_panel_destroy(NULL);
        }

        djui_base_destroy(panel->base);
        free(panel);
        panel = next;
    }

    if (sPanelRemoving != NULL) {
        panel = sPanelRemoving;
        sPanelRemoving = NULL;

        if (panel->on_panel_destroy != NULL) {
            panel->on_panel_destroy(NULL);
        }

        djui_base_destroy(panel->base);
        free(panel);
    }

    sPanelList = NULL;
    sPanelRemoving = NULL;
    sMoveAmount = 0.0f;
}
