//
// Created by wolfboy on 11/27/2025.
//

#include "include/com_channel_protocol.h"

#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
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

    uint16_t error = com_channel_write(channel_id, bytes, 7);
    return error > 0 ? 0 : error;
}

int8_t com_get_uint32(const uint16_t channel_id, uint32_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[7];

    int32_t error = com_channel_read(channel_id, bytes, 7);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 7);
    return error > 0 ? 0 : error;
}

int8_t com_get_int32(const uint16_t channel_id, int32_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[7];

    int32_t error = com_channel_read(channel_id, bytes, 7);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 11);
    return error > 0 ? 0 : error;
}

int8_t com_get_uint64(const uint16_t channel_id, uint64_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    int32_t error = com_channel_read(channel_id, bytes, 11);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 11);
    return error > 0 ? 0 : error;
}

int8_t com_get_int64(const uint16_t channel_id, int64_t* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    int32_t error = com_channel_read(channel_id, bytes, 11);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 7);
    return error > 0 ? 0 : error;
}

int8_t com_get_float(const uint16_t channel_id, float* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[7];

    int32_t error = com_channel_read(channel_id, bytes, 7);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 11);
    return error > 0 ? 0 : error;
}

int8_t com_get_double(const uint16_t channel_id, double* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[11];

    int32_t error = com_channel_read(channel_id, bytes, 11);
    if (error < 0) {
        // there was an error
        return error;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, 4);
    return error > 0 ? 0 : error;
}

int8_t com_get_char(const uint16_t channel_id, char* data, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[4];

    int32_t error = com_channel_read(channel_id, bytes, 4);
    if (error < 0) {
        // there was an error
        return error;
    }

    if (bytes[0] != COM_TYPE_CHAR) {
        return -4; // wrong data type
    }

    char data_char = bytes[3];
    *data = data_char;

    *reason = bytes[1] << 8 | bytes[2];

    return 0;
}

int8_t com_send_char_array(const uint16_t channel_id, const char data[], uint32_t size, const uint16_t reason) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    // initial packet shape:
    // | Type (8) | Reason (16) | Total Size (32) | Num Packets (16) |

    // following packet shape:
    // | Type (8) | Data (n) |

#if CHANNEL_SIZE < 9
#warning CHANNEL_SIZE is too small for this protocol, make it bigger or run the risk of fatal falure if this function is used.

    return -4; // CHANNEL_SIZE is too small, return to prevent a memory issue.
#endif

    // calculate values
    const uint16_t prefix_size = 1;                         // amount of each packet that is not data
    const uint16_t data_size = CHANNEL_SIZE - prefix_size;  // amount of room in each packet for data
    const uint16_t packet_count = (size + data_size - 1) / data_size;

    // send initial packet
    uint8_t initial_packet[9];

    initial_packet[0] = COM_TYPE_STR_I;
    initial_packet[1] = reason >> 8;
    initial_packet[2] = reason;

    initial_packet[3] = size >> 24;
    initial_packet[4] = size >> 16;
    initial_packet[5] = size >> 8;
    initial_packet[6] = size;

    initial_packet[7] = packet_count >> 8;
    initial_packet[8] = packet_count;

    uint16_t error = com_channel_write(channel_id, initial_packet, 9);
    if (error < 0) {
        return error;
    }

    // send the data packets
    for (uint16_t p = 0; p < packet_count; p++) {
        const uint16_t packet_rx_timeout_ms = 500;

        // wait for the previous packet to be received
        for (uint16_t t = 0; t < packet_rx_timeout_ms && !is_channel_ready_to_write(channel_id); ++t) {
            task_sleep_ms(1);
        }

        // if the previous packet has still not been received, fail
        if (!is_channel_ready_to_write(channel_id)) {
            return -3;
        }

        const uint32_t data_left = size - p * data_size;
        uint16_t data_this_packet = data_size;

        // if there is less than a packet's-worth of data, we don't need to fill the entire packet
        if (data_left < data_size) {
            data_this_packet = data_left;
        }

        uint8_t data_packet[prefix_size + data_this_packet];

        data_packet[0] = COM_TYPE_STR_D;

        for (uint16_t b = 0; b < data_this_packet; b++) {
            // b != data_size only on the last packet
            data_packet[b + prefix_size] = data[p * data_size + b];
        }

        uint16_t error = com_channel_write(channel_id, data_packet, prefix_size + data_this_packet);
        if (error < 0) {
            return error;
        }
    }

    return 0;
}

int8_t com_get_char_array(uint16_t channel_id, char* data, uint32_t max_size, uint32_t* size, uint16_t* reason) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    // calculate values
    const uint16_t prefix_size = 1;                         // amount of each packet that is not data
    const uint16_t data_size = CHANNEL_SIZE - prefix_size;  // amount of room in each packet for data

    // get initial packet
    uint8_t initial_packet[9];

    const uint16_t initial_packet_size = com_channel_read(channel_id, initial_packet, CHANNEL_SIZE);
    if (initial_packet_size < 0) {
        // there was an error
        return initial_packet_size;
    }

    if (initial_packet[0] != COM_TYPE_STR_I) {
        return -4; // wrong data type
    }

    if (initial_packet_size < 9) {
        return -5; // protocol error
    }

    *reason = initial_packet[1] << 8 | initial_packet[2];
    *size = initial_packet[3] << 24 | initial_packet[4] << 16 | initial_packet[5] << 8 | initial_packet[6];
    const uint16_t packet_count = initial_packet[7] << 8 | initial_packet[8];

    // receive the data packets
    for (uint16_t p = 0; p < packet_count; p++) {
        const uint16_t packet_rx_timeout_ms = 500;

        // wait for the previous packet to be received
        for (uint16_t t = 0; t < packet_rx_timeout_ms && !is_channel_ready_to_read(channel_id); ++t) {
            task_sleep_ms(1);
        }

        // if the previous packet has still not been received, fail
        if (!is_channel_ready_to_read(channel_id)) {
            return -3;
        }

        const uint32_t data_left = *size - p * data_size;
        uint16_t data_this_packet = data_size;

        // if there is less than a packet's-worth of data, we don't need to fill the entire packet
        if (data_left < data_size) {
            data_this_packet = data_left;
        }

        // don't simplify prefix_size + data_this_packet to CHANNEL_SIZE
        // as data_this_packet != data_size
        uint8_t data_packet[prefix_size + data_this_packet];

        uint16_t error = com_channel_read(channel_id, data_packet, prefix_size + data_this_packet);
        if (error < 0) {
            // there was an error
            return error;
        }

        if (data_packet[0] != COM_TYPE_STR_D) {
            return -4; // wrong data type
        }

        for (uint16_t b = 0; b < data_this_packet; b++) {
            // b != data_size only on the last packet

            if ((p * data_size + b) > max_size) {
                break;
            }

            data[p * data_size + b] = data_packet[b + prefix_size];
        }
    }

    if (*size > max_size) {
        return -6;
    }

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

    uint16_t error = com_channel_write(channel_id, bytes, packet_size);
    return error > 0 ? 0 : error;
}

int8_t com_get_char_array_fast(const uint16_t channel_id, char (*data)[CHANNEL_SIZE], uint16_t* size, uint16_t* reason) {
    // TODO: How the HECK I'm I SUPPOSED to pass a pointer to an ARRAY! >:(

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[CHANNEL_SIZE];

    *size = com_channel_read(channel_id, bytes, CHANNEL_SIZE) - 3;
    if (*size < 0) {
        // there was an error
        return *size;
    }

    if (bytes[0] != COM_TYPE_ARRAY) {
        return -4; // wrong data type
    }

    *reason = bytes[1] << 8 | bytes[2];

    for (uint16_t i = 0; i < *size; i++) {
        (*data)[i] = bytes[3 + i];
    }

    return 0;
}

int8_t com_send_request(const uint16_t channel_id, const uint16_t request) {

    if (!is_channel_ready_to_write(channel_id)) {
        return -3; // current contents have not been read
    }

    uint8_t bytes[3];

    bytes[0] = COM_TYPE_REQ;
    bytes[1] = request >> 8;
    bytes[2] = request;

    uint16_t error = com_channel_write(channel_id, bytes, 3);
    return error > 0 ? 0 : error;
}

int8_t com_get_request(const uint16_t channel_id, uint16_t* request) {

    if (!is_channel_ready_to_read(channel_id)) {
        return -3; // channel empty
    }

    uint8_t bytes[3];

    int32_t error = com_channel_read(channel_id, bytes, 3);
    if (error < 0) {
        // there was an error
        return error;
    }

    if (bytes[0] != COM_TYPE_REQ) {
        return -4; // wrong data type
    }

    *request = bytes[1] << 8 | bytes[2];

    return 0;
}