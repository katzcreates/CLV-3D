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

1. **Node 1 (Hub)**: Main display and interface
2. **Node 2 (Sensor Stack)**: Main control logic, fan driver (DAC), and internal sensors
3. **Node 4 (External Monitor)**: Reference node for room-air quality
4. **Node 3 (Filter Monitor)**: Monitors pressure after the G4 filters to detect clogging

---

## Project Structure

- `firmware/`: Production firmware for all four nodes (includes local `common/` protocol).
- `Wireless Firmware/`: Alternative branch (includes local `common/` protocol).
- `Calibration/`: Utility scripts for sensor and pressure calibration.
- `3D Printing Files/`: Scaled STL files for all custom brackets, mounts, and sensor housings.
- `Cut Enclosure Pieces/`: DXF/Design files for laser-cut acrylic and MDF panels.

---

## Hardware Requirements

The components listed below are what I used for the original build. However, the system is flexible—any ESP32-type boards will work, though alternate boards or displays may require physical adjustments to the 3D printed housings.

### Electronics

- **MCUs**: 1x Feather S2, 3x QTPY ESP32-S3.
- **Sensors**: 4x BME680 (I2C), 2x PMSA0031 (I2C).
- **Display**: Adafruit 2.4" TFT FeatherWing (any small SPI display can be adapted).
- **Control**: GP8413 I2C DAC for 0-10V fan control.

### Ventilation

These are the exact pieces I used for my filtration setup. The design is optimized for these, and if using alternatives, I recommend 6"/150mm sized peripherals.

- **Fan**: [Rhino Ultra Fan (150mm / 6" Non-Silenced)](https://rhinofilter.com/products/rhino-ultra-non-silenced-fan/)
- **Filter**: [AC Infinity 6" Carbon Filter](https://acinfinity.com/duct-carbon-filter-6-with-australian-charcoal/) (Intake)
- **Pre-filtration**: 2x [G4 Pleated Panel Filter (24"x12"x1" / EU4)](https://filtersdirect.uk/product/g4-pleated-panel-filter-1-22mm-depth-eu4/)

---

## Cabinetry & Enclosure

The CLV-3D is designed to be built using standard IKEA METOD cabinetry as a foundation.

### IKEA Parts List

These are the exact pieces used to build the base cabinetry as seen in my project video and in the 3D mockup.

- **Lower Section**:
  - 2x [METOD Base cabinet frame (60x60x80 cm)](https://www.ikea.com/gb/en/p/metod-base-cabinet-frame-black-grey-60591702/)
  - 2x [VALLSTENA Door (60x80 cm, White)](https://www.ikea.com/gb/en/p/vallstena-door-white-80541693/)
  - [FÖRBÄTTRA Plinth (Dark grey)](https://www.ikea.com/gb/en/p/foerbaettra-plinth-dark-grey-80454110/)
  - [UTRUSTA Shelf (Black-grey)](https://www.ikea.com/gb/en/p/utrusta-shelf-black-grey-10591785/)
  - [HÅLLBAR Bin with lid (Light grey)](https://www.ikea.com/gb/en/p/hallbar-bin-with-lid-light-grey-20420203/)
  - [UTRUSTA Hinge w b-in damper for kitchen](https://www.ikea.com/gb/en/p/utrusta-hinge-w-b-in-damper-for-kitchen-80524882/)
  - [UTRUSTA Hinge w b-in damper for kitchen (110°)](https://www.ikea.com/gb/en/p/utrusta-hinge-w-b-in-damper-for-kitchen-10427262/)
  - 4x [METOD Leg](https://www.ikea.com/gb/en/p/metod-leg-70556067/)
- **Upper Section**:
  - 2x [METOD Fridge/freezer top cabinet frame (60x60x40 cm)](https://www.ikea.com/gb/en/p/metod-fridge-freezer-top-cabinet-frame-black-grey-70591706/)
- **Worktop**: [MÖCKLERYD Worktop (White laminate)](https://www.ikea.com/gb/en/p/moeckleryd-worktop-white-laminate-60582180/)
- **Drawers**:
  - [KNIVSHULT Drawer low (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-low-dark-grey-80600668/) (For Form 4, rated for weight)
  - [KNIVSHULT Drawer medium (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-medium-dark-grey-30600675/) (For consumables)
  - [KNIVSHULT Drawer front low (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-front-low-dark-grey-50600702/)
  - [KNIVSHULT Drawer front medium (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-front-medium-dark-grey-80600705/)

### Custom Panel Ordering

If you are using the same base furniture, you can refer to the `Cut Enclosure Pieces/` folder for DXF files for the exact pieces of acrylic and MDF that require custom shapes. For those in the UK, I recommend [CutMy.co.uk](https://cutmy.co.uk/) for well-priced, custom-cut materials.

#### 3mm Clear Acrylic

| Dimensions (mm) | Qty | Shape | File / Notes |
| :--- | :--- | :--- | :--- |
| 1200 x 297 | 1 | Rectangle | Top Door |
| 664 x 793 | 2 | Custom (Upload) | `AcrylicSide_FinalFix.dxf` |
| 600 x 495 | 2 | Rectangle | Side Doors |

#### 3mm Medite Premier MDF

| Dimensions (mm) | Qty | Shape | File / Notes |
| :--- | :--- | :--- | :--- |
| 1194 x 797 | 1 | Custom (Upload) | `Backsplash_FinalFix.dxf` |
| 329 x 285 | 1 | Custom (Upload) | `FilterBlockerFix.dxf` |

---

## Firmware Setup

### 1. Firmware Requirements & Libraries

Ensure the following libraries are installed in the Arduino IDE:

- **Adafruit_BME680**: For Temp/Hum/Pressure/VOC sensors.
- **Adafruit_PM25AQI**: For the PMSA0031 particulate sensor.
- **Adafruit_ILI9341**: For the 2.4" TFT display.
- **Adafruit_GFX**: Core graphics library.
- **Adafruit_STMPE610**: For the resistive touch screen.

### 2. Identify MAC Addresses

To enable ESP-NOW communication, you need the MAC addresses of your boards:

1. Upload a simple "Get MAC" sketch to the boards to find their addresses.
2. Update the `hubAddress` in Node 2 and Node 4 to match Node 1.
3. Update `node2Address` in Node 3 to match Node 2.

### 3. Pressure & Filter Calibration

Before final operation, run the scripts in `Calibration/` to ensure accurate mechanical clogging alerts for the G4 pre-filter.

#### Calibration Process

1. Open the Serial Monitor for Node 2 (115200 baud) running `PressureCalibration.ino`.
2. The script will cycle the fan from 0% to 100%.
3. **Capture the Tare**: Record the `RawDiff` value at **0%** fan speed. Update `G4_FILTER_TARE_OFFSET` in your Node 2 firmware with this value.
4. **Capture the Clean Baseline**: Verify the `FinalDrop` at **80%** (typically 3.0 to 5.0 Pa for clean filters).

### 4. VOC & IAQ Calibration (Self-Healing)

The BME680 sensors automatically handle calibration via Unified Self-Healing logic.

- **First-Run Sync**: Leave the cabinet idle in a clean room for **15-20 minutes** on first power-up to allow the baseline to stabilize.
- **Autosave**: Calibration progress is saved to flash every **5 minutes**.

---

## Assembly & Build Notes

### Enclosure Assembly

1. **Upper/Wall Cabinets**: Install wall cabinets and secure to wall.
2. **Base Cabinets**: Assemble METOD base cabinets and test placement.
3. **Internal Prep**: Cut airflow holes in the back of the lower cabinet and countertop.
4. **Plenum Construction**: Create the rear plenum using foam/MDF.
5. **Base Placement:** Verify leg height is level and place base cabinet so plenum edges are flush against the wall.
6. **Drawers, Countertop & Hardware**: Install drawers and countertop and verify printer clearance.
7. **Backsplash:** Add in backsplash and secure to cabinets/countertop. Seal any major gaps or holes.
8. **Ventilation Install**: Mount inline fan and carbon filter to the bottom of the upper cabinets.
9. **Panels and Filters**: Install acrylic side panels, secure with L-brackets, and mount MDF baffle.
10. **Doors and Hinges:** Align hinges and mount the top and side doors.
11. **Electronics**: Integrate the ESP32-S3 nodes and sensor suite.

### Build Notes

> [!TIP]
> **Gravity & Alignment**
> Real-world assembly often differs from CAD models due to mounting tolerances and "cabinet sag." It is highly recommended to complete the physical frame assembly and measure the specific "as-built" openings before cutting your final acrylic panels.

---

## Maintenance & Reliability

| Indicator | Meaning | Action |
| :--- | :--- | :--- |
| `!! REPLACE G4 !!` | High pressure drop detected. | Replace the G4 pre-filters. |
| `!! REPLACE CARBON !!` | Low VOC removal efficiency. | Flip or replace the carbon filter. |

### Component Lifespan Expectations

| Component | Life Expectancy | Maintenance Type |
| :--- | :--- | :--- |
| **BME680 (VOC)** | 5 - 10 Years | Self-calibrating via firmware nudges. |
| **PMSA0031 (PM)** | ~1.5 Years (24/7) | Can be extended to 10 years with SET-pin duty cycling. |
| **G4 Pre-Filter** | 3 - 6 Months | Replace when Hub reports `!! REPLACE G4 !!`. |
| **Carbon Filter** | ~18-24 Mo (Flip) | Replace after ~3 years total or when alerts persist. |

### Smart Power Management (PM Sensor)

1. **Active Mode**: Laser is ON for instant response.
2. **Idle Mode**: Duty-cycles the laser (30s every 10m).
3. **Deep Sleep**: OFF if the shop is empty for >24 hours.

> [!NOTE]
> **PMSA0031 Hardware Upgrade**
> Enabling the 10-year tiers requires a physical connection to the sensor's **SET pin**.

---

## License & Credits

Designed by Katz Creates. This project is shared for educational and hobbyist use.
