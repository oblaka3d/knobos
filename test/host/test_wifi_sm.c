#include "unity.h"
#include "wifi_sm.h"
void setUp(void) {} void tearDown(void) {}
static void chk(wifi_sm_state_t s, wifi_sm_event_t e, wifi_sm_state_t want) {
    TEST_ASSERT_EQUAL(want, wifi_sm_next(s, e));
}
void test_boot(void)        { chk(WSM_BOOT, WSM_EV_HAS_CREDS, WSM_STA_CONNECTING); chk(WSM_BOOT, WSM_EV_NO_CREDS, WSM_AP); }
void test_sta_connect(void) { chk(WSM_STA_CONNECTING, WSM_EV_GOT_IP, WSM_STA_OK); chk(WSM_STA_CONNECTING, WSM_EV_STA_FAIL, WSM_AP); }
void test_portal_flow(void) { chk(WSM_AP, WSM_EV_NEW_CREDS, WSM_AP_TRYING); chk(WSM_AP_TRYING, WSM_EV_TRY_OK, WSM_STA_OK); chk(WSM_AP_TRYING, WSM_EV_TRY_FAIL, WSM_AP); }
void test_reconnect(void)   { chk(WSM_STA_OK, WSM_EV_STA_FAIL, WSM_STA_CONNECTING); }
void test_unknown_keeps(void){ chk(WSM_AP, WSM_EV_GOT_IP, WSM_AP); chk(WSM_STA_OK, WSM_EV_NO_CREDS, WSM_STA_OK); }
int main(void){ UNITY_BEGIN(); RUN_TEST(test_boot); RUN_TEST(test_sta_connect); RUN_TEST(test_portal_flow); RUN_TEST(test_reconnect); RUN_TEST(test_unknown_keeps); return UNITY_END(); }
