# So1l — Development Branch

> **An embedded soil sensing and environmental data acquisition module built with C and ESP32-C3, designed for scientific instrumentation and research applications.**

`develop` is the active integration branch of So1l. It contains ongoing firmware development, hardware integration, and documentation changes. It is intended for contributors and testers following the implementation as it evolves.

This branch is not a stable release. Features may be incomplete, hardware-dependent, or not yet validated on the physical prototype.

---

## Project Overview

So1l is an experimental embedded systems project for acquiring and presenting soil-related measurements. The initial prototype uses:

- ESP32-C3 Super Mini (USB-C)
- DS18B20 waterproof digital temperature sensor
- Capacitive Soil Moisture Sensor v1.2
- SSD1306 OLED display over I²C
- Optional GPIO-driven status LEDs
- Wi-Fi-based measurement publishing

The firmware is written in **C**, uses **ESP-IDF**, and is built through **PlatformIO**.

So1l emphasizes reliable acquisition, explicit measurement validity, modular firmware, and reproducible hardware testing. It is an independent instrumentation module—not a complete agricultural monitoring platform.

## Development Status

This table separates application behavior represented in the code from capabilities that still require subsystem-level or physical validation.

| Area | Development status |
|---|---|
| ESP32-C3 firmware | Project configured for ESP-IDF through PlatformIO |
| Application orchestration | Initialization and periodic measurement flow present |
| Measurement representation | Measurement structure and per-reading validity fields used by the application |
| Status indicators | Startup, normal, degraded, and critical states represented in application logic |
| OLED display | Initialization, measurement display, and unavailable-display handling integrated in the application flow |
| Soil temperature | DS18B20 acquisition integrated through the measurement subsystem; physical communication and reliability require validation |
| Soil moisture | Raw ADC and derived percentage validity represented; calibration and repeatability require hardware testing |
| Network publishing | Application invokes the network publishing interface; delivery guarantees and offline behavior require verification |
| Scientific calibration | No validated calibration procedure is claimed by this branch |

**A successful build proves that the code compiles for the selected PlatformIO environment. It does not prove that wiring, sensor communication, ADC readings, calibration, display, or network behavior work correctly on the physical prototype.**

## Hardware

### Core prototype

| Component | Interface | Purpose |
|---|---|---|
| ESP32-C3 Super Mini | MCU | Main processing and control |
| DS18B20 waterproof sensor | 1-Wire | Soil temperature acquisition |
| Capacitive Soil Moisture Sensor v1.2 | Analog output → ADC | Soil-moisture-related signal acquisition |
| 0.96-inch SSD1306 OLED | I²C | Local measurement display |
| 4.7 kΩ resistor | Pull-up | DS18B20 data-line pull-up |

Optional components include green/yellow/red status LEDs, one 330 Ω current-limiting resistor per LED, a breadboard, and jumper wires. The breadboard is a prototyping choice, not a firmware requirement.

### Hardware interfaces

**DS18B20 — 1-Wire**

- `VDD` → 3.3 V
- `GND` → common ground
- `DQ` → configured GPIO
- 4.7 kΩ pull-up between `DQ` and 3.3 V

The DS18B20 is digital; its reading does not use the ESP32-C3 ADC.

**Capacitive moisture sensor — ADC**

- `VCC` → intended 3.3 V supply
- `GND` → common ground
- `AOUT` → ADC-capable GPIO

The ESP32-C3 samples the analog output using its ADC. Raw ADC counts are not a universal moisture percentage. A meaningful estimate requires calibration with the actual sensor and documented reference conditions. Confirm that the output voltage remains within the ESP32-C3 input limits.

**SSD1306 — I²C**

- `VCC` → 3.3 V, subject to the breakout specifications
- `GND` → common ground
- `SDA` → configured I²C SDA GPIO
- `SCL` → configured I²C SCL GPIO

`0x3C` is common for SSD1306 modules, but the address must be confirmed on the actual display.

**Status LEDs — GPIO**

Each LED needs its own series current-limiting resistor. GPIO assignments must be checked against the exact ESP32-C3 Super Mini pinout and any boot-strapping, USB, serial-console, or peripheral constraints.

> **Pin assignments are board-specific.** Do not assume a generic ESP32-C3 development-board mapping applies to every ESP32-C3 Super Mini. Confirm the physical board pinout and firmware configuration before wiring.

All peripherals must share a common ground with the microcontroller.

## Firmware Behavior

At the application level, the firmware follows this general sequence:

1. Initialize status indicators and run their startup indication.
2. Initialize the display.
3. Initialize the measurement subsystem.
4. Initialize the network interface.
5. Enter the periodic measurement loop.
6. Read a measurement record.
7. Evaluate measurement validity.
8. Update indicators and, when available, the OLED.
9. Pass the measurement to the network publishing interface.
10. Wait before the next acquisition cycle.

The application defines a nominal measurement interval of **5 seconds**. This is not a guarantee of exact five-second sampling: execution time and scheduling affect the actual period.

If OLED initialization fails, the application can continue without the display and periodically retry initialization. The retry interval is defined as six measurement cycles.

### Device state

The application derives a high-level state from measurement validity:

- **Critical:** temperature and raw moisture acquisition are both invalid.
- **Degraded:** at least one required reading is invalid, or the derived moisture percentage is invalid.
- **Normal:** temperature, raw moisture, and moisture percentage are all valid.

These states describe the application's current validity assessment. They are not a complete hardware-health diagnostic system.

### Measurement integrity

So1l distinguishes raw acquisition, derived or calibrated values, and validity information. Raw ADC readings should remain separate from calibrated moisture estimates. Invalid readings should remain explicitly invalid rather than being silently replaced with plausible-looking values.

## Software Architecture

| Component | Responsibility |
|---|---|
| Application entry point | Coordinates initialization and the acquisition cycle |
| Measurement subsystem | Collects and organizes sensor measurements |
| Temperature driver | Handles DS18B20 communication and temperature acquisition |
| Moisture acquisition | Samples the analog output and derives moisture-related values |
| Display subsystem | Presents measurements and device state on the OLED |
| Indicators subsystem | Controls status LEDs |
| Network subsystem | Provides the application-facing measurement publishing interface |
| Wi-Fi support | Supports wireless connectivity |

The application layer should coordinate components without absorbing their hardware-specific implementation. Sensor acquisition should not depend on the display or network being available.

See [`ARCHITECTURE.md`](ARCHITECTURE.md) for module boundaries and implementation details.

## Build and Run

### Requirements

- ESP32-C3 development board
- USB data cable
- Visual Studio Code with PlatformIO, or PlatformIO Core CLI
- PlatformIO-installed toolchain and ESP-IDF dependencies

### Build

From the repository root:

```bash
pio run -d firmware
```

### Upload

Connect the board and select the correct serial port:

```bash
pio run -d firmware -t upload
```

If automatic port detection fails, configure the upload port in `firmware/platformio.ini` or use the appropriate PlatformIO upload-port option.

### Serial monitor

```bash
pio device monitor -d firmware -b 115200
```

The configured monitor speed is 115200 baud. Check the board's USB/UART and console configuration if no output appears.

### Board configuration warning

The PlatformIO environment uses the generic `esp32-c3-devkitm-1` board definition, while the physical prototype is an ESP32-C3 Super Mini. These may not have identical layouts. Verify flash settings, USB/serial behavior, GPIO mapping, ADC-capable pins, and peripheral conflicts.

A successful compile with a generic board profile does not validate the physical board pinout.

## Hardware Validation Checklist

- [ ] Confirm the exact ESP32-C3 board variant and pinout.
- [ ] Verify 3.3 V and common-ground connections.
- [ ] Confirm DS18B20 presence detection and communication.
- [ ] Validate scratchpad data and CRC handling, where implemented.
- [ ] Compare temperature readings against a reasonable reference.
- [ ] Confirm moisture ADC readings change under controlled conditions.
- [ ] Record raw ADC values for documented dry and wet reference conditions.
- [ ] Establish and document calibration before interpreting readings as percentages.
- [ ] Confirm OLED I²C address and stable rendering.
- [ ] Verify LED polarity, resistors, and GPIO assignments.
- [ ] Test behavior when a sensor is disconnected or returns invalid data.
- [ ] Verify Wi-Fi connection and actual delivery to the intended receiver.
- [ ] Record test conditions, limitations, and measurement instability.

Do not mark a feature as hardware-validated solely because the firmware compiles or reaches its main loop.

## Known Limitations and Work in Progress

Treat the following as active development or validation work unless accompanied by test evidence:

- DS18B20 communication and temperature-reading reliability on the physical prototype.
- Moisture-sensor calibration and repeatability across soil conditions.
- Accuracy and timing of the complete acquisition cycle.
- OLED and GPIO behavior across resets and peripheral failures.
- Network delivery behavior, including disconnects, retries, and recovery.
- Telemetry format, receiver integration, and data persistence.
- Long-duration operation and measurement stability.

The presence of a network publishing function does not imply guaranteed delivery, buffering, persistence, or lossless operation.

## Development Workflow

`develop` is the integration branch for ongoing work. A practical workflow is:

```text
main
└── develop
    ├── feature/ds18b20-validation
    ├── feature/moisture-adc
    ├── feature/display-reliability
    └── feature/network-telemetry
```

Use focused branches when a change benefits from isolated implementation and testing. Merge completed work into `develop` after reviewing the code and recording relevant test results. Keep `main` for stable, documented milestones.

Example commit messages:

```text
docs: clarify hardware interface requirements
test: add DS18B20 communication validation
fix: handle invalid temperature readings
feat: improve moisture ADC acquisition
docs: record prototype validation results
```

## Project Goals

The ongoing work aims to establish reliable temperature and soil-moisture-related acquisition, explicit measurement validity, traceable raw readings, documented calibration, maintainable C firmware, local visualization, structured wireless publishing, and reproducible hardware testing.

The priority is trustworthy, interpretable measurement data—not feature count.

## License

So1l is distributed under the MIT License. See [`LICENSE`](LICENSE).
"""
