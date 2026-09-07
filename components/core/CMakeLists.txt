idf_component_register(SRCS "net.c" "settings.c" "ids.c" "wifi_sm.c" "wifi_mgr.c" INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_netif nvs_flash esp_timer)
