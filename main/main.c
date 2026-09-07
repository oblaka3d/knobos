#include "hal.h"
#include "ui.h"
#include "net.h"
#include "sdkconfig.h"
#include <string.h>

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());
    ui_init();

    if (strlen(CONFIG_KNOBOS_DEV_WIFI_SSID) > 0) {
        if (net_start_sta(CONFIG_KNOBOS_DEV_WIFI_SSID, CONFIG_KNOBOS_DEV_WIFI_PASS) == ESP_OK)
            net_start_sntp("MSK-3");
    }
}
