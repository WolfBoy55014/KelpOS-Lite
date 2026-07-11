# ============================================================
# DRIVER TASKS LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_subdirectory(sd_card/src)

add_library(tasks_driver INTERFACE)

target_sources(tasks_driver INTERFACE
        usb_hid_driver.c
        usb_msc_driver.c
        serial_driver.c
        hw_config.c
        sd_card_driver.c
)

target_include_directories(tasks_driver INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(tasks_driver INTERFACE
        pico_stdlib
        tasks_service
        tinyusb_host
        tinyusb_board
        Kelp-SD-SDIO-SPI
)