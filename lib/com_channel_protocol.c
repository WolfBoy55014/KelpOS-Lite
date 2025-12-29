//
// Created by wolfboy on 11/27/2025.
//

#include "include/com_channel_protocol.h"

#include <string.h>

#include "kernel/include/channel/channel_internal.h"

int8_t com_send_uint32(const uint16_t channel_id, const uint32_t data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[7];

    bytes[0] = COM_TYPE_UINT32;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = data >> 24;
    bytes[4] = data >> 16;
    bytes[5] = data >> 8;
    bytes[6] = data;

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 7);
}

int8_t com_get_uint32(const uint16_t channel_id, uint32_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[9];

    com_channel_read(channel_id, bytes, 9);

    if (bytes[0] != COM_TYPE_UINT32) {
        return -4; // wrong data type
    }

    uint32_t data_uint32 = bytes[3] << 24 | bytes[4] << 16 | bytes[5] << 8 | bytes[6];
    *data = data_uint32;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_int32(const uint16_t channel_id, const int32_t data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[7];

    bytes[0] = COM_TYPE_INT32;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = data >> 24;
    bytes[4] = data >> 16;
    bytes[5] = data >> 8;
    bytes[6] = data;

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 7);
}

int8_t com_get_int32(const uint16_t channel_id, int32_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[9];

    com_channel_read(channel_id, bytes, 9);

    if (bytes[0] != COM_TYPE_INT32) {
        return -4; // wrong data type
    }

    int32_t data_int32 = bytes[3] << 24 | bytes[4] << 16 | bytes[5] << 8 | bytes[6];
    *data = data_int32;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_uint64(const uint16_t channel_id, const uint64_t data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[11];

    bytes[0] = COM_TYPE_UINT64;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = data >> 56;
    bytes[4] = data >> 48;
    bytes[5] = data >> 40;
    bytes[6] = data >> 32;
    bytes[7] = data >> 24;
    bytes[8] = data >> 16;
    bytes[9] = data >> 8;
    bytes[10] = data;

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 11);
}

int8_t com_get_uint64(const uint16_t channel_id, uint64_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    com_channel_read(channel_id, bytes, 11);

    if (bytes[0] != COM_TYPE_UINT64) {
        return -4; // wrong data type
    }

    uint64_t data_uint64 =
        (uint64_t)bytes[3] << 56 |
        (uint64_t)bytes[4] << 48 |
        (uint64_t)bytes[5] << 40 |
        (uint64_t)bytes[6] << 32 |
        (uint64_t)bytes[7] << 24 |
        (uint64_t)bytes[8] << 16 |
        (uint64_t)bytes[9] << 8 |
        (uint64_t)bytes[10];

    *data = data_uint64;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_int64(const uint16_t channel_id, const int64_t data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[11];

    bytes[0] = COM_TYPE_INT64;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = data >> 56;
    bytes[4] = data >> 48;
    bytes[5] = data >> 40;
    bytes[6] = data >> 32;
    bytes[7] = data >> 24;
    bytes[8] = data >> 16;
    bytes[9] = data >> 8;
    bytes[10] = data;

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 11);
}

int8_t com_get_int64(const uint16_t channel_id, int64_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    com_channel_read(channel_id, bytes, 11);

    if (bytes[0] != COM_TYPE_INT64) {
        return -4; // wrong data type
    }

    int64_t data_int64 =
        (uint64_t)bytes[3] << 56 |
        (uint64_t)bytes[4] << 48 |
        (uint64_t)bytes[5] << 40 |
        (uint64_t)bytes[6] << 32 |
        (uint64_t)bytes[7] << 24 |
        (uint64_t)bytes[8] << 16 |
        (uint64_t)bytes[9] << 8 |
        (uint64_t)bytes[10];

    *data = data_int64;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}