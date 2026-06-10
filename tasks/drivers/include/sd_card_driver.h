//
// Created by wolfboy on 6/7/2026.
//

#ifndef KELPOS_LITE_SD_CARD_DRIVER_H
#define KELPOS_LITE_SD_CARD_DRIVER_H

#define SD_CARD_DRIVER_PID 102
#include <stdint.h>

void kelp_task_sd_card(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_SD_CARD_DRIVER_H