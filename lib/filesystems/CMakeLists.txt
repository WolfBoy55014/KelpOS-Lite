add_subdirectory(null/)
add_subdirectory(littlefs/)

# ============================================================
# FILESYSTEMS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(lib_filesystems INTERFACE)

target_link_libraries(lib_filesystems INTERFACE
        littlefs
        nullfs
        pico_stdlib
)