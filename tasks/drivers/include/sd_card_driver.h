//
// Created by wolfboy on 6/7/2026.
//

#ifndef KELPOS_LITE_SD_CARD_DRIVER_H
#define KELPOS_LITE_SD_CARD_DRIVER_H

#define SD_CARD_DRIVER_PID 102

#define SD_CARD_MAX_DEVICES 1

#include <stdbool.h>
#include <stdint.h>

struct sd_driver_details_t {
    bool initialized;
    uint32_t block_count;
    uint32_t size;
};

void kelp_task_sd_card_driver(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_SD_CARD_DRIVER_H