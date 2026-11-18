# UAV-Side ESP-NOW Receiver

## Purpose

This ESP-IDF firmware implements the UAV-side receiver for a controller communication bench test. It receives text messages over ESP-NOW and prints them to the serial console so that controller input can be checked at the receiving end of the wireless link.

The companion [Xbox-controller transmitter](../XBOX-Controller_Side/) reads a wired Xbox controller through USB Host, formats its buttons, triggers, and joystick positions as text, and sends that text to this receiver.

```text
Xbox controller
    | USB
    v
Transmitter ESP32-S3
    | ESP-NOW, channel 1
    v
UAV-side ESP32-S3
    | Receive callback -> FreeRTOS queue -> app_main()
    v
Serial console
```

The current application demonstrates reception and display of controller reports. It does not parse those reports into control commands, drive motors or servos, or implement flight control or a link-loss failsafe. No router or IP connection is required by this application.

## Current configuration

| Setting | Value or behavior |
| --- | --- |
| Build target | ESP32-S3, selected in `sdkconfig` |
| Wi-Fi interface | Station mode |
| ESP-NOW channel | `1`, defined by `ESPNOW_CHANNEL` |
| Wi-Fi power saving | Disabled |
| Wi-Fi configuration storage | RAM |
| Receive buffer | 250 bytes per queued message |
| Accepted payload length | 1 to 249 bytes; one additional byte is reserved for the terminating null character |
| Queue capacity | 10 messages |
| Queue overflow | Incoming message is silently dropped when the queue is full |
| Sender filtering | No source MAC check in the application callback |
| Serial console baud rate | 115200 in the current `sdkconfig` |

The receiver prints its station MAC address at startup. The transmitter's `receiver_mac` must match this address, and both boards must use the same radio channel. The companion transmitter currently sends unencrypted packets; this receiver does not configure encrypted peers.

## Bench test

### Objective and equipment

The bench test checks the complete input path: operating the Xbox controller should produce matching text reports on the UAV-side serial console.

The setup requires this receiver board, a second board running the companion transmitter firmware, a compatible wired Xbox controller connected to the transmitter's USB Host interface, and a computer connection for monitoring the receiver. Monitoring the transmitter as well makes it easier to compare sent and received reports.

### Build and start the receiver

From an ESP-IDF-enabled terminal in this folder, build and flash the application, replacing `COM_PORT` with the receiver's actual serial port:

```sh
idf.py build
idf.py -p COM_PORT flash monitor
```

The current project configuration targets ESP32-S3. At startup, the application prints:

```text
UAV station MAC: <receiver station MAC>
Listening for ESP-NOW messages on channel 1
```

### Procedure and expected observations

1. Start the receiver and note the station MAC printed in its console.
2. Set the companion transmitter's `receiver_mac` to that MAC and confirm its `ESPNOW_CHANNEL` matches the receiver. Rebuild and flash the transmitter if its settings change.
3. Start the transmitter and connect the Xbox controller.
4. Press and release buttons, operate both triggers, and move both joysticks. Check that the corresponding reports appear on the receiver console.
5. Check simultaneous button presses and the return to released or neutral values. If monitoring both boards, compare the formatted reports on each console.
6. Stop the transmitter or disconnect the controller. The receiver should stop printing reports once any queued messages have been consumed. Resume input and check that reports appear again.

An illustrative report is:

```text
Report=12 | LT=0 RT=0 | L=(0,0) R=(0,0) | Buttons=A,LB
```

`Report` is the sequence byte from the controller's USB report; `LT` and `RT` are raw trigger values; `L` and `R` contain raw joystick X/Y values; and `Buttons` lists pressed buttons or `none`. The receiver displays this text without interpreting its fields. The transmitter includes the final newline, and the receiver does not add one.

### Scope of the test

Seeing a report on the receiver confirms that the corresponding payload reached the receiving application and was printed. The transmitter's successful send submission alone does not establish this.

The receiver provides no timeout warning when input stops, no application-level acknowledgment, and no delivery or timing statistics. This README describes the setup and expected behavior from the source; it does not record a completed hardware test or measured latency, range, or packet loss.

## Code structure

### Project files

```text
UAV_Side/
|-- README.md              Purpose, bench procedure, and implementation notes
|-- CMakeLists.txt         ESP-IDF project definition: uav_side
|-- sdkconfig              Target and ESP-IDF build configuration
`-- main/
    |-- CMakeLists.txt     Registers main.c and its component dependencies
    `-- main.c             Receiver implementation
```

The main component declares dependencies on `esp_hw_support`, `esp_wifi`, `esp_event`, `esp_netif`, `nvs_flash`, and `freertos`.

### Application responsibilities

All application logic is in [main/main.c](main/main.c).

| Element | Responsibility |
| --- | --- |
| `ESPNOW_CHANNEL` | Sets the radio channel used by the receiver. |
| `MESSAGE_BUFFER_SIZE` | Defines the 250-byte text buffer size. |
| `received_message_t` | Holds a copied, null-terminated text message. |
| `receive_queue` | Transfers messages from the Wi-Fi callback to the main application task. |
| `receive_callback()` | Rejects invalid pointers and payload lengths, copies the received bytes, appends a null terminator, and attempts a nonblocking queue send. |
| `wireless_init()` | Initializes NVS, network infrastructure, and the default event loop; starts Wi-Fi in station mode; selects the channel; disables power saving; initializes ESP-NOW; and registers the receive callback. |
| `app_main()` | Creates the queue, initializes wireless reception, prints the station MAC and channel, then waits for queued messages and prints them. |

### Execution flow and message handling

`app_main()` creates the queue before registering the callback, ensuring the queue is available when reception starts. Initialization and queue-allocation failures are checked with `ESP_ERROR_CHECK()`; there is no application recovery path for these failures.

ESP-NOW invokes `receive_callback()` in the Wi-Fi task. The callback copies the payload into a local message structure, and `xQueueSend()` copies that structure into the queue. It uses a zero wait time so that waiting for console output cannot block the Wi-Fi task. If all 10 queue slots are occupied, the new message is dropped without a log or counter.

The existing main application task waits on `xQueueReceive()` with `portMAX_DELAY` and prints each dequeued message using `printf("%s", message.text)`. The application creates no additional worker task. This separates reception from potentially slower serial printing.

Received data is treated as text: the callback appends a null terminator, but an embedded null byte would cause printing to stop early. Messages of 250 bytes or more are rejected rather than truncated.

## Future improvements

- [ ] **Detect stale input and link loss:** record the last valid reception time, report timeouts, and define a safe response before connecting the receiver to actuators.
- [ ] **Define and validate a message protocol:** introduce a versioned controller-state format, validate fields and ranges, and separate command decoding from console formatting.
- [ ] **Measure communication performance:** add receiver counters, a dedicated packet sequence number, and timing instrumentation to assess loss, latency, and sustained input rates.
- [ ] **Expose queue overflow:** count dropped messages and monitor queue occupancy; evaluate whether a latest-state buffer better suits control inputs than retaining older reports.
- [ ] **Identify and authenticate the sender:** validate the expected source MAC and configure ESP-NOW peer encryption for the intended deployment.
- [ ] **Add return-path status:** send application acknowledgments or receiver health messages when the transmitter needs confirmation of processing.
- [ ] **Make settings configurable:** expose channel, expected sender, queue depth, and logging options through project configuration instead of source edits.
- [ ] **Improve initialization recovery:** handle recoverable NVS and wireless errors explicitly, with a defined policy before erasing persistent storage.
- [ ] **Separate responsibilities as the application grows:** move wireless setup, message validation, link monitoring, and UAV command handling into focused modules.
- [ ] **Extend validation:** check payload boundaries, embedded null bytes, malformed messages, queue saturation, transmitter interruption, and reception recovery on hardware; record results and the tested ESP-IDF version.
