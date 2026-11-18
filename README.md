# ESP-NOW Controller Link

Two ESP32-S3 boards relay wired Xbox controller input over ESP-NOW for UAV communication bench testing.

```text
Xbox controller -> USB Host ESP32-S3 -> ESP-NOW -> UAV ESP32-S3 -> Serial console
```

## Repository structure

| Folder | Firmware role |
| --- | --- |
| [XBOX-Controller_Side](./XBOX-Controller_Side/README.md) | Initializes the controller through USB Host, decodes buttons, triggers, and joysticks, and sends text reports over ESP-NOW. |
| [UAV_Side](./UAV_Side/README.md) | Receives reports through a FreeRTOS queue and prints them to the serial console. |

Each folder is a standalone ESP-IDF project that is built and flashed separately.

## Quick start

Requires an ESP-IDF environment, two ESP32-S3 boards, and a wired Xbox controller with USB VID/PID `045E:0B12` connected to the transmitter's USB Host interface.

1. Build and flash `UAV_Side`, then note its station MAC address in the serial monitor.
2. Set `receiver_mac` in `XBOX-Controller_Side/main/main.c` to that address. Keep `ESPNOW_CHANNEL` identical on both boards, currently `1`.
3. Build and flash `XBOX-Controller_Side`, connect the controller, and check input reports on the receiver's console.

Run these commands inside each firmware folder, replacing `COM_PORT` with the board's serial port.

```sh
idf.py build
idf.py -p COM_PORT flash monitor
```

Example report at 115200 baud

```text
Report=12 | LT=0 RT=0 | L=(0,0) R=(0,0) | Buttons=A,LB
```

## Status

USB report decoding, controller disconnect handling, ESP-NOW transmission, and queued reception are implemented. The current link uses unencrypted text messages. Flight-controller integration and link-loss failsafe behavior remain future work. See each firmware README for implementation details and bench-test procedures.
