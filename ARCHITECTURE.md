
# So1l Firmware Architecture

## 1. Purpose and Scope

This document describes the firmware architecture of **So1l**, an embedded soil sensing and environmental data acquisition module built around the ESP32-C3 and developed in C with ESP-IDF.

It has two purposes:

1. Describe the architecture that can be verified in the current repository.
2. Establish clear boundaries and design rules for extending the firmware without coupling sensor acquisition, measurement processing, visualization, and communication.

**Implementation status matters:** this document distinguishes behavior visible in the current source from architectural intentions and implementation details that still need verification. A module name or public function call does not, by itself, prove how that module implements its internals.

## 2. Current Repository Layout

The current firmware entry point is:

```text
firmware/
├── src/
│   └── main.c
├── CMakeLists.txt
└── platformio.ini
```

The repository also contains a root-level `ARCHITECTURE.md`, this document.

The current PlatformIO configuration selects:

- Platform: `espressif32`
- Board profile: `esp32-c3-devkitm-1`
- Framework: `espidf`
- Serial monitor speed: `115200`

The configured board profile is a generic ESP32-C3 development-kit profile. The physical prototype uses an ESP32-C3 Super Mini. These names should not be assumed to describe identical board layouts or pin assignments. Validate the selected board's pinout and electrical constraints before finalizing GPIO configuration.

The firmware project uses ESP-IDF's CMake project integration through `firmware/CMakeLists.txt`.

## 3. Architectural Overview

The current application is coordinated by `app_main()` in `firmware/src/main.c`.

```text
                         ESP32-C3
                            |
                         app_main()
                            |
             +--------------+--------------+
             |              |              |
        Initialization  Measurement     Network
             |          Subsystem       Subsystem
             |              |              |
       +-----+-----+        read()      publish()
       |           |          |              |
   Indicators   SSD1306       |              |
       |         Display      |              |
       |           |          v              |
       |           |    Measurement Record  |
       |           |          |              |
       +-----------+----------+--------------+
                            |
                     Periodic Main Loop
                       5-second interval
```

This diagram represents the orchestration visible in `main.c`. It does not imply that every subsystem is implemented in the same source directory or that the internal modules are synchronous or task-free.

### 3.1 Current Initialization Sequence

The current `app_main()` performs these operations in order:

1. Writes a startup banner to the ESP-IDF log.
2. Initializes the LED indicators.
3. Runs the indicator self-test.
4. Sets the indicator state to `STARTUP`.
5. Initializes the SSD1306 display.
6. If display initialization fails, logs a warning and continues in headless mode.
7. Initializes the measurement subsystem.
8. Initializes the network subsystem.
9. Enters the periodic measurement loop.

The OLED is therefore not treated as essential to continued sensor operation: failure to initialize it does not prevent the application from entering its main loop.

### 3.2 Periodic Measurement Loop

The application currently defines:

```c
#define SO1L_MEASUREMENT_INTERVAL_MS 5000
#define DISPLAY_RETRY_INTERVAL_CYCLES 6
```

Each iteration performs the following operations:

1. Increments the cycle counter.
2. Calls `measurement_read()` to obtain a `measurement` record.
3. Evaluates the validity fields in that record to determine device state.
4. Updates the LED indicators.
5. Updates the OLED if the display is available.
6. If the OLED is unavailable, retries display initialization every six measurement cycles.
7. Calls `network_publish_measurement()` with the measurement record.
8. Delays using `vTaskDelay(pdMS_TO_TICKS(...))`.

The nominal delay between loop iterations is five seconds, plus the time consumed by the operations in each iteration. It should therefore not be described as a precision five-second sampling clock.

The visible application loop is sequential. The use of FreeRTOS delay functions does not, by itself, establish that the application has multiple concurrent tasks. Any additional tasks created inside subsystem modules should be documented where those implementations are available.

## 4. Subsystem Responsibilities

The following boundaries are derived from the interfaces used by `main.c`. Internal behavior not visible at that boundary is intentionally not assumed.

### 4.1 Application Orchestration — `main.c`

**Observed interface:** `app_main()`

`main.c` is the application entry point and coordinates the high-level operating sequence.

Its current responsibilities include:

- Initializing the main subsystems.
- Scheduling periodic measurement cycles.
- Evaluating the overall device state from measurement validity.
- Coordinating display and indicator updates.
- Passing measurement records to the network subsystem.
- Logging initialization failures and startup information.

The entry point should remain an orchestration layer. Sensor protocols, ADC conversion, display drawing, Wi-Fi connection management, and telemetry serialization should be implemented in their respective modules rather than accumulated in `main.c`.

### 4.2 Measurement Subsystem

**Observed interfaces:**

```c
measurement_subsystem_init();
measurement measurement_read();
```

**Observed measurement fields used by `main.c`:**

- `temperature_valid`
- `moisture_raw_valid`
- `moisture_percent_valid`

The measurement subsystem provides the application with a `measurement` record. The main loop uses its validity fields to evaluate system state, passes the record to the display, and passes it to the network publisher.

The internal sensor-driver implementation, sampling strategy, calibration procedure, and measurement data structure definition should be documented alongside the corresponding source and header files.

A useful responsibility boundary for this subsystem is:

- Acquire readings from physical sensors.
- Preserve raw sensor output where applicable.
- Produce calibrated or derived values only when their calibration is defined.
- Track validity independently for each measurement.
- Make acquisition failures distinguishable from valid numeric readings.

The application should not interpret an unavailable sensor reading as a real temperature or moisture value.

### 4.3 Indicator Subsystem

**Observed interfaces:**

```c
indicators_init();
indicators_self_test();
indicators_set_state(indicator_state_t state);
```

The indicator subsystem exposes a state-based interface to the application.

The current application recognizes these states:

- `INDICATOR_STATE_STARTUP`
- `INDICATOR_STATE_NORMAL`
- `INDICATOR_STATE_DEGRADED`
- `INDICATOR_STATE_CRITICAL`

The indicator module is responsible for translating a requested logical state into physical LED output. GPIO assignments, LED polarity, timing patterns, and the mapping from states to individual LEDs belong in the indicator implementation and hardware documentation.

### 4.4 Display Subsystem

**Observed interfaces:**

```c
esp_err_t display_init();
bool display_is_available();
void display_show_measurement(
    const measurement *m,
    const char *device_state
);
```

The display subsystem provides an interface for initializing the OLED, checking availability, and rendering a measurement with a textual device-state label.

Current application behavior:

- Attempts display initialization during startup.
- Continues without the display if initialization fails.
- Displays the current measurement and device state when available.
- Retries initialization every six measurement cycles while unavailable.

The display module should own the display communication and rendering details. It should consume measurement data supplied by the application rather than independently acquiring sensor readings.

The exact I²C configuration, display address, rendering layout, and recovery behavior should be verified in the display implementation.

### 4.5 Network Subsystem

**Observed interfaces:**

```c
network_init();
network_publish_measurement(&m);
```

The application initializes the network subsystem once during startup and requests publication of the measurement on each loop iteration.

`main.c` includes `network.h` and `wifi.h`, but does not directly call Wi-Fi functions. This indicates that the application-level integration is currently exposed through the network interface. The precise division of responsibilities between the network and Wi-Fi modules must be confirmed from their implementations.

A maintainable separation is:

- **Wi-Fi module:** connection setup, connection state, disconnection handling, and reconnection.
- **Telemetry module:** conversion of measurement records into a defined external data representation.
- **Network module:** coordination of publication and transport.

The current call to `network_publish_measurement()` does not establish whether messages are buffered, retried, acknowledged, or persisted when connectivity is unavailable. Those guarantees must only be documented after they are implemented and tested.

## 5. Device-State Evaluation

The current application evaluates device state using three measurement-validity flags.

| Condition | Resulting state |
|---|---|
| Temperature invalid **and** raw moisture invalid | `CRITICAL` |
| Temperature invalid **or** raw moisture invalid **or** moisture percentage invalid, without both primary readings being invalid | `DEGRADED` |
| All three validity flags are true | `NORMAL` |

In code, the evaluation order is significant: the critical condition is checked first, followed by the degraded condition.

This is an application-level health assessment based on measurement availability. It should not be interpreted as a complete diagnosis of hardware health, sensor accuracy, soil conditions, or network connectivity.

The current evaluation does not use network status or display availability to determine the device state.

## 6. Hardware Interface Boundaries

The physical prototype uses the following components and interfaces. The descriptions below establish the intended hardware/software boundary; exact GPIO assignments and driver implementation details must be taken from the current hardware configuration and source code.

| Component | Electrical / communication interface | Firmware responsibility |
|---|---|---|
| ESP32-C3 Super Mini | MCU GPIO and peripheral interfaces | Executes the firmware and provides peripheral controllers |
| DS18B20 waterproof temperature sensor | 1-Wire digital bus | Requests conversions and reads digital temperature data |
| Capacitive Soil Moisture Sensor v1.2 | Analog output to an ADC-capable input | Samples raw ADC values and applies documented calibration |
| SSD1306 OLED | I²C (SDA and SCL) | Initializes the display and renders supplied data |
| Status LEDs | Digital GPIO outputs | Translates logical indicator states into physical outputs |
| DS18B20 pull-up resistor | 4.7 kΩ between 1-Wire data and 3.3 V | Provides the required bus pull-up |
| LED resistors | 330 Ω in series, one per LED | Limits LED current |

All peripherals must use compatible logic levels and share a common ground with the ESP32-C3. The moisture sensor's analog output must remain within the ESP32-C3 ADC input limits.

GPIO assignments should be centralized in a hardware configuration rather than scattered across modules. Do not assume that a pin is available solely because it appears as a GPIO in a generic ESP32 example: verify the exact board pinout, ADC capability, boot-strapping constraints, and peripheral conflicts.

## 7. Data Flow and Measurement Integrity

The current application-level flow is:

```text
Physical Sensors
      |
      v
Measurement Subsystem
      |
      v
measurement_read()
      |
      v
Measurement Record
      |
      +--------------------+
      |                    |
      v                    v
Device-State          OLED Display
Evaluation             (if available)
      |
      v
LED Indicators

Measurement Record
      |
      v
Network Publication Request
```

The measurement record is the shared application-level representation passed to the display and network layers.

For scientific traceability, the measurement model should preserve distinctions between:

- Raw sensor readings.
- Calibrated or derived values.
- Measurement validity.
- Acquisition errors.
- Measurement time and its source.

A raw ADC value is not inherently a soil-moisture percentage. Any percentage or physical interpretation must be supported by a documented calibration procedure for the actual sensor and measurement conditions.

Likewise, a timestamp should not imply synchronized wall-clock time unless a valid time source has been established. If uptime is used before clock synchronization, that fact should remain identifiable in the data.

## 8. Error Handling and Degraded Operation

The current `main.c` explicitly supports one degraded operating condition: the OLED can be unavailable while the rest of the application continues running.

The application logs the initial display failure, continues in headless mode, and periodically retries initialization.

Other error-handling guarantees depend on subsystem implementations and should be verified before being described as supported. In particular, inspect and test:

- DS18B20 discovery and communication failures.
- ADC configuration and sampling failures.
- Invalid or out-of-range measurement handling.
- Wi-Fi disconnection and reconnection.
- Telemetry serialization errors.
- Network publication failures and possible data loss.
- Repeated peripheral initialization and recovery.

A failure in one optional subsystem should not silently turn an invalid measurement into a valid one or prevent unrelated subsystems from operating unless the system's safety requirements explicitly demand that behavior.

## 9. Build and Runtime Configuration

The current PlatformIO configuration is:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = espidf
monitor_speed = 115200
```

The firmware's root CMake file delegates project configuration to ESP-IDF:

```cmake
cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(firmware)
```

The application uses ESP-IDF logging and FreeRTOS APIs. The visible entry point uses `ESP_LOGI`, `ESP_LOGW`, `esp_err_to_name`, and `vTaskDelay`.

Build, upload, and serial-monitor commands should be kept consistent with the repository's PlatformIO environment. If the physical board requires a different board definition or upload configuration, update and document that configuration rather than relying on an unverified profile.

## 10. Design Rules for Future Development

The following rules are recommended as the firmware grows:

1. **Keep `main.c` focused on orchestration.** It should coordinate modules, not implement sensor protocols or telemetry formatting.
2. **Keep hardware drivers behind module interfaces.** Other modules should not need to know the low-level details of 1-Wire, ADC, I²C, or GPIO operations.
3. **Separate raw and calibrated data.** Preserve raw readings so calibration can be reviewed and measurements can be reproduced.
4. **Represent invalid data explicitly.** Use validity or error information rather than magic numeric values.
5. **Keep display and network consumers independent.** A display failure or network outage should not invalidate an otherwise successful sensor acquisition.
6. **Centralize hardware configuration.** Keep GPIO assignments and peripheral configuration in a clearly defined location.
7. **Keep credentials out of version control.** Wi-Fi secrets and other private configuration must not be committed to the public repository.
8. **Document behavior that is tested.** Do not claim retry, buffering, persistence, timing guarantees, or sensor accuracy without implementation and evidence.
9. **Prefer small, testable modules.** Each module should have a clear responsibility and a narrow public interface.
10. **Update this document with architectural changes.** The documentation should evolve with the implementation rather than describe a permanently fixed design.

## 11. Next Architecture Work

The next useful steps are to inspect and document the implementations behind the interfaces already called by `main.c`:

- Measurement acquisition and the `measurement` data structure.
- Temperature sensor driver and 1-Wire handling.
- Moisture ADC configuration, sampling, and calibration.
- Indicator GPIO configuration and state mapping.
- SSD1306 I²C configuration and display recovery.
- Wi-Fi connection management and telemetry publication.

Once those implementations are verified, this document can be extended with exact source paths, module dependencies, pin mappings, data structures, and error contracts.

---

**Documentation status:** This document describes the application orchestration visible in the current repository and identifies subsystem boundaries from the interfaces used by `main.c`. Internal behavior not verified from the corresponding source files is intentionally marked as requiring inspection.
