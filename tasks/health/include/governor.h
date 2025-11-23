//
// Created by WolfBoy55014 on 11/20/2025.
//

#ifndef KELPOS_LITE_GOVERNOR_H
#define KELPOS_LITE_GOVERNOR_H

#include "hardware/vreg.h"

#define GOVERNOR_FREQUENCIES    {64000, 72000, 80000, 96000, 110000, 125000, 150000, 175000, 200000, 240000}
#define GOVERNOR_VOLTAGES       {VREG_VOLTAGE_0_90, VREG_VOLTAGE_0_90, VREG_VOLTAGE_0_95, VREG_VOLTAGE_0_95, VREG_VOLTAGE_1_10, VREG_VOLTAGE_1_10, VREG_VOLTAGE_1_15, VREG_VOLTAGE_1_15, VREG_VOLTAGE_1_20, VREG_VOLTAGE_1_20}
#define GOVERNOR_DEFAULT_FREQ 5

#define GOVERNOR_TARGET_UTILIZATION 25

#include "pico/stdlib.h"

extern const uint32_t governor_frequencies[];
extern const enum vreg_voltage governor_voltages[];

/**
 * The task containing the CPU governor
 */
void kelp_governor(uint32_t pid);

#endif //KELPOS_LITE_GOVERNOR_H
