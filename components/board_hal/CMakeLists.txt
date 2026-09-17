if(CONFIG_KNOBOS_BOARD_S3)
    set(board_hal_display_src "hal_display_s3.c")
else()
    set(board_hal_display_src "hal_display_c3.c")
endif()

idf_component_register(SRCS ${board_hal_display_src} "hal_input.c" INCLUDE_DIRS "include"
                       REQUIRES driver esp_lcd esp_lcd_gc9a01 esp_lvgl_port lvgl knob button)
