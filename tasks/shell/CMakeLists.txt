# ============================================================
# SHELL LIBRARY TARGET
# This can be linked by external projects
# microshell Copyright (c) 2021 Marcin Borowicz <marcinbor85@gmail.com>
# Huge thanks to Marcin for this amazing project
# ============================================================

add_subdirectory(microshell/src)

add_library(shell INTERFACE)

target_sources(shell INTERFACE
        ${USH_SRC_FILES}
        shell.c
        kelp_commands.c
)

target_include_directories(shell INTERFACE
        ${USH_INC_DIRS}
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(shell INTERFACE
        pico_stdlib
        tasks_service
        rp2040_kernel
)