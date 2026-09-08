#include "hal.h"
#include "lvgl.h"
#include "ids.h"
#include <string.h>

// Экран онбординга — SSID/пароль AP + QR + статус. Вне карусели (screens.h), не screen_desc_t:
// создаётся и грузится напрямую из ui_onboarding_show/status/hide (ui.c) под lvgl_port_lock.

static lv_obj_t *s_qr;
static lv_obj_t *s_ssid_label;
static lv_obj_t *s_pass_label;
static lv_obj_t *s_status_label;

lv_obj_t *screen_onboarding_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x888888), 0);
    lv_label_set_text(title, "Setup WiFi");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    s_qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(s_qr, 120);
    lv_qrcode_set_dark_color(s_qr, lv_color_black());
    lv_qrcode_set_light_color(s_qr, lv_color_white());
    lv_obj_align(s_qr, LV_ALIGN_CENTER, 0, -20);

    // lv_obj_align_to(отн. QR) не подходит: в момент создания label пуст (0×0), а координата
    // фиксируется один раз и не пересчитывается после set_text — используем ALIGN_CENTER
    // с фиксированными offset'ами (тот же приём, что screen_clock.c).
    s_ssid_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_ssid_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_ssid_label, lv_color_white(), 0);
    lv_obj_align(s_ssid_label, LV_ALIGN_CENTER, 0, 50);

    s_pass_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_pass_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_pass_label, lv_color_white(), 0);
    // y=64, а не 72 — освобождает вертикальный зазор до статус-лейбла (см. ниже), который
    // теперь поднят и шире по высоте (двухстрочные тексты ошибок wifi_mgr).
    lv_obj_align(s_pass_label, LV_ALIGN_CENTER, 0, 64);

    s_status_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(s_status_label, 170);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_status_label, "Open 192.168.4.1");
    // y=-24, а не -10 — на круглом экране хорда у самого низа круга слишком узкая для строки
    // текста; выше строка помещается без обрезки по краям.
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    return scr;
}

void screen_onboarding_set_creds(const char *ap_ssid, const char *ap_pass) {
    lv_label_set_text(s_ssid_label, ap_ssid);
    lv_label_set_text(s_pass_label, ap_pass);
    char qr_data[96];
    knob_wifi_qr(ap_ssid, ap_pass, qr_data, sizeof(qr_data));
    lv_qrcode_update(s_qr, qr_data, strlen(qr_data));
}

void screen_onboarding_set_status(const char *text) {
    if (!s_status_label) return;
    lv_label_set_text(s_status_label, text);
}
