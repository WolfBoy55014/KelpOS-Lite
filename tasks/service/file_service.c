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

static int16_t get_free_file_slot() {
    for (int16_t i = 0; i < FILE_SERVICE_MAX_FILES; i++) {
        if (!kelp_fs_manager.files[i].active) {
            return i;
        }
    }
    return -1;
}

static kelp_fs_file_t* get_file_by_fd(int16_t fd) {
    if (fd < 0 || fd >= FILE_SERVICE_MAX_FILES) {
        return NULL;
    }
    if (!kelp_fs_manager.files[fd].active) {
        return NULL;
    }
    return &kelp_fs_manager.files[fd];
}

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

// ------- Client-Side File Operation Functions -------

kelp_error_t kelp_fs_open(uint8_t device_id, const char* path,
                          uint32_t flags, int16_t* fd_out) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // send device_id as the reason
    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_OPEN);
    KELP_RETURN_ON_ERROR(error);

    // send path
    error = com_send_char_array_fast_blocking(channel_id, path, strlen(path), REASON_FILE_OPEN_PATH);
    KELP_RETURN_ON_ERROR(error);

    // send flags
    error = com_send_uint32_blocking(channel_id, flags, REASON_FILE_OPEN_FLAGS);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back file descriptor
    uint32_t fd_val;
    error = com_get_uint32_blocking(channel_id, &fd_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *fd_out = (int16_t)fd_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_close(int16_t fd) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_CLOSE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);

    return service_error;
}

kelp_error_t kelp_fs_read(int16_t fd, void* buf, uint32_t len, uint32_t* bytes_read) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_READ);
    KELP_RETURN_ON_ERROR(error);

    // send buffer and length
    error = com_send_char_array_fast_blocking(channel_id, buf, len, REASON_FILE_READ_LEN);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, len, REASON_FILE_READ);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back bytes read
    uint32_t bytes_val;
    error = com_get_uint32_blocking(channel_id, &bytes_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *bytes_read = bytes_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_write(int16_t fd, const void* buf, uint32_t len, uint32_t* bytes_written) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    // send data and length
    error = com_send_char_array_fast_blocking(channel_id, buf, len, REASON_FILE_WRITE_LEN);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, len, REASON_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back bytes written
    uint32_t bytes_val;
    error = com_get_uint32_blocking(channel_id, &bytes_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *bytes_written = bytes_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_seek(int16_t fd, int32_t offset, kelp_fs_seek_t whence) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_SEEK);
    KELP_RETURN_ON_ERROR(error);

    // send offset
    error = com_send_int32_blocking(channel_id, offset, REASON_FILE_SEEK_OFFSET);
    KELP_RETURN_ON_ERROR(error);

    // send whence
    error = com_send_uint32_blocking(channel_id, (uint32_t)whence, REASON_FILE_SEEK_WHENCE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }

    return service_error;
}

kelp_error_t kelp_fs_tell(int16_t fd, uint32_t* pos) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_TELL);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back position
    uint32_t pos_val;
    error = com_get_uint32_blocking(channel_id, &pos_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *pos = pos_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_size(int16_t fd, uint32_t* size) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_SIZE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back size
    uint32_t size_val;
    error = com_get_uint32_blocking(channel_id, &size_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *size = size_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_stat(uint8_t device_id, const char* path, kelp_fs_dirent_t* out) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_STAT);
    KELP_RETURN_ON_ERROR(error);

    // send path
    error = com_send_char_array_fast_blocking(channel_id, path, strlen(path), REASON_FILE_STAT_PATH);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back dirent data (name, type, size, modified)
    char name_buf[256];
    uint16_t name_size;
    error = com_get_char_array_fast_blocking(channel_id, &name_buf, &name_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t type_val, size_val, modified_val;
    error = com_get_uint32_blocking(channel_id, &type_val, &reason);
    KELP_RETURN_ON_ERROR(error);
    error = com_get_uint32_blocking(channel_id, &size_val, &reason);
    KELP_RETURN_ON_ERROR(error);
    error = com_get_uint32_blocking(channel_id, &modified_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    strncpy(out->name, name_buf, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';
    out->type = (kelp_fs_dtype_t)type_val;
    out->size = size_val;
    out->modified = modified_val;

    return KELP_OK;
}

// ------- Client-Side File Operation Functions -------

kelp_error_t kelp_fs_open(uint8_t device_id, const char* path,
                          uint32_t flags, int16_t* fd_out) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    // send device_id as the reason
    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_OPEN);
    KELP_RETURN_ON_ERROR(error);

    // send path
    error = com_send_char_array_fast_blocking(channel_id, path, strlen(path), REASON_FILE_OPEN_PATH);
    KELP_RETURN_ON_ERROR(error);

    // send flags
    error = com_send_uint32_blocking(channel_id, flags, REASON_FILE_OPEN_FLAGS);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back file descriptor
    uint32_t fd_val;
    error = com_get_uint32_blocking(channel_id, &fd_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *fd_out = (int16_t)fd_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_close(int16_t fd) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_CLOSE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    KELP_RETURN_ON_ERROR(error);

    return service_error;
}

kelp_error_t kelp_fs_read(int16_t fd, void* buf, uint32_t len, uint32_t* bytes_read) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_READ);
    KELP_RETURN_ON_ERROR(error);

    // send buffer and length
    error = com_send_char_array_fast_blocking(channel_id, buf, len, REASON_FILE_READ_LEN);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, len, REASON_FILE_READ);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back bytes read
    uint32_t bytes_val;
    error = com_get_uint32_blocking(channel_id, &bytes_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *bytes_read = bytes_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_write(int16_t fd, const void* buf, uint32_t len, uint32_t* bytes_written) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    // send data and length
    error = com_send_char_array_fast_blocking(channel_id, buf, len, REASON_FILE_WRITE_LEN);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, len, REASON_FILE_WRITE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back bytes written
    uint32_t bytes_val;
    error = com_get_uint32_blocking(channel_id, &bytes_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *bytes_written = bytes_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_seek(int16_t fd, int32_t offset, kelp_fs_seek_t whence) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_SEEK);
    KELP_RETURN_ON_ERROR(error);

    // send offset
    error = com_send_int32_blocking(channel_id, offset, REASON_FILE_SEEK_OFFSET);
    KELP_RETURN_ON_ERROR(error);

    // send whence
    error = com_send_uint32_blocking(channel_id, (uint32_t)whence, REASON_FILE_SEEK_WHENCE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }

    return service_error;
}

kelp_error_t kelp_fs_tell(int16_t fd, uint32_t* pos) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_TELL);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back position
    uint32_t pos_val;
    error = com_get_uint32_blocking(channel_id, &pos_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *pos = pos_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_size(int16_t fd, uint32_t* size) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_int32_blocking(channel_id, fd, REASON_FILE_SIZE);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back size
    uint32_t size_val;
    error = com_get_uint32_blocking(channel_id, &size_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    *size = size_val;
    return KELP_OK;
}

kelp_error_t kelp_fs_stat(uint8_t device_id, const char* path, kelp_fs_dirent_t* out) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(FILE_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_FILE_STAT);
    KELP_RETURN_ON_ERROR(error);

    // send path
    error = com_send_char_array_fast_blocking(channel_id, path, strlen(path), REASON_FILE_STAT_PATH);
    KELP_RETURN_ON_ERROR(error);

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (error != KELP_OK) {
        return error;
    }
    if (service_error != KELP_OK) {
        return service_error;
    }

    // read back dirent data (name, type, size, modified)
    char name_buf[256];
    uint16_t name_size;
    error = com_get_char_array_fast_blocking(channel_id, &name_buf, &name_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t type_val, size_val, modified_val;
    error = com_get_uint32_blocking(channel_id, &type_val, &reason);
    KELP_RETURN_ON_ERROR(error);
    error = com_get_uint32_blocking(channel_id, &size_val, &reason);
    KELP_RETURN_ON_ERROR(error);
    error = com_get_uint32_blocking(channel_id, &modified_val, &reason);
    KELP_RETURN_ON_ERROR(error);

    strncpy(out->name, name_buf, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';
    out->type = (kelp_fs_dtype_t)type_val;
    out->size = size_val;
    out->modified = modified_val;

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

// ------- File Operation Handlers (service-side) -------

static kelp_error_t kelp_fs_handle_open_request(uint16_t channel_id, uint8_t device_id) {
    // read path from channel
    char path[FILE_SERVICE_MAX_PATH_LEN + 1];
    uint16_t path_size;
    kelp_error_t error;

    error = com_get_char_array_fast_blocking(channel_id, &path, &path_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    // read flags from channel
    uint32_t flags;
    error = com_get_uint32_blocking(channel_id, &flags, &reason);
    KELP_RETURN_ON_ERROR(error);

    // find mount
    int16_t mount_id = get_mount_from_device(device_id);
    if (mount_id < 0) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->open == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    // open the file
    void* file_handle = NULL;
    error = plugin->open(mount, path, flags, &file_handle);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    // allocate a file slot
    int16_t fd = get_free_file_slot();
    if (fd < 0) {
        com_send_error_blocking(channel_id, KELP_NONE_FREE);
        return KELP_NONE_FREE;
    }

    kelp_fs_file_t* file = &kelp_fs_manager.files[fd];
    file->handle = file_handle;
    file->mount_id = mount_id;
    file->active = true;

    // send back the file descriptor
    com_send_int32_blocking(channel_id, fd, REASON_FILE_OPEN_HANDLE);
    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_close_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->close == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    kelp_error_t error = plugin->close(file->handle);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    file->active = false;
    file->handle = NULL;

    com_send_error_blocking(channel_id, KELP_OK);
    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_read_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->read == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    // read buffer and length from channel
    char buf[CHANNEL_SIZE];
    uint16_t buf_size;
    uint16_t reason;
    kelp_error_t error;

    error = com_get_char_array_fast_blocking(channel_id, &buf, &buf_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t len;
    error = com_get_uint32_blocking(channel_id, &len, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t bytes_read = 0;
    error = plugin->read(file->handle, buf, len, &bytes_read);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    // send back bytes read and data
    com_send_uint32_blocking(channel_id, bytes_read, REASON_FILE_READ_BYTES);
    if (bytes_read > 0) {
        com_send_char_array_fast_blocking(channel_id, buf, bytes_read, REASON_FILE_READ);
    }

    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_write_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->write == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    // read data and length from channel
    char buf[CHANNEL_SIZE];
    uint16_t buf_size;
    uint16_t reason;
    kelp_error_t error;

    error = com_get_char_array_fast_blocking(channel_id, &buf, &buf_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t len;
    error = com_get_uint32_blocking(channel_id, &len, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t bytes_written = 0;
    error = plugin->write(file->handle, buf, len, &bytes_written);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    com_send_uint32_blocking(channel_id, bytes_written, REASON_FILE_WRITE_BYTES);
    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_seek_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->seek == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    // read offset and whence
    int32_t offset;
    uint16_t reason;
    kelp_error_t error;

    error = com_get_int32_blocking(channel_id, &offset, &reason);
    KELP_RETURN_ON_ERROR(error);

    uint32_t whence;
    error = com_get_uint32_blocking(channel_id, &whence, &reason);
    KELP_RETURN_ON_ERROR(error);

    error = plugin->seek(file->handle, offset, (kelp_fs_seek_t)whence);
    com_send_error_blocking(channel_id, error);
    return error;
}

static kelp_error_t kelp_fs_handle_tell_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->tell == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    uint32_t pos = 0;
    kelp_error_t error = plugin->tell(file->handle, &pos);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    com_send_uint32_blocking(channel_id, pos, REASON_FILE_TELL_POS);
    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_size_request(uint16_t channel_id, int32_t fd) {
    kelp_fs_file_t* file = get_file_by_fd(fd);
    if (file == NULL) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[file->mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->size == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    uint32_t size = 0;
    kelp_error_t error = plugin->size(file->handle, &size);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    com_send_uint32_blocking(channel_id, size, REASON_FILE_SIZE_VAL);
    return KELP_OK;
}

static kelp_error_t kelp_fs_handle_stat_request(uint16_t channel_id, uint8_t device_id) {
    // read path from channel
    char path[FILE_SERVICE_MAX_PATH_LEN + 1];
    uint16_t path_size;
    uint16_t reason;
    kelp_error_t error;

    error = com_get_char_array_fast_blocking(channel_id, &path, &path_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    // find mount
    int16_t mount_id = get_mount_from_device(device_id);
    if (mount_id < 0) {
        com_send_error_blocking(channel_id, KELP_NO_EXIST);
        return KELP_NO_EXIST;
    }

    kelp_fs_mount_t* mount = &kelp_fs_manager.mounts[mount_id];
    uint8_t plugin_id = mount->plugin_id;
    const kelp_fs_backend_plugin_t* plugin = kelp_fs_manager.plugins[plugin_id];

    if (plugin->stat == NULL) {
        com_send_error_blocking(channel_id, KELP_ERROR);
        return KELP_ERROR;
    }

    kelp_fs_dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent));
    error = plugin->stat(mount, path, &dirent);
    if (error != KELP_OK) {
        com_send_error_blocking(channel_id, error);
        return error;
    }

    // send back dirent data
    com_send_char_array_fast_blocking(channel_id, dirent.name, sizeof(dirent.name), REASON_FILE_STAT_OUT);
    com_send_uint32_blocking(channel_id, dirent.type, REASON_FILE_STAT_OUT);
    com_send_uint32_blocking(channel_id, dirent.size, REASON_FILE_STAT_OUT);
    com_send_uint32_blocking(channel_id, dirent.modified, REASON_FILE_STAT_OUT);

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

    for (uint8_t i = 0; i < FILE_SERVICE_MAX_FILES; i++) {
        kelp_fs_manager.files[i].active = false;
        kelp_fs_manager.files[i].handle = NULL;
        kelp_fs_manager.files[i].mount_id = 0;
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
                    case REASON_FILE_OPEN:
                        // get path and flags from channel
                        error = kelp_fs_handle_open_request(channel_id, (uint8_t)data);
                        break;
                    case REASON_FILE_CLOSE:
                        error = kelp_fs_handle_close_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_READ:
                        error = kelp_fs_handle_read_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_WRITE:
                        error = kelp_fs_handle_write_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_SEEK:
                        error = kelp_fs_handle_seek_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_TELL:
                        error = kelp_fs_handle_tell_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_SIZE:
                        error = kelp_fs_handle_size_request(channel_id, (int32_t)data);
                        break;
                    case REASON_FILE_STAT:
                        error = kelp_fs_handle_stat_request(channel_id, (uint8_t)data);
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
