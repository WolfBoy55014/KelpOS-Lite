# ============================================================
# SERVICE TASKS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(tasks_service INTERFACE)

target_sources(tasks_service INTERFACE
        text_service.c
)

target_include_directories(tasks_service INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(tasks_service INTERFACE
        pico_stdlib
)