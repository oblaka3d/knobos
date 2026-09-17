#ifndef KNOBOS_BOARD_UI_H
#define KNOBOS_BOARD_UI_H

// UI-метрики per board: масштаб координат макета (UI_S) и шрифты (UI_FONT_*).
// Макеты экранов (screen_clock.c, screen_onboarding.c, ...) считаны в "единицах
// C3" (240x240) и масштабируются в единицы платы через UI_S(); шрифты берутся
// из UI_FONT_XL/L/M/S вместо прямых lv_font_montserrat_* / font_m*.

#include "lvgl.h"

#if CONFIG_KNOBOS_BOARD_C3

#define UI_SCALE_NUM 1
#define UI_SCALE_DEN 1

#define UI_FONT_XL (&lv_font_montserrat_48)
#define UI_FONT_L  (&lv_font_montserrat_28)
#define UI_FONT_M  (&lv_font_montserrat_20)
#define UI_FONT_S  (&lv_font_montserrat_16)

#elif CONFIG_KNOBOS_BOARD_S3

#include "fonts.h"

// 466 / 240 = 1.9416... округлено до 194/100.
#define UI_SCALE_NUM 194
#define UI_SCALE_DEN 100

#define UI_FONT_XL (&font_m96)
#define UI_FONT_L  (&font_m56)
#define UI_FONT_M  (&font_m40)
#define UI_FONT_S  (&font_m32)

#else
#error "board_ui.h: CONFIG_KNOBOS_BOARD_C3 или CONFIG_KNOBOS_BOARD_S3 должен быть определён"
#endif

#define UI_S(px) (((px) * UI_SCALE_NUM) / UI_SCALE_DEN)

#endif
