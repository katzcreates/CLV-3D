# Firmware Setup & Calibration

This guide covers the technical setup for the MCUs and the critical calibration steps required for accurate operation.

## 1. Requirements & Libraries

Ensure the following libraries are installed in the Arduino IDE:

- **Adafruit_BME680**: For Temp/Hum/Pressure/VOC sensors.
- **Adafruit_PM25AQI**: For the PMSA0031 particulate sensor.
- **Adafruit_ILI9341**: For the 2.4" TFT display.
- **Adafruit_GFX**: Core graphics library.
- **Adafruit_STMPE610**: For the resistive touch screen.

---

## 2. Identify MAC Addresses

To enable ESP-NOW communication, you need the MAC addresses of your boards. While your specific hardware will differ, here are the addresses used in the original build for reference:

- **Node 1 (Hub)**: `XX:XX:XX:XX:XX:XX`
- **Node 2 (Sensor Stack)**: `XX:XX:XX:XX:XX:XX`

### Setup Steps

1. Upload a "Get MAC" sketch to your boards to find their unique addresses.
2. Update the `hubAddress` in Node 2 and Node 4 to match your Node 1.
3. Update `node2Address` in Node 3 to match your Node 2.

---

## 3. Pressure & Filter Calibration

To ensure accurate mechanical clogging alerts for the G4 pre-filter, a baseline pressure calibration must be performed using the script in `PressureCalibration/` to account for sensor drift.

### Calibration Setup

- **Node 3 (Filter Monitor)**: Running its standard firmware.
- **Node 2 (Sensor Stack)**: Flashed with `PressureCalibration.ino`.

### Calibration Process

1. Open the Serial Monitor for Node 2 (115200 baud).
2. The script will cycle the fan from 0% to 100%.
3. **Capture the Tare**: Look at the `RawDiff` value when the Fan is at **0%**. This is the natural atmospheric difference between your specific sensors.
4. **Capture the Clean Baseline**: Look at the `FinalDrop` value when the Fan is at **80%**. For clean G4 filters, this is typically **3.0 to 5.0 Pa**.

### Updating Node 2 Firmware

1. Open `Node2_SensorStack.ino`.
2. Update `G4_FILTER_TARE_OFFSET` with your 0% `RawDiff` value (e.g., `-40.0`).
3. Ensure `G4_ALERT_THRESHOLD` is set to `5.0`. This triggers the alert only when resistance increases significantly above the clean baseline.
4. Note: The system uses a **30-second safe window** to prevent false alerts from door openings.

---

## 4. VOC & IAQ Calibration (Self-Healing)

The BME680 sensors automatically handle their own calibration via the **Unified Self-Healing** logic.

- **First-Run Sync**: On initial power-up, the system may report high IAQ (80-90) or stay at 0 while the raw resistance "climbs" to match your room air.
- **The Wait**: Simply leave the cabinet idle in a clean room for **15-20 minutes**. You will see the `Base` resistance climb in the Serial Monitor. Once the `Base` matches your room's raw resistance, local IAQ calibration is complete.
- **Autosave**: Progress is saved to flash every **5 minutes**.

---

## 5. Final Firmware Upload

Once calibration is complete, flash each node with its respective `.ino` file from the `firmware/` directory. Ensure Node 2 is flashed with its production firmware, NOT the calibration utility.
