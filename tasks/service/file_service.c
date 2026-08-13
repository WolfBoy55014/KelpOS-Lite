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

static bool is_valid_mount(char mount_id) {
    if (mount_id < 0 || mount_id >= FILE_SERVICE_MAX_MOUNTS) {
        return false;
    }
    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    if (!mount->active) {
        return false;
    }
    return true;
}

// Extract device ID from virtual path like "/0/path/to/file"
// Returns pointer to the actual path (after the device prefix)
// Also writes the device ID to *device_id_out
static const char* extract_device_path(const char* virtual_path, uint8_t* device_id_out) {
    if (virtual_path == NULL || *virtual_path != '/') {
        return NULL;
    }

    char* p = &virtual_path[1];
    *device_id_out = (uint8_t)strtoul(&virtual_path[1], &p, 10);
    if (p == &virtual_path[1]) {
        return NULL; // didn't find any digits
    }


    if (*p != '/') {
        return NULL;  // Invalid format: must be /D/...
    }

    return p + 1;  // Skip the trailing '/'
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

kelp_error_t kelp_fs_stat(const char* path, kelp_fs_dirent_t* out) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, path, strlen(path) + 1, REASON_FILE_STAT);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    uint32_t size;
    uint16_t reason;
    error = com_get_char_array_blocking(channel_id, (char*)out, sizeof(kelp_fs_dirent_t), &size, &reason);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
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

static kelp_error_t kelp_fs_handle_stat_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size > FILE_SERVICE_MAX_NAME) {
        return KELP_TOO_BIG;
    }

    char* path = data;

    // get mount id and path from path
    uint8_t mount_id;
    const char* device_path = extract_device_path(path, &mount_id);
    if (device_path == NULL) {
        return KELP_NO_EXIST;
    }

    // check mount exists
    if (!is_valid_mount(mount_id)) {
        return KELP_NO_EXIST;
    }

    // get mount
    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    printf("Getting stats of %s\n", device_path);

    kelp_fs_dirent_t dirent;
    kelp_error_t error = plugin->stat(mount, device_path, &dirent);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, (char*)&dirent, sizeof(dirent), REASON_FILE_STAT);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static void kelp_fs_init_plugins() {
    kelp_fs_manager.plugins[0] = &kelp_nullfs_plugin;
    kelp_fs_manager.plugins[1] = &kelp_lfsv2_plugin;
    kelp_fs_manager.num_plugins = 2;
}

void kelp_task_file_service(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(512);

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

                case COM_TYPE_STR_I: {
                    // get the reason
                    uint16_t reason = 0;

                    char data[FILE_SERVICE_MAX_NAME];
                    uint32_t size;

                    error = com_get_char_array(channel_id, data, FILE_SERVICE_MAX_NAME, &size, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    switch (reason) {
                    case REASON_FILE_STAT:
                        error = kelp_fs_handle_stat_request(channel_id, data, size);
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
