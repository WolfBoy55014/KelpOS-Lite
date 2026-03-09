//
// Created by wolfboy on 11/26/2025.
//

#ifndef KELPOS_LITE_USB_KEYBOARD_H
#define KELPOS_LITE_USB_KEYBOARD_H

#include <stdint.h>

#define USB_HID_DRIVER_PID 100

// driver Task
void kelp_task_usb_hid(uint32_t pid);

#endif //KELPOS_LITE_USB_KEYBOARD_H