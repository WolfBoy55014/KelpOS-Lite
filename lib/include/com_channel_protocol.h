//
// Created by wolfboy on 11/27/2025.
//

// TODO: Move to Kernel

#ifndef KELPOS_LITE_COM_CHANNEL_PROTOCOL_H
#define KELPOS_LITE_COM_CHANNEL_PROTOCOL_H

#include <stdint.h>

#include "kernel_config.h"

#define COM_TYPE_UINT32 1   // channel contains an unsigned int
#define COM_TYPE_UINT64 2   // channel contains an unsigned long
#define COM_TYPE_INT32  3   // channel contains an int
#define COM_TYPE_INT64  4   // channel contains a long
#define COM_TYPE_FLO    5   // channel contains a float
#define COM_TYPE_DUB    6   // channel contains a double
#define COM_TYPE_CHAR   7   // channel contains a character
#define COM_TYPE_STR_I  8   // channel contains a char[] (initial packet)
#define COM_TYPE_STR_D  9   // channel contains a char[] (data packets)
#define COM_TYPE_ARRAY  10  // channel contains a char[] but faster
#define COM_TYPE_REQ    0   // channel contains a request id

/**
 * A non-blocking way to send an unsigned integer over the channels
 * @param channel_id ID of the channel to send data on
 * @param data the uint32_t to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_uint32(uint16_t channel_id, uint32_t data, uint16_t reason);

/**
 * A non-blocking way to receive an unsigned integer over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the uin32_t to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_uint32(uint16_t channel_id, uint32_t* data, uint16_t* reason);

/**
 * A non-blocking way to send an integer over the channels
 * @param channel_id ID of the channel to send data on
 * @param data the int32_t to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_int32(uint16_t channel_id, int32_t data, uint16_t reason);

/**
 * A non-blocking way to receive an integer over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the in32_t to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_int32(uint16_t channel_id, int32_t* data, uint16_t* reason);

/**
 * A non-blocking way to send an unsigned long over the channels
 * @param channel_id ID of the channel to send data on
 * @param data the uint64_t to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_uint64(uint16_t channel_id, uint64_t data, uint16_t reason);

/**
 * A non-blocking way to receive an unsigned long over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the uint64_t to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_uint64(uint16_t channel_id, uint64_t* data, uint16_t* reason);

/**
 * A non-blocking way to send a long over the channels
 * @param channel_id ID of the channel to send data on
 * @param data the int64_t to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_int64(uint16_t channel_id, int64_t data, uint16_t reason);

/**
 * A non-blocking way to receive a long over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the int64_t to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_int64(uint16_t channel_id, int64_t* data, uint16_t* reason);

/**
 * A non-blocking way to send a float the channels
 * @param channel_id ID of the channel to send data on
 * @param data the float to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_float(uint16_t channel_id, float data, uint16_t reason);

/**
 * A non-blocking way to receive a float over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the float to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_float(uint16_t channel_id, float* data, uint16_t* reason);

/**
 * A non-blocking way to send a double the channels
 * @param channel_id ID of the channel to send data on
 * @param data the double to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_double(uint16_t channel_id, double data, uint16_t reason);

/**
 * A non-blocking way to receive a double over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the double to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_double(uint16_t channel_id, double* data, uint16_t* reason);

/**
 * A non-blocking way to send a char the channels
 * @param channel_id ID of the channel to send data on
 * @param data the char to be sent
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_char(uint16_t channel_id, char data, uint16_t reason);

/**
 * A non-blocking way to receive a char over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the char to
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_char(uint16_t channel_id, char* data, uint16_t* reason);

/**
 * A non-blocking way to send an arbitrarily long "string" over the channels. \n
 * This is a c "string" but it can also carry bytes. \n
 * This splits the data into multiple packets to send longer "strings".
 * @param channel_id ID of the channel to send data on
 * @param data the char to be sent
 * @param size the size (or amount of data to send) of data
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_char_array(uint16_t channel_id, const char data[], uint32_t size, uint16_t reason);

/**
 * A non-blocking way to receive an arbitrarily long "string" over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to save the "string" to
 * @param max_size the size of <code>data</code> to prevent from filling it with too much data
 * @param size the amount of data that was put into <code>data</code> by this function
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_char_array(uint16_t channel_id, char* data, uint32_t max_size, uint32_t* size, uint16_t* reason);

/**
 * A non-blocking way to send a short "string" over the channels. \n
 * This is a c "string" but it can also carry bytes. \n
 * This is <i>fast</i> because there is less of protocol \n
 * but can't send more than on channel's worth of data.
 * @param channel_id ID of the channel to send data on
 * @param data the char to be sent
 * @param size the size (or amount of data to send) of data
 * @param reason an unsigned short sharing the purpose of the data, so the receiver knows what the data is for
 * @return a negative error code, or a positive result
 */
int8_t com_send_char_array_fast(uint16_t channel_id, const char* data, uint16_t size, uint16_t reason);

/**
 * A non-blocking way to receive a short "string" over the channels
 * @param channel_id ID of the channel to receive data from
 * @param data a pointer to an array to save the "string" to
 * @param size the amount of data that was put into <code>data</code> by this function
 * @param reason the reason the sender sent the data
 * @return a negative error code, or a positive result
 */
int8_t com_get_char_array_fast(uint16_t channel_id, char (*data)[CHANNEL_SIZE], uint16_t* size, uint16_t* reason);

/**
 * A non-blocking way to request data over the channels \n
 * This is used if the "client" wants to initialize an interaction, \n
 * such as if a GUI wanted to request updated sensor data to display. \n
 * @param channel_id ID of the channel to send data on
 * @param request an uint16_t representing the request to be made. It's a unique number provided by the task you are sending the request to.
 * @return a negative error code, or a positive result
 */
int8_t com_send_request(uint16_t channel_id, uint16_t request);

/**
 * A non-blocking way to receive a request over the channels
 * @param channel_id ID of the channel to receive data from
 * @param request the unique identifier that identifies the type of request
 * @return a negative error code, or a positive result
 */
int8_t com_get_request(uint16_t channel_id, uint16_t* request);

#endif //KELPOS_LITE_COM_CHANNEL_PROTOCOL_H
