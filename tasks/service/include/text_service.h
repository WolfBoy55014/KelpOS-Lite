//
// Created by wolfboy on 3/1/2026.
//

#ifndef KELPOS_LITE_TEXT_IO_H
#define KELPOS_LITE_TEXT_IO_H

#define TEXT_SERVICE_PID 200

#define TEXT_BUFFER_SIZE 128
#define TEXT_NULL_CHAR '\0'

#define REASON_TEXT_SEND_CHAR 52668
#define REASON_TEXT_SEND_STR  17778
#define REASON_TEXT_READ_CHAR 8399

#include <stdint.h>

int8_t kelp_text_send_char(char c);

int16_t kelp_text_send_string(const char* str, uint8_t len);

char kelp_text_read_char();

void kelp_task_text_service(uint32_t pid);

#endif //KELPOS_LITE_TEXT_IO_H