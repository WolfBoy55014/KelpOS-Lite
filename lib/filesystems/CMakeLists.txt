add_subdirectory(null/)
add_subdirectory(littlefs/)
add_subdirectory(fatfs/)

# ============================================================
# FILESYSTEMS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(lib_filesystems INTERFACE)

target_link_libraries(lib_filesystems INTERFACE
        littlefs
        fatfs
        nullfs
        pico_stdlib
)