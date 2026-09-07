idf_component_register(SRCS "net.c" "settings.c" "ids.c" INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_netif nvs_flash)
