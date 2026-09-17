# Task 4 (M2.5, knobos): component name must be exactly "esp_lcd_touch" (the
# directory name) — esp_lvgl_port's CMakeLists.txt checks build_components for
# the literal name "esp_lcd_touch" (or "espressif__esp_lcd_touch" for the
# managed variant) to decide whether to compile esp_lvgl_port_touch.c /
# lvgl_port_add_touch. See esp_lcd_touch.c for provenance.
#
# CAUTION: if the component registry becomes reachable again and someone adds
# "espressif/esp_lcd_touch" as a managed dependency (idf_component.yml) instead
# of removing this local component first, esp_lvgl_port's CMakeLists.txt
# matches BOTH names and would append esp_lvgl_port_touch.c twice → duplicate
# symbols at link time. Remove this directory before adding the managed dep.
idf_component_register(SRCS "esp_lcd_touch.c" "esp_lcd_touch_cst816s.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_lcd)
