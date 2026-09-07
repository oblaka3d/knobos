#include "hal.h"
#include "iot_knob.h"
#include "iot_button.h"
#include "button_gpio.h"

#define PIN_ENC_A 6
#define PIN_ENC_B 7
#define PIN_BTN   9
#define LONG_PRESS_MS  600
#define SHORT_PRESS_MS 400

static hal_input_cb_t s_cb;
static void *s_arg;

static void knob_left(void *a, void *d)  { s_cb(HAL_IN_ROT_CCW, s_arg); }
static void knob_right(void *a, void *d) { s_cb(HAL_IN_ROT_CW, s_arg); }
static void btn_single(void *a, void *d) { s_cb(HAL_IN_BTN_SHORT, s_arg); }
static void btn_double(void *a, void *d) { s_cb(HAL_IN_BTN_DOUBLE, s_arg); }
static void btn_long(void *a, void *d)   { s_cb(HAL_IN_BTN_LONG, s_arg); }

esp_err_t hal_input_init(hal_input_cb_t cb, void *arg) {
    s_cb = cb; s_arg = arg;
    knob_config_t kcfg = { .gpio_encoder_a = PIN_ENC_A, .gpio_encoder_b = PIN_ENC_B };
    knob_handle_t knob = iot_knob_create(&kcfg);
    if (!knob) return ESP_FAIL;
    iot_knob_register_cb(knob, KNOB_LEFT, knob_left, NULL);
    iot_knob_register_cb(knob, KNOB_RIGHT, knob_right, NULL);

    const button_config_t bcfg = { .long_press_time = LONG_PRESS_MS, .short_press_time = SHORT_PRESS_MS };
    const button_gpio_config_t gcfg = { .gpio_num = PIN_BTN, .active_level = 0 };
    button_handle_t btn = NULL;
    if (iot_button_new_gpio_device(&bcfg, &gcfg, &btn) != ESP_OK) return ESP_FAIL;
    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, btn_single, NULL);
    iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, btn_double, NULL);
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, btn_long, NULL);
    return ESP_OK;
}
