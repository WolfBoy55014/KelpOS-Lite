//
// Created by wolfboy on 11/20/2025.
//

#ifndef KELPOS_LITE_GOVERNOR_H
#define KELPOS_LITE_GOVERNOR_H

#include "hardware/vreg.h"

#define GOVERNOR_FREQUENCIES    {64000, 72000, 80000, 96000, 110000, 125000, 150000, 175000, 200000, 240000}
#define GOVERNOR_VOLTAGES       {VREG_VOLTAGE_0_90, VREG_VOLTAGE_0_90, VREG_VOLTAGE_0_95, VREG_VOLTAGE_0_95, VREG_VOLTAGE_1_10, VREG_VOLTAGE_1_10, VREG_VOLTAGE_1_15, VREG_VOLTAGE_1_15, VREG_VOLTAGE_1_20, VREG_VOLTAGE_1_20}
#define GOVERNOR_DEFAULT_FREQ 5         // should be the index of the frequency the cpu
                                        // normally runs at

#define GOVERNOR_TARGET_PERFORMANCE 25  // target cpu utilization for governor to aim for while in performance mode
#define GOVERNOR_TARGET_BALANCED 50     // target cpu utilization for governor to aim for normally
#define GOVERNOR_TARGET_POWER_SAVE 80   // target cpu utilization for governor to aim for while trying to same power
#define GOVERNOR_TARGET_TOLERANCE 5     // the governor will only adjust the cpu frequency is
                                        // this many percent above or below the target utilization

enum kelp_governor_power_mode {
    GOVERNOR_PERFORMANCE,
    GOVERNOR_BALANCED,
    GOVERNOR_POWER_SAVE
};

#include "pico/stdlib.h"

extern const uint32_t governor_frequencies[];
extern const enum vreg_voltage governor_voltages[];

/**
 * The task containing the CPU governor
 */
void kelp_governor(uint32_t pid);

/**
 * Set the power mode
 */
void kelp_governor_set_power_mode(enum kelp_governor_power_mode power_mode);

/**
 * Get the current power mode
 */
enum kelp_governor_power_mode kelp_governor_get_power_mode();

#endif //KELPOS_LITE_GOVERNOR_H
