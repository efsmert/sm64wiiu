#include "djui_text.h"

#include <stdlib.h>
#include <string.h>

#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/segment2.h"

static u8 djui_ascii_to_dialog_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return ASCII_TO_DIALOG(c);
    }
    if (c >= 'a' && c <= 'z') {
        return ASCII_TO_DIALOG(c);
    }
    if (c >= '0' && c <= '9') {
        return ASCII_TO_DIALOG(c);
    }
    if (c == ' ') {
        return DIALOG_CHAR_SPACE;
    }
    if (c == '.') {
        return DIALOG_CHAR_PERIOD;
    }
    if (c == ',') {
        return DIALOG_CHAR_COMMA;
    }
    if (c == '/') {
        return DIALOG_CHAR_SLASH;
    }
    if (c == '\n') {
        return DIALOG_CHAR_NEWLINE;
    }

    return DIALOG_CHAR_SPACE;
}

static void djui_ascii_to_dialog_string(const char *ascii, u8 *out, size_t outSize) {
    size_t i = 0;

    if (out == NULL || outSize == 0) {
        return;
    }

    if (ascii == NULL) {
        out[0] = DIALOG_CHAR_TERMINATOR;
        return;
    }

    while (ascii[i] != '\0' && i + 1 < outSize) {
        out[i] = djui_ascii_to_dialog_char(ascii[i]);
        i++;
    }

    out[i] = DIALOG_CHAR_TERMINATOR;
}

static s16 djui_dialog_strlen(u8 *dialog) {
    s16 len = 0;

    if (dialog == NULL) {
        return 0;
    }

    while (dialog[len] != DIALOG_CHAR_TERMINATOR && len < 120) {
        len++;
    }

    return len;
}

static s16 djui_text_calc_line_x(struct DjuiText *text, u8 *dialog) {
    s16 left = (s16)text->base.comp.x;
    s16 width = (s16)text->base.comp.width;

    if (text->textHAlign == DJUI_HALIGN_CENTER) {
        return get_str_x_pos_from_center((s16)(left + (width / 2)), dialog, 10.0f);
    }

    if (text->textHAlign == DJUI_HALIGN_RIGHT) {
        s16 strWidth = (s16)(djui_dialog_strlen(dialog) * 9);
        return (s16)(left + width - strWidth);
    }

    return left;
}

static bool djui_text_render(struct DjuiBase *base) {
    struct DjuiText *text = (struct DjuiText *)base;
    const char *cursor = NULL;
    s32 lineCount = 1;
    s16 lineHeight = 14;
    s16 y = 0;

    if (text == NULL || text->message == NULL || text->message[0] == '\0') {
        return true;
    }

    cursor = text->message;
    while (*cursor != '\0') {
        if (*cursor == '\n') {
            lineCount++;
        }
        cursor++;
    }

    if (text->fontScale > 0.0f) {
        lineHeight = (s16)(14.0f * text->fontScale);
        if (lineHeight < 10) {
            lineHeight = 10;
        }
    }

    if (text->textVAlign == DJUI_VALIGN_CENTER) {
        y = (s16)(text->base.comp.y + ((text->base.comp.height + (lineCount * lineHeight)) * 0.5f) - lineHeight);
    } else if (text->textVAlign == DJUI_VALIGN_BOTTOM) {
        y = (s16)(text->base.comp.y + ((lineCount - 1) * lineHeight));
    } else {
        y = (s16)(text->base.comp.y + text->base.comp.height - lineHeight);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    cursor = text->message;
    while (*cursor != '\0') {
        char line[128];
        u8 dialog[128];
        size_t i = 0;

        while (*cursor != '\0' && *cursor != '\n' && i + 1 < sizeof(line)) {
            line[i++] = *cursor++;
        }
        line[i] = '\0';

        if (*cursor == '\n') {
            cursor++;
        }

        djui_ascii_to_dialog_string(line, dialog, sizeof(dialog));

        if (text->dropShadow && text->dropShadowColor.a > 0) {
            gDPSetEnvColor(gDisplayListHead++, text->dropShadowColor.r, text->dropShadowColor.g,
                           text->dropShadowColor.b, text->dropShadowColor.a);
            print_generic_string((s16)(djui_text_calc_line_x(text, dialog) + 1), (s16)(y - 1), dialog);
        }

        gDPSetEnvColor(gDisplayListHead++, base->color.r, base->color.g, base->color.b, base->color.a);
        print_generic_string(djui_text_calc_line_x(text, dialog), y, dialog);

        y = (s16)(y - lineHeight);
    }
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);

    return true;
}

static void djui_text_destroy(struct DjuiBase *base) {
    struct DjuiText *text = (struct DjuiText *)base;
    if (text->message != NULL) {
        free(text->message);
        text->message = NULL;
    }
    free(text);
}

struct DjuiText *djui_text_create(struct DjuiBase *parent, const char *textStr) {
    struct DjuiText *text = calloc(1, sizeof(struct DjuiText));
    if (text == NULL) {
        return NULL;
    }

    djui_base_init(parent, &text->base, djui_text_render, djui_text_destroy);
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base, 1.0f, 20.0f);
    djui_text_set_text(text, textStr);
    djui_text_set_alignment(text, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    djui_text_set_font_scale(text, 1.0f);
    return text;
}

void djui_text_set_text(struct DjuiText *text, const char *message) {
    size_t len = 0;
    char *copy = NULL;

    if (text == NULL) {
        return;
    }

    if (text->message != NULL) {
        free(text->message);
        text->message = NULL;
    }

    if (message == NULL) {
        return;
    }

    len = strlen(message);
    copy = calloc(len + 1, sizeof(char));
    if (copy == NULL) {
        return;
    }
    memcpy(copy, message, len);
    copy[len] = '\0';
    text->message = copy;
}

void djui_text_set_alignment(struct DjuiText *text, enum DjuiHAlign hAlign, enum DjuiVAlign vAlign) {
    if (text == NULL) {
        return;
    }
    text->textHAlign = hAlign;
    text->textVAlign = vAlign;
}

void djui_text_set_drop_shadow(struct DjuiText *text, u8 r, u8 g, u8 b, u8 a) {
    if (text == NULL) {
        return;
    }
    text->dropShadow = true;
    text->dropShadowColor.r = r;
    text->dropShadowColor.g = g;
    text->dropShadowColor.b = b;
    text->dropShadowColor.a = a;
}

void djui_text_set_font(struct DjuiText *text, void *font) {
    (void)text;
    (void)font;
}

void djui_text_set_font_scale(struct DjuiText *text, f32 scale) {
    if (text == NULL) {
        return;
    }
    text->fontScale = scale;
}
