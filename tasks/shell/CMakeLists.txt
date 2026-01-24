# ============================================================
# MICROSHELL LIBRARY TARGET
# This can be linked by external projects
# microshell Copyright (c) 2021 Marcin Borowicz <marcinbor85@gmail.com>
# Huge thanks to Marcin for this amazing project
# ============================================================

add_library(shellminator INTERFACE)

target_sources(shellminator INTERFACE

)

target_include_directories(shellminator INTERFACE

)

target_link_libraries(shellminator INTERFACE
        pico_stdlib
)