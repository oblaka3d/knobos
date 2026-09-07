#ifndef UI_H
#define UI_H

#include "hal.h"

void ui_init(void); // создаёт экраны, вешает hal_input_init(ui_handle_input,...)
void ui_handle_input(hal_input_event_t ev, void *arg);

// Онбординг — вне карусели; зовутся из wifi-колбэка (task-контекст esp_event),
// LVGL-вызовы внутри строго под lvgl_port_lock(0)/lvgl_port_unlock().
void ui_onboarding_show(const char *ap_ssid, const char *ap_pass); // создаёт (однократно) и грузит экран вне карусели
void ui_onboarding_status(const char *text_latin);                  // строка статуса внизу экрана
void ui_onboarding_hide(void);                                      // вернуть текущий экран карусели

#endif
