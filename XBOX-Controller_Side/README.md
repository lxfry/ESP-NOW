# Xbox Controller to ESP-NOW Transmitter

## Purpose

This firmware turns an ESP32-S3 into a bridge between a wired Xbox controller and a second ESP32. It reads controller reports through USB Host, extracts buttons, triggers, and joystick positions, and sends a readable text message to the receiver using ESP-NOW.

The current implementation is a bench-test prototype for the controller side of the UAV project. It demonstrates the input and wireless transmission path. It does not implement flight control, actuator commands, or a complete control-link failsafe.

```text
Xbox controller
    | USB cable
    v
ESP32-S3 USB Host
    | Validate and decode controller report
    v
Text message
    | ESP-NOW, channel 1
    v
Receiving ESP32
```

The receiver firmware is separate from this folder. No router or IP connection is configured by this application.

## Current bench configuration

| Item | Configuration in this project |
| --- | --- |
| Target | ESP32-S3 |
| Resolved ESP-IDF version | 6.1.0, recorded in `dependencies.lock` |
| Resolved USB Host component | `espressif/usb` 1.6.1 |
| Accepted controller USB IDs | VID `0x045E`, PID `0x0B12` |
| USB interface | Interface 0, alternate setting 0 |
| USB endpoints | IN `0x82`, OUT `0x02` |
| USB transfer buffer size | 64 bytes per direction |
| Accepted input report | 48 bytes, header `20 00 <sequence> 2C` |
| Wi-Fi interface | Station mode, with no router connection requested |
| ESP-NOW channel | 1, defined by `ESPNOW_CHANNEL` |
| Receiver station MAC | `E8:F6:0A:A8:D5:68`, defined by `receiver_mac` |
| ESP-NOW encryption | Disabled |
| Wi-Fi power saving | Disabled |
| Serial console | UART0, 115200 baud in the current `sdkconfig` |

The controller IDs, report layout, interface, and endpoints are hard-coded. Other controller variants are not automatically supported. The receiver must use the configured MAC address and channel, or the transmitter settings must be adapted to the bench hardware.

## Bench test

### Objective

The bench test checks whether controller actions can travel through the USB-to-ESP-NOW path and appear correctly at the receiver.


### Procedure and expected observations

1. Power the receiver and start its receiver firmware and serial monitor.
2. Start the transmitter firmware and its serial monitor. Check for the firmware-started and USB-client-ready messages.
3. Connect the controller. Check that the logged VID and PID match the configured values. The firmware then claims the interface and sends the configured controller startup commands.
4. Press buttons, operate both triggers, and move both joysticks. Check that the transmitter logs formatted reports and that matching messages appear at the receiver.
5. Check simultaneous button presses and the return to released/neutral values.
6. Disconnect and reconnect the controller. Check that USB capture stops, resources are released, and reports resume after reconnection.

An illustrative transmitted message is:

```text
Report=12 | LT=0 RT=0 | L=(0,0) R=(0,0) | Buttons=A,LB
```

| Field | Meaning |
| --- | --- |
| `Report` | Sequence byte from the controller's USB report; not a delivery counter |
| `LT`, `RT` | Raw unsigned trigger values |
| `L`, `R` | Raw signed X/Y joystick values; no dead zone or normalization is applied |
| `Buttons` | Comma-separated pressed buttons, or `none` |

The supported button names are A, B, X, Y, Menu, View, Up, Down, Left, Right, LB, RB, LClick, and RClick.

The payload includes a final newline. The terminating C string null byte is not transmitted, so the receiver must respect the received payload length.

### What the test establishes

The transmitter's `ESP-NOW queued` log means that `esp_now_send()` accepted the message for transmission. It does not confirm reception. End-to-end verification requires observing the message at the receiver.

This README describes the test procedure and expected behavior from the source code. It does not record a completed hardware test or measured latency, range, packet loss, or reconnection reliability.

## Code structure

### Project files

```text
.
|-- CMakeLists.txt          Project definition: xbox_controller_side
|-- sdkconfig              Target and ESP-IDF build configuration
|-- dependencies.lock      Resolved component versions
|-- main/
|   |-- CMakeLists.txt      Registers main.c as an application source
|   |-- idf_component.yml  Declares ESP-IDF and USB component dependencies
|   `-- main.c             Application implementation
|-- managed_components/    Dependencies managed by ESP-IDF
`-- build/                 Generated build output
```

### Functions and responsibilities

All application logic currently lives in [main/main.c](main/main.c).

| Function | Responsibility |
| --- | --- |
| `app_main()` | Initializes wireless communication and creates the USB Host task. |
| `wireless_init()` | Initializes NVS and the default event loop, starts Wi-Fi on channel 1, initializes ESP-NOW, and registers the receiver peer. The `esp_netif_init()` call is currently commented out. |
| `usb_host_task()` | Installs the USB Host library, registers a client, creates the client task, and continuously services host-library events. |
| `usb_client_task()` | Processes client events, opens detected devices, checks their VID/PID, starts capture for the matching controller, and closes the device afterward. |
| `usb_client_event_cb()` | Records a newly detected USB address or flags disconnection of the active controller. |
| `read_xbox_packets()` | Claims the interface, allocates reusable IN/OUT transfers, sends startup commands, validates incoming reports, and cleans up when capture ends. |
| `xbox_transfer_done()` | Clears the completed transfer's pending flag through its context pointer. |
| `read_u16_le()` | Combines two little-endian bytes into an unsigned 16-bit value. |
| `read_stick_axis()` | Converts a two-byte joystick field to a signed numeric value. |
| `send_report()` | Decodes report fields, formats a text message, submits it to ESP-NOW, and logs submission status. |

### Execution and resource ownership

- The USB Host task services library-level work, including device enumeration.
- The USB client task handles device notifications and controller transfers. While capture is active, `read_xbox_packets()` continues processing client events.
- USB client and transfer callbacks execute when the client task calls `usb_host_client_handle_events()`. They update state for that task to process.
- ESP-IDF creates its own Wi-Fi and default event-loop tasks during initialization.
- Both application-created USB tasks use priority 5 and a 4096-byte stack.

One transfer and buffer are allocated for each USB direction and reused. Cleanup halts and flushes pending transfers, processes their completion callbacks, frees the transfer objects, and releases the interface. The caller then closes the device handle.

## Future improvements

- [ ] **Add control-link monitoring:** track receiver acknowledgments or heartbeat messages and define receiver behavior when controller input stops or becomes stale.
- [ ] **Measure wireless performance:** record received sequence numbers, loss, latency, and behavior under sustained controller input. Distinguish local submission, radio delivery, and receiver application processing.
- [ ] **Improve recovery:** handle selected NVS initialization failures with an explicit data-preservation policy; define recovery for USB transfer, disconnect, and cleanup failures.
- [ ] **Separate responsibilities:** move USB handling, report decoding, and wireless communication into focused modules; keep `app_main()` as startup coordination.
- [ ] **Define a versioned message format:** separate controller state from display text, with an explicit byte layout if binary messages are introduced.
- [ ] **Control the transmission workload:** consider a bounded queue or latest-state buffer between USB input and wireless sending, and reduce per-report logging when measuring timing.
- [ ] **Make hardware settings configurable:** expose receiver MAC, channel, and supported controller settings instead of embedding them directly in the source.
- [ ] **Improve protocol readability:** name report sizes, offsets, and USB IDs; simplify the longer functions and correct outdated or misleading comments.
- [ ] **Add focused validation:** test report decoding and formatting with known packets; exercise disconnection, reconnection, allocation failures, and receiver absence on hardware.
- [ ] **Evaluate input processing and security:** add joystick calibration/dead zones where required and configure ESP-NOW encryption when moving beyond the unencrypted bench setup.
- [ ] **Align dependency declarations:** update the broad ESP-IDF version constraint and wildcard USB dependency to reflect the versions actually supported and tested.
