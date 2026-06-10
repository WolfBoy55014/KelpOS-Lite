//
// Created by wolfboy on 6/7/2026.
//

#include "sd_card_driver.h"

#include <stdint.h>

#include "hw_config.h"
#include "scheduler.h"
#include "sd_card.h"

void kelp_task_sd_card(uint32_t pid, uint32_t* signals, char* args) {

    task_request_stack(2048);

    bool driver_initialised = false;

    for (int attempt = 0; attempt < 10; ++attempt) {
        if (sd_init_driver()) {
            driver_initialised = true;
            break;
        }
        task_sleep_ms(10);
    }

    if (!driver_initialised) {
        return;
    }

    uint8_t drive_id = 0;
    sd_card_t *sd_card_p = sd_get_by_num(drive_id);

    if (!sd_card_p) {
        return;
    }

    while (!sd_card_detect(sd_card_p)) {
        task_sleep_ms(1000);
    }

    if (!sd_card_p->init(sd_card_p)) {
        return;
    }

    uint32_t num_sectors = sd_card_p->get_num_sectors(sd_card_p);

    printf("Number of sectors on card: %lu", num_sectors);
}
