//
// Created by wolfboy on 11/26/2025.
//

#include <string.h>

#include "usb_hid_driver.h"

#include "scheduler.h"
#include "text_service.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "host/usbh.h"

static uint8_t const keycode2ascii[128][2] = {HID_KEYCODE_TO_ASCII};

static struct usb_keyboard_t usb_keyboards[USB_HID_MAX_KEYBOARDS];

static void process_kbd_report(struct usb_keyboard_t* usb_keyboard, hid_keyboard_report_t const* report);

void kelp_task_usb_hid(uint32_t pid, uint32_t* signals, char* args) {
    board_init();
    // TODO: get this to stop messing with the LED

    // init host stack on configured roothub port
    tuh_init(BOARD_TUH_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    uint32_t l = 0;

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                tuh_deinit(BOARD_TUH_RHPORT);
                return;
            }
        }

        // check for and process callbacks
        tuh_task();

        l++;

        task_sleep_ms(2);
    }
}

// tinyUSB stuff vvv

// all callbacks are called in the same context as `tuh_task()`, so this *is* a part of this task

// invoked when hid device is connected
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        for (uint8_t k = 0; k < USB_HID_MAX_KEYBOARDS; k++) {
            struct usb_keyboard_t* usb_keyboard = &usb_keyboards[k];
            if (!usb_keyboard->device.connected) {
                usb_keyboard->device.connected = true;
                usb_keyboard->device.dev_addr = dev_addr;
                usb_keyboard->device.instance = instance;

                usb_keyboard->caps_lock = false;
                usb_keyboard->num_lock = false;
                usb_keyboard->scroll_lock = false;

                break;
            }
        }
    }

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        // uh oh, we can't request reports!
    }
}

// invoked when hid device is disconnected
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        for (uint8_t k = 0; k < USB_HID_MAX_KEYBOARDS; k++) {
            struct usb_keyboard_t* usb_keyboard = &usb_keyboards[k];
            if (usb_keyboard->device.connected &&
                usb_keyboard->device.dev_addr == dev_addr &&
                usb_keyboard->device.instance == instance) {

                // disconnect
                usb_keyboard->device.connected = false;
                usb_keyboard->device.dev_addr = 0;
                usb_keyboard->device.instance = 0;

                usb_keyboard->caps_lock = false;
                usb_keyboard->num_lock = false;
                usb_keyboard->scroll_lock = false;

                break;
            }
        }
    }
}

// invoked when hid sends a report
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) {
    case HID_ITF_PROTOCOL_KEYBOARD:

        for (uint8_t k = 0; k < USB_HID_MAX_KEYBOARDS; k++) {
            struct usb_keyboard_t* usb_keyboard = &usb_keyboards[k];

            if (usb_keyboard->device.connected &&
                usb_keyboard->device.dev_addr == dev_addr &&
                usb_keyboard->device.instance == instance) {
                process_kbd_report(usb_keyboard, (hid_keyboard_report_t const*)report);
                break;
            }
        }
        break;

    case HID_ITF_PROTOCOL_MOUSE:
        break;

    default:
        break;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        // uh oh, we can't request reports!
    }
}

// look up new key in previous keys
static bool find_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) return true;
    }

    return false;
}

static void update_kbd_leds(struct usb_keyboard_t* usb_keyboard) {
    static uint8_t leds = 0;

    leds = 0;
    leds |= usb_keyboard->caps_lock ? KEYBOARD_LED_CAPSLOCK : 0;
    leds |= usb_keyboard->num_lock ? KEYBOARD_LED_NUMLOCK : 0;
    leds |= usb_keyboard->scroll_lock ? KEYBOARD_LED_SCROLLLOCK : 0;

    tuh_hid_set_report(usb_keyboard->device.dev_addr, usb_keyboard->device.instance,
                       0,  // report_id (0 for boot keyboard)
                       HID_REPORT_TYPE_OUTPUT,
                       &leds, 1);
}

static void process_kbd_report(struct usb_keyboard_t* usb_keyboard, hid_keyboard_report_t const* report) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i]) {
            if (find_key_in_report(&usb_keyboard->last_report, report->keycode[i])) {
                // was in previous report, so it is being held down
            }
            else {
                // the key is newly pressed

                // handle special keys
                switch (report->keycode[i]) {
                case HID_KEY_CAPS_LOCK:
                    usb_keyboard->caps_lock = !usb_keyboard->caps_lock;
                    update_kbd_leds(usb_keyboard);
                    continue;

                case HID_KEY_NUM_LOCK:
                    usb_keyboard->num_lock = !usb_keyboard->num_lock;
                    update_kbd_leds(usb_keyboard);
                    continue;

                case HID_KEY_SCROLL_LOCK:
                    usb_keyboard->scroll_lock = !usb_keyboard->scroll_lock;
                    update_kbd_leds(usb_keyboard);
                    continue;
                }

                bool is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);

                // reverse case if caps lock is on
                if (usb_keyboard->caps_lock) {
                    is_shift = !is_shift;
                }

                uint8_t ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
                kelp_text_send_input_char(ch);
            }
        }
    }

    usb_keyboard->last_report = *report;
}
