# CLV-3D: Closed Loop Ventilation Enclosure

A custom-built, sensor-driven ventilation enclosure designed for the safe operation of resin 3D printers in confined workshops.

## Overview

The CLV-3D is a multifunctional enclosure that addresses fume management and particulate filtration without requiring external exhaust. It features a high-volume recirculating airflow loop, real-time air quality monitoring, and autonomous fan control.

### Key Features
- **Recirculating Airflow**: Multi-pass filtration using G4 pre-filters and high-capacity carbon filters.
- **Distributed Sensor Network**: 4-node ESP-NOW system monitoring Internal, External, Plenum, and Filter environments.
- **Autonomous Control**: Fan speed scales automatically based on VOC (IAQ) and Particulate Matter (PM2.5 and PM10) levels.
- **Smart Idle & Self-Healing**: Adaptive power management to extend sensor life and unified baseline calibration to prevent sensor drift.
- **Integrated Display**: Real-time dashboard showing system health, filter status, and environment telemetry.

---

## System Architecture

The system consists of four wireless nodes communicating via ESP-NOW:

1.  **Node 1 (Hub)**: Main display and interface
2.  **Node 2 (Sensor Stack)**: Main control logic, fan driver (DAC), and internal sensors
3.  **Node 3 (Filter Monitor)**: Monitors pressure after the G4 filters to detect clogging
4.  **Node 4 (External Monitor)**: Reference node for room-air quality

---

## Project Structure

- `firmware/`: Production firmware for all four nodes (includes local `common/` protocol).
- `Wireless Firmware/`: Alternative branch (includes local `common/` protocol).
- `Calibration/`: Utility scripts for sensor and pressure calibration.
- `3D Printing Files/`: Scaled STL files for all custom brackets, mounts, and sensor housings.
- `Cut Enclosure Pieces/`: DXF/Design files for laser-cut acrylic and MDF panels.

---

## Setup & Installation

### Hardware Requirements

Listed below are the components I used, however, the only real requirements for the MCUs are that they are ESP32 type boards. Alternate boards and display can be used, though 3D files will need adjustments.

- **MCUs**: 1x Feather S2, 3x QTPY ESP32-S3.
- **Sensors**: 4x BME680 (I2C), 2x PMSA0031 (I2C).
- **Display**: Adafruit 2.4" TFT FeatherWing (any small display will do)
- **Control**: GP8413 I2C DAC for 0-10V signal.

### Firmware Setup
1.  Install the required libraries: `Adafruit_BME680`, `Adafruit_PM25AQI`, `Adafruit_ILI9341`, `Adafruit_GFX`, and `Adafruit_STMPE610`.
2.  Update the MAC addresses in the source code to match your specific hardware.
3.  Flash the respective `.ino` files from the `firmware/` directory.

### Calibration
Before final operation, run the scripts in `Calibration/` to:
1.  **Tare Pressure Sensors**: Account for natural atmospheric variance between Node 2 and Node 3.
2.  **Set Filter Baselines**: Define the "clean" pressure drop for your specific filter setup.

---

## Maintenance

| Indicator | Meaning | Action |
| :--- | :--- | :--- |
| `!! REPLACE G4 !!` | High pressure drop detected. | Replace the G4 pre-filters. |
| `!! REPLACE CARBON !!` | Low VOC removal efficiency. | Flip or replace the carbon filter. |

---

## License & Credits
Designed by Katz Creates. This project is shared for educational and hobbyist use.
