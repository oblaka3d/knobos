#include "esp_log.h"
#include "hal.h"
#include "esp_lvgl_port.h"

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "knobos");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    lvgl_port_unlock();
}
