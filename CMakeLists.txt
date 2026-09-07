cmake_minimum_required(VERSION 3.16)
# dns_server живёт вложенным в components/web/ (по брифу Task 4) — components/* сканируются
# не рекурсивно, поэтому регистрируем его отдельным компонентом явно.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/web/dns_server")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(knobos)
