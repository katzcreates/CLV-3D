#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_PM25AQI.h>
#include <Preferences.h>
#include "../common/Protocol.h"

// Forward Declaration for Compiler
void saveBaselines();

// I2C Addresses
#define BME680_INTERNAL_ADDR 0x77
#define BME680_PLENUM_ADDR   0x76 
#define GP8413_ADDR         0x59  // M5Stack Unit DAC2 default

// Fan Control Constants
#define MIN_FAN_SPEED 10
#define MAX_FAN_SPEED 80
#define PEAK_FAN_SPEED 100
#define CARBON_ALERT_WINDOW_MS 300000 
#define FILTER_ALERT_WINDOW_MS 30000 

// Smart Idle & Adaptive Sniffing Constants
#define IDLE_STABILITY_MS      1800000   // 30 Minutes
#define SHORT_SNIFF_INTERVAL   21600000  // 6 Hours
#define LONG_SNIFF_INTERVAL    86400000  // 24 Hours
#define ADAPTIVE_THRESHOLD_MS  86400000  
#define SNIFF_DURATION_MS      120000    
#define BASELINE_SAVE_INTERVAL 300000    // 5 Minutes (Checkpoint Timer)
#define IAQ_WAKE_THRESHOLD     100.0     // Wake up threshold
#define IAQ_IDLE_THRESHOLD     35.0      // Entry buffer (35 to enter, 55 to wake)
#define PM_WAKE_THRESHOLD      15.0
#define PM10_WAKE_THRESHOLD    30.0

// Sensor Objects
Adafruit_BME680 bmeInternal(&Wire1);
Adafruit_BME680 bmePlenum(&Wire1);
Adafruit_PM25AQI pm25;

// Temperature Compensation
#define TEMP_OFFSET -13.0 // Reduced from -15.0 based on fan cooling data

// Peer Data
// Peer Data (Directed to Hub)
uint8_t hubAddress[] = {0x84, 0xF7, 0x03, 0x73, 0xB3, 0x7E}; // Node 1 (Hub) MAC
SensorData currentData;

// Global state / Calibration (Independent Baselines)
float gasBaselineInternal = 50.0;     
float gasBaselinePlenum = 50.0;      
const float gasBaselineWeight = 0.05; 
float smoothedFanSpeed = 0.0; 
const float fanSmoothingFactor = 0.2; 
float lastFilterPressure = 101325.0; 
uint32_t carbonAlertStartTime = 0; 
uint32_t filterAlertStartTime = 0; 

// Manual Control State
bool isManualMode = false;
uint8_t manualTargetSpeed = 50;
uint32_t manualModeStartTime = 0;

// UI Smoothing (EMA)
float smoothedIAQInternal = 0;
float smoothedIAQPlenum = 0;
float smoothedPM25 = 0;
float smoothedPM10 = 0;
const float uiSmoothingFactor = 0.5; // 0.5 is a good balance for numbers

// Configuration Offsets
const float G4_FILTER_TARE_OFFSET = -40.0; 
const float G4_ALERT_THRESHOLD = 5.0;      

// Adaptive Idle State Tracking
uint32_t lastActivityTime = 0;
uint32_t lastSniffTime = 0;
bool isSniffing = false;
bool isSystemIdle = false; 
Preferences preferences;
uint32_t lastBaselineSaveTime = 0;
uint32_t vocStableStartTime = 0;
float lastVocSyncRef = 0;
float lastExternalVOC = 30.0; 
uint32_t lastDriftCheckTime = 0;
float lastDriftRef = 0;
const uint32_t DRIFT_WINDOW_MS = 900000; // 15 Minutes for a drift nudge
uint32_t lastLogTime = 0;
bool pmIsSleeping = false;
uint32_t lastPmSampleTime = 0;
const uint32_t PM_SAMPLE_INTERVAL = 600000; // 10 Minutes (Duty-Cycle)
const uint32_t PM_WAKE_WINDOW = 30000;      // 30s sampling window

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorData)) {
        SensorData *receivedData = (SensorData *)incomingData;
        if (receivedData->senderNode == 3) {
            lastFilterPressure = receivedData->filterStaticP;
        } else if (receivedData->senderNode == 4) {
            lastExternalVOC = receivedData->externalVOC;
        }
    } else if (len == sizeof(ControlCommand)) {
        ControlCommand *cmd = (ControlCommand *)incomingData;
        if (cmd->senderNode == 1) { // Node 1 (Hub)
            if (cmd->commandType == 1) { // Manual Mode Override
                isManualMode = true;
                manualTargetSpeed = cmd->manualSpeed;
                manualModeStartTime = millis();
                Serial.printf("Hub Override: Manual %d%%\n", manualTargetSpeed);
            } else if (cmd->commandType == 0) { // Auto Mode
                isManualMode = false;
                Serial.println("Hub Override: Auto Mode Reverted");
            }
        }
    }
}

void setFanSpeed(uint8_t percent) {
    percent = constrain(percent, 0, 100);
    uint16_t dacValue = (uint32_t)percent * 32767 / 100;
    uint16_t transmissionValue = dacValue << 1;
    
    Wire1.beginTransmission(GP8413_ADDR);
    Wire1.write(0x02); // Channel 0
    Wire1.write(transmissionValue & 0xFF);
    Wire1.write((transmissionValue >> 8) & 0xFF);
    Wire1.endTransmission();
    
    currentData.fanSpeed = percent;
}

// RH Compensation using the Magnus Formula
float compensateHumidity(float t_raw, float rh_raw, float t_comp) {
    // SVP = 6.112 * exp((17.625 * T) / (243.04 + T))
    auto svp = [](float t) {
        return 6.112 * exp((17.625 * t) / (243.04 + t));
    };
    float rh_true = rh_raw * (svp(t_raw) / svp(t_comp));
    return constrain(rh_true, 0.0, 100.0);
}

void setup() {
    Serial.begin(115200);
    Wire1.begin(); 
    
    // Initialize Sensors
    if (!bmeInternal.begin(BME680_INTERNAL_ADDR)) Serial.println("Internal BME680 Fail");
    if (!bmePlenum.begin(BME680_PLENUM_ADDR))     Serial.println("Plenum BME680 Fail");
    if (!pm25.begin_I2C(&Wire1))                  Serial.println("PMSA0031 Fail");

    // BME680 Settings (FOR BOTH SENSORS)
    auto configBME = [](Adafruit_BME680 &bme) {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); 
    };
    configBME(bmeInternal);
    configBME(bmePlenum);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(onDataRecv);

    Wire1.beginTransmission(GP8413_ADDR);
    Wire1.write(0x01); Wire1.write(0x11); // 10V Mode
    Wire1.endTransmission();

    // Register Hub Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, hubAddress, 6);
    peerInfo.channel = 0; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Initialize Adaptive Idle
    lastActivityTime = millis();
    lastSniffTime = millis();

    // Load Persistent Baselines (Hardened Load)
    preferences.begin("voc-baseline", true); // Read-only
    gasBaselineInternal = preferences.getFloat("baseInt", 50.0);
    gasBaselinePlenum = preferences.getFloat("basePlen", 50.0);
    preferences.end();
    
    // Sanity check
    if (gasBaselineInternal < 1.0) gasBaselineInternal = 50.0;
    if (gasBaselinePlenum < 1.0) gasBaselinePlenum = 50.0;
    
    lastBaselineSaveTime = millis();

    Serial.println("\n--- Sensor Stack Setup Complete. Loop Starting ---");
}

void loop() {
    bool okInternal = bmeInternal.performReading();
    bool okPlenum = bmePlenum.performReading();
    
    // --- Smart PM Power Management ---
    uint32_t idleTime = millis() - lastActivityTime;
    bool shouldBeActive = !isSystemIdle || isManualMode;
    
    if (shouldBeActive) {
        pmIsSleeping = false; // Sensor stays on physically (I2C limitation)
    } else if (idleTime < 86400000) { // < 24 Hours: Duty Cycle
        uint32_t timeSinceSample = millis() - lastPmSampleTime;
        if (timeSinceSample > PM_SAMPLE_INTERVAL) {
            pmIsSleeping = false; 
            if (timeSinceSample > (PM_SAMPLE_INTERVAL + PM_WAKE_WINDOW)) {
                lastPmSampleTime = millis();
            }
        } else {
            pmIsSleeping = true; 
        }
    } else { // > 24 Hours: Deep Sleep
        pmIsSleeping = true; 
    }

    PM25_AQI_Data pmData;
    bool pmReadOk = false;
    if (!pmIsSleeping) pmReadOk = pm25.read(&pmData);

    // --- Process Internal VOC ---
    float iaqInternal = 0;
    if (okInternal) {
        float rInt = bmeInternal.gas_resistance / 1000.0;
        iaqInternal = map(constrain(rInt, gasBaselineInternal*0.1, gasBaselineInternal), gasBaselineInternal, gasBaselineInternal*0.1, 0, 500);
        
        // If Internal sees cleaner air, update its baseline.
        smoothedIAQInternal = (iaqInternal * uiSmoothingFactor) + (smoothedIAQInternal * (1.0 - uiSmoothingFactor));
        
        // --- Self-Healing Baseline Logic ---
        // 1. Upward (Shared Peak-Seeking): If Internal sees cleaner air, we update both.
        // This prevents the Plenum from 'normalizing' filtered air as its only zero.
        if (rInt > gasBaselineInternal) {
            gasBaselineInternal = (rInt * gasBaselineWeight) + (gasBaselineInternal * (1.0-gasBaselineWeight));
            gasBaselineInternal = constrain(gasBaselineInternal, 10.0, 1500.0); // Reality Cap
            
            if (okPlenum) {
                float rPlen = bmePlenum.gas_resistance / 1000.0;
                // Nudge Plenum upward to stay 'tethered' to the Internal reality.
                if (rPlen > gasBaselinePlenum) {
                    gasBaselinePlenum = (rPlen * gasBaselineWeight) + (gasBaselinePlenum * (1.0-gasBaselineWeight));
                    gasBaselinePlenum = constrain(gasBaselinePlenum, 10.0, 1500.0);
                }
            }
        } 
        // 2. Downward (Drift Correction): If air is stable (>10 IAQ) for 15m.
        else if (smoothedIAQInternal > 10.0) {
            if (abs(smoothedIAQInternal - lastDriftRef) < 10.0) { // Stable (relaxed) for 15m
                if (millis() - lastDriftCheckTime > DRIFT_WINDOW_MS) {
                    // Nudge baseline 10% closer to current resistance
                    gasBaselineInternal = (gasBaselineInternal * 0.90) + (rInt * 0.10);
                    gasBaselineInternal = constrain(gasBaselineInternal, 10.0, 1500.0);
                    lastDriftCheckTime = millis();
                    saveBaselines(); // Immediate Save
                    Serial.println(">> VOC Drift Correction: Nudging Baseline Down.");
                }
            } else {
                lastDriftCheckTime = millis();
                lastDriftRef = smoothedIAQInternal;
            }
        }

        if (millis() - lastLogTime > 5000) {
            Serial.printf("Internal: %.1fkOhm [Base: %.1f] IAQ: %.0f (Smooth: %.0f)\n", rInt, gasBaselineInternal, iaqInternal, smoothedIAQInternal);
        }
    }

    // --- Process Plenum VOC ---
    float iaqPlenum = 0;
    if (okPlenum) {
        float rPlen = bmePlenum.gas_resistance / 1000.0;
        
        // Plenum can also update its own baseline independently if it sees cleaner air.
        if (rPlen > gasBaselinePlenum) gasBaselinePlenum = (rPlen * gasBaselineWeight) + (gasBaselinePlenum * (1.0-gasBaselineWeight));

        // Use individual baseline for IAQ calculation (fixes the scale gap)
        iaqPlenum = map(constrain(rPlen, gasBaselinePlenum*0.1, gasBaselinePlenum), gasBaselinePlenum, gasBaselinePlenum*0.1, 0, 500);
        
        // UI Smoothing
        smoothedIAQPlenum = (iaqPlenum * uiSmoothingFactor) + (smoothedIAQPlenum * (1.0 - uiSmoothingFactor));
        
        // CLAMP: Physics check. Air after the filter cannot be dirtier than air before it. 
        if (smoothedIAQPlenum > smoothedIAQInternal) {
            smoothedIAQPlenum = smoothedIAQInternal;
            
            if (smoothedIAQInternal < 50.0) {
                // If the Plenum 'thinks' it is dirtier than the main cabinet air, it must be drifted.
                if (smoothedIAQPlenum > (smoothedIAQInternal + 5.0)) {
                    float rPlen = bmePlenum.gas_resistance / 1000.0;
                    gasBaselinePlenum = (rPlen * 0.05) + (gasBaselinePlenum * 0.95);
                }
            }
        } 
        if (millis() - lastLogTime > 5000) {
            Serial.printf("Plenum  : %.1fkOhm [Base: %.1f] IAQ: %.0f (Smooth: %.0f)\n", rPlen, gasBaselinePlenum, iaqPlenum, smoothedIAQPlenum);
            lastLogTime = millis();
        }
    }

    // PM2.5 Smoothing (Only update if awake and read succeeded)
    if (pmReadOk) {
        smoothedPM25 = ((float)pmData.pm25_env * uiSmoothingFactor) + (smoothedPM25 * (1.0 - uiSmoothingFactor));
        smoothedPM10 = ((float)pmData.pm10_env * uiSmoothingFactor) + (smoothedPM10 * (1.0 - uiSmoothingFactor));
    }

    // --- External Reference Sync (The Anchor) ---
    // Every 5 minutes, if air is stable, sanity-check against Room Air (Node 4).
    if (abs(smoothedIAQInternal - lastVocSyncRef) < 10.0) {
        if (millis() - vocStableStartTime > 300000) { // 5 Minute Stability
            // Unified Self-Healing: The Plenum 'shadows' the Internal's drift correction.
            if (smoothedIAQInternal < 150.0) {
                float roomRef = lastExternalVOC + 5.0; // Room Ref + 5pt buffer
                
                if (smoothedIAQInternal > roomRef) {
                    // Pull BOTH baselines down by 10% (Gently) to maintain relative filter performance.
                    float rIntRaw = bmeInternal.gas_resistance / 1000.0;
                    gasBaselineInternal = (gasBaselineInternal * 0.90) + (rIntRaw * 0.10);
                    gasBaselineInternal = constrain(gasBaselineInternal, 10.0, 1500.0);
                    
                    float rPlenRaw = bmePlenum.gas_resistance / 1000.0;
                    gasBaselinePlenum = (gasBaselinePlenum * 0.90) + (rPlenRaw * 0.10);
                    gasBaselinePlenum = constrain(gasBaselinePlenum, 10.0, 1500.0);

                    Serial.printf(">> Unified System Sync: Room=%.1f\n", lastExternalVOC);
                    saveBaselines(); // Immediate Save
                }
                vocStableStartTime = millis(); 
            }
        }
    } else {
        vocStableStartTime = millis();
        lastVocSyncRef = smoothedIAQInternal;
    }

    // --- Adaptive Idle & Fan Logic ---
    uint8_t targetSpeed = 0;

    // 1. Detect WAKE triggers (Transitions Idle -> Active)
    if (smoothedIAQInternal > IAQ_WAKE_THRESHOLD || smoothedPM25 > PM_WAKE_THRESHOLD || smoothedPM10 > PM10_WAKE_THRESHOLD) {
        if (isSystemIdle) {
            Serial.println(">> IAQ/PM Threshold Trip: Waking Fan.");
            isSystemIdle = false;
        } else {
            // Only log the reset reason occasionally to avoid spam
            static uint32_t lastResetLog = 0;
            if (millis() - lastResetLog > 60000) {
                if (smoothedIAQInternal > IAQ_WAKE_THRESHOLD) Serial.printf(">> Activity Reset: High VOC (%.1f)\n", smoothedIAQInternal);
                else if (smoothedPM25 > PM_WAKE_THRESHOLD) Serial.printf(">> Activity Reset: High PM2.5 (%.1f)\n", smoothedPM25);
                else Serial.printf(">> Activity Reset: High PM10 (%.1f)\n", smoothedPM10);
                lastResetLog = millis();
            }
        }
        lastActivityTime = millis();
        isSniffing = false; 
    }

    // 2. Detect IDLE conditions (Transitions Active -> Idle)
    if (!isSystemIdle && !isManualMode) {
        if (smoothedIAQInternal > IAQ_IDLE_THRESHOLD) {
            // Air is not clean enough to be idle; reset the timer
            lastActivityTime = millis();
        } else {
            // Air is clean; proceed towards idle (countdown is active)
            uint32_t elapsed = millis() - lastActivityTime;
            if (elapsed > IDLE_STABILITY_MS) {
                Serial.println(">> Air Stable for 30m: Entering Smart Idle.");
                isSystemIdle = true;
                lastSniffTime = millis();
            } else {
                // Periodically log the countdown progress for debugging
                static uint32_t lastProgressLog = 0;
                if (millis() - lastProgressLog > 300000) { // Every 5 mins
                    Serial.printf(">> Idle Countdown: %.0f min remaining...\n", (float)(IDLE_STABILITY_MS - elapsed)/60000.0);
                    lastProgressLog = millis();
                }
            }
        }
    }

    // 3. Handle Manual Mode Override
    if (isManualMode) {
        if (millis() - manualModeStartTime > 600000) { // 10 min auto-revert
            isManualMode = false;
            Serial.println("Manual mode timer expired. Reverting to Smart Idle.");
        } else {
            targetSpeed = manualTargetSpeed;
        }
    }

    // 4. Set Target Speed based on State
    if (!isManualMode) {
        if (!isSystemIdle) {
            // ACTIVE STATE: Calculate required speeds for each metric
            long vocSpeed = MIN_FAN_SPEED;
            long pm25Speed = MIN_FAN_SPEED;
            long pm10Speed = MIN_FAN_SPEED;

            if (smoothedIAQInternal > 50) vocSpeed = map((long)smoothedIAQInternal, 50, 200, MIN_FAN_SPEED, MAX_FAN_SPEED);
            if (smoothedIAQInternal > 300) vocSpeed = PEAK_FAN_SPEED;

            if (smoothedPM25 > PM_WAKE_THRESHOLD) pm25Speed = map((long)smoothedPM25, (long)PM_WAKE_THRESHOLD, 150, MIN_FAN_SPEED, PEAK_FAN_SPEED);
            if (smoothedPM10 > PM10_WAKE_THRESHOLD) pm10Speed = map((long)smoothedPM10, (long)PM10_WAKE_THRESHOLD, 300, MIN_FAN_SPEED, PEAK_FAN_SPEED);

            // Select the highest required speed
            long highestSpeed = max(vocSpeed, max(pm25Speed, pm10Speed));
            
            // Constrain just in case map() overshoots
            targetSpeed = (uint8_t)constrain(highestSpeed, MIN_FAN_SPEED, PEAK_FAN_SPEED);
        } else {
            // IDLE STATE: 0% speed (plus adaptive sniffing)
            uint32_t idleDuration = millis() - lastActivityTime;
            uint32_t sniffInterval = (idleDuration < ADAPTIVE_THRESHOLD_MS) ? SHORT_SNIFF_INTERVAL : LONG_SNIFF_INTERVAL;
            uint32_t timeSinceSniff = millis() - lastSniffTime;

            if (timeSinceSniff > sniffInterval) {
                targetSpeed = 20; 
                isSniffing = true;
                if (timeSinceSniff > (sniffInterval + SNIFF_DURATION_MS)) {
                    lastSniffTime = millis();
                    targetSpeed = 0;
                    isSniffing = false;
                }
            } else {
                targetSpeed = 0;
                isSniffing = false;
            }
        }
    }

    smoothedFanSpeed = (targetSpeed * fanSmoothingFactor) + (smoothedFanSpeed * (1.0 - fanSmoothingFactor));
    setFanSpeed((uint8_t)round(smoothedFanSpeed));

    // Periodic Baseline Save
    if (millis() - lastBaselineSaveTime > BASELINE_SAVE_INTERVAL) {
        saveBaselines();
        lastBaselineSaveTime = millis();
    }

    // G4 Filter Health (Pressure Differential with Software Tare)
    float pInt = bmeInternal.pressure;
    float pFil = lastFilterPressure;
    float g4Drop = (pInt - pFil) - G4_FILTER_TARE_OFFSET;

    if (okInternal) {
        // Only evaluate pressure drop if the fan is actually pulling significant air
        if (smoothedFanSpeed > 30.0 && g4Drop > G4_ALERT_THRESHOLD) {
            if (filterAlertStartTime == 0) filterAlertStartTime = millis();
            uint32_t elapsed = millis() - filterAlertStartTime;
            
            if (elapsed > FILTER_ALERT_WINDOW_MS) {
                currentData.filterAlert = true;
                Serial.printf("!! G4 FILTER ALERT ACTIVE (Drop: %.1f) !!\n", g4Drop);
            } else {
                Serial.printf("G4 PENDING: %.0fs (Drop: %.1f)\n", (float)(FILTER_ALERT_WINDOW_MS - elapsed)/1000.0, g4Drop);
            }
        } else {
            filterAlertStartTime = 0;
            currentData.filterAlert = false;
        }
    } else {
        // SENSOR GLITCH: We do NOT reset the timer here. We just skip this loop's update.
        // This makes the alert 'sticky' even if the I2C bus is noisy.
    }
    if (okPlenum) currentData.plenumStaticP = bmePlenum.pressure;

    // Carbon Filter Health (Efficiency check with time-based window)
    if (smoothedIAQInternal > 100.0) { 
        float efficiency = 0;
        if (smoothedIAQInternal > 0.1) { // Zero-Division Shield
            efficiency = (smoothedIAQInternal - smoothedIAQPlenum) / smoothedIAQInternal;
        }
        
        if (efficiency < 0.5) {
            if (carbonAlertStartTime == 0) carbonAlertStartTime = millis();
            if (millis() - carbonAlertStartTime > CARBON_ALERT_WINDOW_MS) {
                currentData.carbonAlert = true;
            }
        } else {
            carbonAlertStartTime = 0;
            currentData.carbonAlert = false;
        }
    } else {
        carbonAlertStartTime = 0;
        currentData.carbonAlert = false;
    }

    // Pack Telemetry
    if (okInternal) {
        float tRaw = bmeInternal.temperature;
        float tComp = tRaw + TEMP_OFFSET;
        currentData.internalTemp = tComp;
        currentData.internalHum = compensateHumidity(tRaw, bmeInternal.humidity, tComp);
        currentData.internalVOC = smoothedIAQInternal;
        currentData.internalStaticP = bmeInternal.pressure;
    }
    currentData.particulate = smoothedPM25;
    currentData.particulate10 = smoothedPM10;
    currentData.plenumVOC = smoothedIAQPlenum;
    currentData.senderNode = 2;
    currentData.fanSpeed = (uint8_t)round(smoothedFanSpeed); // FIX: Pack actual dynamic speed for the Hub's local sync
    // Send telemetry directly to Hub
    esp_now_send(hubAddress, (uint8_t *) &currentData, sizeof(currentData));
    delay(2000); 
}

void saveBaselines() {
    static float lastSavedInt = 0;
    static float lastSavedPlen = 0;
    
    // Smart-Save: Only write if values have changed by more than 0.5%
    if (abs(gasBaselineInternal - lastSavedInt) < 0.5 && abs(gasBaselinePlenum - lastSavedPlen) < 0.5) {
        return; 
    }

    preferences.begin("voc-baseline", false); // Read-Write
    preferences.putFloat("baseInt", gasBaselineInternal);
    preferences.putFloat("basePlen", gasBaselinePlenum);
    preferences.end(); // Force flush
    
    lastSavedInt = gasBaselineInternal;
    lastSavedPlen = gasBaselinePlenum;
    Serial.println(">> VOC Baselines COMMITTED to Flash.");
}
