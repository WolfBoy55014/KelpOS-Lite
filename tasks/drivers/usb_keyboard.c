//
// Created by wolfboy on 11/26/2025.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "usb_keyboard.h"

#include "scheduler.h"
#include "bsp/board_api.h"
#include "tusb.h"

extern void hid_app_task(void);

void kelp_usb_keyboard(uint32_t pid) {
    board_init();

    // init host stack on configured roothub port
    tuh_init(BOARD_TUH_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    while (1) {
        // tinyusb host task
        tuh_task();

        hid_app_task();
        task_yield();
    }
}