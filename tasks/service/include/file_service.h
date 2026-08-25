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
#define REASON_FILE_CONNECT 34678
#define REASON_FILE_REMOVE 37590
#define REASON_FILE_MOUNT 25678
#define REASON_FILE_UNMOUNT 7890
#define REASON_FILE_OPEN 6789
#define REASON_FILE_CLOSE 342
#define REASON_FILE_READ 8902
#define REASON_FILE_WRITE 34561
#define REASON_FILE_STAT 23597
#define REASON_FILE_SEEK 9034
#define REASON_FILE_TELL 34908

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

/* Open: open a file on a mounted slot. Returns handle. */
typedef kelp_error_t (*kelp_fs_open_fn)(kelp_fs_mount_t* mount, const char* path,
                                  uint32_t flags, void** handle);

/* Close: close a file handle. */
typedef kelp_error_t (*kelp_fs_close_fn)(kelp_fs_mount_t* mount, void* handle);

/* Read/Write: transfer data through a file handle. */
typedef kelp_error_t (*kelp_fs_read_fn)(kelp_fs_mount_t* mount, void* handle, uint8_t* buf, uint32_t len,
                                  uint32_t* bytes_read);
typedef kelp_error_t (*kelp_fs_write_fn)(kelp_fs_mount_t* mount, void* handle, const uint8_t* buf, uint32_t len,
                                   uint32_t* bytes_written);

/* Seek/Tell/Size: position queries. */
typedef kelp_error_t (*kelp_fs_seek_fn)(kelp_fs_mount_t* mount, void* handle, int32_t offset, kelp_fs_seek_t whence);
typedef kelp_error_t (*kelp_fs_tell_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t* pos);
typedef kelp_error_t (*kelp_fs_size_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t* size);

/* Stat: get file info by path. */
typedef kelp_error_t (*kelp_fs_stat_fn)(kelp_fs_mount_t* mount, const char* path,
                                  kelp_fs_dirent_t* out);

/* Dir ops */
typedef kelp_error_t (*kelp_fs_mkdir_fn)(kelp_fs_mount_t* mount, const char* path);
typedef kelp_error_t (*kelp_fs_opendir_fn)(kelp_fs_mount_t* mount, const char* path,
                                     void** dir_handle);
typedef kelp_error_t (*kelp_fs_readdir_fn)(void* dir_handle, kelp_fs_dirent_t* entry);
typedef kelp_error_t (*kelp_fs_closedir_fn)(void* dir_handle);
typedef kelp_error_t (*kelp_fs_rewinddir_fn)(void* dir_handle);

/* Rename: rename or move a file or directory */
typedef kelp_error_t (*kelp_fs_rename_fn)(kelp_fs_mount_t* mount, const char* old_path, const char* new_path);

/* Sync: synchronize a file on storage */
typedef kelp_error_t (*kelp_fs_sync_fn)(kelp_fs_mount_t* mount, void* handle);

/* Truncate: shrink size of file to specified size */
typedef kelp_error_t (*kelp_fs_truncate_fn)(kelp_fs_mount_t* mount, void* handle, uint32_t size);

/* Plugin descriptor */
struct kelp_fs_backend_plugin {
    const char* name;             // "littlefs", "fatfs", "myfs"

    /* Required ops */
    kelp_fs_probe_fn     probe;
    kelp_fs_mount_fn     mount;
    kelp_fs_unmount_fn   unmount;
    kelp_fs_open_fn      open;
    kelp_fs_close_fn     close;
    kelp_fs_read_fn      read;
    kelp_fs_write_fn     write;
    kelp_fs_seek_fn      seek;
    kelp_fs_tell_fn      tell;
    kelp_fs_size_fn      size;
    kelp_fs_stat_fn      stat;
    kelp_fs_rename_fn    rename;

    /* Optional ops — set to NULL if not supported */
    kelp_fs_mkdir_fn     mkdir;
    kelp_fs_opendir_fn   opendir;
    kelp_fs_readdir_fn   readdir;
    kelp_fs_closedir_fn  closedir;
    kelp_fs_rewinddir_fn rewinddir;
    kelp_fs_sync_fn      flush;
    kelp_fs_truncate_fn  truncate;

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

kelp_error_t kelp_fs_open(const char* path, uint32_t flags, uint32_t* handle);

kelp_error_t kelp_fs_close(uint32_t handle);

kelp_error_t kelp_fs_read(uint32_t handle, uint8_t* buffer, uint32_t length, uint32_t* bytes_read);

kelp_error_t kelp_fs_write(uint32_t handle, const uint8_t* buffer, uint32_t length, uint32_t* bytes_written);

kelp_error_t kelp_fs_seek(uint32_t handle, int32_t offset, kelp_fs_seek_t whence);

kelp_error_t kelp_fs_tell(uint32_t handle, uint32_t* pos);

// file service task
void kelp_task_file_service(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_FILE_SERVICE_H
