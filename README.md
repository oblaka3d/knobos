# knobos

Своя прошивка (ESP-IDF) для круглой крутилки VIEWE UEDX24240013-MD50E
(ESP32-C3, 4MB flash, без PSRAM). Полная спецификация — [`docs/spec.md`](docs/spec.md).

## Сборка

```bash
. ~/esp/esp-idf/export.sh
idf.py set-target esp32c3
idf.py build
```

## Прошивка

```bash
idf.py -p /dev/cu.usbmodem21301 flash monitor
```

Выход из монитора — `Ctrl+]`.

## Партиции

Разметка flash — `partitions.csv`, одна `factory`-секция (OTA-слоты
появятся в M4). Менять только осознанно, чтобы не сдвинуть адреса.
