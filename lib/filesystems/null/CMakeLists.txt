# ============================================================
# NULLFS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(nullfs INTERFACE)

target_sources(nullfs INTERFACE
        nullfs.c
)

target_include_directories(nullfs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(nullfs INTERFACE
        pico_stdlib
)