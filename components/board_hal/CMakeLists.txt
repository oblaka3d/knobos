if(CONFIG_KNOBOS_BOARD_S3)
    set(board_hal_display_src "hal_display_s3.c" "co5300/esp_lcd_co5300.c")
    set(board_hal_display_inc "co5300")
    set(board_hal_touch_src "hal_touch_s3.c")
    # esp_lcd_touch — vendored local component (components/esp_lcd_touch/), не
    # реестровая зависимость: esp_lvgl_port подхватывает её по имени компонента,
    # см. комментарий в components/esp_lcd_touch/CMakeLists.txt.
    set(board_hal_touch_req "esp_lcd_touch")
else()
    set(board_hal_display_src "hal_display_c3.c")
    set(board_hal_display_inc "")
    set(board_hal_touch_src "hal_touch_none.c")
    set(board_hal_touch_req "")
endif()

idf_component_register(SRCS ${board_hal_display_src} ${board_hal_touch_src} "hal_input.c" INCLUDE_DIRS "include" ${board_hal_display_inc}
                       REQUIRES driver esp_lcd esp_lcd_gc9a01 esp_lvgl_port lvgl knob button ${board_hal_touch_req})

if(CONFIG_KNOBOS_BOARD_S3)
    # PRIV_REQUIRES esp_psram у idf_component_register молча теряется для компонентов
    # с idf_component.yml (наблюдалось на IDF 5.3.2) — линкуем алиас напрямую, это
    # подтягивает esp_psram.h (нужен для esp_psram_get_size() в логе heap/PSRAM).
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_psram)
    # Тот же баг: REQUIRES esp_lcd_touch (Task 4) — локальный (не managed) компонент,
    # не упомянутый в idf_component.yml этого компонента — сам факт присутствия в
    # REQUIRES собирает esp_lcd_touch и заносит его в build_components (это видит
    # esp_lvgl_port и компилирует esp_lvgl_port_touch.c), но INCLUDE_DIRS от него
    # молча не долетают до hal_touch_s3.c ("No such file: esp_lcd_touch_cst816s.h").
    # Обходится так же — прямой линк алиаса.
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_lcd_touch)
endif()
