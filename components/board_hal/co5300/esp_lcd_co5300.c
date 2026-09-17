/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Ported from the vendor's standalone esp_lcd_sh8601 driver:
// /tmp/viewe-s3/examples/PlatformIO/encoder15/lib/
//   ESP32_Display_Panel-bugfix-missing_lcd_load_vendor_config/src/lcd/base/esp_lcd_sh8601.c
// (git clone --depth 1 https://github.com/VIEWESMART/UEDX46460015-MD50ESP32-1.5inch-Touch-Knob-Display)
// Changes from the original: dropped the ESP32_Display_Panel framework
// dependency (ESP_PanelLog.h / esp_lcd_vendor_types.h) in favor of plain
// esp_log.h + esp_check.h and the local co5300_vendor_config_t type; renamed
// sh8601 -> co5300 throughout for consistency with our board's documented
// chip name (see esp_lcd_co5300.h for why). Panel command logic and the
// default init sequence are otherwise unchanged.

#include <stdlib.h>
#include <sys/cdefs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"

#include "esp_lcd_co5300.h"

#define LCD_OPCODE_WRITE_CMD        (0x02ULL)
#define LCD_OPCODE_WRITE_COLOR      (0x32ULL)

// Замерено на реальном железе (Task 2, M2.5): esp_lcd_panel_io_tx_color() на QSPI
// возвращал ESP_ERR_NO_MEM в узком окне сразу после esp_wifi_init()/esp_wifi_start().
// Корень найден в esp_driver_spi (setup_priv_desc(), spi_master.c): цветовой буфер
// LVGL лежит в PSRAM, а esp_ptr_dma_capable() для PSRAM-адресов на S3 всегда false
// (SOC_DMA_LOW/HIGH покрывают только внутреннюю SRAM) — GP-SPI (SPI2/SPI3) поэтому
// на КАЖДУЮ QSPI-цветовую транзакцию сам аллоцирует временный internal-RAM
// DMA-буфер под весь чанк и копирует в него данные. Если внутренней RAM в этот
// момент мало (а ровно в этот момент её активно расходует стек WiFi), аллокация
// проваливается. Настоящий фикс — уменьшенный LCD_BUF_LINES в hal_display_s3.c
// (меньше internal-RAM нужно на транзакцию); диагностика (прямые draw_bitmap ДО
// старта wifi — все ESP_OK) исключила проблему геометрии/офсетов. Ниже —
// дополнительный bounded retry на случай кратковременной нехватки памяти при
// более высокой нагрузке: esp_lvgl_port для LVGL9 не проверяет код возврата
// esp_lcd_panel_draw_bitmap() и не вызывает lv_disp_flush_ready() сам — при
// сбое colour-callback не приходит, и задача LVGL виснет в wait_for_flushing()
// навсегда (task_wdt). Бортовой bounce-buffer (trans_size) для LVGL9 в
// esp_lvgl_port 2.9.0 не реализован (есть только для lvgl8).
#define CO5300_IO_RETRY_MAX 20
#define CO5300_IO_RETRY_DELAY_MS 2

static const char *TAG = "co5300";

static esp_err_t panel_co5300_del(esp_lcd_panel_t *panel);
static esp_err_t panel_co5300_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_co5300_init(esp_lcd_panel_t *panel);
static esp_err_t panel_co5300_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_co5300_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_co5300_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_co5300_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_co5300_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_co5300_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // текущее значение регистра LCD_CMD_MADCTL
    uint8_t colmod_val; // текущее значение регистра LCD_CMD_COLMOD
    const co5300_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int use_qspi_interface: 1;
        unsigned int reset_level: 1;
    } flags;
} co5300_panel_t;

esp_err_t esp_lcd_new_panel_co5300(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    co5300_panel_t *co5300 = NULL;
    co5300 = calloc(1, sizeof(co5300_panel_t));
    ESP_GOTO_ON_FALSE(co5300, ESP_ERR_NO_MEM, err, TAG, "no mem for co5300 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        co5300->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        co5300->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    uint8_t fb_bits_per_pixel = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        co5300->colmod_val = 0x55;
        fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        co5300->colmod_val = 0x66;
        fb_bits_per_pixel = 24;
        break;
    case 24: // RGB888
        co5300->colmod_val = 0x77;
        fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    co5300->io = io;
    co5300->reset_gpio_num = panel_dev_config->reset_gpio_num;
    co5300->fb_bits_per_pixel = fb_bits_per_pixel;
    co5300_vendor_config_t *vendor_config = (co5300_vendor_config_t *)panel_dev_config->vendor_config;
    if (vendor_config) {
        co5300->init_cmds = vendor_config->init_cmds;
        co5300->init_cmds_size = vendor_config->init_cmds_size;
        co5300->flags.use_qspi_interface = vendor_config->flags.use_qspi_interface;
    }
    co5300->flags.reset_level = panel_dev_config->flags.reset_active_high;
    co5300->base.del = panel_co5300_del;
    co5300->base.reset = panel_co5300_reset;
    co5300->base.init = panel_co5300_init;
    co5300->base.draw_bitmap = panel_co5300_draw_bitmap;
    co5300->base.invert_color = panel_co5300_invert_color;
    co5300->base.set_gap = panel_co5300_set_gap;
    co5300->base.mirror = panel_co5300_mirror;
    co5300->base.swap_xy = panel_co5300_swap_xy;
    co5300->base.disp_on_off = panel_co5300_disp_on_off;
    *ret_panel = &(co5300->base);
    ESP_LOGD(TAG, "new co5300 panel @%p", co5300);

    return ESP_OK;

err:
    if (co5300) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(co5300);
    }
    return ret;
}

static esp_err_t tx_param(co5300_panel_t *co5300, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (co5300->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    }
    esp_err_t ret = ESP_OK;
    for (int attempt = 0; attempt < CO5300_IO_RETRY_MAX; attempt++) {
        ret = esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(CO5300_IO_RETRY_DELAY_MS));
    }
    ESP_LOGW(TAG, "tx_param cmd=0x%x failed after %d retries: %s", lcd_cmd, CO5300_IO_RETRY_MAX, esp_err_to_name(ret));
    return ret;
}

static esp_err_t tx_color(co5300_panel_t *co5300, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (co5300->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    }
    esp_err_t ret = ESP_OK;
    for (int attempt = 0; attempt < CO5300_IO_RETRY_MAX; attempt++) {
        ret = esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(CO5300_IO_RETRY_DELAY_MS));
    }
    ESP_LOGW(TAG, "tx_color cmd=0x%x failed after %d retries: %s", lcd_cmd, CO5300_IO_RETRY_MAX, esp_err_to_name(ret));
    return ret;
}

static esp_err_t panel_co5300_del(esp_lcd_panel_t *panel)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);

    if (co5300->reset_gpio_num >= 0) {
        gpio_reset_pin(co5300->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del co5300 panel @%p", co5300);
    free(co5300);
    return ESP_OK;
}

static esp_err_t panel_co5300_reset(esp_lcd_panel_t *panel)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    esp_lcd_panel_io_handle_t io = co5300->io;

    if (co5300->reset_gpio_num >= 0) {
        gpio_set_level(co5300->reset_gpio_num, co5300->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(co5300->reset_gpio_num, !co5300->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(150));
    } else { // программный сброс
        ESP_RETURN_ON_ERROR(tx_param(co5300, io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(80));
    }

    return ESP_OK;
}

// Дефолт из оригинального driver'а (используется только если vendor_config->init_cmds не задан —
// у нас всегда задан свой, см. hal_display_s3.c).
static const co5300_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0x44, (uint8_t []){0x00, 0xc8}, 2, 0},
    {0x35, (uint8_t []){0x00}, 0, 0},
    {0x53, (uint8_t []){0x20}, 1, 25},
};

static esp_err_t panel_co5300_init(esp_lcd_panel_t *panel)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    esp_lcd_panel_io_handle_t io = co5300->io;
    const co5300_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    ESP_RETURN_ON_ERROR(tx_param(co5300, io, LCD_CMD_MADCTL, (uint8_t[]) {
        co5300->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(co5300, io, LCD_CMD_COLMOD, (uint8_t[]) {
        co5300->colmod_val,
    }, 1), TAG, "send command failed");

    if (co5300->init_cmds) {
        init_cmds = co5300->init_cmds;
        init_cmds_size = co5300->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(co5300_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            co5300->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            co5300->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(tx_param(co5300, io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

// Если tx_param/tx_color здесь (CASET/RASET/RAMWR) исчерпают ретраи (см.
// CO5300_IO_RETRY_MAX), esp_lvgl_port для LVGL9 всё равно не проверяет код
// возврата esp_lcd_panel_draw_bitmap() и не позовёт lv_disp_flush_ready() сам —
// on_color_trans_done не придёт (цветовая транзакция либо не отправлена вовсе,
// либо отправлена без корректного окна CASET/RASET), и taskLVGL зависнет в
// wait_for_flushing() НАВСЕГДА (см. task-2-report.md, Fix round 1, п.2).
// Для продаваемого устройства самолечение перезагрузкой лучше вечно
// замёрзшего экрана: NVS/WiFi-креды целы, устройство просто перезапустится и
// вернётся в рабочий режим. Отсюда явная проверка + esp_restart() вместо
// молчаливого ESP_RETURN_ON_ERROR, как было раньше.
static void draw_bitmap_fatal(const char *what, esp_err_t err) __attribute__((noreturn));
static void draw_bitmap_fatal(const char *what, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed after retries (%s) — LVGL flush would hang forever, restarting", what, esp_err_to_name(err));
    esp_restart();
}

static esp_err_t panel_co5300_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = co5300->io;

    x_start += co5300->x_gap;
    x_end += co5300->x_gap;
    y_start += co5300->y_gap;
    y_end += co5300->y_gap;

    esp_err_t ret;
    ret = tx_param(co5300, io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4);
    if (ret != ESP_OK) {
        draw_bitmap_fatal("CASET", ret);
    }
    ret = tx_param(co5300, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);
    if (ret != ESP_OK) {
        draw_bitmap_fatal("RASET", ret);
    }
    size_t len = (x_end - x_start) * (y_end - y_start) * co5300->fb_bits_per_pixel / 8;
    ret = tx_color(co5300, io, LCD_CMD_RAMWR, color_data, len);
    if (ret != ESP_OK) {
        draw_bitmap_fatal("RAMWR/tx_color", ret);
    }

    return ESP_OK;
}

static esp_err_t panel_co5300_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    esp_lcd_panel_io_handle_t io = co5300->io;
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    ESP_RETURN_ON_ERROR(tx_param(co5300, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_co5300_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    esp_lcd_panel_io_handle_t io = co5300->io;
    esp_err_t ret = ESP_OK;

    if (mirror_x) {
        co5300->madctl_val |= BIT(6);
    } else {
        co5300->madctl_val &= ~BIT(6);
    }
    if (mirror_y) {
        ESP_LOGE(TAG, "mirror_y is not supported by this panel");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
    ESP_RETURN_ON_ERROR(tx_param(co5300, io, LCD_CMD_MADCTL, (uint8_t[]) {
        co5300->madctl_val
    }, 1), TAG, "send command failed");
    return ret;
}

static esp_err_t panel_co5300_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    // Отличие от исходника VIEWE: тот логирует ESP_LOGE и возвращает
    // ESP_ERR_NOT_SUPPORTED безусловно, даже для swap_axes=false — а
    // esp_lvgl_port вызывает эту функцию при каждом lvgl_port_add_disp()
    // независимо от .rotation.swap_xy, так что на каждой загрузке в лог
    // попадала ложная ошибка. false — не No-op с точки зрения панели.
    if (!swap_axes) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "swap_xy is not supported by this panel");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_co5300_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    co5300->x_gap = x_gap;
    co5300->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_co5300_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    co5300_panel_t *co5300 = __containerof(panel, co5300_panel_t, base);
    esp_lcd_panel_io_handle_t io = co5300->io;
    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;
    ESP_RETURN_ON_ERROR(tx_param(co5300, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

esp_err_t co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t value)
{
    // Наш борд всегда QSPI (см. hal_display_s3.c) — то же кодирование команды, что в tx_param() выше.
    // Тот же bounded retry, что и в tx_param/tx_color — на случай гонки с flash/NVS (см. комментарий выше).
    uint32_t lcd_cmd = LCD_CMD_WRDISBV & 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    esp_err_t ret = ESP_OK;
    for (int attempt = 0; attempt < CO5300_IO_RETRY_MAX; attempt++) {
        ret = esp_lcd_panel_io_tx_param(io, lcd_cmd, &value, 1);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(CO5300_IO_RETRY_DELAY_MS));
    }
    ESP_LOGW(TAG, "set_brightness failed after %d retries: %s", CO5300_IO_RETRY_MAX, esp_err_to_name(ret));
    return ret;
}
