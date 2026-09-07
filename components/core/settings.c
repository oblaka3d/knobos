#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "knobos"

static nvs_handle_t s_handle;
static bool s_initialized = false;

esp_err_t settings_init(void) {
    if (s_initialized) return ESP_OK;

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) return err;

    s_initialized = true;
    return ESP_OK;
}

bool settings_get_str(const char *key, char *out, size_t out_len) {
    esp_err_t err = nvs_get_str(s_handle, key, out, &out_len);
    return err == ESP_OK;
}

esp_err_t settings_set_str(const char *key, const char *val) {
    esp_err_t err = nvs_set_str(s_handle, key, val);
    if (err != ESP_OK) return err;
    return nvs_commit(s_handle);
}

esp_err_t settings_erase_key(const char *key) {
    esp_err_t err = nvs_erase_key(s_handle, key);
    if (err != ESP_OK) return err;
    return nvs_commit(s_handle);
}
