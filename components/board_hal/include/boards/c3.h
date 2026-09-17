#ifndef BOARD_PINS_C3_H
#define BOARD_PINS_C3_H

// Пины ESP32-C3 (VIEWE UEDX24240013-MD50E, GC9A01 240x240).
// Перенесены дословно из components/board_hal/hal_display_c3.c и
// hal_input_c3.c при выделении сборочной матрицы (Task 1, M2.5).

#define PIN_SCLK 1
#define PIN_MOSI 0
#define PIN_CS   10
#define PIN_DC   4
#define PIN_RST  2
#define PIN_BL   8

#define PIN_ENC_A 6
#define PIN_ENC_B 7
#define PIN_BTN   9

#endif
