# So1l

**An embedded soil sensing and environmental data acquisition module.**

So1l is an experimental embedded systems project focused on soil sensing, environmental data acquisition, and measurement processing. Built around the ESP32-C3 and programmed in C, the module is designed to collect, process, validate, and transmit soil-related measurements.

The project explores the engineering principles behind reliable sensor instrumentation, modular firmware architecture, measurement calibration, and structured telemetry for scientific and research applications.

## Objectives

- Develop modular and maintainable embedded firmware in C.
- Implement reliable acquisition of soil-related measurements.
- Establish structured measurement representation and timestamping.
- Explore sensor calibration and data validation techniques.
- Implement wireless communication and structured telemetry.
- Provide a foundation for external data collection, storage, and analysis.

## Hardware Components

The initial hardware configuration consists of:

| Component | Specification | Purpose |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini (USB-C) | Main processing and control unit |
| Temperature Sensor | DS18B20 Waterproof | Soil temperature measurement |
| Soil Moisture Sensor | Capacitive Soil Moisture Sensor v1.2 | Soil moisture measurement |
| Display | 0.96" SSD1306 OLED (I²C) | Local measurement visualization |
| Resistor | 4.7 kΩ | DS18B20 pull-up resistor |
| Prototyping Board | 830-point breadboard | Circuit assembly and prototyping |
| Jumper Wires | Male-to-male, male-to-female, female-to-female | Electrical connections |

### Optional Components

| Component | Specification | Purpose |
|---|---|---|
| Status LEDs | Green, yellow, and red LEDs | Visual indication of system states |
| Current-Limiting Resistors | 330 Ω (one per LED) | Limit LED current and protect GPIO pins |

The optional status LEDs are intended to provide visual feedback about system operation, connectivity, and measurement status. Their exact state assignments will be defined during firmware development.

## Software and Development Tools

| Technology | Purpose |
|---|---|
| C | Firmware development |
| ESP-IDF | Embedded development framework |
| PlatformIO | Build system and development environment |
| Python | Planned external data processing and analysis |

## Architecture

The firmware is designed around modular components with clearly defined responsibilities.

```text
So1l
│
├── measurements/
│   ├── measurement
│   ├── temperature
│   ├── moisture
│   └── timestamp
│
├── network/
│   ├── network
│   ├── wifi
│   └── telemetry
│
├── display/
│
├── system/
│
└── main.c
```

The architecture aims to maintain separation between sensor acquisition, measurement management, communication, and local visualization.

This modular approach allows individual components to be developed, tested, and maintained independently.

## Data Acquisition Workflow

The intended data acquisition pipeline follows these stages:

```text
Physical Sensors
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
Data Transmission
       |
       v
External Analysis
```

## Development Roadmap

- [ ] **Phase 1 — Repository and Architecture:** Establish repository structure, documentation, and firmware foundations.
- [ ] **Phase 2 — Sensor Acquisition:** Implement temperature and soil moisture acquisition.
- [ ] **Phase 3 — Measurement Management:** Develop structured measurements and timestamping.
- [ ] **Phase 4 — Data Processing:** Implement validation and experimental sensor calibration.
- [ ] **Phase 5 — Local Visualization:** Integrate the OLED display.
- [ ] **Phase 6 — Wireless Telemetry:** Implement Wi-Fi connectivity and structured data transmission.
- [ ] **Phase 7 — Data Collection:** Develop external data ingestion and persistence.
- [ ] **Phase 8 — Data Analysis:** Explore statistical analysis and visualization of acquired measurements.

## Engineering Focus

So1l provides a practical environment for exploring:

- Embedded systems and microcontroller programming.
- C programming and modular software architecture.
- Sensor interfacing and hardware communication.
- Scientific instrumentation and data acquisition.
- Measurement calibration and signal processing.
- Data validation and reliability.
- Wireless communication and telemetry.
- Scientific data analysis and reproducibility.

## Project Philosophy

So1l prioritizes engineering fundamentals, measurement reliability, and reproducibility over feature quantity.

The objective is not simply to obtain sensor readings, but to develop a well-structured instrumentation module capable of producing meaningful, documented, and reusable measurement data.

The project is independently developed and intended to remain applicable to different scientific and engineering contexts.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
