#pragma once

#include <stdbool.h>
#include <PR/os_cont.h>

struct DjuiBase;

#define PAD_BUTTON_A     ((u16)(1 << 15))
#define PAD_BUTTON_B     ((u16)(1 << 14))
#define PAD_BUTTON_Z     ((u16)(1 << 13))
#define PAD_BUTTON_START ((u16)(1 << 12))

struct DjuiInteractable {
    bool enabled;
    void (*update_style)(struct DjuiBase *base);
    void (*on_hover)(struct DjuiBase *base);
    void (*on_hover_end)(struct DjuiBase *base);
    void (*on_cursor_down_begin)(struct DjuiBase *base, bool inputCursor);
    void (*on_cursor_down)(struct DjuiBase *base);
    void (*on_cursor_down_end)(struct DjuiBase *base);
    void (*on_focus_begin)(struct DjuiBase *base);
    void (*on_focus)(struct DjuiBase *base);
    void (*on_focus_end)(struct DjuiBase *base);
    void (*on_click)(struct DjuiBase *base);
    void (*on_value_change)(struct DjuiBase *base);
    void (*on_bind)(struct DjuiBase *base);
    void (*on_enabled_change)(struct DjuiBase *base);
};

extern bool gInteractableOverridePad;
extern OSContPad gInteractablePad;
extern struct DjuiBase *gDjuiHovered;
extern struct DjuiBase *gDjuiCursorDownOn;
extern struct DjuiBase *gInteractableFocus;
extern struct DjuiBase *gInteractableBinding;
extern struct DjuiBase *gInteractableMouseDown;

bool djui_interactable_is_binding(void);
void djui_interactable_set_binding(struct DjuiBase *base);
void djui_interactable_set_input_focus(struct DjuiBase *base);
bool djui_interactable_is_input_focus(struct DjuiBase *base);
void djui_interactable_ignore_until_release(void);

void djui_interactable_update(void);

void djui_interactable_hook_hover(struct DjuiBase *base, void (*on_hover)(struct DjuiBase *),
                                  void (*on_hover_end)(struct DjuiBase *));
void djui_interactable_hook_cursor_down(struct DjuiBase *base,
                                        void (*on_cursor_down_begin)(struct DjuiBase *, bool),
                                        void (*on_cursor_down)(struct DjuiBase *),
                                        void (*on_cursor_down_end)(struct DjuiBase *));
void djui_interactable_hook_focus(struct DjuiBase *base, void (*on_focus_begin)(struct DjuiBase *),
                                  void (*on_focus)(struct DjuiBase *), void (*on_focus_end)(struct DjuiBase *));
void djui_interactable_hook_click(struct DjuiBase *base, void (*on_click)(struct DjuiBase *));
void djui_interactable_hook_value_change(struct DjuiBase *base, void (*on_value_change)(struct DjuiBase *));
void djui_interactable_hook_bind(struct DjuiBase *base, void (*on_bind)(struct DjuiBase *));
void djui_interactable_hook_enabled_change(struct DjuiBase *base, void (*on_enabled_change)(struct DjuiBase *));
void djui_interactable_create(struct DjuiBase *base, void (*update_style)(struct DjuiBase *));
