#include "unity.h"
#include "ids.h"
#include <string.h>

void setUp(void) {} void tearDown(void) {}

void test_ap_ssid_from_mac(void) {
    uint8_t mac[6] = {0x34,0xcd,0xb0,0xce,0x56,0x64};
    char out[16];
    knob_ap_ssid(mac, out);
    TEST_ASSERT_EQUAL_STRING("Knob-5664", out);
}

void test_wifi_qr_format(void) {
    char out[96];
    knob_wifi_qr("Knob-5664", "abc123", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:Knob-5664;P:abc123;;", out);
}

void test_gen_pass_len_and_charset(void) {
    uint8_t rnd[8] = {0, 31, 200, 5, 99, 254, 17, 42};
    char out[9];
    knob_gen_pass(rnd, sizeof(rnd), out, sizeof(out));
    TEST_ASSERT_EQUAL(8, strlen(out));
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_NOT_NULL(strchr("abcdefghjkmnpqrstuvwxyz23456789", out[i]));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ap_ssid_from_mac);
    RUN_TEST(test_wifi_qr_format);
    RUN_TEST(test_gen_pass_len_and_charset);
    return UNITY_END();
}
