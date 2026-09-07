#pragma once

#include "esp_err.h"
#include "wifi_sm.h"

// Captive portal: httpd на :80 без аутентификации (эндпоинты ниже) + dns_server,
// отвечающий «*» IP-адресом AP (192.168.4.1). Идемпотентно — повторный вызов при уже
// запущенном сервере ничего не делает. Зовётся из main в AP-режиме (Task 2).
//
// GET  /                → portal.html (embedded gzip, Content-Encoding: gzip)
// GET  /api/wifi/scan   → JSON от wifi_mgr_scan_json
// POST /api/wifi        → {"ssid":"...","pass":"..."}, запускает wifi_mgr_try_credentials
//                          в отдельной таске; отвечает 200 {"status":"trying"} сразу,
//                          409 {"error":"busy"} если попытка уже идёт
// GET  /api/wifi/status → {"state":"ap"|"trying"|"ok"|"fail","detail":"..."}
// любой другой GET (404) → 302 Location: http://192.168.4.1/
esp_err_t web_start_portal(void);

// Запоминает последнее состояние wifi_mgr для /api/wifi/status. Вызывается из main
// безусловно на каждый колбэк wifi_mgr (даже до старта портала).
void web_notify_wifi_state(wifi_sm_state_t st, const char *detail);
