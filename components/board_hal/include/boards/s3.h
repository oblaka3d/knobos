#ifndef BOARD_PINS_S3_H
#define BOARD_PINS_S3_H

// Пины ESP32-S3 (VIEWE UEDX46460015-MD50E, AMOLED CO5300/SH8601 466x466 QSPI,
// тач CST820 I2C, энкодер + кнопка). Источник — официальный клон:
// git clone --depth 1 https://github.com/VIEWESMART/UEDX46460015-MD50ESP32-1.5inch-Touch-Knob-Display /tmp/viewe-s3
//
// QSPI-дисплей + RST + digital-enable — из:
//   Libraries/ESP32_Display_Panel/src/board/supported/viewe/BOARD_VIEWE_UEDX46460015_MD50ET.h
// Тач I2C (SDA/SCL) и адрес по умолчанию CST820 (класс-совместимый драйвер
// CST816S) — из того же файла и из:
//   Libraries/ESP32_Display_Panel/src/drivers/touch/port/esp_lcd_touch_cst820.h (ADDRESS 0x15)
// Тач RST/INT: BOARD_VIEWE_UEDX46460015_MD50ET.h (новая версия либы) отдаёт их
// как -1 (не используются), но более старый форк даёт конкретные пины —
//   examples/PlatformIO/encoder15/lib/ESP32_Display_Panel-bugfix-missing_lcd_load_vendor_config/src/board/viewe/UEDX46460015-MD50E.h (RST=2, INT=4)
// Расхождение не разрешено (тач — предмет Task 4), ниже взяты конкретные
// значения из второго источника — при реализации тача сверить со схемой.
// Физический чип тача (CST820, не CST816S) подтверждён датащитом в клоне:
//   datasheet/CST820_DataSheet-En_V1.0.pdf
// Энкодер A/B и кнопка для платы BOARD_UEDX46460015_MD50E — из:
//   examples/PlatformIO/encoder15/src/ESP_Panel_Board_Supported.h

// QSPI дисплей (CO5300/SH8601, 40 МГц, режим 0)
#define PIN_QSPI_SCLK 10
#define PIN_QSPI_D0   13
#define PIN_QSPI_D1   11
#define PIN_QSPI_D2   14
#define PIN_QSPI_D3   9
#define PIN_QSPI_CS   12
#define PIN_RST       8

// AMOLED самосветящийся — отдельной подсветки нет; GPIO17 — цифровой enable
// панели (держится в 1 после инициализации), яркость идёт командой панели
// 0x51 (реализация — Task 2).
#define PIN_DISP_EN 17

// Тач CST820 (I2C, класс-драйвер CST816S)
#define PIN_TOUCH_SDA 1
#define PIN_TOUCH_SCL 3
#define PIN_TOUCH_RST 2
#define PIN_TOUCH_INT 4
#define TOUCH_CST820_I2C_ADDR 0x15

// Энкодер + кнопка
#define PIN_ENC_A 6
#define PIN_ENC_B 5
#define PIN_BTN   0

#endif
