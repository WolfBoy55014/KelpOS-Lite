//
// Created by hopli on 7/10/2026.
//

#ifndef KELPOS_LITE_USB_MSC_DRIVER_H
#define KELPOS_LITE_USB_MSC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "tusb_config.h"
#include "class/hid/hid.h"

#define USB_HOST_DRIVER_PID 103

#define USB_MSC_MAX_DEVICES CFG_TUH_MSC
#define USB_HID_MAX_KEYBOARDS CFG_TUH_HID

// ----------- MSC -----------+

struct msc_driver_details_t {
    bool initialized;
    volatile bool busy;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t size;
};

typedef enum {
    MSC_REQ_IDLE = 0,
    MSC_REQ_READ_PENDING,
    MSC_REQ_WRITE_PENDING,
} msc_req_state_t;

typedef struct {
    msc_req_state_t state;
    uint16_t channel_id;
    uint8_t* buffer;
    uint32_t buffer_size;
    bool success;
} msc_inflight_request_t;

// ----------- HID -----------+

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

void kelp_task_usb_host_driver(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_USB_MSC_DRIVER_H
