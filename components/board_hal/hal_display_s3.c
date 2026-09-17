#include "hal.h"
#include "esp_log.h"

// Стаб до Task 2 (M2.5): реализация дисплея AMOLED CO5300/SH8601 QSPI
// на ESP32-S3. Пины уже зафиксированы в boards/s3.h.

static const char *TAG = "hal_display_s3";

esp_err_t hal_display_init(void) {
    ESP_LOGE(TAG, "S3 display: Task 2");
    return ESP_ERR_NOT_SUPPORTED;
}

void hal_backlight_set(uint8_t percent) {
    (void)percent;
}

lv_display_t *hal_lv_display(void) { return NULL; }
