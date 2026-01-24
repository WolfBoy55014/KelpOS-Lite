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

    uint8_t bytes[7];

    com_channel_read(channel_id, bytes, 7);

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

    uint8_t bytes[7];

    com_channel_read(channel_id, bytes, 7);

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

int8_t com_send_float(const uint16_t channel_id, const float data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    union {
        float f;
        uint8_t b[4]; // shares the same memory space as f
    } d;

    d.f = data;

    uint8_t bytes[7];

    bytes[0] = COM_TYPE_FLO;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = d.b[0];
    bytes[4] = d.b[1];
    bytes[5] = d.b[2];
    bytes[6] = d.b[3];

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 7);
}

int8_t com_get_float(const uint16_t channel_id, float* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[7];

    com_channel_read(channel_id, bytes, 7);

    if (bytes[0] != COM_TYPE_FLO) {
        return -4; // wrong data type
    }

    memcpy(data, bytes + 3, 4);
    // WARNING: dependent on endianness of system

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_double(const uint16_t channel_id, const double data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    union {
        double f;
        uint8_t b[8]; // shares the same memory space as f
    } d;

    d.f = data;

    uint8_t bytes[11];

    bytes[0] = COM_TYPE_DUB;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = d.b[0];
    bytes[4] = d.b[1];
    bytes[5] = d.b[2];
    bytes[6] = d.b[3];
    bytes[7] = d.b[4];
    bytes[8] = d.b[5];
    bytes[9] = d.b[6];
    bytes[10] = d.b[7];

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 11);
}

int8_t com_get_double(const uint16_t channel_id, double* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    com_channel_read(channel_id, bytes, 11);

    if (bytes[0] != COM_TYPE_DUB) {
        return -4; // wrong data type
    }

    memcpy(data, bytes + 3, 8);
    // WARNING: dependent on endianness of system

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_char(const uint16_t channel_id, const char data, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[4];

    bytes[0] = COM_TYPE_CHAR;
    bytes[1] = reason >> 8;
    bytes[2] = reason;
    bytes[3] = data;

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, 4);
}

int8_t com_get_char(const uint16_t channel_id, char* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[4];

    com_channel_read(channel_id, bytes, 4);

    if (bytes[0] != COM_TYPE_CHAR) {
        return -4; // wrong data type
    }

    char data_char = bytes[3];
    *data = data_char;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_char_array(const uint16_t channel_id, char data[], uint32_t size, const uint16_t reason) {

    // packet shape:
    // | Type (8) | Reason (16) | isFirst (1) | Index (15) | Size (16) | Payload |
    // ^^^ per send
    // 62.5% of data is actual data with 4 packets with channel size of 16

    // OR

    // initial packet shape:
    // | Type (8) | Reason (16) | Total Size (32) | Num Packets (16) |

    // following packet shape:
    // | Type (8) | Reason (16) | Size (16) |

    return 0;
}

int8_t com_send_char_array_fast(const uint16_t channel_id, const char* data, const uint16_t size, const uint16_t reason) {

    const uint32_t packet_size = size + 3;

    if (packet_size > CHANNEL_SIZE) {
        return -4; // array too big, increase channel size, or decrease array size
    }

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[packet_size];

    bytes[0] = COM_TYPE_ARRAY;
    bytes[1] = reason >> 8;
    bytes[2] = reason;

    for (uint16_t i = 0; i < size; i++) {
        bytes[3 + i] = data[i];
    }

    // TODO Handle non-zero return
    return com_channel_write(channel_id, bytes, packet_size);
}

int8_t com_get_char_array_fast(const uint16_t channel_id, char (*data)[CHANNEL_SIZE], uint16_t* size, uint16_t* reason) {
    // TODO: How the HECK I'm I SUPPOSED to pass a pointer to an ARRAY! >:(

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[CHANNEL_SIZE];

    *size = com_channel_read(channel_id, bytes, CHANNEL_SIZE) - 3;

    if (bytes[0] != COM_TYPE_ARRAY) {
        return -4; // wrong data type
    }

    *reason = bytes[1] << 8 | bytes[2];

    for (uint16_t i = 0; i < *size; i++) {
        (*data)[i] = bytes[3 + i];
    }

    return 0;
}