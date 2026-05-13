# Maintenance & Reliability

The CLV-3D is designed for long-term industrial reliability. This guide covers sensor longevity, filter maintenance, and system stability.

## Component Lifespan Expectations

| Component | Life Expectancy | Maintenance Type |
| :--- | :--- | :--- |
| **BME680 (VOC)** | 5 - 10 Years | Self-calibrating via firmware nudges. |
| **PMSA0031 (PM)** | ~1.5 Years (24/7) | Can be extended to 10 years with SET-pin duty cycling. |
| **G4 Pre-Filter** | 3 - 6 Months | Replace when Hub reports `!! REPLACE G4 !!`. |
| **Carbon Filter** | ~18-24 Mo (Flip) | Replace after ~3 years total or when alerts persist. |
| **Rhino Fan** | 40,000+ Hours | Brushless EC motor, zero maintenance. |

## Smart Power Management (PM Sensor)

To prevent laser burnout, the system uses three power tiers for the PMSA0031 sensor:

1. **Active Mode (Continuous)**: Laser is 100% ON during prints, sanding, or painting for sub-second response.
2. **Idle Mode (Duty-Cycle)**: If the room is clean for 30 minutes, the laser sleeps and wakes every 10 minutes for a 30-second "sniff."
3. **Deep Sleep (OFF)**: If the shop is empty for >24 hours, the laser stays 100% OFF. It wakes instantly if the low-power VOC sensors detect any fumes (>100 IAQ) or if you touch the Hub.

> [!NOTE]
> **PMSA0031 Hardware Upgrade**
> The I2C version of the PM sensor requires a physical connection to its **SET pin** to enable software sleep. Currently, the laser is on 24/7 (1.5 year life). To enable the 10-year tiers, connect the sensor's SET pin to any spare GPIO on your QTPY and add a simple `digitalWrite` to the `pmIsSleeping` logic.

## Carbon Filter Maintenance

The cabinet contains two continuous VOC sources (Form Wash IPA and Form 4 resin vat). The fan's Smart Idle logic cycles it on and off as needed to maintain air quality without excessive filter wear.

| Milestone | Estimated Timeline | What to Do |
| :--- | :--- | :--- |
| **First Flip** | ~18–24 months | Rotate the filter 180° to bring the fresh end forward. |
| **Full Replacement** | ~3 years total | Replace the filter entirely. |

**How your firmware tells you it's time:**
- The Hub's **Carbon Alert** (`!! REPLACE CARBON !!`) fires when the Plenum VOC reading is within 50% of the Internal reading—i.e., air is passing through the carbon *without being scrubbed*.
- **Occasional alerts** → time to flip.
- **Persistent alerts** → time to replace.

> [!TIP]
> You don't need to track dates manually. Trust the Plenum sensor—it is measuring filter performance in real-time.

## Stability Hardening Flags

The firmware includes several "mathematical safety shields" to ensure decade-long operation without crashes or memory degradation:

- **Zero-Division Shield**: Protection against crashes if IAQ reach theoretical 0.0.
- **Baseline Sanity Caps**: VOC baselines are clamped between 10kΩ and 1,500kΩ to prevent digital "runaway."
- **Smart-Save Flash Protection**: The ESP32 only commits calibration data to flash if a change > 1% occurs, protecting the memory chips from wear-out over time.
