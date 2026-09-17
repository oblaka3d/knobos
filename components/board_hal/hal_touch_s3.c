#include "hal.h"
#include "board_pins.h"
#include "driver/i2c_master.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port_touch.h"

// ESP_LOGD компилируется только если LOG_LOCAL_LEVEL >= DEBUG на этапе сборки
// (в sdkconfig.defaults.s3 CONFIG_LOG_MAXIMUM_LEVEL=INFO вырезает ESP_LOGD из
// бинаря целиком, esp_log_level_set() runtime-порог этого не обходит) —
// поднимаем локально для файла тем же приёмом, что esp_lcd_panel_io_i2c_v2.c
// (CONFIG_LCD_ENABLE_DEBUG_LOG), чтобы esp_log_level_set(TAG, ESP_LOG_DEBUG)
// ниже реально включал координаты в логе без пересборки.
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

// Тач CST820 на S3 (VIEWE UEDX46460015-MD50E) как LVGL-указатель — UX не
// меняется (экраны без кликабельных объектов), это только приёмочная
// проверка живости сенсора. Драйвер и провенанс — components/esp_lcd_touch/
// (esp_lcd_touch.c/esp_lcd_touch_cst816s.c), см. там подробный комментарий.

// Видимого отдельного разрешения тача (своей координатной матрицы) ни в
// текущем, ни в старом board-конфиге VIEWE не найдено — берём разрешение
// панели (см. LCD_HRES/LCD_VRES в hal_display_s3.c). Заметка: x_max/y_max
// используются esp_lcd_touch только в SW-зеркалировании (x = x_max - x) —
// у нас flags.mirror_x/y/swap_xy = 0, так что при флипе/зеркале осей на
// приёмке править нужно flags здесь, а не эту константу.
#define TOUCH_RESOLUTION 466
#define TOUCH_I2C_PROBE_TIMEOUT_MS 100

static const char *TAG = "touch";

// process_coordinates зовётся из esp_lcd_touch_get_data() только когда
// реально было касание (touched=true) — ESP_LOGD, приёмка включает через
// esp_log_level_set ниже, без пересборки.
static void touch_log_coords(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength,
                              uint8_t *point_num, uint8_t max_point_num) {
    (void)tp;
    (void)strength;
    (void)max_point_num;
    if (*point_num) {
        ESP_LOGD(TAG, "x=%u y=%u", x[0], y[0]);
    }
}

esp_err_t hal_touch_init(void) {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = PIN_TOUCH_SDA,
        .scl_io_num = PIN_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    i2c_master_bus_handle_t bus;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init failed (SDA=%d SCL=%d): %s", PIN_TOUCH_SDA, PIN_TOUCH_SCL, esp_err_to_name(err));
        return err;
    }

    // ACK-проба независимо от RST/INT — до создания esp_lcd_touch (который
    // сразу дёргает reset() и read_id()); отдельная точка данных для разбора
    // T1-ноты про расхождение RST=2/INT=4 vs -1/-1 в разных версиях либы VIEWE.
    err = i2c_master_probe(bus, TOUCH_CST820_I2C_ADDR, TOUCH_I2C_PROBE_TIMEOUT_MS);
    ESP_LOGI(TAG, "i2c probe addr=0x%02X: %s", TOUCH_CST820_I2C_ADDR, esp_err_to_name(err));

    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    io_cfg.dev_addr = TOUCH_CST820_I2C_ADDR;
    esp_lcd_panel_io_handle_t io;
    err = esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel io i2c failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_touch_config_t touch_cfg = {
        .x_max = TOUCH_RESOLUTION,
        .y_max = TOUCH_RESOLUTION,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .process_coordinates = touch_log_coords,
    };
    esp_lcd_touch_handle_t tp;
    err = esp_lcd_touch_new_i2c_cst816s(io, &touch_cfg, &tp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CST820 init failed (RST=%d INT=%d): %s", (int)PIN_TOUCH_RST, (int)PIN_TOUCH_INT,
                 esp_err_to_name(err));
        return err;
    }

    lvgl_port_touch_cfg_t lv_touch_cfg = { .disp = hal_lv_display(), .handle = tp };
    lv_indev_t *indev = lvgl_port_add_touch(&lv_touch_cfg);
    if (!indev) {
        ESP_LOGE(TAG, "lvgl_port_add_touch failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "touch CST820 ready (addr 0x%02X, RST=%d INT=%d)", TOUCH_CST820_I2C_ADDR, (int)PIN_TOUCH_RST,
             (int)PIN_TOUCH_INT);
    return ESP_OK;
}
