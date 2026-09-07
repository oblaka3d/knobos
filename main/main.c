#include "hal.h"
#include "ui.h"
#include "net.h"
#include "settings.h"
#include "wifi_mgr.h"
#include "web.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

// Временный сброс WiFi кнопкой (M2; экран настроек придёт в M3): GPIO9 (инвертированная,
// см. board_hal PIN_BTN), удержание 3с при включении стирает сохранённые креды.
// iot_button ещё не создан на этом этапе загрузки — используется прямой gpio_get_level.
#define RESET_BTN_GPIO GPIO_NUM_9
#define RESET_HOLD_POLL_MS 100
#define RESET_HOLD_POLL_COUNT 30 // 30 * 100мс = 3с

static const char *TAG = "main";
static bool s_sntp_started = false;
static bool s_web_normal_started = false;

static void on_wifi_status(wifi_sm_state_t st, const char *detail) {
    web_notify_wifi_state(st, detail); // безусловно, первым делом — для GET /api/wifi/status
    ESP_LOGI(TAG, "wifi state=%d detail=%s", st, detail);
    switch (st) {
    case WSM_AP:
        // show идемпотентен (лениво создаёт экран один раз, ssid/pass не меняются);
        // detail непустой после TRY_FAIL (или "sta timeout") — перекрывает статус-строку ошибкой.
        ui_onboarding_show(wifi_mgr_ap_ssid(), wifi_mgr_ap_pass());
        if (detail && detail[0]) ui_onboarding_status(detail);
        web_start_portal(); // идемпотентно — httpd/dns_server поднимаются один раз
        break;
    case WSM_AP_TRYING:
        ui_onboarding_status("Connecting...");
        break;
    case WSM_STA_OK:
        ui_onboarding_hide();
        if (!s_sntp_started) {
            s_sntp_started = true;
            net_start_sntp("MSK-3");
        }
        // Флаг ставится только при успехе — неудачный httpd_start (см. web_start_normal)
        // не должен блокировать повторную попытку на следующем WSM_STA_OK (реконнект).
        if (!s_web_normal_started && web_start_normal() == ESP_OK) {
            s_web_normal_started = true;
        }
        break;
    default:
        break;
    }
}

// Кнопка GPIO9 удерживается непрерывно 3с при включении -> сброс сохранённых WiFi-кредов
// (устройство уйдёт в AP-онбординг). Отпустил раньше -> выходим без изменений.
static void reset_wifi_if_button_held(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RESET_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    if (gpio_get_level(RESET_BTN_GPIO) != 0) return; // не нажата

    for (int i = 0; i < RESET_HOLD_POLL_COUNT; i++) {
        vTaskDelay(pdMS_TO_TICKS(RESET_HOLD_POLL_MS));
        if (gpio_get_level(RESET_BTN_GPIO) != 0) return; // отпустили — без сброса
    }

    ESP_LOGW(TAG, "reset button held 3s at boot — erasing WiFi credentials");
    settings_erase_key("wifi_ssid");
    settings_erase_key("wifi_pass");
}

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());

    settings_init();
    reset_wifi_if_button_held();

    ui_init();
    wifi_mgr_start(on_wifi_status);
}
