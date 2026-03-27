# ============================================================
# COREMARK APP LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(app_coremark INTERFACE)

target_sources(app_coremark INTERFACE
        core_portme.c
        core_list_join.c
        core_main.c
        core_matrix.c
        core_state.c
        core_util.c
        ee_printf.c
        cvt.c
)

target_include_directories(app_coremark INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(app_coremark INTERFACE
        pico_stdlib
        tasks_service
)