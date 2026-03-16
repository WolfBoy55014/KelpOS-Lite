//
// Created by wolfboy on 3/15/2026.
//

#include <string.h>

#include "scheduler.h"
#include "inc/ush.h"
#include "inc/ush_types.h"

void kelp_cmd_signal_callback(struct ush_object *self, struct ush_file_descriptor const *file, int argc, char *argv[]) {

    static char *signal_names[6] = {
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
    char *endptr;
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
