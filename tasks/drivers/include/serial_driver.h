//
// Created by wolfboy on 3/9/2026.
//

#ifndef KELPOS_LITE_SERIAL_H
#define KELPOS_LITE_SERIAL_H

#include <stdio.h>

#include "text_service.h"
#include "pico/stdlib.h"

#define SERIAL_DRIVER_PID 101

void kelp_serial_driver(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_SERIAL_H