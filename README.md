# ESP-NOW Controller Link

This project uses two ESP32-S3 boards to relay input from a wired Xbox controller over ESP-NOW. The goal is to carry joystick, trigger, and button states to the quadcopter side for eventual integration with its flight controller.

## Repository structure

| Folder | Firmware role |
| --- | --- |
| [`XBOX-Controller_Side/`](./XBOX-Controller_Side/) | USB host that reads and decodes Xbox controller reports, then sends the input state over ESP-NOW. |
| [`UAV_Side/`](./UAV_Side/) | Receiver that reads ESP-NOW packets and makes the decoded input available for later flight controller integration. |

Each folder is a standalone ESP-IDF project that is built and flashed separately.

## Current status

Both projects are initialized and print a startup message. USB report decoding and ESP-NOW communication are planned for the next implementation stages.
