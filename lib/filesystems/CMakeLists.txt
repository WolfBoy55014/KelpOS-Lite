add_subdirectory(null/)

# ============================================================
# FILESYSTEMS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(lib_filesystems INTERFACE)

target_link_libraries(lib_filesystems INTERFACE
        nullfs
        pico_stdlib
)