//
// Created by wolfboy on 6/7/2026.
//

#include "sd_card_driver.h"

#include <stdint.h>

#include "block_service.h"
#include "hw_config.h"
#include "scheduler.h"
#include "sd_card.h"

static int16_t device_id_2_sd_id[BLOCK_SERVICE_MAX_DEVICES];
static bool is_sd_card_mounted[SD_CARD_MAX_DEVICES];

static struct sd_driver_details_t sd_driver_details[SD_CARD_MAX_DEVICES];

void kelp_task_sd_card_driver(uint32_t pid, uint32_t* signals, char* args) {

    task_request_stack(2048);

    uint32_t l = 0;

    // initialize id mappings
    for (uint8_t id = 0; id < BLOCK_SERVICE_MAX_DEVICES; id++) {
        device_id_2_sd_id[id] = -1;
    }

    // set all sd cards to unmounted
    for (uint8_t id = 0; id < SD_CARD_MAX_DEVICES; id++) {
        is_sd_card_mounted[id] = false;
    }

    // initialize sd card driver
    bool driver_initialised = false;

    for (uint32_t attempt = 0; attempt < 10; attempt++) {
        if (sd_init_driver()) {
            driver_initialised = true;
            break;
        }
        task_sleep_ms(10);
    }

    if (!driver_initialised) {
        return;
    }

    /*
     * main driver loop:
     * 1. loop through drives and check if they are connected
     *      a. if so initialize them
     */

    while (true) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }

            l = 0;
        }

        for (uint8_t id = 0; id < SD_CARD_MAX_DEVICES; id++) {
            sd_card_t* sd_card_p = sd_get_by_num(id);
            if (!sd_card_p) {
                return;
            }

            if (sd_card_detect(sd_card_p) && !is_sd_card_mounted[id]) {
                if (sd_card_p->init(sd_card_p)) {
                    uint8_t device_id;
                    kelp_error_t error = kelp_block_mount_device(&device_id, 512, sd_card_p->get_num_sectors(sd_card_p));

                    if (error == KELP_OK) {
                        device_id_2_sd_id[device_id] = id;
                        is_sd_card_mounted[id] = true;
                    }
                }
            }
        }

        l++;

        task_sleep_us(250);
    }

    // uint8_t drive_id = 0;
    // sd_card_t *sd_card_p = sd_get_by_num(drive_id);
    //
    // if (!sd_card_p) {
    //     return;
    // }
    //
    // while (!sd_card_detect(sd_card_p)) {
    //     task_sleep_ms(1000);
    // }
    //
    // if (!sd_card_p->init(sd_card_p)) {
    //     return;
    // }
    //
    // const uint32_t num_sectors = sd_card_p->get_num_sectors(sd_card_p);
    //
    // printf("\nNumber of sectors on card: %lu\n", num_sectors);
    // printf("Starting Speed Test\n");
    //
    // uint32_t sector = 0;
    // uint32_t duration = 10;
    // uint32_t start = time_us_32();
    // uint32_t end = start + duration * 1000000;
    // for (sector = 0; sector < num_sectors; sector += 8) {
    //     uint8_t buffer[4096];
    //     block_dev_err_t rc = sd_card_p->read_blocks(sd_card_p, buffer, sector, 8);
    //
    //     if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
    //         printf("Error reading sector %lu\n", sector);
    //         return;
    //     }
    //
    //     if (time_us_32() > end) {
    //         break;
    //     }
    // }
    //
    // uint32_t bytes_read = sector * 512;
    // double bytes_per_sec = (double) bytes_read / (double) duration;
    //
    // printf("Read %lu sectors\n", sector);
    // printf("Bytes per second: %f\n", bytes_per_sec);
}
