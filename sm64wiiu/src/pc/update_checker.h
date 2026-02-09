#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <stdbool.h>

extern bool gUpdateMessage;

void show_update_popup(void);
void check_for_updates(void);

#endif
