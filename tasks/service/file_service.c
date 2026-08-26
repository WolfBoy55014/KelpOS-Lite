//
// Created by wolfboy on 7/18/2026.
//

#include "file_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "channel.h"
#include "com_channel_protocol.h"
#include "kernel_config.h"
#include "littlefs.h"
#include "nullfs.h"
#include "scheduler.h"

// TODO: I really like the print statements in here and the plugins, but we need an actual debug message system

static kelp_fs_manager_t kelp_fs_manager;

static int16_t get_free_mount() {
    for (uint8_t i = 0; i < FILE_SERVICE_MAX_MOUNTS; i++) {
        kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[i];
        if (!mount->active) {
            return i;
        }
    }
    return -1;
}

static int64_t get_free_handle() {
    for (uint32_t i = 0; i < FILE_SERVICE_MAX_HANDLES; i++) {
        kelp_fs_handle_t* handle = &kelp_fs_manager.handles[i];
        if (!handle->active) {
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

static bool task_owns_handle(uint32_t pid, kelp_fs_handle_t* handle) {
    if (handle == NULL) {
        return false;
    }
    return handle->owner_pid == pid;
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

static void clear_handle(kelp_fs_handle_t* handle) {
    // TODO: track if the file is open and close it if the user forgot.
    handle->active = false;
    handle->owner_pid = 0;
    handle->handle = NULL;
    handle->mount = NULL;
    handle->type = FS_HANDLE_FILE;
}

static bool is_valid_mount(uint8_t mount_id) {
    if (mount_id >= FILE_SERVICE_MAX_MOUNTS) {
        return false;
    }
    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    if (!mount->active) {
        return false;
    }
    return true;
}

static bool is_valid_handle(uint32_t handle_id) {
    if (handle_id >= FILE_SERVICE_MAX_HANDLES) {
        return false;
    }
    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];
    if (!handle->active) {
        return false;
    }
    return true;
}

static uint32_t get_uint32_be(const uint8_t* data) {
    return ((uint8_t)data[0] << 24) | ((uint8_t)data[1] << 16) |
           ((uint8_t)data[2] << 8)  |  (uint8_t)data[3];
}

// Pack a 32-bit value into a buffer in big-endian
static void put_uint32_be(uint8_t* data, uint32_t val) {
    data[0] = val >> 24;
    data[1] = val >> 16;
    data[2] = val >> 8;
    data[3] = val;
}

// Extract mount ID from virtual path like "/0/path/to/file"
// Returns pointer to the actual path (after the device prefix)
// Also writes the mount ID to *mount_id_out
static const char* extract_mount_path(const char* virtual_path, uint8_t* mount_id_out) {
    if (virtual_path == NULL || *virtual_path != '/') {
        return NULL;
    }

    char* p = (char*)&virtual_path[1];
    *mount_id_out = (uint8_t)strtoul(&virtual_path[1], &p, 10);
    if (p == &virtual_path[1]) {
        return NULL; // didn't find any digits
    }

    if (*p != '/') {
        return NULL;  // Invalid format: must be /M/...
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

// non-blocking sibling to unmount
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

// ============================================================================
// File operation client APIs
// ============================================================================

kelp_error_t kelp_fs_file_open(const char* path, uint32_t flags, uint32_t* handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // send path
    error = com_send_char_array_blocking(channel_id, path, strlen(path) + 1, REASON_FILE_FILE_OPEN);
    KELP_RETURN_ON_ERROR(error);

    // send flags
    error = com_send_uint32_blocking(channel_id, flags, REASON_FILE_FILE_OPEN);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    // get handle
    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, handle, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_OPEN) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_file_close(uint32_t handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, handle, REASON_FILE_FILE_CLOSE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_file_read(uint32_t handle, uint8_t* buffer, uint32_t length, uint32_t* bytes_read) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // build informational packet
    uint64_t packet = ((uint64_t)length << 32) | handle;

    error = com_send_uint64_blocking(channel_id, packet, REASON_FILE_FILE_READ);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    // get data
    uint16_t reason;
    error = com_get_char_array_blocking(channel_id, (char*)buffer, length, bytes_read, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_READ) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_file_write(uint32_t handle, const uint8_t* buffer, uint32_t length, uint32_t* bytes_written) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // build informational packet
    uint64_t packet = ((uint64_t)length << 32) | handle;

    // send packet
    error = com_send_uint64_blocking(channel_id, packet, REASON_FILE_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    // send data
    error = com_send_char_array_blocking(channel_id, (char*)buffer, length, REASON_FILE_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    // get bytes_written
    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, bytes_written, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_WRITE) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_file_seek(uint32_t handle, int32_t offset, kelp_fs_seek_t whence) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    uint8_t packet[12];
    put_uint32_be(packet, handle);
    put_uint32_be(&packet[4], offset);
    put_uint32_be(&packet[8], whence);

    error = com_send_char_array_fast_blocking(channel_id, (char*)packet, 12, REASON_FILE_FILE_SEEK);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_file_tell(uint32_t handle, uint32_t* pos) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, handle, REASON_FILE_FILE_TELL);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, pos, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_TELL) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_file_size(uint32_t handle, uint32_t* size) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, handle, REASON_FILE_FILE_SIZE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, size, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_SIZE) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_file_flush(uint32_t handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, handle, REASON_FILE_FILE_FLUSH);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_file_truncate(uint32_t handle, uint32_t size) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, handle, REASON_FILE_FILE_TRUNCATE);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, size, REASON_FILE_FILE_TRUNCATE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_file_rename(const char* old_path, const char* new_path) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // send old path
    error = com_send_char_array_blocking(channel_id, old_path, strlen(old_path) + 1, REASON_FILE_FILE_RENAME);
    KELP_RETURN_ON_ERROR(error);

    // send new path
    error = com_send_char_array_blocking(channel_id, new_path, strlen(new_path) + 1, REASON_FILE_FILE_RENAME);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

// ============================================================================
// Directory operation client APIs
// ============================================================================

kelp_error_t kelp_fs_dir_opendir(const char* path, uint32_t* dir_handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, path, strlen(path) + 1, REASON_FILE_DIR_OPENDIR);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, dir_handle, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_DIR_OPENDIR) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_dir_readdir(uint32_t dir_handle, kelp_fs_dirent_t* entry) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, dir_handle, REASON_FILE_DIR_READDIR);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    uint16_t reason;
    uint32_t bytes_read;
    error = com_get_char_array_blocking(channel_id, (char*)entry, sizeof(kelp_fs_dirent_t), &bytes_read, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_DIR_READDIR) {
        return KELP_WRONG_REASON;
    }
    
    if (bytes_read != sizeof(kelp_fs_dirent_t)) {
        return KELP_PROTOCOL;
    }

    return KELP_OK;
}

kelp_error_t kelp_fs_dir_closedir(uint32_t dir_handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, dir_handle, REASON_FILE_DIR_CLOSEDIR);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_dir_rewinddir(uint32_t dir_handle) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, dir_handle, REASON_FILE_DIR_REWINDDIR);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

kelp_error_t kelp_fs_dir_mkdir(const char* path) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, path, strlen(path) + 1, REASON_FILE_DIR_MKDIR);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);
    KELP_RETURN_ON_ERROR(service_error);

    return KELP_OK;
}

// ============================================================================
// Service-side request handlers
// ============================================================================

static kelp_error_t kelp_fs_handle_mount_request(uint16_t channel_id, uint8_t device_id) {
    if (exists_mount_with_device(device_id)) {
        return KELP_ID_TAKEN;
    }

    int16_t mount_id = get_free_mount();
    if (mount_id < 0) {
        return KELP_NONE_FREE;
    }

    // probe disk for filesystem
    const kelp_fs_backend_plugin_t* matched_plugin = NULL;
    uint8_t plugin_id = 0;

    for (; plugin_id < FILE_SERVICE_MAX_PLUGINS; plugin_id++) {
        const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];
        if (plugin == NULL) {
            continue;
        }
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
    mount_context = matched_plugin->mount(device_id);
    if (mount_context == NULL) {
        return KELP_ERROR;
    }

    // success! save values for later
    printf("File Service Successfully Mounted %s\n", matched_plugin->name);
    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    mount->active = true;
    mount->context = mount_context;
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

    kelp_error_t error = plugin->unmount(mount);
    KELP_RETURN_ON_ERROR(error);

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
    const char* device_path = extract_mount_path(path, &mount_id);
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

    kelp_fs_dirent_t dirent;
    kelp_error_t error = plugin->stat(mount, device_path, &dirent);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, (char*)&dirent, sizeof(dirent), REASON_FILE_STAT);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_open_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size > FILE_SERVICE_MAX_NAME) {
        return KELP_TOO_BIG;
    }

    // get flags
    uint32_t flags;
    uint16_t reason;
    kelp_error_t error = com_get_uint32_blocking(channel_id, &flags, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_FILE_FILE_OPEN) {
        return KELP_WRONG_REASON;
    }

    char* path = data;

    // get mount id and path from path
    uint8_t mount_id;
    const char* device_path = extract_mount_path(path, &mount_id);
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

    int64_t handle_id = get_free_handle();
    if (handle_id < 0) {
        return KELP_NONE_FREE;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    error = plugin->file_open(mount, device_path, flags, &handle->handle);
    KELP_RETURN_ON_ERROR(error);

    // Set the handle type to FILE
    handle->type = FS_HANDLE_FILE;

    error = com_send_uint32_blocking(channel_id, handle_id, REASON_FILE_FILE_OPEN);
    KELP_RETURN_ON_ERROR(error);

    handle->active = true;
    handle->owner_pid = get_channel_partner_pid(channel_id);
    handle->mount = mount;

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_close_request(uint16_t channel_id, uint32_t handle_id) {
    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't close a directory handle with file_close
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];
    kelp_error_t error = plugin->file_close(mount, handle->handle);
    KELP_RETURN_ON_ERROR(error);

    clear_handle(handle);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_read_request(uint16_t channel_id, uint64_t data) {
    uint32_t length = data >> 32;
    uint32_t handle_id = data & 0xFFFFFFFF;

    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't read from a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    uint8_t* buffer = malloc(length);
    if (buffer == NULL) {
        return KELP_MEMORY;
    }

    uint32_t bytes_read;
    kelp_error_t error = plugin->file_read(mount, handle->handle, buffer, length, &bytes_read);
    if (error != KELP_OK) {
        free(buffer);
        return error;
    }

    error = com_send_char_array_blocking(channel_id, (char*)buffer, bytes_read, REASON_FILE_FILE_READ);
    free(buffer);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_write_request(uint16_t channel_id, uint64_t data) {
    uint32_t length = data >> 32;
    uint32_t handle_id = data & 0xFFFFFFFF;

    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't write to a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    uint8_t* buffer = malloc(length);
    if (buffer == NULL) {
        return KELP_MEMORY;
    }

    // get data buffer
    uint32_t bytes_received;
    uint16_t reason;
    kelp_error_t error = com_get_char_array_blocking(channel_id, (char*)buffer, length, &bytes_received, &reason);

    if (error != KELP_OK) {
        free(buffer);
        return error;
    }
    if (reason != REASON_FILE_FILE_WRITE) {
        free(buffer);
        return KELP_WRONG_REASON;
    }
    if (bytes_received != length) {
        free(buffer);
        return KELP_PROTOCOL;
    }

    // interact with filesystem
    uint32_t bytes_written;
    error = plugin->file_write(mount, handle->handle, buffer, length, &bytes_written);
    free(buffer);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, bytes_written, REASON_FILE_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_seek_request(uint16_t channel_id, uint8_t* data, uint32_t size) {
    if (size != 12) {
        return KELP_PROTOCOL;
    }

    uint32_t handle_id = get_uint32_be(data);
    int32_t offset = (int32_t)get_uint32_be(&data[4]);
    kelp_fs_seek_t whence = get_uint32_be(&data[8]);

    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't seek on a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    kelp_error_t error = plugin->file_seek(mount, handle->handle, offset, whence);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_tell_request(uint16_t channel_id, uint32_t handle_id) {
    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't tell on a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    uint32_t position;
    kelp_error_t error = plugin->file_tell(mount, handle->handle, &position);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, position, REASON_FILE_FILE_TELL);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_size_request(uint16_t channel_id, uint32_t handle_id) {
    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't get size of a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    uint32_t size;
    kelp_error_t error = plugin->file_size(mount, handle->handle, &size);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, size, REASON_FILE_FILE_SIZE);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_flush_request(uint16_t channel_id, uint32_t handle_id) {
    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't flush a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    kelp_error_t error = plugin->file_flush(mount, handle->handle);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_truncate_request(uint16_t channel_id, uint32_t handle_id, uint32_t size) {
    if (!is_valid_handle(handle_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_FILE) {
        return KELP_WRONG_TYPE;  // Can't truncate a directory handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    kelp_error_t error = plugin->file_truncate(mount, handle->handle, size);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_file_rename_request(uint16_t channel_id, char* old_path, char* new_path) {
    uint8_t mount_id;
    const char* old_device_path = extract_mount_path(old_path, &mount_id);
    if (old_device_path == NULL) {
        return KELP_NO_EXIST;
    }
    if (!is_valid_mount(mount_id)) {
        return KELP_NO_EXIST;
    }

    uint8_t new_mount_id;
    const char* new_device_path = extract_mount_path(new_path, &new_mount_id);
    if (new_device_path == NULL) {
        return KELP_NO_EXIST;
    }
    if (!is_valid_mount(new_mount_id)) {
        return KELP_NO_EXIST;
    }

    // Both paths must be on the same mount
    if (mount_id != new_mount_id) {
        return KELP_WRONG_TYPE;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    kelp_error_t error = plugin->file_rename(mount, old_device_path, new_device_path);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_dir_opendir_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size > FILE_SERVICE_MAX_NAME) {
        return KELP_TOO_BIG;
    }

    char* path = data;

    uint8_t mount_id;
    const char* device_path = extract_mount_path(path, &mount_id);
    if (device_path == NULL) {
        return KELP_NO_EXIST;
    }

    if (!is_valid_mount(mount_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    // Check if plugin supports opendir
    if (plugin->dir_opendir == NULL) {
        return KELP_WRONG_TYPE;
    }

    int64_t handle_id = get_free_handle();
    if (handle_id < 0) {
        return KELP_NONE_FREE;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[handle_id];

    kelp_error_t error = plugin->dir_opendir(mount, device_path, &handle->handle);
    if (error != KELP_OK) {
        return error;
    }

    // Set the handle type to DIR
    handle->type = FS_HANDLE_DIR;

    error = com_send_uint32_blocking(channel_id, handle_id, REASON_FILE_DIR_OPENDIR);
    KELP_RETURN_ON_ERROR(error);

    handle->active = true;
    handle->owner_pid = get_channel_partner_pid(channel_id);
    handle->mount = mount;

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_dir_readdir_request(uint16_t channel_id, uint32_t dir_handle) {
    if (!is_valid_handle(dir_handle)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[dir_handle];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_DIR) {
        return KELP_WRONG_TYPE;  // Can't readdir on a file handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    if (plugin->dir_readdir == NULL) {
        return KELP_WRONG_TYPE;
    }

    kelp_fs_dirent_t entry;
    kelp_error_t error = plugin->dir_readdir(mount, handle->handle, &entry);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_char_array_blocking(channel_id, (char*)&entry, sizeof(entry), REASON_FILE_DIR_READDIR);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_dir_closedir_request(uint16_t channel_id, uint32_t dir_handle) {
    if (!is_valid_handle(dir_handle)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[dir_handle];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_DIR) {
        return KELP_WRONG_TYPE;  // Can't closedir on a file handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    if (plugin->dir_closedir == NULL) {
        return KELP_WRONG_TYPE;
    }

    kelp_error_t error = plugin->dir_closedir(mount, handle->handle);
    KELP_RETURN_ON_ERROR(error);

    clear_handle(handle);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_dir_rewinddir_request(uint16_t channel_id, uint32_t dir_handle) {
    if (!is_valid_handle(dir_handle)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_handle_t* handle = &kelp_fs_manager.handles[dir_handle];

    if (!task_owns_handle(get_channel_partner_pid(channel_id), handle)) {
        return KELP_NOT_OWNER;
    }

    // Validate handle type
    if (handle->type != FS_HANDLE_DIR) {
        return KELP_WRONG_TYPE;  // Can't rewinddir on a file handle
    }

    kelp_fs_mount_t* mount = handle->mount;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    if (plugin->dir_rewinddir == NULL) {
        return KELP_WRONG_TYPE;
    }

    kelp_error_t error = plugin->dir_rewinddir(mount, handle->handle);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_dir_mkdir_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size > FILE_SERVICE_MAX_NAME) {
        return KELP_TOO_BIG;
    }

    char* path = data;

    uint8_t mount_id;
    const char* device_path = extract_mount_path(path, &mount_id);
    if (device_path == NULL) {
        return KELP_NO_EXIST;
    }

    if (!is_valid_mount(mount_id)) {
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[mount->plugin_id];

    if (plugin->dir_mkdir == NULL) {
        return KELP_WRONG_TYPE;
    }

    kelp_error_t error = plugin->dir_mkdir(mount, device_path);
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

    for (uint32_t i = 0; i < FILE_SERVICE_MAX_HANDLES; i++) {
        kelp_fs_handle_t* handle = &kelp_fs_manager.handles[i];
        clear_handle(handle);
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

                    switch (reason) {
                    case REASON_FILE_STAT:
                        error = kelp_fs_handle_stat_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_OPEN:
                        error = kelp_fs_handle_file_open_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_DIR_OPENDIR:
                        error = kelp_fs_handle_dir_opendir_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_DIR_MKDIR:
                        error = kelp_fs_handle_dir_mkdir_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_RENAME:
                        // rename needs two strings; parse first, then read second
                        // For simplicity, handle rename in a separate path below
                        break;
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

                    switch (reason) {
                    case REASON_FILE_CONNECT:
                        kelp_fs_handle_mount_request(channel_id, (uint8_t)data);
                        break;
                    case REASON_FILE_REMOVE:
                        kelp_fs_handle_unmount_request(channel_id, (uint8_t)data);
                        break;
                    case REASON_FILE_MOUNT:
                        error = kelp_fs_handle_mount_request(channel_id, (uint8_t)data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_UNMOUNT:
                        error = kelp_fs_handle_unmount_request(channel_id, (uint8_t)data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_CLOSE:
                        error = kelp_fs_handle_file_close_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_TELL:
                        error = kelp_fs_handle_file_tell_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_SIZE:
                        error = kelp_fs_handle_file_size_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_FLUSH:
                        error = kelp_fs_handle_file_flush_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_TRUNCATE:
                        // truncate needs size as second param; handled below
                        break;
                    case REASON_FILE_DIR_CLOSEDIR:
                        error = kelp_fs_handle_dir_closedir_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_DIR_REWINDDIR:
                        error = kelp_fs_handle_dir_rewinddir_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_DIR_READDIR:
                        error = kelp_fs_handle_dir_readdir_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    default: break;
                    }
                } break;

                case COM_TYPE_UINT64: {
                    // get the reason
                    uint16_t reason = 0;

                    uint64_t data;

                    error = com_get_uint64(channel_id, &data, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    switch (reason) {
                    case REASON_FILE_FILE_READ:
                        error = kelp_fs_handle_file_read_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_FILE_FILE_WRITE:
                        error = kelp_fs_handle_file_write_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    default: break;
                    }
                } break;

                case COM_TYPE_STR_I: {
                    // get the reason
                    uint16_t reason = 0;

                    char data[FILE_SERVICE_MAX_NAME]; // only works for paths
                    uint32_t size;

                    error = com_get_char_array(channel_id, data, FILE_SERVICE_MAX_NAME, &size, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    switch (reason) {
                    case REASON_FILE_FILE_RENAME:
                        // rename: first string is old path, second is new path
                        // We need to read the second string from the channel
                        {
                            char new_path[FILE_SERVICE_MAX_NAME];
                            uint32_t new_size;
                            uint16_t new_reason;
                            kelp_error_t err = com_get_char_array(channel_id, new_path, FILE_SERVICE_MAX_NAME, &new_size, &new_reason);
                            if (err != KELP_OK) {
                                break;
                            }
                            if (new_reason != REASON_FILE_FILE_RENAME) {
                                break;
                            }
                            error = kelp_fs_handle_file_rename_request(channel_id, data, new_path);
                            com_send_error_blocking(channel_id, error);
                        }
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
