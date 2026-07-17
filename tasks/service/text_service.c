//
// Created by wolfboy on 3/1/2026.
//

#include "include/text_service.h"

#include <stdint.h>
#include <stdio.h>

#include "channel_internal.h"
#include "com_channel_protocol.h"
#include "scheduler.h"

char input_text_buffer[TEXT_BUFFER_SIZE];
uint16_t input_text_buffer_front = 0;
uint16_t input_text_buffer_back = 0;

char output_text_buffer[TEXT_BUFFER_SIZE];
uint16_t output_text_buffer_front = 0;
uint16_t output_text_buffer_back = 0;

kelp_error_t kelp_text_send_input_char(const char c) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_char_blocking(channel_id, c, REASON_TEXT_SEND_INPUT_CHAR);
    if (error != KELP_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_text_send_input_string(const char* str, const uint8_t len) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_char_array_fast_blocking(channel_id, str, len, REASON_TEXT_SEND_INPUT_STR);
    if (error != KELP_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_text_read_input_char(char* c) {
    // send request

    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true , &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_request_blocking(channel_id, REASON_TEXT_READ_INPUT_CHAR);
    if (error != KELP_OK) {
        return error;
    }

    // get result

    *c = '\0';
    uint16_t reason;

    error = com_get_char_blocking(channel_id, c, &reason);
    if (error != KELP_OK) {
        return error;
    }

    if (reason != REASON_TEXT_READ_INPUT_CHAR) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_text_send_output_char(const char c) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_char_blocking(channel_id, c, REASON_TEXT_SEND_OUTPUT_CHAR);
    if (error != KELP_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_text_send_output_string(const char* str, const uint8_t len) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_char_array_fast_blocking(channel_id, str, len, REASON_TEXT_SEND_OUTPUT_STR);
    if (error != KELP_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_text_read_output_char(char* c) {
    // send request

    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(TEXT_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_request_blocking(channel_id, REASON_TEXT_READ_OUTPUT_CHAR);
    if (error != KELP_OK) {
        return error;
    }

    // get result

    *c = '\0';
    uint16_t reason;

    error = com_get_char_blocking(channel_id, c, &reason);
    if (error != KELP_OK) {
        return error;
    }

    if (reason != REASON_TEXT_READ_OUTPUT_CHAR) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

void add_char_to_input_buffer(const char c) {
    if (((input_text_buffer_front + 1) % TEXT_BUFFER_SIZE) == input_text_buffer_back) {
        return;
    }
    input_text_buffer[input_text_buffer_front] = c;
    input_text_buffer_front = (input_text_buffer_front + 1) % TEXT_BUFFER_SIZE;
}

char get_char_from_input_buffer() {
    if (input_text_buffer_back == input_text_buffer_front) {
        return '\0';
    }
    char c = input_text_buffer[input_text_buffer_back];
    input_text_buffer[input_text_buffer_back] = '\0';
    input_text_buffer_back = (input_text_buffer_back + 1) % TEXT_BUFFER_SIZE;
    return c;
}

void add_char_to_output_buffer(const char c) {
    if (((output_text_buffer_front + 1) % TEXT_BUFFER_SIZE) == output_text_buffer_back) {
        return;
    }
    output_text_buffer[output_text_buffer_front] = c;
    output_text_buffer_front = (output_text_buffer_front + 1) % TEXT_BUFFER_SIZE;
}

char get_char_from_output_buffer() {
    if (output_text_buffer_back == output_text_buffer_front) {
        return '\0';
    }
    char c = output_text_buffer[output_text_buffer_back];
    output_text_buffer[output_text_buffer_back] = '\0';
    output_text_buffer_back = (output_text_buffer_back + 1) % TEXT_BUFFER_SIZE;
    return c;
}

void kelp_task_text_service(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(256);

    uint32_t l = 0;

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

                // does this channel contain a char?
                if (content_type == COM_TYPE_CHAR) {
                    // get the reason
                    uint16_t reason = 0;
                    char received_char = '\0';

                    error = com_get_char(channel_id, &received_char, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to send input for saving?
                    if (reason == REASON_TEXT_SEND_INPUT_CHAR) {
                        if (received_char == '\0') {
                            // might mean they didn't send anything
                            // we don't want to save this anyway
                            continue;
                        }

                        add_char_to_input_buffer(received_char);
                    }

                    // or do they want to send output for saving?
                    else if (reason == REASON_TEXT_SEND_OUTPUT_CHAR) {
                        if (received_char == '\0') {
                            // might mean they didn't send anything
                            // we don't want to save this anyway
                            continue;
                        }

                        add_char_to_output_buffer(received_char);
                    }
                }

                // does this channel contain a string?
                else if (content_type == COM_TYPE_ARRAY) {
                    // get the reason
                    uint16_t reason = 0;
                    uint16_t size = 0;
                    char received_chars[CHANNEL_SIZE] = {'\0'};

                    error = com_get_char_array_fast(channel_id, &received_chars, &size, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to send input for saving?
                    if (reason == REASON_TEXT_SEND_INPUT_STR) {
                        for (uint16_t i = 0; i < size; i++) {
                            if (received_chars[i] == '\0') {
                                // might mean they didn't send anything
                                // we don't want to save this anyway
                                continue;
                            }

                            add_char_to_input_buffer(received_chars[i]);
                        }
                    }

                    // or do they want to send output for saving?
                    else if (reason == REASON_TEXT_SEND_OUTPUT_STR) {
                        for (uint16_t i = 0; i < size; i++) {
                            if (received_chars[i] == '\0') {
                                // might mean they didn't send anything
                                // we don't want to save this anyway
                                continue;
                            }

                            add_char_to_output_buffer(received_chars[i]);
                        }
                    }
                }

                // are they requesting for data?
                else if (content_type == COM_TYPE_REQ) {
                    // get the reason
                    uint16_t reason = 0;

                    error = com_get_request(channel_id, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want input char?
                    if (reason == REASON_TEXT_READ_INPUT_CHAR) {

                        // send them their character
                        char character = get_char_from_input_buffer();

                        error = com_send_char_blocking(channel_id, character, REASON_TEXT_READ_INPUT_CHAR);
                        if (error != KELP_OK) {
                            continue;
                        }
                    }

                    // or do they want output?
                    else if (reason == REASON_TEXT_READ_OUTPUT_CHAR) {

                        // send them their character
                        char character = get_char_from_output_buffer();

                        error = com_send_char_blocking(channel_id, character, REASON_TEXT_READ_OUTPUT_CHAR);
                        if (error != KELP_OK) {
                            continue;
                        }
                    }
                }
            }
        }

        l++;

        task_sleep_us(250);
    }
}
