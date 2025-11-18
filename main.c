#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"




int main()
{
    stdio_init_all();

    // Watchdog example code
    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        // Whatever action you may take if a watchdog caused a reboot
    }
    
    // Enable the watchdog, requiring the watchdog to be updated every 100ms or the chip will reboot
    // second arg is pause on debug which means the watchdog will pause when stepping through code
    watchdog_enable(100, 1);
    
    // You need to call this function at least more often than the 100ms in the enable call to prevent a reboot
    watchdog_update();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
