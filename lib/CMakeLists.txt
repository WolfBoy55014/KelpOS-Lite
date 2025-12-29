# ============================================================
# COM CHANNEL PROTOCOL LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(lib_com_protocol INTERFACE)

target_sources(lib_com_protocol INTERFACE
        com_channel_protocol.c
)

target_include_directories(lib_com_protocol INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(lib_com_protocol INTERFACE
        pico_stdlib
        rp2040_kernel
)