#pragma once

#include "esp_err.h"
#include "wifi_sm.h"
#include <stdbool.h>
#include <stddef.h>

// Драйвер WiFi APSTA поверх wifi_sm: читает/пишет креды через settings (Task 1).

typedef void (*wifi_mgr_status_cb_t)(wifi_sm_state_t st, const char *detail); // detail: IP, текст ошибки или ""

// Читает settings: креды есть -> WIFI_MODE_STA (AP не поднимается вовсе, таймаут 15с -> AP);
// нет -> WIFI_MODE_APSTA сразу (AP: ssid knob_ap_ssid(MAC), пароль settings "ap_pass",
// генерируется при отсутствии; "web_pass" тоже заполняется при отсутствии).
esp_err_t wifi_mgr_start(wifi_mgr_status_cb_t cb);

// В AP-режиме: пробует STA-креды до 15с; успех -> сохраняет wifi_ssid/wifi_pass,
// cb(WSM_STA_OK, ip), через 1.5с esp_restart(); провал -> cb(WSM_AP, "..."), ESP_FAIL.
esp_err_t wifi_mgr_try_credentials(const char *ssid, const char *pass);

void wifi_mgr_reset_credentials(void);   // erase wifi_ssid/wifi_pass + esp_restart()
bool wifi_mgr_is_provisioned(void);      // есть ли wifi_ssid в settings
const char *wifi_mgr_ip(void);           // "192.168.x.x" или ""
const char *wifi_mgr_ap_ssid(void);      // актуальный SSID AP
const char *wifi_mgr_ap_pass(void);

// Блокирующий скан, результат JSON-массивом: [{"ssid":"...","rssi":-60,"secure":true},...]
// по убыванию rssi, дубликаты ssid схлопнуты, максимум 20.
esp_err_t wifi_mgr_scan_json(char *out, size_t out_len);
