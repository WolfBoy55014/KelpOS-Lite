//
// Created by wolfboy on 11/20/2025.
//

#include <stdio.h>

#include "pico/stdlib.h"
#include "governor.h"

#include <stdlib.h>

#include "scheduler.h"
#include "scheduler_internal.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

const uint32_t governor_frequencies[] = GOVERNOR_FREQUENCIES;
const enum vreg_voltage governor_voltages[] = GOVERNOR_VOLTAGES;

enum kelp_governor_power_mode current_power_mode = GOVERNOR_BALANCED;
uint8_t target_utilization = GOVERNOR_TARGET_BALANCED;

void kelp_governor(const uint32_t pid) {
    uint32_t current_freq = GOVERNOR_DEFAULT_FREQ;

    while (1) {
        // printf("kelp_governor: pid=%lu\n", pid);

        uint32_t new_freq = current_freq;

        uint8_t core_usage = 0;
        for (int c = 0; c < CORE_COUNT; c++) {
            core_usage += get_core_usage(c);
        }
        core_usage /= CORE_COUNT;

        // printf(">core_usage: %u\r\n", core_usage);

        if (core_usage > target_utilization + GOVERNOR_TARGET_TOLERANCE) {
            if (new_freq < 9) {
                new_freq++;
            }
        }
        else if (core_usage < target_utilization - GOVERNOR_TARGET_TOLERANCE) {
            if (new_freq > 0) {
                new_freq--;
            }
        }

        if (new_freq != current_freq) {
            enum vreg_voltage target_voltage = governor_voltages[new_freq];

            // increase voltage before increasing frequency
            if (new_freq > current_freq) {
                vreg_set_voltage(target_voltage);
                busy_wait_us(100);  // Let voltage stabilize
            }

            if (set_sys_clock_khz(governor_frequencies[new_freq], false)) {
                vreg_set_voltage(governor_voltages[new_freq]);

                // decrease voltage after decreasing frequency
                if (new_freq < current_freq) {
                    vreg_set_voltage(target_voltage);
                }

                current_freq = new_freq;

                refresh_systick_all_cores();
                stdio_init_all();

                printf("Successfully set system clock frequency to %lu\n", governor_frequencies[current_freq]);
            }
        }
        // printf(">clock_speed: %lu\r\n", current_freq);
        task_sleep_ms(1000);
    }
}

void kelp_governor_set_power_mode(const enum kelp_governor_power_mode power_mode) {
    current_power_mode = power_mode;

    switch (power_mode) {
    case (GOVERNOR_PERFORMANCE):
        target_utilization = GOVERNOR_TARGET_PERFORMANCE;
        break;
    case (GOVERNOR_BALANCED):
        target_utilization = GOVERNOR_TARGET_BALANCED;
        break;
    case (GOVERNOR_POWER_SAVE):
        target_utilization = GOVERNOR_TARGET_POWER_SAVE;
        break;
    default:
        target_utilization = GOVERNOR_TARGET_BALANCED;
        break;
    }
}

enum kelp_governor_power_mode kelp_governor_get_power_mode() {
    return current_power_mode;
}
