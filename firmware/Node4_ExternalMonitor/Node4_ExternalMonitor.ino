#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_PM25AQI.h>
#include <Preferences.h>
#include "../common/Protocol.h"

// Forward Declaration for Compiler
void saveBaseline();

// I2C Address
#define BME680_EXT_ADDR 0x77

// Sensor Objects
Adafruit_BME680 bmeExt(&Wire1);
Adafruit_PM25AQI pm25;

// Temperature Compensation
#define TEMP_OFFSET -12.0 // Corrected to match room ambient (was -15.0)

// Peer Data (Directed to Hub)
uint8_t hubAddress[] = {0x84, 0xF7, 0x03, 0x73, 0xB3, 0x7E}; // Node 1 (Hub) MAC
uint8_t node2Address[] = {0xD8, 0x3B, 0xDA, 0x8E, 0xF7, 0x3C}; // Node 2 (Sensor Stack) MAC
SensorData currentData;

// Baseline for IAQ (Adaptive, similar to Node 2)
float gasBaseline = 50.0;
const float gasBaselineWeight = 0.05;

// UI Smoothing (EMA)
float smoothedIAQ = 0;
float smoothedPM25 = 0;
float smoothedPM10 = 0;
const float uiSmoothingFactor = 0.5;
Preferences preferences;
uint32_t lastBaselineSaveTime = 0;
#define BASELINE_SAVE_INTERVAL 300000 // 5 Minutes (Checkpoint Timer)
const uint32_t DRIFT_WINDOW_MS = 900000; // 15 Minutes for a drift nudge
uint32_t lastDriftCheckTime = 0;
float lastVocSyncRef = 0;
uint32_t lastLogTime = 0;
// Note: PM Power Management removed (hardware requires SET-pin wire for sleep)

// ESP-NOW Send Callback
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Transmission Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

// RH Compensation using the Magnus Formula
float compensateHumidity(float t_raw, float rh_raw, float t_comp) {
    auto svp = [](float t) {
        return 6.112 * exp((17.625 * t) / (243.04 + t));
    };
    float rh_true = rh_raw * (svp(t_raw) / svp(t_comp));
    return constrain(rh_true, 0.0, 100.0);
}

void setup() {
    Serial.begin(115200);
    //while (!Serial);
    delay(2000); // Wait for serial monitor
    Serial.println("\n--- Node 4 External Monitor Booting ---");
    Wire1.begin();
    
    if (!bmeExt.begin(BME680_EXT_ADDR)) Serial.println("External BME680 Fail");
    if (!pm25.begin_I2C(&Wire1))        Serial.println("PMSA0031 Fail");

    bmeExt.setTemperatureOversampling(BME680_OS_8X);
    bmeExt.setHumidityOversampling(BME680_OS_2X);
    bmeExt.setPressureOversampling(BME680_OS_4X);
    bmeExt.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bmeExt.setGasHeater(320, 150);

    // WiFi & ESP-NOW
    Serial.println("Init WiFi & ESP-NOW...");
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }

    // Register send callback
    esp_now_register_send_cb((esp_now_send_cb_t)onDataSent);

    // Register Hub Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, hubAddress, 6);
    peerInfo.channel = 0; 
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Hub peer");
        return;
    }

    // Register Node 2 Peer
    memcpy(peerInfo.peer_addr, node2Address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add Node 2 peer");
        return;
    }

    // Load Persistent Baseline (Hardened Load)
    preferences.begin("voc-baseline", true);
    gasBaseline = preferences.getFloat("baseExt", 50.0);
    preferences.end();
    
    if (gasBaseline < 1.0) gasBaseline = 50.0;
    lastBaselineSaveTime = millis();

    Serial.println("\n--- External Monitor Setup Complete. 24/7 Mode ---");
}

void loop() {
    bool bmeOk = bmeExt.performReading();
    
    PM25_AQI_Data pmData;
    bool pmSuccess = pm25.read(&pmData);

    // Adaptive IAQ Logic
    float currentGasRes = bmeOk ? (bmeExt.gas_resistance / 1000.0) : gasBaseline;
    float iaqCalculated = map(constrain(currentGasRes, gasBaseline * 0.1, gasBaseline), gasBaseline, gasBaseline * 0.1, 0, 500);

    // --- Self-Healing Baseline Logic ---
    // 1. Upward (Standard): If air is cleaner than base, jump up.
    if (currentGasRes > gasBaseline) {
        gasBaseline = (currentGasRes * gasBaselineWeight) + (gasBaseline * (1.0 - gasBaselineWeight));
        gasBaseline = constrain(gasBaseline, 10.0, 1500.0); // Reality Cap
        // No immediate save here to prevent spam during initial climb. 
        // Periodic save handles this.
    }
    // 2. Downward (Drift Correction): If air is stable (>10 IAQ) for 5 minutes.
    else if (smoothedIAQ > 10.0) {
        if (abs(smoothedIAQ - lastVocSyncRef) < 5.0) { // Tightened stability window
            if (millis() - lastDriftCheckTime > 300000) { 
                // Nudge baseline 10% closer to current resistance (Gentler)
                gasBaseline = (gasBaseline * 0.90) + (currentGasRes * 0.10);
                gasBaseline = constrain(gasBaseline, 10.0, 1500.0);
                saveBaseline(); // Immediate Save
                lastDriftCheckTime = millis();
                Serial.println(">> VOC Drift Correction: Nudging Baseline Down.");
            }
        } else {
            lastDriftCheckTime = millis();
            lastVocSyncRef = smoothedIAQ;
        }
    }

    // Filter Smoothing
    smoothedIAQ = (iaqCalculated * uiSmoothingFactor) + (smoothedIAQ * (1.0 - uiSmoothingFactor));
    
    float rawPM = pmSuccess ? (float)pmData.pm25_env : 0;
    float rawPM10 = pmSuccess ? (float)pmData.pm10_env : 0;
    smoothedPM25 = (rawPM * uiSmoothingFactor) + (smoothedPM25 * (1.0 - uiSmoothingFactor));
    smoothedPM10 = (rawPM10 * uiSmoothingFactor) + (smoothedPM10 * (1.0 - uiSmoothingFactor));

    // Populate Protocol
    float tRaw = bmeExt.temperature;
    float tComp = tRaw + TEMP_OFFSET;
    currentData.externalTemp = bmeOk ? tComp : 0;
    currentData.externalHum = bmeOk ? compensateHumidity(tRaw, bmeExt.humidity, tComp) : 0;
    currentData.externalVOC = smoothedIAQ;
    currentData.externalParticulate = smoothedPM25;
    currentData.externalParticulate10 = smoothedPM10;
    currentData.externalStaticP = bmeOk ? bmeExt.pressure : 0;
    currentData.senderNode = 4;
    currentData.timestamp = millis();

    // Print to Serial (Every 5 seconds)
    if (millis() - lastLogTime > 5000) {
        Serial.println("\n--- Sensor Readings ---");
        Serial.printf("Temp: %.1f C (Raw: %.1f)\n", currentData.externalTemp, bmeExt.temperature);
        Serial.printf("Hum: %.1f %%\n", currentData.externalHum);
        Serial.printf("VOC IAQ: %.0f [Base: %.1f]\n", currentData.externalVOC, gasBaseline);
        if (pmSuccess) {
            Serial.printf("PM2.5 (Standard): %.1f ug/m3\n", (float)pmData.pm25_standard);
            Serial.printf("PM2.5 (Environmental): %.1f ug/m3\n", (float)pmData.pm25_env);
            Serial.printf("PM10 (Environmental) : %.1f ug/m3\n", (float)pmData.pm10_env);
        } else {
            Serial.println("PM2.5 Sensor Read Fail");
        }
        lastLogTime = millis();
    }

    // Send to Hub and Node 2
    esp_now_send(hubAddress, (uint8_t *) &currentData, sizeof(currentData));
    esp_now_send(node2Address, (uint8_t *) &currentData, sizeof(currentData));

    // Periodic Baseline Save
    if (millis() - lastBaselineSaveTime > BASELINE_SAVE_INTERVAL) {
        saveBaseline();
        lastBaselineSaveTime = millis();
    }

    delay(2000); // 2s polling
}

void saveBaseline() {
    static float lastSavedBase = 0;
    
    // Smart-Save: Only write if value changed by > 0.5%
    if (abs(gasBaseline - lastSavedBase) < 0.5) return;

    preferences.begin("voc-baseline", false); // Read-Write
    preferences.putFloat("baseExt", gasBaseline);
    preferences.end();
    
    lastSavedBase = gasBaseline;
    Serial.println(">> VOC Baseline COMMITTED to Flash.");
}
