#include "djui_button.h"

#include <stdlib.h>

#include "djui_interactable.h"

static void djui_button_update_style(struct DjuiBase *base) {
    struct DjuiButton *button = (struct DjuiButton *)base;
    bool backStyle = (button->style == DJUI_BUTTON_STYLE_BACK);

    if (!button->base.enabled) {
        djui_base_set_border_color(base, 64, 64, 64, 255);
        djui_base_set_color(&button->rect->base, 20, 20, 20, 220);
        djui_base_set_location(&button->text->base, 0.0f, 0.0f);
        return;
    }

    if (gDjuiCursorDownOn == base) {
        djui_base_set_border_color(base, 95, 130, 180, 255);
        djui_base_set_color(&button->rect->base, 64, 70, 80, 255);
        djui_base_set_location(&button->text->base, 1.0f, 1.0f);
        return;
    }

    if (gDjuiHovered == base) {
        djui_base_set_border_color(base, 0, 120, 215, 255);
        djui_base_set_color(&button->rect->base, 96, 96, 96, 255);
        djui_base_set_location(&button->text->base, 0.0f, 0.0f);
        return;
    }

    if (backStyle) {
        djui_base_set_border_color(base, 88, 88, 88, 255);
        djui_base_set_color(&button->rect->base, 26, 26, 26, 230);
    } else {
        djui_base_set_border_color(base, 75, 75, 75, 255);
        djui_base_set_color(&button->rect->base, 22, 22, 22, 210);
    }
    djui_base_set_location(&button->text->base, 0.0f, 0.0f);
}

void djui_button_set_style(struct DjuiButton *button, enum DjuiButtonStyle style) {
    if (button == NULL) {
        return;
    }
    button->style = style;
    djui_button_update_style(&button->base);
}

static void djui_button_destroy(struct DjuiBase *base) {
    free((struct DjuiButton *)base);
}

struct DjuiButton *djui_button_create(struct DjuiBase *parent, const char *message,
                                      enum DjuiButtonStyle style, void (*on_click)(struct DjuiBase *)) {
    struct DjuiButton *button = calloc(1, sizeof(struct DjuiButton));
    struct DjuiRect *rect = NULL;
    struct DjuiText *text = NULL;

    if (button == NULL) {
        return NULL;
    }

    djui_base_init(parent, &button->base, NULL, djui_button_destroy);
    djui_base_set_size_type(&button->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&button->base, 1.0f, 22.0f);
    djui_base_set_border_width(&button->base, 2.0f);

    djui_interactable_create(&button->base, djui_button_update_style);
    djui_interactable_hook_click(&button->base, on_click);

    rect = djui_rect_create(&button->base);
    if (rect != NULL) {
        djui_base_set_size_type(&rect->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
        djui_base_set_size(&rect->base, 1.0f, 1.0f);
        button->rect = rect;

        text = djui_text_create(&rect->base, message);
        if (text != NULL) {
            djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
            djui_base_set_size(&text->base, 1.0f, 1.0f);
            djui_base_set_color(&text->base, 255, 255, 255, 255);
            djui_text_set_alignment(text, DJUI_HALIGN_CENTER, DJUI_VALIGN_CENTER);
            djui_text_set_drop_shadow(text, 64, 64, 64, 100);
            button->text = text;
        }
    }

    button->style = (u8)style;
    djui_button_update_style(&button->base);
    return button;
}

struct DjuiButton *djui_button_left_create(struct DjuiBase *parent, const char *message,
                                           enum DjuiButtonStyle style, void (*on_click)(struct DjuiBase *)) {
    struct DjuiButton *button = djui_button_create(parent, message, style, on_click);

    if (button == NULL) {
        return NULL;
    }

    djui_base_set_size(&button->base, 0.485f, 22.0f);
    djui_base_set_alignment(&button->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    return button;
}

struct DjuiButton *djui_button_right_create(struct DjuiBase *parent, const char *message,
                                            enum DjuiButtonStyle style, void (*on_click)(struct DjuiBase *)) {
    struct DjuiButton *button = djui_button_create(parent, message, style, on_click);

    if (button == NULL) {
        return NULL;
    }

    djui_base_set_size(&button->base, 0.485f, 22.0f);
    djui_base_set_alignment(&button->base, DJUI_HALIGN_RIGHT, DJUI_VALIGN_TOP);
    return button;
}
