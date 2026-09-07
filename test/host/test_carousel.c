#include "unity.h"
#include "carousel.h"

void setUp(void) {} void tearDown(void) {}

void test_next_wraps_around(void) {
    carousel_t c; carousel_init(&c, 3);
    TEST_ASSERT_EQUAL(1, carousel_next(&c));
    TEST_ASSERT_EQUAL(2, carousel_next(&c));
    TEST_ASSERT_EQUAL(0, carousel_next(&c));
}

void test_next_skips_disabled(void) {
    carousel_t c; carousel_init(&c, 3);
    carousel_set_enabled(&c, 1, false);
    TEST_ASSERT_EQUAL(2, carousel_next(&c));  // 1 пропущен
    TEST_ASSERT_EQUAL(0, carousel_next(&c));
}

void test_single_enabled_stays(void) {
    carousel_t c; carousel_init(&c, 3);
    carousel_set_enabled(&c, 1, false);
    carousel_set_enabled(&c, 2, false);
    TEST_ASSERT_EQUAL(0, carousel_next(&c));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_next_wraps_around);
    RUN_TEST(test_next_skips_disabled);
    RUN_TEST(test_single_enabled_stays);
    return UNITY_END();
}
