CONFIG_IDF_TARGET="esp32c3"
CONFIG_KNOBOS_BOARD_C3=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
# Шрифты UI_FONT_* (board_ui.h) для C3 — встроенные lv_font_montserrat_*;
# на S3 их место занимают сгенерённые font_m*.c (components/ui/fonts), эти
# Kconfig-опции там не нужны и не влияют на размер образа.
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
CONFIG_LV_FONT_MONTSERRAT_28=y
CONFIG_LV_FONT_MONTSERRAT_48=y
