//
// Created by wolfboy on 10/7/2026.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_service.h"
#include "ff.h"
#include "ffconf.h"
#include "pico/types.h"

static int translate_flags(uint32_t flags) {
    int ff_flags = 0;
    if (flags & FS_O_RDONLY)  ff_flags |= FA_READ;
    if (flags & FS_O_WRONLY)  ff_flags |= FA_WRITE;
    if (flags & FS_O_RDWR)    ff_flags |= (FA_READ | FA_WRITE);

    if ((flags & FS_O_CREAT) && (flags & FS_O_EXCL))    ff_flags |= FA_CREATE_NEW;
    if ((flags & FS_O_CREAT) && (flags & FS_O_TRUNC))   ff_flags |= FA_CREATE_ALWAYS;
    if ((flags & FS_O_CREAT) && (flags & FS_O_APPEND))  ff_flags |= FA_OPEN_APPEND;
    if (flags & FS_O_CREAT)                             ff_flags |= FA_OPEN_ALWAYS;
    if (!(flags & FS_O_CREAT))                          ff_flags |= FA_OPEN_EXISTING;
    return ff_flags;
}

static bool kelp_fatfs_probe(uint8_t device_id) {
    FATFS* fatfs = malloc(sizeof(FATFS));

    if (fatfs == NULL) {
        return false;
    }

    FRESULT error = f_mount(fatfs, device_id, 1);
    free(fatfs);

    if (error == FR_NO_FILESYSTEM) {
        return false;
    }

    return true;
}

void* kelp_fatfs_mount(uint8_t device_id) {
    FATFS* fatfs = malloc(sizeof(FATFS));

    if (fatfs == NULL) {
        return NULL;
    }

    FRESULT error = f_mount(fatfs, device_id, 1);

    if (error != FR_OK) {
        free(fatfs);
        return NULL;
    }

    return fatfs;
}

kelp_error_t kelp_fatfs_unmount(kelp_fs_mount_t* mount) {
    free(mount->context);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_stat(kelp_fs_mount_t* mount, const char* path,
                             kelp_fs_dirent_t* out) {
    FATFS* fatfs = mount->context;
    FILINFO info;

    if (strcmp(path, "/") == 0) {
        out->size = 0;
        out->type = FS_DT_DIR;
        out->name[0] = '\0';
        out->modified = 0;
    } else {
        FRESULT error = f_stat(fatfs, path, &info);
        if (error != FR_OK) {
            printf("FATFS Error %d at line %d\n", error, __LINE__);
            return error; // TODO: Make error code conversions
        }

        if (info.fattrib & AM_DIR) {
            out->type = FS_DT_DIR;
        } else {
            out->type = FS_DT_FILE;
        }

        out->size = info.fsize;
        out->modified = 0;

        if (strlen(info.fname) >= FILE_SERVICE_MAX_NAME) {
            return KELP_TOO_BIG;
        }

        strncpy(out->name, info.fname, FILE_SERVICE_MAX_NAME);
    }

    return KELP_OK;
}

kelp_error_t kelp_fatfs_rename(kelp_fs_mount_t* mount, const char* old_path, const char* new_path) {
    FATFS* fatfs = mount->context;
    FRESULT error = f_rename(fatfs, old_path, new_path);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_remove(kelp_fs_mount_t* mount, const char* path) {
    FATFS* fatfs = mount->context;
    FRESULT error = f_unlink(fatfs, path);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_open(kelp_fs_mount_t* mount, const char* path,
                                  uint32_t flags, void** handle) {
    FIL* file = malloc(sizeof(FIL));
    if (file == NULL) {
        printf("FATFS failed to malloc memory at %d\n", __LINE__);
        return KELP_MEMORY;
    }

    FATFS* fatfs = mount->context;

    BYTE ff_flags = translate_flags(flags);

    FRESULT error = f_open(fatfs, file, path, ff_flags);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        free(file);
        return error;
    }

    *handle = file;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_close(kelp_fs_mount_t* mount, void* handle) {
    FRESULT error = f_close(handle);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    free(handle);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_read(kelp_fs_mount_t* mount, void* handle, uint8_t* buf, uint32_t len,
                                  uint32_t* bytes_read) {
    uint br;
    FRESULT error = f_read(handle, buf, len, &br);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_read = (uint32_t)br;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_write(kelp_fs_mount_t* mount, void* handle, const uint8_t* buf, uint32_t len,
                                   uint32_t* bytes_written) {
    uint bw;
    FRESULT error = f_write(handle, buf, len, &bw);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_written = (uint32_t)bw;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_seek(kelp_fs_mount_t* mount, void* handle, int32_t offset, kelp_fs_seek_t whence) {
    FRESULT error = FR_OK;
    switch (whence) {
        case FS_SEEK_SET:
            if (offset < 0) {
                return KELP_PROTOCOL;
            }
            error = f_lseek(handle, offset);
            break;
        case FS_SEEK_CUR:
            if (handle == NULL) return KELP_INVALID_ID;
            FSIZE_t current_pos = f_tell((FIL*)handle);
            if (((int32_t)current_pos + offset) < 0) {
                return KELP_PROTOCOL;
            }
            error = f_lseek(handle, current_pos + offset);
            break;
        case FS_SEEK_END:
            if (handle == NULL) return KELP_INVALID_ID;
            FSIZE_t size = f_size((FIL*)handle);
            if (((int32_t)size + offset) < 0) {
                return KELP_PROTOCOL;
            }
            error = f_lseek(handle, size + offset);
            break;
    }

    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_tell(kelp_fs_mount_t* mount, void* handle, uint32_t* pos) {
    if (handle == NULL) return KELP_INVALID_ID;
    FSIZE_t current_pos = f_tell((FIL*)handle);

    *pos = (uint32_t)current_pos;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_size(kelp_fs_mount_t* mount, void* handle, uint32_t* size) {
    if (handle == NULL) return KELP_INVALID_ID;
    FSIZE_t file_size = f_size((FIL*)handle);

    *size = (uint32_t)file_size;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_flush(kelp_fs_mount_t* mount, void* handle) {
    FRESULT error = f_sync(handle);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_truncate(kelp_fs_mount_t* mount, void* handle, uint32_t size) {

    FSIZE_t current_pos = f_tell((FIL*)handle);
    FRESULT error = f_lseek(handle, size);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    error = f_truncate(handle);
    if (error != FR_OK) {
        f_lseek(handle, current_pos);
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    f_lseek(handle, current_pos);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_mkdir(kelp_fs_mount_t* mount, const char* path) {
    FATFS* fatfs = mount->context;
    FRESULT error = f_mkdir(fatfs, path);
    if (error != FR_OK) {
        printf("FATFS Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_open(kelp_fs_mount_t* mount, const char* path, void** dir_handle) {
    DIR* dir = malloc(sizeof(DIR));
    if (dir == NULL) {
        return KELP_MEMORY;
    }

    FATFS* fatfs = mount->context;
    FRESULT error = f_opendir(fatfs, dir, path);
    if (error != FR_OK) {
        free(dir);
        return error;
    }

    *dir_handle = dir;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_close(kelp_fs_mount_t* mount, void* dir_handle) {
    FRESULT error = f_closedir(dir_handle);
    if (error != FR_OK) {
        return error;
    }
    free(dir_handle);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_read(kelp_fs_mount_t* mount, void* dir_handle, kelp_fs_dirent_t* entry) {
    FILINFO info;
    FRESULT error = f_readdir(dir_handle, &info);
    if (error != FR_OK) {
        return error;
    }

    if (info.fname[0] == '\0') {
        return KELP_NO_EXIST;
    }

    entry->type = (info.fattrib & AM_DIR) ? FS_DT_DIR : FS_DT_FILE;
    entry->size = info.fsize;
    entry->modified = 0;
    strncpy(entry->name, info.fname, FILE_SERVICE_MAX_NAME);
    entry->name[FILE_SERVICE_MAX_NAME - 1] = '\0';

    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_rewind(kelp_fs_mount_t* mount, void* dir_handle) {
    FRESULT error = f_rewinddir(dir_handle);
    if (error != FR_OK) {
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_seek(kelp_fs_mount_t* mount, void* dir_handle, int32_t offset) {
    DIR* handle = dir_handle;
    if (!dir_handle) return KELP_NO_EXIST;
    // TODO: Make sure this and tell work
    FRESULT error = f_dir_sdi(handle, offset * 32);

    if (error != FR_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_tell(kelp_fs_mount_t* mount, void* dir_handle, int32_t *pos) {
    DIR* handle = dir_handle;
    if (!dir_handle) return KELP_NO_EXIST;
    *pos = handle->dptr / 32;
    return KELP_OK;
}

const struct kelp_fs_backend_plugin kelp_fatfs_plugin = {
    .name = "fatfs",
    .probe = kelp_fatfs_probe,
    .mount = kelp_fatfs_mount,
    .unmount = kelp_fatfs_unmount,
    .stat = kelp_fatfs_stat,
    .rename = kelp_fatfs_rename,
    .remove = kelp_fatfs_remove,
    
    /* File ops */
    .file_open = kelp_fatfs_file_open,
    .file_close = kelp_fatfs_file_close,
    .file_read = kelp_fatfs_file_read,
    .file_write = kelp_fatfs_file_write,
    .file_seek = kelp_fatfs_file_seek,
    .file_tell = kelp_fatfs_file_tell,
    .file_size = kelp_fatfs_file_size,
    .file_flush = kelp_fatfs_file_flush,
    .file_truncate = kelp_fatfs_file_truncate,
    
    /* Directory ops */
    .dir_mkdir = kelp_fatfs_dir_mkdir,
    .dir_open = kelp_fatfs_dir_open,
    .dir_close = kelp_fatfs_dir_close,
    .dir_read = kelp_fatfs_dir_read,
    .dir_rewind = kelp_fatfs_dir_rewind,
    .dir_seek = kelp_fatfs_dir_seek,
    .dir_tell = kelp_fatfs_dir_tell,

    .max_name_len = FF_MAX_LFN,
    .flags = 0,
};
