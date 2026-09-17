#ifndef KNOBOS_UI_FONTS_H
#define KNOBOS_UI_FONTS_H

// Шрифты Montserrat для S3 (466x466), сгенерированы tools/gen_fonts.sh
// (lv_font_conv) из Montserrat-Regular.ttf, диапазон 0x20-0x7F,0xB0, bpp4.
// Линкуются только при CONFIG_KNOBOS_BOARD_S3 (components/ui/CMakeLists.txt).

#include "lvgl.h"

extern const lv_font_t font_m32;
extern const lv_font_t font_m40;
extern const lv_font_t font_m56;
extern const lv_font_t font_m96;

#endif
