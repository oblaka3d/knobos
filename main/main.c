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
    if (st == WSM_STA_OK && !s_sntp_started) {
        s_sntp_started = true;
        net_start_sntp("MSK-3");
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());
    ui_init();

    settings_init();
    wifi_mgr_start(on_wifi_status);
}
