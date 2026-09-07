#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Тонкая обёртка над NVS, namespace "knobos".
// Ключи M2: "wifi_ssid", "wifi_pass", "ap_pass", "web_pass"

esp_err_t settings_init(void); // nvs_flash_init с retry-паттерном + nvs_open("knobos", ...)
bool settings_get_str(const char *key, char *out, size_t out_len); // false, если ключа нет
esp_err_t settings_set_str(const char *key, const char *val); // + nvs_commit
esp_err_t settings_erase_key(const char *key);
