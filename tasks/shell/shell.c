//
// Created by wolfboy on 3/9/2026.
//

#include <string.h>

#include "scheduler.h"
#include "text_service.h"
#include "microshell/src/microshell.h"

extern void kelp_cmd_signal_callback(struct ush_object *self, struct ush_file_descriptor const *file, int argc, char *argv[]);
extern void kelp_cmd_cpu_callback(struct ush_object *self, struct ush_file_descriptor const *file, int argc, char *argv[]);
extern void kelp_cmd_bench_callback(struct ush_object* self, struct ush_file_descriptor const* file, int argc, char* argv[]);
extern void kelp_cmd_bench_service(struct ush_object *self, struct ush_file_descriptor const *file);

// non-blocking read interface
static int ush_read(struct ush_object* self, char* ch) {
    char c = kelp_text_read_input_char();

    if (c != '\0') {
        *ch = c;
        return 1;
    }

    return 0;
}

// non-blocking write interface
static int ush_write(struct ush_object* self, char ch) {

    if (kelp_text_send_output_char(ch) == 0) {
        return 1;
    }

    return 0;
}

// I/O interface descriptor
static const struct ush_io_interface ush_iface = {
    .read = ush_read,
    .write = ush_write,
};

#define BUF_IN_SIZE    64
#define BUF_OUT_SIZE   64
#define PATH_MAX_SIZE  64

static char ush_in_buf[BUF_IN_SIZE];
static char ush_out_buf[BUF_OUT_SIZE];

// microshell instance handler
static struct ush_object ush;

// microshell descriptor
static const struct ush_descriptor ush_desc = {
    .io = &ush_iface, // I/O interface pointer
    .input_buffer = ush_in_buf, // working input buffer
    .input_buffer_size = sizeof(ush_in_buf), // working input buffer size
    .output_buffer = ush_out_buf, // working output buffer
    .output_buffer_size = sizeof(ush_out_buf), // working output buffer size
    .path_max_length = PATH_MAX_SIZE, // path maximum length (stack)
    .hostname = "kelp", // hostname (in prompt)
};

// root directory handler
static struct ush_node_object root;

const struct ush_file_descriptor g_ush_kelp_commands[] = {
    {
        .name = "sig",
        .description = "send a signal to a task",
        .help = "usage: sig [task id] [signal]\r\n",
        .exec = kelp_cmd_signal_callback,
    },
    {
        .name = "cpu",
        .description = "get cpu related information",
        .help = "usage: cpu <temp/usage/freq>\r\n",
        .exec = kelp_cmd_cpu_callback,
    },
    {
        .name = "bench",
        .description = "run the CoreMark benchmark",
        .help = "usage: bench\r\n",
        .exec = kelp_cmd_bench_callback,
        .process = kelp_cmd_bench_service,
    },
};

struct ush_node_object kelp_commands;

void kelp_task_shell(uint32_t pid, uint32_t* signals, char* args) {
    // initialize microshell instance
    ush_init(&ush, &ush_desc);

    ush_commands_add(&ush, &kelp_commands, g_ush_kelp_commands, sizeof(g_ush_kelp_commands) / sizeof(g_ush_kelp_commands[0]));

    // mount root directory (root must be first)
    ush_node_mount(&ush, "/", &root, NULL, 0);

    task_request_stack(128);

    uint32_t l = 0;

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                ush_deinit(&ush);
                return;
            }

            l = 0;
        }

        // non-blocking microshell service
        ush_service(&ush);

        l++;

        task_sleep_ms(1);
    }
}
