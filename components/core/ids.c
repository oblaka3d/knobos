#include "ids.h"
#include <stdio.h>
#include <string.h>

static const char PASS_CHARSET[] = "abcdefghjkmnpqrstuvwxyz23456789";

void knob_ap_ssid(const uint8_t mac[6], char out[16]) {
    snprintf(out, 16, "Knob-%02X%02X", mac[4], mac[5]);
}

void knob_wifi_qr(const char *ssid, const char *pass, char *out, size_t out_len) {
    snprintf(out, out_len, "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
}

void knob_gen_pass(const uint8_t *rnd, size_t rnd_len, char *out, size_t out_len) {
    size_t n = out_len - 1;
    if (n > rnd_len) n = rnd_len;
    for (size_t i = 0; i < n; i++)
        out[i] = PASS_CHARSET[rnd[i] % (sizeof(PASS_CHARSET) - 1)];
    out[n] = '\0';
}
