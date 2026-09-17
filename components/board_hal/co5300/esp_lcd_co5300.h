/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Task 2 (M2.5, knobos): vendored esp_lcd panel driver for the VIEWE
// UEDX46460015-MD50E AMOLED module (466x466, QSPI). The espressif component
// registry (components.espressif.com / components-file.espressif.com) was
// unreachable from this machine at implementation time, so this is ported
// from the vendor's own source instead of pulled as a managed component —
// see co5300_vendor_init.h for exact provenance.
//
// The panel is documented as "CO5300" (datasheet in the VIEWE clone,
// /tmp/viewe-s3/datasheet/CO5300_Datasheet_V0.00_20230328.pdf), but VIEWE's
// own board bring-up code drives it through their "SH8601" esp_lcd port
// (Libraries/ESP32_Display_Panel/src/drivers/lcd/port/esp_lcd_sh8601.c and
// the older standalone fork at examples/PlatformIO/encoder15/lib/
// ESP32_Display_Panel-bugfix-missing_lcd_load_vendor_config/src/lcd/base/
// esp_lcd_sh8601.c) — CO5300/SH8601 are command-compatible AMOLED
// controllers, a common pairing for this class of round AMOLED modules, and
// no file literally named esp_lcd_co5300.* exists anywhere in the clone.
// This file is that standalone SH8601 port, stripped of its
// ESP32_Display_Panel framework dependencies (ESP_PanelLog.h,
// esp_lcd_vendor_types.h) and renamed to match our board's documented chip
// name; the command sequence and logic are unchanged from the vendor source.

#pragma once

#include <stdint.h>
#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD panel initialization command: {cmd, data, data_bytes, delay_ms}
typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} co5300_lcd_init_cmd_t;

// Passed via esp_lcd_panel_dev_config_t.vendor_config
typedef struct {
    const co5300_lcd_init_cmd_t *init_cmds; // NULL = built-in default sequence
    uint16_t init_cmds_size;
    struct {
        unsigned int use_qspi_interface: 1;
    } flags;
} co5300_vendor_config_t;

/**
 * @brief Create an esp_lcd panel handle for the CO5300/SH8601 AMOLED controller.
 *
 * @param[in]  io               LCD panel IO handle (SPI or QSPI)
 * @param[in]  panel_dev_config Panel device config; vendor_config selects QSPI
 *                               and/or overrides the default init sequence
 * @param[out] ret_panel        Returned panel handle
 */
esp_err_t esp_lcd_new_panel_co5300(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Set AMOLED brightness via the panel's WRDISBV command (0x51).
 *
 * Uses the same QSPI command framing as the panel driver (opcode 0x02 in
 * bits[31:24], register in bits[15:8], per the vendor's tx_param()) — exposed
 * separately because brightness is set directly through the IO handle, not
 * through an esp_lcd_panel_t entry point.
 *
 * @param[in] io    LCD panel IO handle (QSPI)
 * @param[in] value Brightness, 0..255
 */
esp_err_t co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t value);

// QSPI panel IO config for this controller: 32-bit "commands" (opcode +
// 1-byte register address packed into the top bits by the driver), 8-bit
// params, quad_mode so esp_lcd drives all 4 data lines.
#define CO5300_PANEL_IO_QSPI_CONFIG(cs_gpio, cb, cb_ctx)   \
    {                                                       \
        .cs_gpio_num = (cs_gpio),                           \
        .dc_gpio_num = -1,                                  \
        .spi_mode = 0,                                      \
        .pclk_hz = 40 * 1000 * 1000,                        \
        .trans_queue_depth = 10,                            \
        .on_color_trans_done = (cb),                        \
        .user_ctx = (cb_ctx),                               \
        .lcd_cmd_bits = 32,                                 \
        .lcd_param_bits = 8,                                \
        .flags = {                                          \
            .quad_mode = true,                              \
        },                                                  \
    }

#ifdef __cplusplus
}
#endif
