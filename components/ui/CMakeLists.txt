set(ui_srcs "carousel.c" "ui.c" "screen_clock.c" "screen_onboarding.c")
if(CONFIG_KNOBOS_BOARD_S3)
    # Крупные шрифты (lv_font_conv) нужны только на S3 (466x466); на C3 не
    # линкуются, чтобы не расти в размере (см. board_ui.h).
    list(APPEND ui_srcs "fonts/font_m32.c" "fonts/font_m40.c" "fonts/font_m56.c" "fonts/font_m96.c")
endif()

idf_component_register(SRCS ${ui_srcs} INCLUDE_DIRS "include" "fonts"
                       REQUIRES board_hal esp_lvgl_port core)

if(CONFIG_KNOBOS_BOARD_S3)
    # lv_font_conv генерирует `#include "lvgl.h"` только при LV_LVGL_H_INCLUDE_SIMPLE;
    # проект вместо этого определяет LV_CONF_INCLUDE_SIMPLE (для lv_conf.h), поэтому
    # без этого define font_m*.c падает на `#include "lvgl/lvgl.h"`.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
endif()
