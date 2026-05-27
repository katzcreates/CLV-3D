# CLV-3D: Closed Loop Ventilation Enclosure

A custom-built, sensor-driven ventilation enclosure designed for the safe operation of resin 3D printers in confined workshops.

## Overview

The CLV-3D is a multifunctional enclosure that addresses fume management and particulate filtration without requiring external exhaust. It features a high-volume recirculating airflow loop, real-time air quality monitoring, and autonomous fan control.

### Key Features

- **Recirculating Airflow**: Multi-pass filtration using G4 pre-filters and high-capacity carbon filters.
- **Distributed Sensor Network**: 4-node ESP-NOW system monitoring Internal, External, Plenum, and Filter environments.
- **Autonomous Control**: Fan speed scales automatically based on VOC (IAQ) and Particulate Matter (PM2.5 and PM10) levels.
- **Smart Idle & Self-Healing**: Adaptive power management and unified baseline calibration to prevent sensor drift.
- **Integrated Display**: Real-time dashboard showing system health and telemetry.

---

## Documentation

For detailed information on how to build, program, and maintain the CLV-3D, please refer to the following chapters:

- **[Electronics & Control Logic](docs/electronics.md)**: System architecture, wiring, and autonomous fan logic.
- **[Cabinetry & Assembly](docs/assembly.md)**: IKEA parts list, panel ordering, and physical construction.
- **[Firmware Setup & Calibration](docs/firmware.md)**: Library requirements, MAC addresses, and sensor calibration.
- **[Maintenance & Reliability](docs/maintenance.md)**: Filter lifespan, power management, and long-term stability.

*Documentation for Wireless Firmware coming soon.*

---

## System Architecture

The system consists of four wireless nodes communicating via ESP-NOW:

1. **Node 1 (Hub)**: Main display and user interface.
2. **Node 2 (Sensor Stack)**: Main control logic, fan driver, and internal sensors.
3. **Node 3 (Filter Monitor)**: Monitors pressure after the G4 filters to detect clogging.
4. **Node 4 (External Monitor)**: Reference node for room-air quality.

The as built system also features the following ventilation hardware:

- **Fan**: [Rhino Ultra Fan (150mm / 6" Non-Silenced)](https://rhinofilter.com/products/rhino-ultra-non-silenced-fan/)
- **Filter**: [AC Infinity 6" Carbon Filter](https://acinfinity.com/duct-carbon-filter-6-with-australian-charcoal/) (Intake)
- **Pre-filtration**: 2x [G4 Pleated Panel Filter (24"x12"x1" / EU4)](https://filtersdirect.uk/product/g4-pleated-panel-filter-1-22mm-depth-eu4/)

Other fans and filters can be used but adaptations to the design might be necessary to accomodate them. 

---

## Project Structure

- `firmware/`: Production firmware for all four nodes.
- `docs/`: Detailed technical documentation and assembly guides.
- `PressureCalibration/`: Utility script for sensor and pressure calibration.
- `3D Model References/`: Contains the master STEP assembly model (`CLV-3D Enclosure.step`) for the full enclosure.
- `3D Printing Files/`: Scaled STL files for all custom housings.
- `Cut Enclosure Pieces/`: DXF files for laser-cut panels.

---

## License & Credits

Designed by Katz Creates. This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**.

See the [LICENSE](LICENSE) file for the full legal text.
