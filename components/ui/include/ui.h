#ifndef UI_H
#define UI_H

#include "hal.h"

void ui_init(void); // создаёт экраны, вешает hal_input_init(ui_handle_input,...)
void ui_handle_input(hal_input_event_t ev, void *arg);

#endif
