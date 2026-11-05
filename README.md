# So1l

**An embedded soil sensing and environmental data acquisition module.**

So1l is an experimental embedded systems project focused on soil sensing, environmental data acquisition, and measurement processing. Built around the ESP32-C3 and programmed in C, the module is designed to acquire, process, validate, and transmit soil-related measurements.

The project explores the engineering principles behind sensor interfacing, modular firmware architecture, measurement calibration, data validation, and structured telemetry for scientific and research applications.

## Objectives

- Develop modular and maintainable embedded firmware in C.
- Implement temperature and soil moisture acquisition.
- Establish structured measurement representation and timestamping.
- Explore sensor calibration, signal processing, and data validation.
- Implement local measurement visualization.
- Develop wireless communication and structured telemetry.
- Provide a foundation for external data collection, storage, and analysis.
- Document hardware connections, firmware interfaces, and measurement procedures to support reproducibility.

## Hardware Components

### Core Hardware

The initial hardware configuration consists of:

| Component | Specification | Purpose |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini (USB-C) | Main processing and control unit |
| Temperature Sensor | DS18B20 Waterproof | Soil temperature measurement |
| Soil Moisture Sensor | Capacitive Soil Moisture Sensor v1.2 | Analog soil moisture measurement |
| Display | 0.96" SSD1306 OLED (I²C) | Local measurement visualization |
| Pull-up Resistor | 4.7 kΩ | DS18B20 1-Wire data-line pull-up |

### Optional Components and Prototyping

| Component | Specification | Purpose |
|---|---|---|
| Status LEDs | Green, yellow, and red LEDs | Visual indication of system states |
| Current-Limiting Resistors | 330 Ω (one per LED) | Limit current through each LED |
| Prototyping Board | 830-point breadboard | Circuit assembly and prototyping |

The status LEDs are intended to provide visual feedback about system operation, connectivity, and measurement status. Their exact state assignments will be defined during firmware development.

The breadboard is an assembly choice rather than a requirement of the firmware. The initial prototype uses an 830-point breadboard, but other options can be considered, including prototyping PCBs and custom-designed printed circuit boards.

3D models for an electronics enclosure and housing may be added to the repository in a future development stage.

## Hardware Interfaces and Firmware Integration

The ESP32-C3 communicates with the sensors and peripheral components through dedicated hardware interfaces. Each interface is handled by a corresponding firmware module, keeping hardware-level operations separate from measurement processing and application logic.

### 1. DS18B20 — Temperature Acquisition

**Communication interface:** 1-Wire digital protocol.

The DS18B20 communicates with the ESP32-C3 through a single data line. The sensor uses a digital protocol, so its temperature reading does not require an ADC input.

**Electrical connections:**

| DS18B20 Pin | ESP32-C3 / Circuit |
|---|---|
| VDD | 3.3 V |
| GND | GND |
| DQ | Configured GPIO |
| Pull-up | 4.7 kΩ between DQ and 3.3 V |

The 4.7 kΩ resistor provides the pull-up required by the 1-Wire data bus. The sensor and microcontroller must share a common ground.

**Firmware responsibilities:**

The `temperature` module will handle:

- Initializing the 1-Wire communication interface.
- Detecting and communicating with the sensor.
- Requesting temperature conversions.
- Reading the returned temperature value.
- Handling communication errors and invalid readings.
- Exposing temperature measurements to the measurement management layer.

Temperature acquisition should account for the sensor's conversion time and configured resolution. Failed communication must be reported as an acquisition error rather than silently treated as a valid temperature.

### 2. Capacitive Soil Moisture Sensor — Analog Acquisition

**Communication interface:** Analog voltage through the ESP32-C3 ADC.

The capacitive moisture sensor produces an analog output voltage related to the surrounding soil conditions. The ESP32-C3's Analog-to-Digital Converter (ADC) samples this voltage and converts it into a digital value that firmware can process.

**Electrical connections:**

| Sensor Pin | ESP32-C3 / Circuit |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| AOUT | Configured ADC-capable GPIO |

Powering the sensor from 3.3 V is the intended initial configuration. The analog output must remain within the ESP32-C3's permitted input voltage range. Never connect an output that may exceed the microcontroller's electrical limits directly to an ADC pin.

**Firmware responsibilities:**

The `moisture` module will handle:

- Configuring the appropriate ADC unit and channel.
- Sampling the sensor's analog output.
- Handling raw ADC readings.
- Applying ADC calibration where supported.
- Filtering or averaging samples to reduce measurement noise.
- Exposing processed readings to the measurement management layer.

Raw ADC values are not automatically equivalent to soil moisture percentages. Meaningful moisture estimates require experimental calibration using the actual sensor and representative soil conditions.

A calibration procedure should record sensor readings under defined reference conditions, such as dry and sufficiently saturated soil. Calibration values and conversion logic should be documented rather than assuming a universal voltage-to-moisture relationship.

### 3. SSD1306 OLED — Local Visualization

**Communication interface:** I²C.

The SSD1306 display communicates with the ESP32-C3 through the I²C bus, using two signal lines:

- **SDA:** Serial Data.
- **SCL:** Serial Clock.

**Electrical connections:**

| OLED Pin | ESP32-C3 / Circuit |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | Configured I²C SDA GPIO |
| SCL | Configured I²C SCL GPIO |

The display's I²C address must be confirmed for the specific module. Common SSD1306 modules use address `0x3C`, although this should be verified during hardware initialization.

**Firmware responsibilities:**

The `display` module will handle:

- Initializing the I²C master bus.
- Configuring and communicating with the display.
- Rendering measurement values and system information.
- Updating the display without taking responsibility for sensor acquisition.
- Handling display initialization and communication failures.

The display should consume measurement data from the measurement management layer. It should not independently read the sensors.

### 4. Status LEDs — System State Indication

**Communication interface:** Digital GPIO output.

Each LED is controlled by an ESP32-C3 GPIO configured as a digital output.

**Electrical connections:**

Each LED must have its own 330 Ω current-limiting resistor connected in series. The LED polarity must be respected:

- Anode: connected toward the GPIO output through the resistor.
- Cathode: connected toward GND.

The exact GPIO assignments and LED state definitions will be documented after validating the selected ESP32-C3 board's pinout and checking for conflicts with other peripherals or programming interfaces.

**Firmware responsibilities:**

A system-status module will control the LEDs and provide visual indications of selected operating states, such as:

- System initialization.
- Successful sensor acquisition.
- Measurement or sensor errors.
- Network connectivity.

The meaning of each LED state should be explicitly defined in the firmware documentation.

## Electrical and Pin Configuration Notes

All components must share a common ground with the ESP32-C3.

GPIO assignments are board-dependent and should not be assumed from generic ESP32 examples. Before finalizing the wiring, verify the exact ESP32-C3 Super Mini board pinout, ADC-capable pins, and any GPIO restrictions.

The firmware should centralize hardware pin assignments in a configuration header instead of scattering GPIO numbers throughout the source code. This makes the hardware configuration easier to review and modify.

Additional considerations:

- Verify the operating voltage and signal levels of every peripheral.
- Check whether the OLED breakout already includes I²C pull-up resistors.
- Keep the DS18B20 4.7 kΩ pull-up connected between its data line and 3.3 V.
- Avoid assigning a GPIO to multiple peripherals unless the hardware design explicitly supports it.
- Keep sensor wiring stable and avoid unnecessarily long analog signal connections.
- Validate the ADC input range before connecting the moisture sensor.

## Software and Development Tools

| Technology | Purpose |
|---|---|
| C | Firmware development |
| ESP-IDF | ESP32-C3 embedded development framework and peripheral drivers |
| PlatformIO | Project configuration, build, upload, and serial monitoring |
| Python | Planned external data processing and analysis |
| Git / GitHub | Version control, documentation, and project collaboration |

The firmware is developed in C using ESP-IDF. PlatformIO provides the development workflow and project configuration.

ESP-IDF provides peripheral drivers for GPIO, ADC, I²C, Wi-Fi, and other microcontroller functionality. Hardware-specific operations should use these interfaces rather than being implemented directly inside the application entry point.

## Firmware Architecture

The firmware is organized into modular components with clearly defined responsibilities. The intended architecture separates hardware acquisition, measurement management, communication, visualization, and system-level coordination.

```text
So1l/
│
├── include/
│   ├── measurements/
│   │   ├── measurement.h
│   │   ├── temperature.h
│   │   ├── moisture.h
│   │   └── timestamp.h
│   │
│   ├── network/
│   │   ├── network.h
│   │   ├── wifi.h
│   │   └── telemetry.h
│   │
│   ├── display/
│   │   └── display.h
│   │
│   └── system/
│       ├── system.h
│       └── config.h
│
├── src/
│   ├── measurements/
│   │   ├── measurement.c
│   │   ├── temperature.c
│   │   ├── moisture.c
│   │   └── timestamp.c
│   │
│   ├── network/
│   │   ├── network.c
│   │   ├── wifi.c
│   │   └── telemetry.c
│   │
│   ├── display/
│   │   └── display.c
│   │
│   ├── system/
│   │   └── system.c
│   │
│   └── main.c
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── HARDWARE.md
│   └── CALIBRATION.md
│
├── platformio.ini
├── CMakeLists.txt
├── README.md
└── LICENSE
```

This is a proposed organizational structure. Files and modules should be introduced as their responsibilities are implemented, rather than creating empty modules solely to match the diagram.

### Module Responsibilities

| Module | Responsibility |
|---|---|
| `main.c` | Application entry point and high-level initialization |
| `system/` | System configuration, initialization, and coordination |
| `temperature` | DS18B20 communication and temperature acquisition |
| `moisture` | ADC configuration, analog sampling, and moisture acquisition |
| `measurement` | Measurement data structures, organization, and validation |
| `timestamp` | Measurement timestamps and time-source management |
| `display` | OLED initialization and local visualization |
| `wifi` | Wi-Fi initialization, connection, and connection state |
| `telemetry` | Serialization and formatting of measurement data |
| `network` | Coordination of communication and data transmission |

Each public module should expose its interface through a `.h` header and implement its functionality in a corresponding `.c` source file.

The header declares what other modules can use; the source file contains how that functionality is implemented.

### Separation of Responsibilities

The firmware should follow a clear dependency flow:

- Sensor modules communicate with physical hardware and return readings or errors.
- The measurement layer organizes readings into consistent data structures.
- Validation and calibration logic process acquired measurements.
- The display consumes measurements for local visualization.
- The telemetry layer converts measurements into a structured representation.
- The Wi-Fi module manages the wireless connection.
- The network layer coordinates data transmission.
- `main.c` initializes and coordinates the application rather than containing the implementation of every feature.

This separation reduces coupling between components and makes individual modules easier to test, replace, and maintain.

## Data Acquisition and Processing Workflow

The intended measurement pipeline follows these stages:

```text
Physical Sensors
       |
       v
Hardware Interfaces
(1-Wire / ADC)
       |
       v
Sensor Acquisition
       |
       v
Measurement Representation
       |
       v
Data Validation
       |
       v
Calibration & Processing
       |
       v
Structured Telemetry
       |
       v
Wireless Transmission
       |
       v
External Data Collection
       |
       v
Storage and Analysis
```

The OLED display and status indicators provide local feedback and operate alongside this pipeline.

### Measurement Representation

Measurements should be represented consistently before they are displayed, processed, or transmitted.

A measurement record may eventually contain:

- Soil temperature.
- Raw soil moisture ADC reading.
- Calibrated soil moisture estimate, when available.
- Measurement timestamp.
- Acquisition status or validity information.

Raw sensor readings should remain distinguishable from calibrated or derived values. This preserves traceability and allows later analysis to revisit the calibration process.

### Timestamping

Measurement timestamps should use a consistent time standard, preferably UTC.

When network connectivity is available, the system can synchronize its clock using SNTP. Until a valid synchronization has occurred, monotonic uptime can be used to track elapsed time.

The firmware should distinguish a synchronized wall-clock timestamp from an uptime-based timestamp so that measurements are not incorrectly presented as having a verified date and time.

### Validation and Calibration

Validation and calibration are separate responsibilities:

- **Validation** checks whether a reading is available, within expected bounds, and suitable for further processing.
- **Calibration** establishes the relationship between sensor output and a meaningful physical measurement under documented reference conditions.

The firmware should preserve raw readings and report acquisition failures explicitly. Filtering, averaging, and calibration should be documented so that the resulting measurements can be interpreted and reproduced.

## Network and Telemetry

Wireless communication is planned through the ESP32-C3's Wi-Fi capabilities.

The network functionality will be separated into connection management, data formatting, and transmission. This prevents sensor drivers from depending on network availability.

The telemetry layer will be responsible for converting measurement records into a structured format suitable for external systems. JSON is a possible initial format, while the transport protocol will be selected during implementation.

Network credentials must be kept outside public source code and excluded from version control.

The initial communication design should account for:

- Connection and disconnection states.
- Transmission failures and timeouts.
- Measurement timestamps and validity.
- Consistent field names and units.
- Reconnection behavior.
- Avoiding loss or misrepresentation of measurements when connectivity is unavailable.

## Development Roadmap

- [ ] **Phase 1 — Repository and Architecture:** Establish repository structure, documentation, and firmware foundations.
- [ ] **Phase 2 — Hardware Initialization:** Validate power, GPIO assignments, and peripheral connections.
- [ ] **Phase 3 — Sensor Acquisition:** Implement DS18B20 communication and soil moisture ADC sampling.
- [ ] **Phase 4 — Measurement Management:** Develop measurement structures, timestamping, and acquisition status handling.
- [ ] **Phase 5 — Data Processing:** Implement validation, filtering, and experimental sensor calibration.
- [ ] **Phase 6 — Local Visualization:** Integrate the SSD1306 OLED and status LEDs.
- [ ] **Phase 7 — Wireless Telemetry:** Implement Wi-Fi connectivity, structured serialization, and data transmission.
- [ ] **Phase 8 — Data Collection:** Develop external data ingestion and persistence.
- [ ] **Phase 9 — Data Analysis:** Explore statistical analysis and visualization of acquired measurements.
- [ ] **Phase 10 — Enclosure and Documentation:** Develop enclosure models and improve hardware assembly documentation.

## Engineering Focus

So1l provides a practical environment for exploring:

- Embedded systems and microcontroller programming.
- C programming and modular software architecture.
- Digital and analog sensor interfacing.
- ADC configuration and signal acquisition.
- GPIO and I²C peripheral communication.
- Scientific instrumentation and data acquisition.
- Measurement calibration and signal processing.
- Data validation and measurement reliability.
- Wireless communication and telemetry.
- Scientific data analysis and reproducibility.
- Hardware documentation and prototyping.

## Project Philosophy

So1l prioritizes engineering fundamentals, measurement reliability, and reproducibility over feature quantity.

The objective is not simply to obtain sensor readings, but to develop a well-structured instrumentation module capable of producing meaningful, documented, and reusable measurement data.

The project is independently developed and intended to remain applicable to different scientific and engineering contexts.

## Technical References

- [ESP32-C3 Technical Reference and Datasheet](https://www.espressif.com/en/products/socs/esp32-c3/resources)
- [ESP-IDF Programming Guide — ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/)
- [ESP-IDF ADC Driver — ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/adc.html)
- [ESP-IDF I²C Driver — ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/i2c.html)
- [Analog Devices — DS18B20 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
