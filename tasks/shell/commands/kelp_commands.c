//
// Created by wolfboy on 3/15/2026.
//

#include <string.h>

#include "coremark.h"
#include "scheduler.h"
#include "scheduler_internal.h"
#include "inc/ush.h"
#include "inc/ush_types.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "inc/ush_internal.h"

void kelp_cmd_signal_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
    static char* signal_names[6] = {
        "sigterm",
        "sigkill",
        "sigstop",
        "sigcont",
        "sigexcp",
        "sigwtdg",
    };

    if (argc != 3) {
        ush_print_status(self, USH_STATUS_ERROR_COMMAND_WRONG_ARGUMENTS);
        return;
    }

    // get task id from arguments
    char* endptr;
    uint32_t task_id = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0') {
        ush_print_status(self, USH_STATUS_ERROR_COMMAND_SYNTAX_ERROR);
    }

    // check if task_id is valid
    if (!task_exists(task_id)) {
        ush_print_status(self, USH_STATUS_ERROR_COMMAND_SYNTAX_ERROR);
    }

    // check if the signal is valid
    for (int i = 0; i < 6; i++) {
        if (strcmp(argv[2], signal_names[i]) == 0) {
            switch (i) {
            case 0:
                task_signal(task_id, TASK_SIGTERM);
                break;
            case 1:
                task_signal(task_id, TASK_SIGKILL);
                break;
            case 2:
                task_signal(task_id, TASK_SIGSTOP);
                break;
            case 3:
                task_signal(task_id, TASK_SIGCONT);
                break;
            case 4:
                task_signal(task_id, TASK_SIGEXCP);
                break;
            case 5:
                task_signal(task_id, TASK_SIGWTDG);
                break;
            default:
                ush_print_status(self, USH_STATUS_ERROR_COMMAND_SYNTAX_ERROR);
            }

            break;
        }
    }

    ush_print_status(self, USH_STATUS_OK);
}

void kelp_cmd_cpu_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
    char* subcommands[3] = {
        "temp",
        "usage",
        "freq",
    };

    if (argc < 2) {
        ush_print_status(self, USH_STATUS_ERROR_COMMAND_WRONG_ARGUMENTS);
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (strcmp(argv[1], subcommands[i]) == 0) {
            switch (i) {
            case 0:
                adc_init();

                adc_set_temp_sensor_enabled(1);

                adc_select_input(4);

                const float conversion_factor = 3.3f / (1 << 12);
                float voltage = adc_read() * conversion_factor;

                float temp = 27 - (voltage - 0.706) / 0.001721;

                ush_printf(self, "CPU Temp: %fC\n", temp);
                break;
            case 1:
                for (uint8_t c = 0; c < CORE_COUNT; ++c) {
                    uint8_t core_usage = get_core_usage(c);

                    ush_printf(self, "Core %u Usage: %u\n", c, core_usage);
                }
                break;
            case 2:
                ush_printf(self, "System Clock: %lu kHz\n", clock_get_hz(clk_sys) / 1000);
                break;
            default:
                ush_print_status(self, USH_STATUS_ERROR_COMMAND_SYNTAX_ERROR);
            }
        }
    }
}

void kelp_cmd_bench_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]) {
    ush_printf(self, "Starting CoreMark Benchmark...\n");
    ush_process_start(self, file);
}

bool kelp_cmd_bench_service(struct ush_object* self, struct ush_file_descriptor const* file) {
    (void)file;

    USH_ASSERT(self != NULL);
    USH_ASSERT(file != NULL);

    bool processed = true;

    static uint32_t pid = 0;

    switch (self->state) {
    case USH_STATE_PROCESS_START:
        for (uint32_t p = 400; p < UINT32_MAX; p++) {
            const int32_t error = task_add(coremark_task_main, p, 255);

            if (error == -1) {
                continue;
            }

            if (error < 0) {
                ush_printf(self, "Error: couldn't create task\n");
                self->state = USH_STATE_RESET_PROMPT;
                break;
            }

            pid = p;
            ush_printf(self, "Started CoreMark benchmark\n");
            self->state = USH_STATE_PROCESS_SERVICE;
            break;
        }

        break;
    case USH_STATE_PROCESS_SERVICE:
        // only continue if we have a valid pid to track
        if (pid == 0) {
            // no task was started, skip to finish
            self->state = USH_STATE_PROCESS_FINISH;
            break;
        }

        if (!task_exists(pid)) {
            self->state = USH_STATE_PROCESS_FINISH;
        }
        break;
    case USH_STATE_PROCESS_FINISH:
        ush_printf(self, "Finished benchmark\n");
        self->state = USH_STATE_RESET_PROMPT;
        break;
    default:
        processed = false;
        break;
    }

    return processed;
}
