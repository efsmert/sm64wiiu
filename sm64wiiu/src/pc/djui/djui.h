#ifndef SM64_PC_DJUI_H
#define SM64_PC_DJUI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool gDjuiInMainMenu;
extern bool gDjuiDisabled;
extern bool gDjuiUseDonorStack;

void djui_init(void);
void djui_init_late(void);
void djui_shutdown(void);
void djui_update(void);
void djui_update_menu_level(void);
void djui_render(void);
void djui_open_main_menu(void);
void djui_close_main_menu(void);
void djui_set_donor_stack_enabled(bool enabled);
bool djui_is_donor_stack_enabled(void);

#ifdef __cplusplus
}
#endif

#endif
