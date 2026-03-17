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
