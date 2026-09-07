#include "hal.h"
#include "ui.h"
#include "net.h"
#include "settings.h"
#include "wifi_mgr.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "main";
static bool s_sntp_started = false;

static void on_wifi_status(wifi_sm_state_t st, const char *detail) {
    ESP_LOGI(TAG, "wifi state=%d detail=%s", st, detail);
    switch (st) {
    case WSM_AP:
        // show идемпотентен (лениво создаёт экран один раз, ssid/pass не меняются);
        // detail непустой после TRY_FAIL (или "sta timeout") — перекрывает статус-строку ошибкой.
        ui_onboarding_show(wifi_mgr_ap_ssid(), wifi_mgr_ap_pass());
        if (detail && detail[0]) ui_onboarding_status(detail);
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
        break;
    default:
        break;
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());
    ui_init();

    settings_init();
    wifi_mgr_start(on_wifi_status);
}
