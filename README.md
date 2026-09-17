# knobos

Своя прошивка (ESP-IDF) для круглой крутилки VIEWE. Поддерживаются две платы
(см. «Сборка под платы»): ESP32-C3 UEDX24240013-MD50E (готово) и
ESP32-S3 UEDX46460015-MD50E (AMOLED, дисплей пока стаб). Полная
спецификация — [`docs/spec.md`](docs/spec.md).

## Сборка под платы

Каждая плата собирается в свою папку сборки со своим набором
`sdkconfig.defaults.<board>` (пины, таргет, партиции, флеш):

```bash
. ~/esp/esp-idf/export.sh
idf.py -B build.c3 -DSDKCONFIG=build.c3/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.c3" build
idf.py -B build.s3 -DSDKCONFIG=build.s3/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.s3" build
```

- C3 — VIEWE UEDX24240013-MD50E, GC9A01 240×240, 4MB flash, без PSRAM.
- S3 — VIEWE UEDX46460015-MD50E, AMOLED CO5300/SH8601 466×466 QSPI, тач
  CST820, 16MB flash, 8MB Octal PSRAM. Дисплей — стаб (`ESP_ERR_NOT_SUPPORTED`)
  до отдельной задачи с реализацией.

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
