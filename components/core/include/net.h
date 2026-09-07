#pragma once

void net_start_sntp(const char *tz);                          // esp_sntp + setenv("TZ",tz,1)+tzset()
