/* 
 * Pressure Calibration Script (Node 2 Master)
 * 
 * INSTRUCTIONS:
 * 1. Flash this to Node 2 (Sensor Stack).
 * 2. Ensure Node 3 (Filter Monitor) is running its NORMAL firmware.
 * 3. Open Serial Monitor on Node 2 (115200 baud).
 * 4. This script will cycle the fan and print the Delta-P for both the 
 *    Plenum and the G4 Filter.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "../common/Protocol.h"

#define BME680_INTERNAL_ADDR 0x77
#define BME680_PLENUM_ADDR   0x76 
#define GP8413_ADDR         0x59  // M5Stack Unit DAC2 default

// Sensor Objects (QTPY S3 uses Wire1 for STEMMA QT)
Adafruit_BME680 bmeInternal(&Wire1);
Adafruit_BME680 bmePlenum(&Wire1);

// Shared Global for Node 3 Data
float node3Pressure = 0;

// ESP-NOW Callback (Receive from Node 3)
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorData)) {
        SensorData *data = (SensorData *)incomingData;
        if (data->senderNode == 3) {
            node3Pressure = data->filterStaticP;
        }
    }
}

void setFanSpeed(uint8_t percent) {
    uint16_t dacValue = (uint32_t)percent * 32767 / 100;
    uint16_t transmissionValue = dacValue << 1;
    
    Wire1.beginTransmission(GP8413_ADDR);
    // Register 0x01 = 0x11 (10V range), Register 0x02 = Data
    Wire1.write(0x02); 
    Wire1.write(transmissionValue & 0xFF);
    Wire1.write((transmissionValue >> 8) & 0xFF);
    Wire1.endTransmission();
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(2000);
    
    Wire1.begin();
    
    if (!bmeInternal.begin(BME680_INTERNAL_ADDR)) Serial.println("Internal Fail");
    if (!bmePlenum.begin(BME680_PLENUM_ADDR))     Serial.println("Plenum Fail");

    // WiFi & ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Fail");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // Initialize DAC Range (Register 0x01 = 0x11 for 0-10V mode)
    Wire1.beginTransmission(GP8413_ADDR);
    Wire1.write(0x01); 
    Wire1.write(0x11);
    Wire1.endTransmission();

    Serial.println("\n--- Pressure Calibration Starting ---");
    Serial.println("Ensure Node 3 is powered and transmitting!");
    Serial.println("Speed(%), P_Internal(Pa), P_Plenum(Pa), P_Node3(Pa), Fan_Static_Head(Pa), G4_Filter_Drop(Pa)");
}

void loop() {
    uint8_t testSpeeds[] = {0, 20, 40, 60, 80, 100};
    
    for (int i = 0; i < 6; i++) {
        uint8_t speed = testSpeeds[i];
        setFanSpeed(speed);
        
        Serial.printf("\nStabilizing at %d%%...\n", speed);
        delay(15000); // 15s for pressure to stabilize and to catch Node 3 packets
        
        float avgInternal = 0, avgPlenum = 0, avgNode3 = 0;
        int samples = 20;

        for(int j=0; j<samples; j++) {
            bmeInternal.performReading();
            bmePlenum.performReading();
            avgInternal += bmeInternal.pressure;
            avgPlenum += bmePlenum.pressure;
            avgNode3 += node3Pressure;
            delay(200);
        }
        
        avgInternal /= samples;
        avgPlenum /= samples;
        avgNode3 /= samples;
        
        Serial.printf("%d, %.2f, %.2f, %.2f, %.2f, %.2f\n", 
            speed, avgInternal, avgPlenum, avgNode3, 
            avgPlenum - avgInternal, avgInternal - avgNode3);
    }
    
    Serial.println("\nCalibration Complete. Please copy this table.");
    setFanSpeed(20); // Back to baseline
    while(1); 
}
