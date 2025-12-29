# ============================================================
# USB KEYBOARD DRIVER LIBRARY TARGET
# This can be linked by external projects
# ============================================================

add_library(driver_usb_keyboard INTERFACE)

target_sources(driver_usb_keyboard INTERFACE
        usb_keyboard.c
        hid_app.c
)

target_include_directories(driver_usb_keyboard INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(driver_usb_keyboard INTERFACE
        pico_stdlib
        tinyusb_host
        tinyusb_board
)