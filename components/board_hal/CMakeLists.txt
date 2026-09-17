if(CONFIG_KNOBOS_BOARD_S3)
    set(board_hal_display_src "hal_display_s3.c" "co5300/esp_lcd_co5300.c")
    set(board_hal_display_inc "co5300")
else()
    set(board_hal_display_src "hal_display_c3.c")
    set(board_hal_display_inc "")
endif()

idf_component_register(SRCS ${board_hal_display_src} "hal_input.c" INCLUDE_DIRS "include" ${board_hal_display_inc}
                       REQUIRES driver esp_lcd esp_lcd_gc9a01 esp_lvgl_port lvgl knob button)

if(CONFIG_KNOBOS_BOARD_S3)
    # PRIV_REQUIRES esp_psram у idf_component_register молча теряется для компонентов
    # с idf_component.yml (наблюдалось на IDF 5.3.2) — линкуем алиас напрямую, это
    # подтягивает esp_psram.h (нужен для esp_psram_get_size() в логе heap/PSRAM).
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_psram)
endif()
