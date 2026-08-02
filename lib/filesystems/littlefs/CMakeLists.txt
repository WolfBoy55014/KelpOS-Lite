# ============================================================
# LITTLE FS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(littlefs INTERFACE)

target_sources(littlefs INTERFACE
        littlefs.c
        lfs2/lfs.c
        lfs2/lfs_util.c
)

target_include_directories(littlefs INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lfs2
)

target_link_libraries(littlefs INTERFACE
        pico_stdlib
)