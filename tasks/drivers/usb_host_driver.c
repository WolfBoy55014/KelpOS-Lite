//
// Created by wolfboy on 7/10/2026.
//


#include <stdlib.h>

#include "usb_host_driver.h"
#include "block_service.h"
#include "channel.h"
#include "com_channel_protocol.h"
#include "hw_config.h"
#include "scheduler.h"
#include "kernel_config.h"
#include "error_codes.h"
#include "text_service.h"
#include "tusb.h"
#include "bsp/board_api.h"

// ---------------------------+
// Static Variables
// ---------------------------+

// ----------- MSC -----------+

static int16_t device_id_2_msc_id[BLOCK_SERVICE_MAX_DEVICES];
static struct msc_driver_details_t msc_driver_details[USB_MSC_MAX_DEVICES];

static msc_inflight_request_t inflight_requests[USB_MSC_MAX_DEVICES];

// ----------- HID -----------+

static uint8_t const keycode2ascii[128][2] = {HID_KEYCODE_TO_ASCII};

static struct usb_keyboard_t usb_keyboards[USB_HID_MAX_KEYBOARDS];

// ---------------------------+
// Utility Functions
// ---------------------------+

// ----------- MSC -----------+

static bool kelp_msc_io_done(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data);

static int16_t msc_id_2_device_id(uint8_t msc_id) {
    for (uint8_t device_id = 0; device_id < BLOCK_SERVICE_MAX_DEVICES; device_id++) {
        uint8_t temp_msc_id = device_id_2_msc_id[device_id];
        if (temp_msc_id == msc_id) {
            return device_id;
        }
    }
    return -1;
}

static uint32_t msc_bound_count(uint8_t msc_id, uint32_t start, uint32_t count) {
    if (msc_id >= USB_MSC_MAX_DEVICES) {
        return 0;
    }
    struct msc_driver_details_t* details = &msc_driver_details[msc_id];

    // Clamp start to valid range
    if (start >= details->block_count) {
        return 0;
    }

    uint32_t end = start + count;
    if (end <= details->block_count) {
        return count;
    }

    return details->block_count - start;
}

// Extract a 32-bit big-endian value from a buffer
static inline uint32_t get_uint32_be(const char* data) {
    return ((uint8_t)data[0] << 24) | ((uint8_t)data[1] << 16) |
           ((uint8_t)data[2] << 8)  |  (uint8_t)data[3];
}

static kelp_error_t kelp_msc_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t device_id = data[0];
    if (device_id >= BLOCK_SERVICE_MAX_DEVICES) {
        return KELP_INVALID_ID;
    }
    int16_t msc_id = device_id_2_msc_id[device_id];
    if (msc_id < 0 || msc_id >= USB_MSC_MAX_DEVICES) {
        return KELP_INVALID_ID;
    }

    if (inflight_requests[msc_id].state != MSC_REQ_IDLE) {
        return KELP_BUSY;
    }

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    count = msc_bound_count(msc_id, start, count);
    if (count == 0 || count > UINT16_MAX) {
        return KELP_TOO_BIG;
    }
    uint32_t return_buffer_size = count * 512;

    uint8_t* buffer = malloc(return_buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    msc_inflight_request_t* request = &inflight_requests[msc_id];
    request->state = MSC_REQ_READ_PENDING;
    request->channel_id = channel_id;
    request->buffer = buffer;
    request->buffer_size = return_buffer_size;
    request->success = false;

    msc_driver_details[msc_id].busy = true;
    const uint8_t dev_addr = msc_id + 1;
    const uint8_t lun = 0;
    if (!tuh_msc_read10(dev_addr, lun, buffer, start, (uint16_t)count, kelp_msc_io_done, 0)) {
        free(buffer);
        request->state = MSC_REQ_IDLE;
        request->buffer = NULL;
        msc_driver_details[msc_id].busy = false;
        return KELP_IO;
    }

    return KELP_OK;
}

static kelp_error_t kelp_msc_handle_write_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t device_id = data[0];
    if (device_id >= BLOCK_SERVICE_MAX_DEVICES) {
        return KELP_INVALID_ID;
    }
    int16_t msc_id = device_id_2_msc_id[device_id];
    if (msc_id < 0 || msc_id >= USB_MSC_MAX_DEVICES) {
        return KELP_INVALID_ID;
    }

    if (inflight_requests[msc_id].state != MSC_REQ_IDLE) {
        return KELP_BUSY;
    }

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    count = msc_bound_count(msc_id, start, count);
    if (count == 0 || count > UINT16_MAX) {
        return KELP_TOO_BIG;
    }
    uint32_t buffer_size = count * 512;

    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    uint32_t received_buffer_size = 0;
    uint16_t reason;
    kelp_error_t error = com_get_char_array_blocking(channel_id, buffer, buffer_size, &received_buffer_size, &reason);
    if (error != KELP_OK) {
        free(buffer);
        return error;
    }
    if (reason != REASON_BLOCK_WRITE_BYTES) {
        free(buffer);
        return KELP_WRONG_REASON;
    }
    if (buffer_size != received_buffer_size) {
        free(buffer);
        return KELP_PROTOCOL;
    }

    msc_inflight_request_t* request = &inflight_requests[msc_id];
    request->state = MSC_REQ_WRITE_PENDING;
    request->channel_id = channel_id;
    request->buffer = buffer;
    request->buffer_size = buffer_size;
    request->success = false;

    msc_driver_details[msc_id].busy = true;
    const uint8_t dev_addr = msc_id + 1;
    const uint8_t lun = 0;
    if (!tuh_msc_write10(dev_addr, lun, buffer, start, (uint16_t)count, kelp_msc_io_done, 0)) {
        free(buffer);
        request->state = MSC_REQ_IDLE;
        request->buffer = NULL;
        msc_driver_details[msc_id].busy = false;
        return KELP_IO;
    }

    return KELP_OK;
}

static void kelp_msc_service_completions() {
    for (uint8_t msc_id = 0; msc_id < USB_MSC_MAX_DEVICES; msc_id++) {
        msc_inflight_request_t* req = &inflight_requests[msc_id];

        if (req->state == MSC_REQ_IDLE) {
            continue;
        }
        if (msc_driver_details[msc_id].busy) {
            // Still in flight - check again next iteration.
            continue;
        }

        if (req->state == MSC_REQ_READ_PENDING) {
            kelp_error_t error;
            if (req->success) {
                error = com_send_char_array_blocking(req->channel_id, req->buffer, req->buffer_size, REASON_BLOCK_READ_BYTES);
            } else {
                error = KELP_IO;
            }
            if (error != KELP_OK) {
                com_send_int32_blocking(req->channel_id, error, REASON_BLOCK_ERROR);
            }
        } else if (req->state == MSC_REQ_WRITE_PENDING) {
            kelp_error_t error;
            if (req->success) {
                error = com_send_uint32_blocking(req->channel_id, req->buffer_size, REASON_BLOCK_WRITE_BYTES);
            } else {
                error = KELP_IO;
            }
            if (error != KELP_OK) {
                com_send_int32_blocking(req->channel_id, error, REASON_BLOCK_ERROR);
            }
        }

        free(req->buffer);
        req->buffer = NULL;
        req->state = MSC_REQ_IDLE;
    }
}

// ----------- HID -----------+

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

// ---------------------------+
// Task Functions
// ---------------------------+

// ----------- MSC -----------+

static void kelp_init_msc_driver() {
    // initialize id mappings
    for (uint8_t id = 0; id < BLOCK_SERVICE_MAX_DEVICES; id++) {
        device_id_2_msc_id[id] = -1;
    }

    // set all usb drives to unmounted
    for (uint8_t id = 0; id < USB_MSC_MAX_DEVICES; id++) {
        msc_driver_details[id].initialized = false;
        inflight_requests[id].state = MSC_REQ_IDLE;
        inflight_requests[id].buffer = NULL;
    }
}

static void kelp_update_msc_driver() {
    // pick up any transfers that finished since the last iteration
    kelp_msc_service_completions();

    // get connected channels
    uint16_t channel_ids[NUM_CHANNELS];
    uint16_t num_connected = 0;
    kelp_error_t error = get_connected_channels(channel_ids, &num_connected, NUM_CHANNELS);

    if (error != KELP_OK) {
        return;
    }

    // check for messages
    for (uint32_t c = 0; c < num_connected; c++) {
        uint16_t channel_id = channel_ids[c];

        // can we read from this channel?
        if (is_channel_ready_to_read(channel_id)) {
            uint8_t content_type = 0;
            error = com_channel_peek(channel_id, &content_type);
            if (error != KELP_OK) {
                continue;
            }

            // does this channel contain an array?
            if (content_type == COM_TYPE_ARRAY) {
                // get the reason
                uint16_t reason = 0;

                char data[CHANNEL_SIZE];
                uint16_t size;

                error = com_get_char_array_fast(channel_id, &data, &size, &reason);
                if (error != KELP_OK) {
                    continue;
                }

                // handle block device operations
                switch (reason) {
                case REASON_BLOCK_READ_BYTES:
                    // send the requested bytes
                    error = kelp_msc_handle_read_request(channel_id, data, size);
                    if (error != KELP_OK) {
                        com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                    }
                    break;
                case REASON_BLOCK_WRITE_BYTES:
                    // write the provided bytes
                    error = kelp_msc_handle_write_request(channel_id, data, size);
                    if (error != KELP_OK) {
                        com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                    }
                    break;
                default: continue;
                }
            }
        }
    }
}

// ----------- HID -----------+

static void kelp_update_hid_driver() {
}

// ----------- GEN -----------+

void kelp_task_usb_host_driver(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(1024);

    // TODO: support PIO USB and MAX3421

    // init host stack on configured roothub port
    tuh_init(BOARD_TUH_RHPORT);
    // TODO: Make sure this doesn't happen twice

    kelp_init_msc_driver();

    uint32_t l = 0;

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                tuh_deinit(BOARD_TUH_RHPORT);
                return;
            }

            l = 0;
        }

        // check for and process callbacks
        tuh_task();

        kelp_update_hid_driver();
        kelp_update_msc_driver();

        l++;

        task_sleep_us(250);
    }
}

// ---------------------------+
// TinyUSB Callbacks
// ---------------------------+

// ----------- MSC -----------+

// define the buffer to be place in USB/DMA memory with correct alignment/cache line size
CFG_TUH_MEM_SECTION static struct {
    TUH_EPBUF_TYPE_DEF(scsi_inquiry_resp_t, inquiry);
} scsi_resp;

// Kept short per callback-context guidance: just record the result and
// clear busy. kelp_msc_service_completions() does the actual work
// (sending the response, freeing the buffer) from the main loop.
static bool kelp_msc_io_done(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data) {
    uint8_t msc_id = dev_addr - 1;
    struct msc_driver_details_t* details = &msc_driver_details[msc_id];
    msc_inflight_request_t* req = &inflight_requests[msc_id];
    req->success = (cb_data->csw->status == MSC_CSW_STATUS_PASSED);
    details->busy = false;
    return true;
}

static bool inquiry_complete_cb(uint8_t dev_addr, const tuh_msc_complete_data_t* cb_data) {
    const msc_cbw_t* cbw = cb_data->cbw;
    const msc_csw_t* csw = cb_data->csw;

    if (csw->status != 0) {
        // printf("Inquiry failed\r\n");
        return false;
    }

    // Get capacity of device
    const uint32_t block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
    const uint32_t block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

    uint8_t msc_id = dev_addr - 1;
    uint8_t device_id;
    kelp_error_t error = kelp_block_mount_device(&device_id, block_size, block_count);

    if (error == KELP_OK) {
        struct msc_driver_details_t* details = &msc_driver_details[msc_id];
        device_id_2_msc_id[device_id] = msc_id;
        details->initialized = true;
        details->busy = false;
        details->block_size = block_size;
        details->block_count = block_count;
        details->size = block_count * block_size;
    }

    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr) {
    const uint8_t lun = 0;
    tuh_msc_inquiry(dev_addr, lun, &scsi_resp.inquiry, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
    uint8_t msc_id = dev_addr - 1;

    // If a transfer was in flight on this device, it will never complete
    // now - free the buffer and tell the waiting channel rather than
    // leaving it hanging forever.
    msc_inflight_request_t* req = &inflight_requests[msc_id];
    if (req->state != MSC_REQ_IDLE) {
        com_send_int32_blocking(req->channel_id, KELP_IO, REASON_BLOCK_ERROR);
        free(req->buffer);
        req->buffer = NULL;
        req->state = MSC_REQ_IDLE;
    }
    msc_driver_details[msc_id].busy = false;

    int16_t device_id = msc_id_2_device_id(msc_id);

    if (device_id == -1) {
        return;
    }

    kelp_error_t error = kelp_block_unmount_device(device_id);

    if (error == KELP_OK) {
        struct msc_driver_details_t* details = &msc_driver_details[msc_id];
        device_id_2_msc_id[device_id] = -1;
        details->initialized = false;
        details->busy = false;
        details->block_size = 0;
        details->block_count = 0;
        details->size = 0;
    }
}

// ----------- HID -----------+

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
