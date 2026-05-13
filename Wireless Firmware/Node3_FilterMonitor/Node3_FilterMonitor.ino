#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "../../common/Protocol.h"
#include "../common/WirelessConfig.h"

// I2C Address
#define BME680_FILTER_ADDR 0x77

// Sensor Object
Adafruit_BME680 bmeFilter(&Wire1);

// Peer Data (Directed to Hub and Stack)
uint8_t hubAddress[] = {0x84, 0xF7, 0x03, 0x73, 0xB3, 0x7E};
uint8_t node2Address[] = {0xD8, 0x3B, 0xDA, 0x8E, 0xF7, 0x3C};
SensorData outData;

void setup() {
    Serial.begin(115200);
    Wire1.begin();
    
    if (!bmeFilter.begin(BME680_FILTER_ADDR)) {
        Serial.println("Filter BME680 Fail");
    }

    bmeFilter.setPressureOversampling(BME680_OS_4X);
    bmeFilter.setIIRFilterSize(BME680_FILTER_SIZE_3);

    // --- Wireless OTA: Connect WiFi & Init ESP-NOW ---
    setupWirelessOTA("cabinet-filtermonitor");

    // --- ESP-NOW Peers (registered AFTER wireless init) ---
    esp_now_peer_info_t peer1 = {};
    memcpy(peer1.peer_addr, hubAddress, 6);
    peer1.channel = 0; peer1.encrypt = false;
    esp_now_add_peer(&peer1);

    esp_now_peer_info_t peer2 = {};
    memcpy(peer2.peer_addr, node2Address, 6);
    peer2.channel = 0; peer2.encrypt = false;
    esp_now_add_peer(&peer2);

    Serial.println("\n--- Filter Monitor Setup Complete (Wireless OTA). Loop Starting ---");
}

void loop() {
    // --- OTA: Check for incoming wireless updates ---
    ArduinoOTA.handle();

    if (!bmeFilter.performReading()) return;

    outData.filterStaticP = bmeFilter.pressure;
    outData.senderNode = 3;
    outData.timestamp = millis();

    esp_now_send(NULL, (uint8_t *) &outData, sizeof(outData));

    delay(2000);
}
