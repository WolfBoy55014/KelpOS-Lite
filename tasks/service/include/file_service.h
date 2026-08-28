//
// Created by wolfboy on 7/18/2026.
//

#ifndef KELPOS_LITE_FILE_SERVICE_H
#define KELPOS_LITE_FILE_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#include "error_codes.h"

#define FILE_SERVICE_PID 202

#define FILE_SERVICE_MAX_MOUNTS 4 // limit of uint8_t max
#define FILE_SERVICE_MAX_PLUGINS 4 // limit of uint8_t max
#define FILE_SERVICE_MAX_HANDLES 16 // limit of uint32_t max

#define FILE_SERVICE_MAX_NAME 256

#define REASON_FILE_ERROR 45674
#define REASON_FILE_OK 45674

/* Path-based operations (no handle) */
#define REASON_FILE_CONNECT 34678
#define REASON_FILE_REMOVE 37590
#define REASON_FILE_MOUNT 25678
#define REASON_FILE_UNMOUNT 7890
#define REASON_FILE_STAT 23597
#define REASON_FILE_RENAME 10010
#define REASON_FILE_DELETE 48956

/* File handle operations */
#define REASON_FILE_FILE_OPEN 10001
#define REASON_FILE_FILE_CLOSE 10002
#define REASON_FILE_FILE_READ 10003
#define REASON_FILE_FILE_WRITE 10004
#define REASON_FILE_FILE_SEEK 10005
#define REASON_FILE_FILE_TELL 10006
#define REASON_FILE_FILE_SIZE 10007
#define REASON_FILE_FILE_FLUSH 10008
#define REASON_FILE_FILE_TRUNCATE 10009

/* Directory handle operations */
#define REASON_FILE_DIR_MKDIR 10105
#define REASON_FILE_DIR_OPEN 10101
#define REASON_FILE_DIR_CLOSE 10102
#define REASON_FILE_DIR_READ 10103
#define REASON_FILE_DIR_REWIND 10104
#define REASON_FILE_DIR_SEEK 8572
#define REASON_FILE_DIR_TELL 23859

/* Handle type — distinguishes file vs directory handles */
typedef enum {
    FS_HANDLE_FILE,
    FS_HANDLE_DIR
} kelp_fs_handle_type_t;

// ------- Mount Struct ------+
typedef struct {
    void* context;
    uint8_t plugin_id;
    uint8_t device_id; // this will be used as the prefix for directory (`/0/documents`, `/1/config`, etc.)
    bool active;
} kelp_fs_mount_t;

// ---------------------------+
// API Description
// ---------------------------+

/* Seek whence values (POSIX-compatible) */
typedef enum {
    FS_SEEK_SET,    // offset from start
    FS_SEEK_CUR,    // offset from current
    FS_SEEK_END     // offset from end
} kelp_fs_seek_t;

/* Open flags */
typedef enum {
    FS_O_RDONLY  = 0x01,
    FS_O_WRONLY  = 0x02,
    FS_O_RDWR    = 0x03,
    FS_O_CREAT   = 0x04,
    FS_O_EXCL    = 0x08,
    FS_O_TRUNC   = 0x10,
    FS_O_APPEND  = 0x20
} kelp_fs_open_flags_t;

/* Directory entry type */
typedef enum {
    FS_DT_UNKNOWN = 0,
    FS_DT_FILE    = 1,
    FS_DT_DIR     = 2,
    FS_DT_SYMLINK = 3
} kelp_fs_dtype_t;

/* Directory entry — returned by readdir */
typedef struct {
    char name[FILE_SERVICE_MAX_NAME + 1]; // max filename length (FS_MAX_NAME)
    kelp_fs_dtype_t type;
    uint32_t size;        // file size, 0 for dirs
    uint32_t modified;    // timestamp
} kelp_fs_dirent_t;

/* --- The plugin interface --- */
typedef struct kelp_fs_backend_plugin kelp_fs_backend_plugin_t;

/* Probe: returns true if this FS is present. */
typedef bool (*kelp_fs_probe_fn)(uint8_t device_id);

/* Mount: initialize and mount on a slot. */
typedef void* (*kelp_fs_mount_fn)(uint8_t device_id);

/* Unmount: clean up and unmount. */
typedef kelp_error_t (*kelp_fs_unmount_fn)(kelp_fs_mount_t* mount);

/* === File operations === */

/* Open: open a file on a mounted slot. Returns handle. */
typedef kelp_error_t (*kelp_fs_file_open_fn)(kelp_fs_mount_t* mount, const char* path,
                                      uint32_t flags, void** handle);

/* Close: close a file handle. */
typedef kelp_error_t (*kelp_fs_file_close_fn)(kelp_fs_mount_t* mount, void* handle);

/* Read/Write: transfer data through a file handle. */
typedef kelp_error_t (*kelp_fs_file_read_fn)(kelp_fs_mount_t* mount, void* handle, uint8_t* buf, uint32_t len,
                                      uint32_t* bytes_read);
typedef kelp_error_t (*kelp_fs_file_write_fn)(kelp_fs_mount_t* mount, void* handle, const uint8_t* buf, uint32_t len,
                                      uint32_t* bytes_written);

/* Seek/Tell/Size/Flush: file position and metadata. */
typedef kelp_error_t (*kelp_fs_file_seek_fn)(kelp_fs_mount_t* mount, void* handle, int32_t offset, kelp_fs_seek_t whence);
typedef kelp_error_t (*kelp_fs_file_tell_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t* pos);
typedef kelp_error_t (*kelp_fs_file_size_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t* size);
typedef kelp_error_t (*kelp_fs_file_flush_fn)(kelp_fs_mount_t* mount, void* handle);
typedef kelp_error_t (*kelp_fs_file_truncate_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t size);

/* === Directory operations === */

/* Create directory */
typedef kelp_error_t (*kelp_fs_dir_mkdir_fn)(kelp_fs_mount_t* mount, const char* path);

/* Open/close directory */
typedef kelp_error_t (*kelp_fs_dir_open_fn)(kelp_fs_mount_t* mount, const char* path,
                                       void** dir_handle);
typedef kelp_error_t (*kelp_fs_dir_close_fn)(kelp_fs_mount_t* mount, void* dir_handle);

/* Read/rewind directory */
typedef kelp_error_t (*kelp_fs_dir_read_fn)(kelp_fs_mount_t* mount, void* dir_handle, kelp_fs_dirent_t* entry);
typedef kelp_error_t (*kelp_fs_dir_rewind_fn)(kelp_fs_mount_t* mount, void* dir_handle);
typedef kelp_error_t (*kelp_fs_dir_seek_fn)(kelp_fs_mount_t* mount, void* dir_handle, int32_t offset);
typedef kelp_error_t (*kelp_fs_dir_tell_fn)(kelp_fs_mount_t* mount, void* dir_handle, int32_t* pos);

/* === Path-based operations (no handle) === */

/* Stat: get file/directory info by path. */
typedef kelp_error_t (*kelp_fs_stat_fn)(kelp_fs_mount_t* mount, const char* path,
                                  kelp_fs_dirent_t* out);

/* Rename: rename or move a file or directory */
typedef kelp_error_t (*kelp_fs_rename_fn)(kelp_fs_mount_t* mount, const char* old_path, const char* new_path);

/* Remove: remove/delete a file or directory */
typedef kelp_error_t (*kelp_fs_remove_fn)(kelp_fs_mount_t* mount, const char *path);

/* Plugin descriptor */
struct kelp_fs_backend_plugin {
    const char* name;

    /* Required ops */
    kelp_fs_probe_fn     probe;
    kelp_fs_mount_fn     mount;
    kelp_fs_unmount_fn   unmount;
    kelp_fs_stat_fn      stat;
    kelp_fs_rename_fn    rename;
    kelp_fs_remove_fn    remove;

    /* File ops */
    kelp_fs_file_open_fn      file_open;
    kelp_fs_file_close_fn     file_close;
    kelp_fs_file_read_fn      file_read;
    kelp_fs_file_write_fn     file_write;
    kelp_fs_file_seek_fn      file_seek;
    kelp_fs_file_tell_fn      file_tell;
    kelp_fs_file_size_fn      file_size;
    kelp_fs_file_flush_fn     file_flush;
    kelp_fs_file_truncate_fn  file_truncate;

    /* Directory ops */
    kelp_fs_dir_mkdir_fn      dir_mkdir;
    kelp_fs_dir_open_fn       dir_open;
    kelp_fs_dir_close_fn      dir_close;
    kelp_fs_dir_read_fn       dir_read;
    kelp_fs_dir_rewind_fn     dir_rewind;
    kelp_fs_dir_seek_fn       dir_seek;
    kelp_fs_dir_tell_fn       dir_tell;

    /* Per-mount context size (allocated by FS manager) */
    uint32_t mount_ctx_size;

    /* Per-handle context size (allocated by open) */
    uint32_t handle_ctx_size;

    /* Max filename length supported */
    uint16_t max_name_len;

    /* Flags */
    uint32_t flags;  // FS_FLAG_DIR_SUPPORTED, FS_FLAG_TRIM_SUPPORTED, etc.
};

// ---------------------------+
// Internal Structs
// ---------------------------+

typedef struct {
    void* handle;
    kelp_fs_handle_type_t type;
    kelp_fs_mount_t* mount;
    bool active;
    uint32_t owner_pid;
} kelp_fs_handle_t;

// ------ Manager Struct -----+
typedef struct {
    const kelp_fs_backend_plugin_t* plugins[FILE_SERVICE_MAX_PLUGINS];
    kelp_fs_mount_t mounts[FILE_SERVICE_MAX_MOUNTS]; // TODO: Dynamically allocate?
    kelp_fs_handle_t handles[FILE_SERVICE_MAX_HANDLES];
    uint8_t num_plugins;
    uint8_t num_mounts;
} kelp_fs_manager_t;

// ---------------------------+
// API Functions
// ---------------------------+

// used by block device to start mounting process
kelp_error_t kelp_fs_device_connected(uint8_t device_id);

// used by block device to start unmounting/cleanup process
kelp_error_t kelp_fs_device_removed(uint8_t device_id);

kelp_error_t kelp_fs_mount(uint8_t device_id);

kelp_error_t kelp_fs_unmount(uint8_t device_id);

kelp_error_t kelp_fs_stat(const char* path, kelp_fs_dirent_t* out);

/* File operations */
kelp_error_t kelp_fs_file_open(const char* path, uint32_t flags, uint32_t* handle);

kelp_error_t kelp_fs_file_close(uint32_t handle);

kelp_error_t kelp_fs_file_read(uint32_t handle, uint8_t* buffer, uint32_t length, uint32_t* bytes_read);

kelp_error_t kelp_fs_file_write(uint32_t handle, const uint8_t* buffer, uint32_t length, uint32_t* bytes_written);

kelp_error_t kelp_fs_file_seek(uint32_t handle, int32_t offset, kelp_fs_seek_t whence);

kelp_error_t kelp_fs_file_tell(uint32_t handle, uint32_t* pos);

kelp_error_t kelp_fs_file_size(uint32_t handle, uint32_t* size);

kelp_error_t kelp_fs_file_flush(uint32_t handle);

kelp_error_t kelp_fs_file_truncate(uint32_t handle, uint32_t size);

kelp_error_t kelp_fs_rename(const char* old_path, const char* new_path);

/* Directory operations */
kelp_error_t kelp_fs_dir_mkdir(const char* path);

kelp_error_t kelp_fs_dir_open(const char* path, uint32_t* handle);

kelp_error_t kelp_fs_dir_close(uint32_t handle);

kelp_error_t kelp_fs_dir_read(uint32_t handle, kelp_fs_dirent_t* entry);

kelp_error_t kelp_fs_dir_rewind(uint32_t handle);

kelp_error_t kelp_fs_dir_seek(uint32_t handle, int32_t offset);

kelp_error_t kelp_fs_dir_tell(uint32_t handle, uint32_t* pos);

// file service task
void kelp_task_file_service(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_FILE_SERVICE_H
