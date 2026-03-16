# KelpOS-Lite
A simple multitasking OS for the RP2 family of microcontrollers

This is *not* designed to be used like an RTOS but instead aims to provide a more dynamic experience like that of older desktop system, similar to PicoMit.

The goal is to have multiple methods of I/O such as (**Bold** being prioritized):
 - **USB keyboards**, **serial** and wireless connections for text
 - **SPI**, E-Ink, VGA, and **HDMI** video with various color settings
 - **Flash**, **SD Cards**, USB Drives, [PingFS](https://github.com/yarrick/pingfs), and FRAM for data storage
 - **PWM** and I2S audio
 - **Wifi** and RNS for networking
 - Multiple file systems such as FAT32 and **littleFS**

Such interfaces will be provided using services that will make it so no changes in application code will be need to change interfaces.
These interfaces will also allow input to come from multiple sources (such as from keyboard *and* serial), and leave to multiple places (to an on-screen terminal *and* over ssh)

The goal includes not only providing programming-orientated software but media and other desktop applications (**Bold** being prioritized):
 - **Audio Playback** (mp3, wav)
 - **Image Displaying** (jpg, png)
 - Emulation (Game Boy, NES, Sega Genesis)
 - **Text Editor**
 - **Code Editor**
 - **Programming Languages** (Micro Python, MMBasic, Chai Script, Java?)

# Programmer's Notes

## Task Priority Guidelines
| Task Priority |   Description    | Usage                                                                                                                                                                                                                  |
|:-------------:|:----------------:|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|    112-128    | DO NOT INTERRUPT | Only use if the task must NOT get interrupted. Better yet, don't use this.                                                                                                                                             |
|    96-111     |    Real-time     | About as high as any task should get, but beware using it too much, as if more than one task as this priority it could prevent the OS from running. If using only one core, the same thing could happen with only one. |
|     80-95     |   System Tasks   | Where most tasks belonging to the OS reside.                                                                                                                                                                           |
|     64-79     |       High       | High priority user or application code or low priority OS tasks. Better to use this than Real-time.                                                                                                                    |
|     48-63     |      Medium      | Most user and application code.                                                                                                                                                                                        |
|     32-47     |       Low        | Low priority user and application code.                                                                                                                                                                                |
|     16-31     |    Background    | Tasks that do not need to happen while other things are going on, and are OK only using left-over time.                                                                                                                |
|     0-15      |       Idle       | Used for tasks made to replace the default idle tasks (except for 0, the priority of the default idle tasks)                                                                                                           |


## Task ID Guidelines
| Task ID |              Description               |
|:-------:|:--------------------------------------:|
|  0-99   | Idle tasks (leaves room for 100 cores) |
| 100-199 |                Drivers                 |
| 200-299 |            System Services             |
| 300-399 |          Other OS Components           |
|  400+   |       User and Application Tasks       |

## Existing OS Components

### Services

Text Service
 - Header: `tasks/service/include/text_service.h`
 - Task ID: `TEXT_SERVICE_PID` = 200

### Drivers

USB HID Driver
 - Header: `tasks/drivers/include/usb_hid.h`
 - Task ID: `USB_HID_DRIVER_PID` = 100

| Area | Current Implementation | Suggested Improvement | Rationale |
|------|------------------------|-----------------------|-----------|
| Spin‑lock granularity | Entire scheduler state protected by a single scheduler_spin_lock() in many functions. | Split locks: one for task list, another for per‑core metrics (ticks_executing, ticks_idling). | Reduces contention, especially when multiple cores call get_next_task() or update statistics concurrently. |
| Task array traversal | Loops over MAX_TASKS even when only a few tasks are active (e.g., in calculate_stack_usage(), get_next_task()). | Maintain a linked list or ring buffer of active task indices and iterate that instead. | O(active_tasks) vs O(MAX_TASKS). On RP2040, memory is tight; fewer iterations mean lower cache pressure and less interrupt latency. |
| Stack monitoring | Linear scan from stack_hwm to end for each task in calculate_stack_usage(). | Pre‑compute a bitmap of used stack words or use a high‑watermark update during context switch. | Eliminates per‑tick scans; only updates when the task actually writes to the stack. |
| Dynamic stack resizing | Calls realloc() inside resize_stack() and find_and_resolve_stack_overflow(). | Use a fixed pool allocator or bump‑pointer allocator for stacks, or pre‑allocate maximum size once per task. | Avoids heap fragmentation; RP2040’s heap is limited and realloc may fail spuriously. |
| Task creation | Allocates stack with malloc() each time in task_add_args(). | Allocate a contiguous pool of stack space for all tasks at startup, or use static arrays for small numbers of tasks. | Reduces runtime allocation overhead and improves deterministic latency. |
| Priority comparison | Uses signed 16‑bit int16_t highest_priority initialized to -1; compares with unsigned task->priority. | Store priority as signed and initialize to the lowest possible value (-1). | Simplifies logic, removes potential signed/unsigned mismatch warnings. |
| State transitions | Multiple places set task state to TASK_READY, TASK_RUNNING, etc., often surrounded by lock/unlock pairs. | Encapsulate state changes in inline helper functions that acquire/release the spin‑lock once per operation. | Less boilerplate, reduces chance of forgetting a unlock. |
| Branch prediction | Frequent checks like if (potential_task->state == TASK_DEAD) inside tight loops. | Use compiler hints: __builtin_expect(potential_task->state == TASK_DEAD, 0). | Improves instruction pipeline on Cortex‑M0+. |
| get_next_task() loop | Uses a simple circular scan starting at current index and checks every task until one is found. | Maintain a priority queue (e.g., binary heap) of ready tasks. | Guarantees O(log n) selection; avoids scanning idle slots. |
| Use of absolute_time_t | Calls get_absolute_time() once per call to get_next_task(). | Cache the current time and only update when needed, or use a tick counter derived from SysTick. | Reduces expensive time conversion on each task switch. |
| CPU usage calculation | Divides by zero guard: if (ticks_executing <= 0) { core_usage = 0; } else { ... }. | Compute using floating point or fixed‑point once per tick and store as a scaled integer. | Avoids division in hot path if not needed for display. |
| calculate_cpu_usage() & calculate_stack_usage() | Both acquire the same lock, run full scans, then release. | Combine into a single periodic task that runs under one lock, or use separate locks per metric. | Reduces lock hold time and allows other operations to proceed. |
| Task ID uniqueness check | Linear scan in task_exists_no_lock(). | Keep a hash table (e.g., open addressing) of used IDs for O(1) lookup. | Useful if the number of tasks grows; though with small MAX_TASKS the benefit is modest. |
| task_sleep_us() | Uses make_timeout_time_us(us) which internally may call get_absolute_time(). | Inline a simple timeout calculation using current tick count and compare directly in isr_systick(). | Eliminates extra function calls for short sleeps. |
| Memory barriers | No explicit memory barrier after stack allocation or resizing. | Use __sync_synchronize() or the RP2040’s asm volatile("dsb sy") when modifying shared task metadata. | Ensures correct visibility across cores. |
| Code size | Many small inline functions (set_scheduler_started, get_current_task, etc.) scattered throughout. | Group related helpers in a single header/implementation file and mark as static inline. | Reduces relocation overhead on the RP2040’s limited flash. |
