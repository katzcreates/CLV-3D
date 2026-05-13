# Firmware Setup & Calibration

This guide covers the technical setup for the MCUs and the critical calibration steps required for accurate operation.

## 1. Requirements & Libraries

Ensure the following libraries are installed in the Arduino IDE:
- **Adafruit_BME680**: For Temp/Hum/Pressure/VOC sensors.
- **Adafruit_PM25AQI**: For the PMSA0031 particulate sensor.
- **Adafruit_ILI9341**: For the 2.4" TFT display.
- **Adafruit_GFX**: Core graphics library.
- **Adafruit_STMPE610**: For the resistive touch screen.

## 2. ESP-NOW Configuration

To enable wireless communication, you must update the MAC addresses in the source code to match your specific hardware.

1. Upload a "Get MAC" sketch to your boards.
2. Update the `hubAddress` in Node 2 and Node 4 to match Node 1.
3. Update `node2Address` in Node 3 to match Node 2.

---

## 3. Pressure Calibration

To ensure accurate mechanical clogging alerts for the G4 pre-filter, a baseline pressure calibration must be performed.

### Calibration Process
1. Flash Node 2 with `PressureCalibration.ino`.
2. Open the Serial Monitor (115200 baud).
3. The script will cycle the fan from 0% to 100%.
4. **Capture the Tare**: Record the `RawDiff` value at **0%** fan speed. Update `G4_FILTER_TARE_OFFSET` in your Node 2 firmware with this value.
5. **Capture the Clean Baseline**: Verify the `FinalDrop` at **80%** (typically 3.0 to 5.0 Pa for clean filters).

---

## 4. VOC & IAQ Calibration

The BME680 sensors handle their own calibration via the **Unified Self-Healing** logic.

- **First-Run Sync**: On initial power-up, the system may report high IAQ. Leave the cabinet idle in a clean room for **15-20 minutes** for the baseline to stabilize.
- **Autosave**: Progress is saved to flash every **5 minutes**. 
- **Drift Protection**: If the cabinet stays in clean air too long, it uses the External node (Node 4) as an "anchor" to prevent the internal sensors from drifting.
