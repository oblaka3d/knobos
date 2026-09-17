#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

esp_err_t hal_display_init(void);        // esp_lcd GC9A01 + esp_lvgl_port; после вызова LVGL готов
void hal_backlight_set(uint8_t percent); // 0..100, LEDC GPIO8 inverted
lv_display_t *hal_lv_display(void);
// потокобезопасность LVGL: lvgl_port_lock(0)/lvgl_port_unlock() из esp_lvgl_port

typedef enum { HAL_IN_ROT_CW, HAL_IN_ROT_CCW, HAL_IN_BTN_SHORT, HAL_IN_BTN_LONG, HAL_IN_BTN_DOUBLE } hal_input_event_t;
typedef void (*hal_input_cb_t)(hal_input_event_t ev, void *arg);
esp_err_t hal_input_init(hal_input_cb_t cb, void *arg);  // колбэк из task-контекста (не ISR)

// Тач (M2.5, Task 4): на S3 — CST820 по I2C как LVGL-указатель (lvgl_port_add_touch),
// экраны не имеют кликабельных объектов, UX не меняется. На C3 тача нет физически —
// ESP_ERR_NOT_SUPPORTED, звать после hal_display_init() (нужен LVGL-дисплей).
esp_err_t hal_touch_init(void);

#endif
