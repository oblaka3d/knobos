#include "screens.h"
#include "hal.h"
#include <time.h>

static lv_obj_t *s_time_label, *s_date_label;
static int s_brightness = 80;

static void tick_cb(lv_timer_t *t) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year < 100) { lv_label_set_text(s_time_label, "--:--"); return; }
    lv_label_set_text_fmt(s_time_label, "%02d:%02d", tm.tm_hour, tm.tm_min);
    static const char *wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    lv_label_set_text_fmt(s_date_label, "%02d.%02d %s", tm.tm_mday, tm.tm_mon + 1, wd[tm.tm_wday]);
}

static lv_obj_t *create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    s_time_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -10);
    lv_label_set_text(s_time_label, "--:--");
    s_date_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0x888888), 0);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 40);
    lv_label_set_text(s_date_label, "");
    lv_timer_create(tick_cb, 1000, NULL);
    return scr;
}

static void on_input(hal_input_event_t ev) {
    if (ev == HAL_IN_ROT_CW && s_brightness < 100) s_brightness += 5;
    if (ev == HAL_IN_ROT_CCW && s_brightness > 10) s_brightness -= 5;
    hal_backlight_set(s_brightness);
}

static void on_show(void) {}
const screen_desc_t screen_clock = { .create = create, .on_input = on_input, .on_show = on_show };
