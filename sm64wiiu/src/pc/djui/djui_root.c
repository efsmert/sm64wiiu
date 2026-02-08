#include "djui_root.h"

#include <stdlib.h>

struct DjuiRoot *gDjuiRoot = NULL;

static bool djui_root_render(struct DjuiBase *base) {
    if (base == NULL) {
        return false;
    }

    // Keep root in fixed internal UI space for now; donor scale logic is added in later slices.
    djui_base_set_location(base, 0.0f, 0.0f);
    djui_base_set_size(base, 320.0f, 240.0f);
    djui_base_compute(base);
    return true;
}

static void djui_root_destroy(struct DjuiBase *base) {
    struct DjuiRoot *root = (struct DjuiRoot *)base;
    if (gDjuiRoot == root) {
        gDjuiRoot = NULL;
    }
    free(root);
}

struct DjuiRoot *djui_root_create(void) {
    struct DjuiRoot *root = calloc(1, sizeof(struct DjuiRoot));
    if (root == NULL) {
        return NULL;
    }

    djui_base_init(NULL, &root->base, djui_root_render, djui_root_destroy);
    djui_base_set_location(&root->base, 0.0f, 0.0f);
    djui_base_set_size(&root->base, 320.0f, 240.0f);
    djui_base_set_color(&root->base, 0, 0, 0, 0);
    gDjuiRoot = root;

    return root;
}
