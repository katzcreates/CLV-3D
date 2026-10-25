#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_PM25AQI.h>
#include <Preferences.h>
#include "../common/Protocol.h"
#include "../common/WirelessConfig.h"

// Set to -1 when disconnected, or your actual pin (e.g., A0) when wired
#define PM_SET_PIN -1

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
#define CARBON_WARN_WINDOW_MS  1800000    // 30 Minutes for Level 1 Warning (<70% eff)
#define CARBON_ALERT_WINDOW_MS 3600000   // 60 Minutes for Level 2 Critical (<50% eff)
#define FILTER_ALERT_WINDOW_MS 30000 
#define CARBON_GATE_IAQ        75.0      // Minimum IAQ before efficiency is checked
#define CARBON_WARN_THRESHOLD  0.70      // Level 1 Warning: filter removing <70% of VOCs
#define CARBON_ALERT_THRESHOLD 0.50      // Level 2 Critical: filter removing <50% of VOCs
#define HUMIDITY_GATE_MAX      60.0      // Maximum RH % before carbon monitoring is frozen
#define SAFE_DRIFT_IAQ_CEILING 25.0      // Only drift down when IAQ is genuinely clean (<25)
#define ROOM_EVENT_IAQ_GATE    50.0      // If Room VOC > 50, external event is active

// Smart Idle & Adaptive Sniffing Constants
#define IDLE_STABILITY_MS      3600000   // 1 Hour (must stay clean for a full hour)
#define SHORT_SNIFF_INTERVAL   21600000  // 6 Hours
#define LONG_SNIFF_INTERVAL    86400000  // 24 Hours
#define ADAPTIVE_THRESHOLD_MS  86400000  
#define SNIFF_DURATION_MS      120000    
#define BASELINE_SAVE_INTERVAL 300000    // 5 Minutes (Checkpoint Timer)
#define IAQ_WAKE_THRESHOLD     50.0      // Wake up threshold
#define IAQ_IDLE_THRESHOLD     10.0      // Entry buffer (Must be below 10 to start 1h timer)
#define PM_WAKE_THRESHOLD      15.0
#define PM10_WAKE_THRESHOLD    30.0

// Sensor Objects
Adafruit_BME680 bmeInternal(&Wire1);
Adafruit_BME680 bmePlenum(&Wire1);
Adafruit_PM25AQI pm25;

// Temperature Compensation
#define TEMP_OFFSET -13.0 // Reduced from -15.0 based on fan cooling data

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
uint32_t carbonWarnStartTime = 0;    // Tracks how long efficiency has been in the warning zone
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
const float G4_ALERT_THRESHOLD = 5.0; 
float g4RunHours = 0.0;     

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

float lastExternalPM25 = 0.0;

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorData)) {
        SensorData *receivedData = (SensorData *)incomingData;
        if (receivedData->senderNode == 3) {
            lastFilterPressure = receivedData->filterStaticP;
        } else if (receivedData->senderNode == 4) {
            lastExternalVOC = receivedData->externalVOC;
            lastExternalPM25 = receivedData->externalParticulate;
        }
    } else if (len == sizeof(ControlCommand)) {
        ControlCommand *cmd = (ControlCommand *)incomingData;
        if (cmd->senderNode == 1) { // Node 1 (Hub)
            if (cmd->commandType == 1) { // Manual Mode Override
                isManualMode = true;
                manualTargetSpeed = cmd->manualSpeed;
                manualModeStartTime = millis();
                isSystemIdle = false;        // Interrupt Idle immediately
                lastActivityTime = millis(); // Reset the Idle countdown
                Serial.printf("Hub Override: Manual %d%% â€” Idle timer reset.\n", manualTargetSpeed);
            } else if (cmd->commandType == 0) { // Auto Mode
                isManualMode = false;
                lastActivityTime = millis(); // Start fresh 1-hour countdown
                Serial.println("Hub Override: Auto Mode â€” Idle timer reset.");
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

    // --- Wireless OTA: Connect WiFi & Init ESP-NOW ---
    setupWirelessOTA("cabinet-sensorstack");

    // --- ESP-NOW Callbacks & Peers (registered AFTER wireless init) ---
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

    preferences.begin("g4-health", true);
    g4RunHours = preferences.getFloat("g4Hours", 0.0);
    preferences.end();
    
    if (gasBaselineInternal < 1.0) gasBaselineInternal = 50.0;
    if (gasBaselinePlenum < 1.0) gasBaselinePlenum = 50.0;
    
lastBaselineSaveTime = millis();

    //Check PM SET Pin Connection
    if (PM_SET_PIN >= 0) {
    pinMode(PM_SET_PIN, OUTPUT);
    digitalWrite(PM_SET_PIN, HIGH); // Sensor on by default
    }

    Serial.println("\n--- Sensor Stack Setup Complete (Wireless OTA). Loop Starting ---");
}

void loop() {
    // --- OTA: Check for incoming wireless updates ---
    ArduinoOTA.handle();

    static uint32_t lastSensorRead = 0;
    if (millis() - lastSensorRead > 2000) {

    bool okInternal = bmeInternal.performReading();
    bool okPlenum = bmePlenum.performReading();
    
    // --- Smart PM Power Management ---
    uint32_t idleTime = millis() - lastActivityTime;
    if (!isSystemIdle) {
        if (pmIsSleeping) { 
            if (PM_SET_PIN >= 0) digitalWrite(PM_SET_PIN, HIGH); 
            pmIsSleeping = false; 
        }
    } else if (idleTime < 86400000) {
        uint32_t timeSinceSample = millis() - lastPmSampleTime;
        if (timeSinceSample > PM_SAMPLE_INTERVAL) {
            if (pmIsSleeping) { 
                if (PM_SET_PIN >= 0) digitalWrite(PM_SET_PIN, HIGH); 
                pmIsSleeping = false; 
            }
            if (timeSinceSample > (PM_SAMPLE_INTERVAL + PM_WAKE_WINDOW)) {
                lastPmSampleTime = millis();
            }
        } else {
            if (!pmIsSleeping) { 
                if (PM_SET_PIN >= 0) digitalWrite(PM_SET_PIN, LOW); 
                pmIsSleeping = true; 
            }
        }
    } else {
        if (!pmIsSleeping) { 
            if (PM_SET_PIN >= 0) digitalWrite(PM_SET_PIN, LOW); 
            pmIsSleeping = true; 
        }
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
        if (rInt > gasBaselineInternal) {
            gasBaselineInternal = (rInt * gasBaselineWeight) + (gasBaselineInternal * (1.0-gasBaselineWeight));
            gasBaselineInternal = constrain(gasBaselineInternal, 10.0, 1000.0);
            
            if (okPlenum) {
                float rPlen = bmePlenum.gas_resistance / 1000.0;
                if (rPlen > gasBaselinePlenum) {
                    gasBaselinePlenum = (rPlen * gasBaselineWeight) + (gasBaselinePlenum * (1.0-gasBaselineWeight));
                    gasBaselinePlenum = constrain(gasBaselinePlenum, 10.0, 1000.0);
                }
            }
        } 
        
        // 2. Continuous Baseline Decay
        static uint32_t lastDecayTime = millis();
        if (millis() - lastDecayTime > 21600000) { // 6 Hours
            gasBaselineInternal = constrain(gasBaselineInternal * 0.98, 10.0, 1000.0);
            if (okPlenum) {
                gasBaselinePlenum = constrain(gasBaselinePlenum * 0.98, 10.0, 1000.0);
            }
            lastDecayTime = millis();
            saveBaselines();
            Serial.println(">> Baseline Decay: Relaxing zero-point by 2%.");
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
        
        // Plenum Drift Check: Air after the filter cannot be significantly dirtier than air before it.
        if (smoothedIAQInternal < 50.0 && smoothedIAQPlenum > (smoothedIAQInternal + 5.0)) {
            // Nudge plenum baseline down to close the phantom gap
            float rPlen = bmePlenum.gas_resistance / 1000.0;
            gasBaselinePlenum = (rPlen * 0.10) + (gasBaselinePlenum * 0.90);
            gasBaselinePlenum = constrain(gasBaselinePlenum, 10.0, 1500.0);
        }
        // Display clamp: During active airflow, clamp plenum for UI consistency
        if (smoothedFanSpeed > 10.0 && smoothedIAQPlenum > smoothedIAQInternal) {
            smoothedIAQPlenum = smoothedIAQInternal;
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

    // --- Node 4 External Event Shield ---
    bool isExternalRoomEvent = (lastExternalVOC > ROOM_EVENT_IAQ_GATE) || (lastExternalPM25 > 35.0);
    if (isExternalRoomEvent) {
        static uint32_t lastRoomLog = 0;
        if (millis() - lastRoomLog > 60000) {
            Serial.printf(">> External Room Event Active: Room IAQ=%.0f (Carbon monitor shielded)\n", lastExternalVOC);
            lastRoomLog = millis();
        }
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
        // Must be clean across ALL metrics to proceed to idle
        if (smoothedIAQInternal > IAQ_IDLE_THRESHOLD || smoothedPM25 > 5.0 || smoothedPM10 > 10.0) {
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
            isSystemIdle = false;        // Never jump straight back to Idle
            lastActivityTime = millis(); // Must serve the full 1-hour countdown
            Serial.println("Manual mode expired. Starting fresh 1-hour Idle countdown.");
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

// --- G4 Filter Health (Particulate-Weighted Run-Hours) ---
    static uint32_t lastRunHourUpdate = millis();
    
    if (smoothedFanSpeed > 15.0) {
        float hoursPassed = (millis() - lastRunHourUpdate) / 3600000.0;
        float wearMultiplier = 0.05; 
        
        if (smoothedPM10 > 300.0) wearMultiplier = 10.0;
        else if (smoothedPM10 > 150.0) wearMultiplier = 5.0;  
        else if (smoothedPM10 > 50.0) wearMultiplier = 2.0;
        else if (smoothedPM10 > 20.0) wearMultiplier = 1.0;
        
        g4RunHours += (hoursPassed * wearMultiplier);
    }
    lastRunHourUpdate = millis();

    // Trigger alert at 500 equivalent hours
    currentData.filterAlert = (g4RunHours > 500.0);

    // Save run-hours to NVS every hour
    static uint32_t lastRunHourSave = millis();
    if (millis() - lastRunHourSave > 3600000) { 
        preferences.begin("g4-health", false);
        preferences.putFloat("g4Hours", g4RunHours);
        preferences.end();
        lastRunHourSave = millis();
    }
    if (okPlenum) currentData.plenumStaticP = bmePlenum.pressure;

    // Pack Internal Environmental Telemetry (needed for fresh humidity in carbon check)
    if (okInternal) {
        float tRaw = bmeInternal.temperature;
        float tComp = tRaw + TEMP_OFFSET;
        currentData.internalTemp = tComp;
        currentData.internalHum = compensateHumidity(tRaw, bmeInternal.humidity, tComp);
        currentData.internalVOC = smoothedIAQInternal;
        currentData.internalStaticP = bmeInternal.pressure;
    }

    // Carbon Filter Health (Two-tier efficiency check with true pause gates)
    // Level 1 Warning  (amber): efficiency < 70% for 15 min — check filter
    // Level 2 Critical (red):   efficiency < 50% for 20 min — replace filter
    // GATES: High humidity (>60%) or External Room Spike (>50 IAQ) freeze timers.
    static uint32_t lastCarbonEvalTime = millis();
    uint32_t loopDt = millis() - lastCarbonEvalTime;
    lastCarbonEvalTime = millis();

    bool isHighHumidity = (currentData.internalHum > HUMIDITY_GATE_MAX);
    bool isCarbonPaused = isHighHumidity || isExternalRoomEvent;

    if (isCarbonPaused) {
        // TRUE FREEZE: Shift start times forward by loopDt so elapsed duration stays frozen in place
        if (carbonAlertStartTime != 0) carbonAlertStartTime += loopDt;
        if (carbonWarnStartTime != 0) carbonWarnStartTime += loopDt;
    } else if (smoothedIAQInternal > CARBON_GATE_IAQ) {
        float efficiency = (smoothedIAQInternal > 0.1)
            ? (smoothedIAQInternal - smoothedIAQPlenum) / smoothedIAQInternal
            : 0.0;

        if (efficiency < CARBON_ALERT_THRESHOLD) {
            // Critical zone (<50%): reset warn timer, evaluate 20m critical timer
            carbonWarnStartTime = 0;
            if (carbonAlertStartTime == 0) carbonAlertStartTime = millis();
            if (millis() - carbonAlertStartTime > CARBON_ALERT_WINDOW_MS) {
                currentData.carbonAlert = 2;
            }
        } else if (efficiency < CARBON_WARN_THRESHOLD) {
            // Warning zone (50-70%): reset critical timer, evaluate 15m warn timer
            carbonAlertStartTime = 0;
            if (currentData.carbonAlert == 2) currentData.carbonAlert = 1;
            if (carbonWarnStartTime == 0) carbonWarnStartTime = millis();
            if (millis() - carbonWarnStartTime > CARBON_WARN_WINDOW_MS) {
                currentData.carbonAlert = 1;
            }
        } else {
            // Clean (>70%): clear timers and alert
            carbonAlertStartTime = 0;
            carbonWarnStartTime = 0;
            currentData.carbonAlert = 0;
        }
    } else {
        // IAQ below evaluation gate (<75): clear timers and alert
        carbonAlertStartTime = 0;
        carbonWarnStartTime = 0;
        currentData.carbonAlert = 0;
    }

    currentData.particulate = smoothedPM25;
    currentData.particulate10 = smoothedPM10;
    currentData.plenumVOC = smoothedIAQPlenum;
    currentData.senderNode = 2;
    currentData.fanSpeed = (uint8_t)round(smoothedFanSpeed); // FIX: Pack actual dynamic speed for the Hub's local sync
    // Send telemetry directly to Hub
    esp_now_send(hubAddress, (uint8_t *) &currentData, sizeof(currentData));
    lastSensorRead = millis();
    }
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
