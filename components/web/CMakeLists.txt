idf_component_register(SRCS "web.c" INCLUDE_DIRS "include"
                       REQUIRES core dns_server json esp_http_server esp_wifi mbedtls esp_app_format)

# portal.html гзипуется на этапе сборки и встраивается как объект (Content-Encoding: gzip
# отдаётся как есть, без распаковки на устройстве). Источник portal.html держим рядом для правок.
set(portal_gz "${CMAKE_CURRENT_BINARY_DIR}/portal.html.gz")
add_custom_command(
    OUTPUT "${portal_gz}"
    COMMAND gzip -9 -n -c "${CMAKE_CURRENT_SOURCE_DIR}/portal.html" > "${portal_gz}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/portal.html"
    VERBATIM
)
target_add_binary_data(${COMPONENT_LIB} "${portal_gz}" BINARY DEPENDS "${portal_gz}")
