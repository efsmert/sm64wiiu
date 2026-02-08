#pragma once

#include "djui_base.h"

struct DjuiText {
    struct DjuiBase base;
    char *message;
    bool dropShadow;
    struct DjuiColor dropShadowColor;
    f32 fontScale;
    enum DjuiHAlign textHAlign;
    enum DjuiVAlign textVAlign;
};

struct DjuiText *djui_text_create(struct DjuiBase *parent, const char *text);
void djui_text_set_text(struct DjuiText *text, const char *message);
void djui_text_set_alignment(struct DjuiText *text, enum DjuiHAlign hAlign, enum DjuiVAlign vAlign);
void djui_text_set_drop_shadow(struct DjuiText *text, u8 r, u8 g, u8 b, u8 a);
void djui_text_set_font(struct DjuiText *text, void *font);
void djui_text_set_font_scale(struct DjuiText *text, f32 scale);
