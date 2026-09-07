#ifndef SCREENS_H
#define SCREENS_H

#include "hal.h"
#include "lvgl.h"

// screens.h — контракт экрана
typedef struct {
    lv_obj_t *(*create)(void);              // создать lv-объект экрана
    void (*on_input)(hal_input_event_t ev); // события кроме LONG (LONG забирает карусель)
    void (*on_show)(void);
} screen_desc_t;

#define SCREEN_CLOCK 0
#define SCREEN_COUNT 1 // растёт в M3

#endif
