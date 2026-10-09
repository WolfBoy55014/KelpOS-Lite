# ============================================================
# FATFS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(fatfs INTERFACE)

target_sources(fatfs INTERFACE
        diskio.c
        ff.c
        ffsystem.c
        ffunicode.c
        ff_kelp.c
)

target_include_directories(fatfs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(fatfs INTERFACE
        pico_stdlib
)