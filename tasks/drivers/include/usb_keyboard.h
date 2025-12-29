//
// Created by wolfboy on 11/26/2025.
//

#ifndef KELPOS_LITE_USB_KEYBOARD_H
#define KELPOS_LITE_USB_KEYBOARD_H

#include "class/hid/hid.h"
#include "pico/stdlib.h"

// Keyboard modifier keys (bitmask)
#define KEYBOARD_MODIFIER_LEFTCTRL   0x01
#define KEYBOARD_MODIFIER_LEFTSHIFT  0x02
#define KEYBOARD_MODIFIER_LEFTALT    0x04
#define KEYBOARD_MODIFIER_LEFTGUI    0x08
#define KEYBOARD_MODIFIER_RIGHTCTRL  0x10
#define KEYBOARD_MODIFIER_RIGHTSHIFT 0x20
#define KEYBOARD_MODIFIER_RIGHTALT   0x40
#define KEYBOARD_MODIFIER_RIGHTGUI   0x80

// HID keyboard report structure
// typedef struct {
//     uint8_t modifiers;
//     uint8_t reserved;
//     uint8_t keycodes[6];
// } hid_keyboard_report_t;

// Callback function types
typedef void (*keyboard_key_callback_t)(uint8_t keycode, bool pressed);
typedef void (*keyboard_report_callback_t)(hid_keyboard_report_t *report);

// Initialize the USB keyboard host
bool usb_keyboard_host_init(void);

// Set callback for individual key press/release events
void usb_keyboard_set_key_callback(keyboard_key_callback_t callback);

// Set callback for raw HID reports
void usb_keyboard_set_report_callback(keyboard_report_callback_t callback);

// Check if a modifier key is pressed
bool usb_keyboard_modifier_pressed(uint8_t modifier);

// Get the current keyboard report
const hid_keyboard_report_t* usb_keyboard_get_report(void);

// Convert HID keycode to ASCII character (considering modifiers)
char usb_keyboard_keycode_to_ascii(uint8_t keycode, uint8_t modifiers);

// Task function - call this regularly in your main loop
void usb_keyboard_task(void);

// Driver Task
void kelp_usb_keyboard(uint32_t pid);

#endif //KELPOS_LITE_USB_KEYBOARD_H