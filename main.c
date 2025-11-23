#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "scheduler.h"
#include "governor.h"
#include "kernel_config.h"
#include "hardware/clocks.h"

void test_task(const uint32_t pid) {
    volatile uint32_t iterations = 0;
    volatile uint32_t result = 1;

    for (int d = 0; d < 1000000; d += 15000) {
        for (int u = 0; u < 20; ++u) {
            uint64_t start = time_us_64();

            // Do some actual computation (not optimized away)
            for (uint32_t i = 0; i < d; i++) {
                result = (result * 1103515245 + 12345) & 0x7fffffff; // LCG PRNG
                iterations++;
            }

            // Optional: store result somewhere so compiler doesn't optimize it away
            (void)result;

            uint64_t time_taken = time_us_64() - start;

            task_sleep_us(100000 - time_taken);
        }
    }
}

int main() {
    stdio_init_all();

    task_add(kelp_governor, 8, 127);
    task_add(test_task, 9, 127);

    kernel_start();

    while (true) {
        tight_loop_contents();
    }
}
