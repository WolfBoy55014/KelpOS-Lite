//
// Created by wolfboy on 3/1/2026.
//

#include "include/text_service.h"

#include <stdint.h>
#include <stdio.h>

#include "channel_internal.h"
#include "com_channel_protocol.h"
#include "scheduler.h"

char text_buffer[TEXT_BUFFER_SIZE];
uint16_t text_buffer_front = 0;
uint16_t text_buffer_back = 0;

int8_t kelp_text_send_char(const char c) {
    int32_t channel_id = com_channel_request(TEXT_SERVICE_PID, true);
    if (channel_id < 0) {
        return channel_id;
    }

    for (uint8_t i = 0; i < 100; i++) {
        if (is_channel_ready_to_write(channel_id)) {
            break;
        }

        task_yield();
    }

    if (!is_channel_ready_to_write(channel_id)) {
        return -1;
    }

    int8_t error = com_send_char(channel_id, c, REASON_TEXT_SEND_CHAR);
    if (error < 0) {
        return error;
    }

    return 0;
}

int16_t kelp_text_send_string(const char* str, const uint8_t len) {
    int32_t channel_id = com_channel_request(TEXT_SERVICE_PID, true);
    if (channel_id < 0) {
        return channel_id;
    }

    for (uint16_t i = 0; i < 1000; i++) {
        if (is_channel_ready_to_write(channel_id)) {
            break;
        }

        task_sleep_ms(1);
    }

    if (!is_channel_ready_to_write(channel_id)) {
        return -1;
    }

    int8_t error = com_send_char_array_fast(channel_id, str, len, REASON_TEXT_SEND_STR);
    if (error < 0) {
        return error;
    }

    return 0;
}

char kelp_text_read_char() {
    // send request

    int32_t channel_id = com_channel_request(TEXT_SERVICE_PID, true);
    if (channel_id < 0) {
        return '\0';
    }

    for (uint16_t i = 0; i < 1000; i++) {
        if (is_channel_ready_to_write(channel_id)) {
            break;
        }

        task_sleep_ms(1);
    }

    if (!is_channel_ready_to_write(channel_id)) {
        return '\0';
    }

    int8_t error = com_send_request(channel_id, REASON_TEXT_READ_CHAR);
    if (error < 0) {
        return '\0';
    }

    // get result

    for (uint16_t i = 0; i < 1000; i++) {
        if (is_channel_ready_to_read(channel_id)) {
            break;
        }

        task_sleep_ms(1);
    }

    if (!is_channel_ready_to_read(channel_id)) {
        return '\0';
    }

    char c = '\0';
    uint16_t reason;

    error = com_get_char(channel_id, &c, &reason);
    if ((error < 0) || (reason != REASON_TEXT_READ_CHAR)) {
        return '\0';
    }

    return c;
}

void add_char_to_buffer(const char c) {
    if (((text_buffer_front + 1) % TEXT_BUFFER_SIZE) == text_buffer_back) {
        return;
    }
    text_buffer[text_buffer_front] = c;
    text_buffer_front = (text_buffer_front + 1) % TEXT_BUFFER_SIZE;
}

char get_char_from_buffer() {
    if (text_buffer_back == text_buffer_front) {
        return '\0';
    }
    char c = text_buffer[text_buffer_back];
    text_buffer[text_buffer_back] = '\0';
    text_buffer_back = (text_buffer_back + 1) % TEXT_BUFFER_SIZE;
    return c;
}

void kelp_task_text_service(uint32_t pid) {
    while (1) {
        // get connected channels
        uint16_t channel_ids[NUM_CHANNELS];
        int32_t num_connected = get_connected_channels(channel_ids, NUM_CHANNELS);

        // check for messages
        for (int c = 0; c < num_connected; c++) {
            uint16_t channel_id = channel_ids[c];

            // can we read from this channel?
            if (is_channel_ready_to_read(channel_id)) {
                uint8_t content_type = com_channel_peek(channel_id);

                // does this channel contain a char?
                if (content_type == COM_TYPE_CHAR) {
                    // get the reason
                    uint16_t reason = 0;
                    char received_char = '\0';

                    int8_t error = com_get_char(channel_id, &received_char, &reason);
                    if (error < 0) {
                        continue;
                    }

                    // do they want to send a char for saving?
                    if (reason == REASON_TEXT_SEND_CHAR) {
                        if (received_char == '\0') {
                            // might mean they didn't send anything
                            // we don't want to save this anyway
                            continue;
                        }

                        add_char_to_buffer(received_char);
                    }
                }

                // does this channel contain a string?
                else if (content_type == COM_TYPE_ARRAY) {
                    // get the reason
                    uint16_t reason = 0;
                    uint16_t size = 0;
                    char received_chars[CHANNEL_SIZE] = {'\0'};

                    int8_t error = com_get_char_array_fast(channel_id, &received_chars, &size, &reason);
                    if (error < 0) {
                        continue;
                    }

                    // do they want to send a char for saving?
                    if (reason == REASON_TEXT_SEND_STR) {
                        for (uint16_t i = 0; i < size; i++) {
                            if (received_chars[i] == '\0') {
                                // might mean they didn't send anything
                                // we don't want to save this anyway
                                continue;
                            }

                            add_char_to_buffer(received_chars[i]);
                        }
                    }
                }

                // are they requesting for data?
                else if (content_type == COM_TYPE_REQ) {
                    // get the reason
                    uint16_t reason = 0;

                    int8_t error = com_get_request(channel_id, &reason);
                    if (error < 0) {
                        continue;
                    }

                    // do they want a character?
                    if (reason == REASON_TEXT_READ_CHAR) {

                        // send them their character
                        char character = get_char_from_buffer();

                        for (uint16_t i = 0; i < 1000; i++) {
                            if (is_channel_ready_to_write(channel_id)) {
                                break;
                            }

                            task_sleep_ms(1);
                        }

                        if (!is_channel_ready_to_write(channel_id)) {
                            continue;
                        }

                        error = com_send_char(channel_id, character, REASON_TEXT_READ_CHAR);
                        if (error < 0) {
                            continue;
                        }
                    }
                }
            }
        }
        task_sleep_ms(10);
    }
}
