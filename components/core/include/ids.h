#pragma once

#include <stdint.h>
#include <stddef.h>

// Чистые функции идентичности устройства — без зависимостей от IDF, хост-тестируемые.

// "Knob-XXXX", XXXX = HEX последних 2 байт MAC, верхний регистр. out — буфер >=16 байт.
void knob_ap_ssid(const uint8_t mac[6], char out[16]);

// "WIFI:T:WPA;S:<ssid>;P:<pass>;;"
void knob_wifi_qr(const char *ssid, const char *pass, char *out, size_t out_len);

// out_len-1 символов из "abcdefghjkmnpqrstuvwxyz23456789" (без похожих), rnd байт → символ по модулю.
void knob_gen_pass(const uint8_t *rnd, size_t rnd_len, char *out, size_t out_len);
