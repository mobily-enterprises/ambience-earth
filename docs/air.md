ESP32-S3
SLZB-06 class (drop-in coordinator appliance)


OR CHEAPER:

The Brain (ESP32-S3): Handles Wi-Fi, your custom application logic, the telemetry to your server, and the heavy lifting of the Zigbee/Matter software stack.

The Radio (ESP32-H2): Acts purely as a "modem." It does nothing but listen for and transmit Zigbee/Thread signals. It has its own dedicated antenna.


# Technical Specification: Professional Dual-Radio Embedded Gateway (2026)

## 1. Executive Summary
This architecture defines a high-performance, industrial-grade smart home hub built to handle complex automation logic (heaters, air quality management) while maintaining a cloud-connected telemetry link. By utilizing a Tri-Chip design, the system eliminates performance bottlenecks and packet loss associated with single-antenna multiprotocol chips, ensuring near-100% reliability for mission-critical home automations.

## 2. Hardware Architecture: The Tri-Chip Design
The hub uses a primary application processor (“The Brain”) and two dedicated radio co-processors (“The Radios”).

| Component     | Role                     | Connectivity                          | Physical Interface   |
| ------------- | ------------------------ | ------------------------------------- | -------------------- |
| ESP32-S3      | Application Brain        | Wi-Fi 6 (2.4GHz), Bluetooth 5 (LE)    | Main CPU             |
| ESP32-H2 (A)  | Zigbee Coordinator       | Dedicated 802.15.4 (Zigbee 3.0)       | UART 1 (Serial)      |
| ESP32-H2 (B)  | Thread Border Router     | Dedicated 802.15.4 (Thread/Matter)    | UART 2 (Serial)      |

### Hardware Implementation Details
- RF isolation: Each chip uses its own dedicated antenna. For commercial PCB design, antennas are spaced 10–15 cm apart to prevent RF deafening when Wi-Fi and Zigbee transmit simultaneously.
- Independent channels: H2 chips operate on fixed, non-overlapping channels (e.g., Zigbee on Ch. 25, Thread on Ch. 11, Wi-Fi on Ch. 1) to reduce 2.4 GHz interference.
- BOM efficiency: Total hardware cost for the chipsets in 2026 is approximately $15–$25 AUD, significantly lower than Linux-based alternatives.

## 3. Software Architecture
The system runs on the ESP-IDF (v5.2+) framework, providing a real-time operating system (FreeRTOS) environment.

### A. Radio Co-Processor (RCP) Model
- Role: Specialized RCP firmware turns each H2 into a high-speed modem handling low-level radio timing and mesh networking.
- Communication: The S3 Brain sends high-level commands (e.g., `SET_PLUG_OFF`) via ESP-Zigbee-Gateway or ESP-Matter stacks.
- Reliability: Offloading radio management ensures heavy Wi-Fi telemetry tasks do not cause lag in local Zigbee/Thread device responses.

### B. Local Automation Engine
- Sensor aggregation: Real-time data collection from wireless temperature, CO2, and humidity sensors.
- Logic execution: A “Rules Engine” processes triggers (e.g., “If CO2 > 1000 ppm, activate Ventilation Plug”).
- State management: Maintains a local database of all paired device states.

### C. Telemetry & Cloud Enrollment
- Secure handshake: TLS-encrypted MQTT or HTTPS connects to the cloud on first boot.
- Unique identity: Each unit ships with a unique Hardware ID (UID) used for claim-code enrollment on the web portal.
- Remote UI: Sensor data streams to the server so users can view historical charts and adjust heater schedules via the web interface.

## 4. User Experience & Ecosystem
- Wireless standard: Supports any Zigbee 3.0 or Matter-over-Thread sensor.
- Zero-wire setup: The user only plugs in the main hub; battery-powered sensors communicate wirelessly via the H2 radios.
- Universal compatibility: Dual support for Zigbee (legacy workhorse) and Matter/Thread (future-proof) enables interoperability with devices from Aqara, Sonoff, IKEA, Eve, and more.

## 5. Development Summary
| Feature        | Implementation                                  |
| -------------- | ----------------------------------------------- |
| Boot Time      | < 2 seconds (instant-on appliance)              |
| Programming    | C++ via ESP-IDF                                 |
| Stability      | High (no SD card corruption, no Linux OS overhead) |
| Updates        | OTA firmware updates supported for all three chips |

