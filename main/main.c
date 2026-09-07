#include "hal.h"
#include "ui.h"

void app_main(void) {
    ESP_ERROR_CHECK(hal_display_init());
    ui_init();
}
