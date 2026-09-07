#include "hal.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"

#define PIN_SCLK 1
#define PIN_MOSI 0
#define PIN_CS 10
#define PIN_DC 4
#define PIN_RST 2
#define PIN_BL 8
#define LCD_HRES 240
#define LCD_VRES 240
#define LCD_SPI_HOST SPI2_HOST

static lv_display_t *s_disp;

esp_err_t hal_display_init(void) {
    // подсветка: LEDC, инвертированная (0 duty = максимум яркости)
    ledc_timer_config_t tcfg = { .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));
    ledc_channel_config_t ccfg = { .gpio_num = PIN_BL, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0 };
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
    hal_backlight_set(80);

    spi_bus_config_t bus = { .sclk_io_num = PIN_SCLK, .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_HRES * 40 * sizeof(uint16_t) };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = { .dc_gpio_num = PIN_DC, .cs_gpio_num = PIN_CS,
        .pclk_hz = 40 * 1000 * 1000, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .spi_mode = 0, .trans_queue_depth = 10 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t pcfg = { .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));   // как в ESPHome-конфиге
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    const lvgl_port_cfg_t lv_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lv_cfg));
    const lvgl_port_display_cfg_t dcfg = {
        .io_handle = io, .panel_handle = panel,
        .buffer_size = LCD_HRES * 40,           // 2 буфера по 40 строк — ~38KB RAM
        .double_buffer = true,
        .hres = LCD_HRES, .vres = LCD_VRES,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_dma = true },
    };
    s_disp = lvgl_port_add_disp(&dcfg);
    return s_disp ? ESP_OK : ESP_FAIL;
}

void hal_backlight_set(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = (1023 * (100 - percent)) / 100;  // inverted
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

lv_display_t *hal_lv_display(void) { return s_disp; }
