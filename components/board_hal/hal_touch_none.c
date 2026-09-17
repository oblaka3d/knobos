#include "hal.h"

// C3 (VIEWE UEDX24240013-MD50E) не имеет тача — экран резистивный/без сенсора.
// Реализация-стаб, симметричная hal_display стабу до Task 2.
esp_err_t hal_touch_init(void) { return ESP_ERR_NOT_SUPPORTED; }
