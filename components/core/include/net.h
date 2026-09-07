#pragma once

#include "esp_err.h"

esp_err_t net_start_sta(const char *ssid, const char *pass); // блокирует до IP или таймаута 15с
void net_start_sntp(const char *tz);                          // esp_sntp + setenv("TZ",tz,1)+tzset()
