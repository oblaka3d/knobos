idf_component_register(SRCS "carousel.c" "ui.c" "screen_clock.c" "screen_onboarding.c" INCLUDE_DIRS "include"
                       REQUIRES board_hal esp_lvgl_port core)
