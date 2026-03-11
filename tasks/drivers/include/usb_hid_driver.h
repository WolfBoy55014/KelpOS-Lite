//
// Created by wolfboy on 11/26/2025.
//

#ifndef KELPOS_LITE_USB_KEYBOARD_H
#define KELPOS_LITE_USB_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "class/hid/hid.h"

#define USB_HID_DRIVER_PID 100

#define USB_HID_MAX_KEYBOARDS 2

struct usb_hid_device_t {
    bool connected;
    uint8_t dev_addr;
    uint8_t instance;
};

struct usb_keyboard_t {
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
    struct usb_hid_device_t device;
    hid_keyboard_report_t last_report;
};

// driver Task
void kelp_task_usb_hid(uint32_t pid, uint32_t signals, char* args);

#endif //KELPOS_LITE_USB_KEYBOARD_H