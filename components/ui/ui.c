#include "ui.h"
#include "screens.h"
#include "carousel.h"
#include "hal.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

extern const screen_desc_t screen_clock;
static const screen_desc_t *s_screens[SCREEN_COUNT] = { &screen_clock };
static lv_obj_t *s_objs[SCREEN_COUNT];
static carousel_t s_car;

void ui_init(void) {
    carousel_init(&s_car, SCREEN_COUNT);
    lvgl_port_lock(0);
    for (int i = 0; i < SCREEN_COUNT; i++) s_objs[i] = s_screens[i]->create();
    lv_screen_load(s_objs[0]);
    lvgl_port_unlock();
    s_screens[0]->on_show();
    ESP_ERROR_CHECK(hal_input_init(ui_handle_input, NULL));
}

void ui_handle_input(hal_input_event_t ev, void *arg) {
    static const char *names[] = {"CW", "CCW", "SHORT", "LONG", "DOUBLE"};
    ESP_LOGI("input", "%s", names[ev]);
    if (ev == HAL_IN_BTN_LONG) {
        int idx = carousel_next(&s_car);
        lvgl_port_lock(0);
        lv_screen_load_anim(s_objs[idx], LV_SCR_LOAD_ANIM_MOVE_LEFT, 150, 0, false);
        lvgl_port_unlock();
        s_screens[idx]->on_show();
        return;
    }
    const screen_desc_t *scr = s_screens[carousel_current(&s_car)];
    if (scr->on_input) scr->on_input(ev);
}
