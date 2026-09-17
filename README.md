# knobos

Своя прошивка (ESP-IDF) для круглой крутилки VIEWE. Поддерживаются две платы
(см. «Сборка под платы»): ESP32-C3 UEDX24240013-MD50E (готово) и
ESP32-S3 UEDX46460015-MD50E (AMOLED, дисплей пока стаб). Полная
спецификация — [`docs/spec.md`](docs/spec.md).

## Платы

Поддерживаются две платы VIEWE с круглым дисплеем:

| Параметр | C3 | S3 |
|----------|----|----|
| **Модель** | UEDX24240013-MD50E | UEDX46460015-MD50E |
| **Дисплей** | GC9A01, 240×240, SPI | AMOLED CO5300, 466×466, QSPI |
| **Flash** | 4MB | 16MB |
| **PSRAM** | нет | 8MB Octal |
| **Тачскрин** | нет | CST820 |
| **Статус** | готово | стаб (дисплей) |

### Команды сборки

Каждая плата собирается в свою папку со своим набором `sdkconfig.defaults.<board>`:

```bash
. ~/esp/esp-idf/export.sh
idf.py -B build.c3 -DSDKCONFIG=build.c3/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.c3" build
idf.py -B build.s3 -DSDKCONFIG=build.s3/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.s3" build
```

### Яркость дисплея

На **C3** яркость регулируется через LEDC (PWM импульс):
```c
// hal_display_c3.c
esp_lcd_panel_disp_on_off(panel_handle, brightness != 0);
ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness);
```

На **S3** яркость устанавливается командой дисплея 0x51:
```c
// board_ui.c
uint8_t brightness_cmd[] = {0x51, brightness};
esp_lcd_panel_io_tx_param(io_handle, 0x51, brightness);
```

## Прошивка

```bash
idf.py -B build.c3 -p /dev/cu.usbmodem21301 flash monitor
```

(для S3 — `-B build.s3`). Перед прошивкой сверяйте плату: `esptool --port
/dev/cu.usbmodem21301 chip-id` — на столе может быть воткнута любая из двух.
Выход из монитора — `Ctrl+]`.

## Партиции

Разметка flash — `partitions.csv` (C3) / `partitions_s3.csv` (S3), у каждой
одна `factory`-секция (OTA-слоты появятся в M4). Менять только осознанно,
чтобы не сдвинуть адреса.

## Регенерация шрифтов

На S3 (466×466) UI использует крупные шрифты Montserrat, сгенерённые из TTF
через `lv_font_conv` (`components/ui/fonts/font_m32.c` / `font_m40.c` /
`font_m56.c` / `font_m96.c`, подключаются только при `CONFIG_KNOBOS_BOARD_S3`
— см. `components/ui/CMakeLists.txt` и `components/ui/include/board_ui.h`).
На C3 (240×240) те же роли (`UI_FONT_XL/L/M/S`) занимают встроенные
`lv_font_montserrat_*` из Kconfig (`sdkconfig.defaults.c3`).

Сгенерённые `.c` коммитятся в репозиторий; исходный TTF — нет. Регенерация
нужна только при смене набора размеров/диапазона символов:

```bash
tools/gen_fonts.sh   # требует node/npm (npx); скачивает Montserrat-Regular.ttf во временный каталог
```

## Первое включение

На чистом устройстве (пустой NVS) на экране появляется онбординг с QR-кодом.
Отсканируйте его телефоном — он подключится к точке доступа устройства
(`Knob-XXXX`), и должна открыться страница мастера (или откройте
`http://192.168.4.1/` вручную). В мастере: скан сетей → выбор своей WiFi →
пароль → «Готово». Устройство перезагрузится и подключится к домашней сети;
на экране появятся часы.

После первого подключения к домашней сети веб-морда устройства доступна по
его IP (порт 80, basic auth `admin` / пароль AP с экрана онбординга):

```bash
curl -su admin:<пароль> http://<ip>/api/status
```

## Сброс WiFi

Если устройство подключено не к той сети или креды устарели — включите
питание и в течение 2 секунд зажмите кнопку; держите 3 секунды, пока в
режиме онбординга не появится экран настройки WiFi. Отпустить кнопку
раньше — сброса не будет, загрузка продолжится как обычно.
