//
// Created by wolfboy on 7/18/2026.
//

#include "file_service.h"

#include <stdio.h>
#include <stdlib.h>

#include "channel.h"
#include "com_channel_protocol.h"
#include "kernel_config.h"
#include "littlefs.h"
#include "nullfs.h"
#include "scheduler.h"

static kelp_fs_manager_t kelp_fs_manager;

static int16_t get_free_mount() {
    for (int8_t i = 0; i < FILE_SERVICE_MAX_MOUNTS; i++) {
        kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[i];
        if (!mount->active) {
            return i;
        }
    }
    return -1;
}

static bool exists_mount_with_device(uint8_t device_id) {
    for (int8_t i = 0; i < FILE_SERVICE_MAX_MOUNTS; i++) {
        kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[i];
        if (mount->active && mount->device_id == device_id) {
            return true;
        }
    }
    return false;
}

static int16_t get_mount_from_device(uint16_t device_id) {
    for (int8_t i = 0; i < FILE_SERVICE_MAX_MOUNTS; i++) {
        kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[i];
        if (mount->active && mount->device_id == device_id) {
            return i;
        }
    }
    return -1;
}

static void clear_mount(kelp_fs_mount_t* mount) {
    mount->active = false;
    if (mount->context != NULL) {
        free(mount->context);
    }
    mount->context = NULL;
    mount->plugin_id = 0;
    mount->device_id = 0;
}

// non-blocking sibling to mount
kelp_error_t kelp_fs_device_connected(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_CONNECT);
    return error;
}

// non-blocking sibling to mount
kelp_error_t kelp_fs_device_removed(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_REMOVE);
    return error;
}

kelp_error_t kelp_fs_mount(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_MOUNT);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);

    return service_error;
}

kelp_error_t kelp_fs_unmount(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_UNMOUNT);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);

    return service_error;
}

static kelp_error_t kelp_fs_handle_mount_request(uint16_t channel_id, uint8_t device_id) {
    printf("File Service Received Mount Request\n");
    if (exists_mount_with_device(device_id)) {
        return KELP_ID_TAKEN;
    }

    int16_t mount_id = get_free_mount();
    printf("File Service Found Free Slot\n");
    if (mount_id < 0) {
        return KELP_NONE_FREE;
    }

    // probe disk for filesystem
    const kelp_fs_backend_plugin_t* matched_plugin = NULL;
    uint8_t plugin_id = 0;
    printf("File Service Probing Device\n");

    for (; plugin_id < FILE_SERVICE_MAX_PLUGINS; plugin_id++) {
        const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];
        if (plugin == NULL) {
            continue;
        }
        printf("File Service Probing for %s\n", plugin->name);
        if (plugin->probe(device_id)) {
            printf("File Service Found %s on Disk\n", plugin->name);
            matched_plugin = plugin;
            break;
        }
    }

    if (matched_plugin == NULL) {
        printf("File Service No Systems Found\n");
        return KELP_NO_EXIST; // no matching filesystem
    }

    // attempt mounting with matching filesystem
    void* mount_context = NULL;
    printf("File Service Attempting to Mount %s\n", matched_plugin->name);
    mount_context = matched_plugin->mount(device_id);
    if (mount_context == NULL) {
        printf("File Service Error Attempting to Mount %s\n", matched_plugin->name);
        return KELP_ERROR;
    }

    // success! save values for later
    printf("File Service Successfully Mounted %s\n", matched_plugin->name);
    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    mount->active = true;
    mount->context = mount_context; // TODO: CAN'T COME FROM STACK! MUST BE FREED!
    mount->plugin_id = plugin_id;
    mount->device_id = device_id;
    kelp_fs_manager.num_mounts++;

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_unmount_request(uint16_t channel_id, uint8_t device_id) {
    int16_t mount_id = get_mount_from_device(device_id);
    if (mount_id < 0) {
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    printf("File Service Attempting to Unmount %s\n", plugin->name);

    kelp_error_t error = plugin->unmount(mount);
    if (error != KELP_OK) {
        printf("File Service Error Attempting to Unmount %s\n", plugin->name);
        return error;
    }

    clear_mount(mount);

    kelp_fs_manager.num_mounts--;
    printf("File Service Successfully Unmounted %s\n", plugin->name);

    return KELP_OK;
}

static void kelp_fs_init_plugins() {
    kelp_fs_manager.plugins[0] = &kelp_nullfs_plugin;
    kelp_fs_manager.plugins[1] = &kelp_lfsv2_plugin;
    kelp_fs_manager.num_plugins = 2;
}

void kelp_task_file_service(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(256);

    uint32_t l = 0;

    for (uint8_t i = 0; i < FILE_SERVICE_MAX_MOUNTS; i++) {
        kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[i];
        clear_mount(mount);
    }

    for (uint8_t i = 0; i < FILE_SERVICE_MAX_PLUGINS; i++) {
        kelp_fs_manager.plugins[i] = NULL;
    }

    kelp_fs_init_plugins();

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }

            l = 0;
        }

        // get connected channels
        uint16_t channel_ids[NUM_CHANNELS];
        uint16_t num_connected = 0;
        kelp_error_t error = get_connected_channels(channel_ids, &num_connected, NUM_CHANNELS);

        if (error != KELP_OK) {
            continue;
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

                switch (content_type) {

                case COM_TYPE_ARRAY: {
                    // get the reason
                    uint16_t reason = 0;

                    char data[CHANNEL_SIZE];
                    uint16_t size;

                    error = com_get_char_array_fast(channel_id, &data, &size, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to mount a device?
                    switch (reason) {

                    default: break;
                    }
                } break;

                case COM_TYPE_UINT32: {
                    // get the reason
                    uint16_t reason = 0;

                    uint32_t data;

                    error = com_get_uint32(channel_id, &data, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to get the driver pid?
                    switch (reason) {
                    case REASON_FILE_CONNECT:
                        kelp_fs_handle_mount_request(channel_id, (uint8_t)data);
                        break;
                    case REASON_FILE_REMOVE:
                        kelp_fs_handle_unmount_request(channel_id, (uint8_t)data);
                        break;
                    case REASON_FILE_MOUNT:
                        // get device id from channel
                        error = kelp_fs_handle_mount_request(channel_id, (uint8_t)data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_UNMOUNT:
                        // get device id from channel
                        error = kelp_fs_handle_unmount_request(channel_id, (uint8_t)data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    default: break;
                    }
                } break;
                default: break;
                }
            }
        }

        l++;

        task_sleep_us(250);
    }
}
