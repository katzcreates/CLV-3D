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

## Hardware Requirements

Listed below are the components I used, however, the only real requirements for the MCUs are that they are ESP32 type boards. Alternate boards and display can be used, though 3D files will need adjustments.

- **MCUs**: 1x Feather S2, 3x QTPY ESP32-S3.
- **Sensors**: 4x BME680 (I2C), 2x PMSA0031 (I2C).
- **Display**: Adafruit 2.4" TFT FeatherWing (any small display will do)
- **Control**: GP8413 I2C DAC for 0-10V signal.

### Ventilation

These are the exact pieces I used for my filtration setup. Again, alternatives can be used, but the design is optimised for these. If using alternatives, I recommend use of 6"/150mm sized peripherals.

- **Fan**: [Rhino Ultra Fan (150mm / 6" Non-Silenced)](https://rhinofilter.com/products/rhino-ultra-non-silenced-fan/)
- **Filter**: [AC Infinity 6" Carbon Filter](https://acinfinity.com/duct-carbon-filter-6-with-australian-charcoal/) (Intake)
- **Pre-filtration**: 2x [G4 Pleated Panel Filter (24"x12"x1" / EU4)](https://filtersdirect.uk/product/g4-pleated-panel-filter-1-22mm-depth-eu4/)

### Cabinetry

These are the exact pieces used to build the base cabinetry as seen in my project video and in the 3D mockup. Feel free to make adjustments as makes sense for your space.

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

## Firmware Setup
#### 1. Firmware Requirements & Libraries

Ensure the following libraries are installed in the Arduino IDE:

- **Adafruit_BME680**: For Temp/Hum/Pressure/VOC sensors.
- **Adafruit_PM25AQI**: For the PMSA0031 particulate sensor.
- **Adafruit_ILI9341**: For the 2.4" TFT display.
- **Adafruit_GFX**: Core graphics library.
- **Adafruit_STMPE610**: For the resistive touch screen.

#### 2. Identify MAC Addresses

To enable ESP-NOW communication, you need the MAC addresses of your boards:

- **Node 1 (Hub)**
- **Node 2 (Sensor Stack)**

1. Upload a simple "Get MAC" sketch to the remaining boards (Node 3, 4) to find their addresses if needed.
2. Update the `hubAddress` in Node 2 and Node 4.
3. Update `node2Address` in Node 3.

#### 3. Pressure & Filter Calibration

To ensure accurate mechanical clogging alerts for the G4 pre-filter, a baseline pressure calibration must be performed to account for sensor drift.

##### Calibration Setup

- **Node 3 (Filter Monitor)**: Running its standard firmware.
- **Node 2 (Sensor Stack)**: Flashed with 'PressureCalibration.ino'.

##### Calibration Process

1. Open the Serial Monitor for Node 2 (115200 baud).
2. The script will cycle the fan from 0% to 100%.
3. **Capture the Tare**: Look at the `RawDiff` value when the Fan is at **0%**. This is the natural atmospheric difference between your specific sensors.
4. **Capture the Clean Baseline**: Look at the `FinalDrop` value when the Fan is at **80%**. For clean G4 filters, this is typically **3.0 to 5.0 Pa**.

##### Updating Node 2 Firmware

1. Open `Node2_SensorStack.ino`.
2. Update `G4_FILTER_TARE_OFFSET` with your 0% `RawDiff` value (e.g., `-40.0`).
3. Ensure `G4_ALERT_THRESHOLD` is set to `5.0`. This triggers the alert only when resistance increases significantly above the clean baseline.
4. The system uses a **30-second safe window** to prevent false alerts from door openings.

#### 4. VOC & IAQ Calibration (Self-Healing)

The BME680 sensors automatically handle their own calibration via the **Unified Self-Healing** logic (see Control Logic for full details).

- **First-Run Sync**: On initial power-up, the system may report high IAQ (80-90) or stay at 0 while the raw resistance "climbs" to match your room air.
- **The Wait**: Simply leave the cabinet idle in a clean room for **15-20 minutes**. You will see the `Base` resistance climb in the Serial Monitor. Once the `Base` matches your room's raw resistance, local IAQ calibration is complete.
- **Autosave**: Progress is saved to flash every **5 minutes**. Pulling the plug during the initial climb will result in the system reverting to its last 5-minute checkpoint.

#### 5. Final Firmware Upload

Flash each node with its respective `.ino` file from the `firmware/` directory.

###Wireless Firmware Instructions Coming Soon

---
## Enclosure Assembly

1. **Upper/Wall Cabinets**: Install wall cabinets and secure to wall.
2. **Base Cabinets**: Assemble METOD base cabinets and test placement.
3. **Internal Prep**: Cut airflow holes in the back of the lower cabinet and countertop.
4. **Plenum Construction**: Create the rear plenum using foam/MDF.
5. **Base Placement:** Verify leg height is level and place base cabinet so plenum edges are flush against the wall.
6. **Drawers, Countertop & Hardware**: Install drawers and countertop and verify printer clearance.
7. **Backsplash:** Add in backsplash and secure to cabinets/countertop. Seal any major gaps or holes.
8. **Ventilation Install**: Mount inline fan and carbon filter to the bottom of the upper cabinets.
9. **Panels and Filter**s: Install acrylic side panels first, and secure with L-brackets. Mount MDF baffle to upper cabinet and feed ducting through the hole, securing both ends to the fan and carbon filter. Align panel filter brackets with MDF baffle and attach to backsplash and acrylic side panel. Slide panel filters into place.
10. **Doors and Hinges:** Align top door hinges with side panels to ensure the top door will rest on the front edge of the acrylic side panel and mount to upper cabinet. Align side doors and attach hinges. Insert top door panel into hinge and bring to a closed position.
11. **Electronics**: Integrate ESP32-S3 and sensor suite before final assembly.

### Build Notes

> [!TIP] **Gravity & Alignment**
> Real-world assembly often differs from CAD models due to mounting tolerances and "cabinet sag."
>
> - **Acrylic Panels**: The upper cabinets may hang slightly lower than designed (~2-5mm offset). It is highly recommended to complete the physical frame assembly and measure the specific "as-built" openings before cutting your final acrylic panels.
> - **MDF Backing**: Similarly, verify the rear backsplash dimensions after mounting to ensure the plenum seal is tight.

---

## Maintenance

| Indicator | Meaning | Action |
| :--- | :--- | :--- |
| `!! REPLACE G4 !!` | High pressure drop detected. | Replace the G4 pre-filters. |
| `!! REPLACE CARBON !!` | Low VOC removal efficiency. | Flip or replace the carbon filter. |

###Component Lifespan Expectations

| Component | Life Expectancy | Maintenance Type |
| :--- | :--- | :--- |
| **BME680 (VOC)** | 5 - 10 Years | Self-calibrating via firmware nudges. |
| **PMSA0031 (PM)** | ~1.5 Years (laser on 24/7) | Upgrade to SET-pin wiring for 10-year life. See note below. |
| **G4 Pre-Filter** | 3 - 6 Months | Replace when Hub reports `!! REPLACE G4 !!`. |
| **Carbon Filter** | ~18-24 Months (flip) / ~3 Years (replace) | Flip when Carbon Alert becomes frequent. Replace when persistent. |
| **Rhino Fan** | 40,000+ Hours | Brushless EC motor, zero maintenance. |

### Smart Power Management (PM Sensor)

To prevent laser burnout (a common failure in air monitors), the system uses three power tiers:

1. **Active Mode (Continuous)**: Laser is 100% ON during prints, sanding, or painting for instant, sub-second response.
2. **Idle Mode (Duty-Cycle)**: If the room is clean for 30 minutes, the laser sleeps and wakes every 10 minutes for a 30-second "sniff."
3. **Deep Sleep (OFF)**: If the shop is empty for >24 hours, the laser stays 100% OFF. It wakes instantly if the low-power VOC sensors detect any fumes (>100 IAQ) or if you touch the Hub.

> [!NOTE] **PMSA0031 Hardware Upgrade**
> The I2C version of the PM sensor requires a physical connection to its **SET pin** to enable software sleep. Currently, the laser is on 24/7 (1.5 year life). To enable the 10-year Smart Power tiers described above, connect the sensor's SET pin to any spare GPIO on your QTPY and add a simple `digitalWrite` to the `pmIsSleeping` logic.

### Carbon Filter Maintenance

The cabinet contains two continuous VOC sources (Form Wash IPA and Form 4 resin vat), which means the carbon filter is actively working even when the printer is idle. The fan's Smart Idle logic cycles it on and off as needed, which is not harmful to the filter.

**Lifespan Estimate (AC Infinity 6"/150mm):**

| Milestone | Estimated Timeline | What to Do |
| :--- | :--- | :--- |
| **First Flip** | ~18–24 months | Rotate the filter 180° to bring the fresh end forward. |
| **Full Replacement** | ~12–18 months after flip (~3 years total) | Replace the filter entirely. |

**How your firmware tells you it's time:**

- The Hub's **Carbon Alert** (`!! REPLACE CARBON !!`) fires when the Plenum VOC reading is within 50% of the Internal reading — i.e., air is passing through the carbon *without being scrubbed*.
- **Occasional alerts** → time to flip.
- **Persistent alerts** → time to replace.

> [!TIP] You don't need to track dates manually. Trust the Plenum sensor — it's measuring filter performance in real time.

### Stability Hardening Flags

- **Zero-Division Shield**: Mathematical protection against crashes if IAQ reaches perfect 0.0.
- **Baseline Sanity Caps**: VOC baselines are clamped between 10kΩ and 1,500kΩ to prevent digital "runaway."
- **Smart-Save Flash Protection**: The ESP32 only commits calibration data to flash if a change > 1% occurs, protecting the memory chips from wear-out over the coming decade.


---

## License & Credits
Designed by Katz Creates. This project is shared for educational and hobbyist use.
