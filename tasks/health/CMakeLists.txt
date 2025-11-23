# ============================================================
# HEALTH TASKS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(tasks_health INTERFACE)

target_sources(tasks_health INTERFACE
        governor.c
)

target_include_directories(tasks_health INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(tasks_health INTERFACE
        pico_stdlib
        hardware_clocks
)