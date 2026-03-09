//
// Created by wolfboy on 3/9/2026.
//

#include "scheduler.h"
#include "text_service.h"
#include "microshell/src/microshell.h"

// non-blocking read interface
static int ush_read(struct ush_object* self, char* ch) {
    char c = kelp_text_read_char();

    if (c != '\0') {
        *ch = c;
        return 1;
    }

    return 0;
}

// non-blocking write interface
static int ush_write(struct ush_object* self, char ch) {
    // TODO make this use the service somehow
    printf("%c", ch);
    return 1;
}

// I/O interface descriptor
static const struct ush_io_interface ush_iface = {
    .read = ush_read,
    .write = ush_write,
};

#define BUF_IN_SIZE    32
#define BUF_OUT_SIZE   32
#define PATH_MAX_SIZE  32

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

void kelp_task_shell(uint32_t pid) {
    // initialize microshell instance
    ush_init(&ush, &ush_desc);

    // mount root directory (root must be first)
    ush_node_mount(&ush, "/", &root, NULL, 0);

    while (1) {
        // non-blocking microshell service
        ush_service(&ush);

        task_sleep_ms(5);
    }
}
