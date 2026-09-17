#include "hal.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_co5300.h"
#include "esp_lvgl_port.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

// S3: AMOLED CO5300 466x466 по QSPI (VIEWE UEDX46460015-MD50E). Пины — boards/s3.h.
// Init-последовательность и геометрия — из официального клона VIEWE
// (git clone --depth 1 https://github.com/VIEWESMART/UEDX46460015-MD50ESP32-1.5inch-Touch-Knob-Display),
// файл Libraries/ESP32_Display_Panel/src/board/supported/viewe/BOARD_VIEWE_UEDX46460015_MD50ET.h
// (макросы ESP_PANEL_BOARD_LCD_*). Сам esp_lcd-драйвер — vendored, см. co5300/esp_lcd_co5300.*
// (реестр компонентов был недоступен с этой машины на момент реализации — см. отчёт).

#define LCD_HRES 466
#define LCD_VRES 466
// Бриф просил .buffer_size = 466*100; на реальном железе это отказывало с
// ESP_ERR_NO_MEM ровно в момент esp_wifi_init()/esp_wifi_start() (см. подробный
// разбор в co5300/esp_lcd_co5300.c рядом с CO5300_IO_RETRY_MAX) — GP-SPI на
// каждую QSPI-цветовую транзакцию из PSRAM аллоцирует временный internal-RAM
// bounce-буфер под весь чанк (466*100*2 = ~91КБ), а именно в этот момент
// internal RAM активно расходует стек WiFi. Уменьшено до 20 строк (~18КБ на
// транзакцию) — проверено на устройстве, свежих сбоев не было за >40с работы
// с поднятым Wi-Fi AP. Буферы по-прежнему в PSRAM (buff_spiram=true) — уменьшился
// только размер одного чанка, а не факт вынесения в PSRAM.
#define LCD_BUF_LINES 20
#define LCD_QSPI_HOST SPI2_HOST

// Отдельная константа (не просто LCD_HRES*LCD_BUF_LINES*2 инлайном в spi_bus_config_t)
// специально для того, чтобы предупреждение было видно рядом с местом использования:
// это одновременно и предел одной QSPI-транзакции, и, из-за ESP_ERR_NO_MEM выше,
// верхняя граница internal-RAM bounce-буфера, который esp_driver_spi аллоцирует
// НА КАЖДУЮ цветовую транзакцию из PSRAM. Не увеличивать «ради производительности»
// без переоценки internal-RAM запаса рядом с esp_wifi_init() — см. LCD_BUF_LINES.
#define LCD_SPI_MAX_TRANSFER_BYTES (LCD_HRES * LCD_BUF_LINES * sizeof(uint16_t))

// Физическая GDDRAM панели шире видимой области (472 колонки по борд-конфигу VIEWE
// против 466 видимых) — véndor-офсет колонки, применяется на каждый draw_bitmap
// драйвером (co5300->x_gap): 6 + 466 = 472. Строки без офсета (y_gap=0).
#define LCD_X_GAP 6
#define LCD_Y_GAP 0

static const char *TAG = "hal_display_s3";
static lv_display_t *s_disp;
static esp_lcd_panel_io_handle_t s_io;

// Vendor init-последовательность — дословно из BOARD_VIEWE_UEDX46460015_MD50ET.h
// (ESP_PANEL_BOARD_LCD_VENDOR_INIT_CMD), не сочинялась. 0x51 здесь выставляет 0xFF
// (максимум) при инициализации панели — реальная стартовая яркость (80%) выставляется
// отдельно через hal_backlight_set() после старта, как и на C3.
static const co5300_lcd_init_cmd_t s_vendor_init_cmds[] = {
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 0, 10},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 10},
    {0x63, (uint8_t[]){0xFF}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x06, 0x01, 0xDD}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x11, (uint8_t[]){0x00}, 0, 60},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

esp_err_t hal_display_init(void) {
    // DISP_EN — цифровое питание AMOLED-панели, поднять ДО reset/init и держать в 1.
    gpio_config_t en_conf = {
        .pin_bit_mask = 1ULL << PIN_DISP_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&en_conf));
    ESP_ERROR_CHECK(gpio_set_level(PIN_DISP_EN, 1));

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_QSPI_SCLK,
        .data0_io_num = PIN_QSPI_D0,
        .data1_io_num = PIN_QSPI_D1,
        .data2_io_num = PIN_QSPI_D2,
        .data3_io_num = PIN_QSPI_D3,
        .max_transfer_sz = LCD_SPI_MAX_TRANSFER_BYTES,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_QSPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(PIN_QSPI_CS, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_QSPI_HOST, &io_cfg, &s_io));

    co5300_vendor_config_t vendor_cfg = {
        .init_cmds = s_vendor_init_cmds,
        .init_cmds_size = sizeof(s_vendor_init_cmds) / sizeof(s_vendor_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t pcfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // ESP_PANEL_BOARD_LCD_COLOR_BGR_ORDER=0 у VIEWE
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(s_io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, LCD_X_GAP, LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    // Стартовая яркость — до lvgl_port_init/add_disp: панель уже готова принимать
    // команды, а LVGL-таск ещё не поднят и не флашит первый кадр, так что tx_param
    // команды WRDISBV не может встрять между транзакциями первого QSPI-флаша.
    hal_backlight_set(80);

    const lvgl_port_cfg_t lv_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lv_cfg));
    const lvgl_port_display_cfg_t dcfg = {
        .io_handle = s_io, .panel_handle = panel,
        .buffer_size = LCD_HRES * LCD_BUF_LINES,
        .double_buffer = true,
        .hres = LCD_HRES, .vres = LCD_VRES,
        // Без mirror/swap_xy — VIEWE-конфиг для этой панели их не требует (в отличие
        // от C3/GC9A01); crутить, если картинка на реальном экране придёт кривой.
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_spiram = true, .swap_bytes = true },  // RGB565 big-endian на SPI/QSPI-панели, как на C3
    };
    s_disp = lvgl_port_add_disp(&dcfg);

    // internal_free — реальный запас, за который боремся с esp_wifi_init() (см.
    // ESP_ERR_NO_MEM разбор у LCD_BUF_LINES); heap_free суммирует internal+PSRAM
    // и сам по себе эту границу не показывает.
    ESP_LOGI(TAG, "heap_free=%lu internal_free=%lu psram_size=%u", (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)esp_psram_get_size());

    return s_disp ? ESP_OK : ESP_FAIL;
}

void hal_backlight_set(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint8_t value = (uint16_t)percent * 255 / 100;
    co5300_set_brightness(s_io, value);
}

lv_display_t *hal_lv_display(void) { return s_disp; }
