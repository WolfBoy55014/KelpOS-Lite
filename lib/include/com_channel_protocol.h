//
// Created by wolfboy on 11/27/2025.
//

// TODO: Move to Kernel

#ifndef KELPOS_LITE_COM_CHANNEL_PROTOCOL_H
#define KELPOS_LITE_COM_CHANNEL_PROTOCOL_H

#include <stdint.h>

#include "kernel_config.h"

#define COM_TYPE_UINT32 1   // chanel contains an unsigned int
#define COM_TYPE_UINT64 2   // channel contains an unsigned long
#define COM_TYPE_INT32  3   // channel contains an int
#define COM_TYPE_INT64  4   // channel contains a long
#define COM_TYPE_FLO    5   // channel contains a float
#define COM_TYPE_DUB    6   // channel contains a double
#define COM_TYPE_CHAR   7   // channel contains a character
#define COM_TYPE_STR    8   // channel contains a char[]
#define COM_TYPE_ARRAY  9   // channel contains a char[] but faster
#define COM_TYPE_REQ    0   // channel contains a request id

/**
 * A non-blocking way to send an unsigned integer over the channels
 * @param channel_id ID of the channel to send data on
 * @param data the uint32_t to be sent
 * @param reason an unsigned integer sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_uint32(uint16_t channel_id, uint32_t data, uint16_t reason);
int8_t com_get_uint32(uint16_t channel_id, uint32_t* data, uint16_t* reason);

int8_t com_send_int32(uint16_t channel_id, int32_t data, uint16_t reason);
int8_t com_get_int32(uint16_t channel_id, int32_t* data, uint16_t* reason);

int8_t com_send_uint64(uint16_t channel_id, uint64_t data, uint16_t reason);
int8_t com_get_uint64(uint16_t channel_id, uint64_t* data, uint16_t* reason);

int8_t com_send_int64(uint16_t channel_id, int64_t data, uint16_t reason);
int8_t com_get_int64(uint16_t channel_id, int64_t* data, uint16_t* reason);

int8_t com_send_float(uint16_t channel_id, float data, uint16_t reason);
int8_t com_get_float(uint16_t channel_id, float* data, uint16_t* reason);

int8_t com_send_double(uint16_t channel_id, double data, uint16_t reason);
int8_t com_get_double(uint16_t channel_id, double* data, uint16_t* reason);

int8_t com_send_char(uint16_t channel_id, char data, uint16_t reason);
int8_t com_get_char(uint16_t channel_id, char* data, uint16_t* reason);

int8_t com_send_char_array(uint16_t channel_id, char data[], uint32_t size, uint16_t reason);
int8_t com_get_char_array(uint16_t channel_id, char* data[], uint16_t* reason);

int8_t com_send_char_array_fast(uint16_t channel_id, const char* data, uint16_t size, uint16_t reason);
int8_t com_get_char_array_fast(uint16_t channel_id, char (*data)[CHANNEL_SIZE], uint16_t* size, uint16_t* reason);

#endif //KELPOS_LITE_COM_CHANNEL_PROTOCOL_H