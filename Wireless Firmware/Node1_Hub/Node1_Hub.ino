#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Preferences.h>
#include "../../common/Protocol.h"
#include "../common/WirelessConfig.h"

// TFT Pins for Feather S2 + 2.4" TFT Wing
#define STMPE_CS 38
#define TFT_CS   1
#define TFT_DC   3
#define AD_CS    33
#define TFT_LED  8

#define DIM_TIME   300000 
#define SLEEP_TIME 900000 

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_STMPE610 touch = Adafruit_STMPE610(STMPE_CS);

#define CLR_BG      0x0821
#define CLR_CARD    0x2104
#define CLR_TEXT    0xFFFF
#define CLR_HEADER  0x4A69
#define CLR_GOOD    0x5700
#define CLR_WARN    0xE620
#define CLR_BAD     0xD000
#define CLR_PURPLE  0x780F
#define CLR_ACCENT  0x039F

SensorData node2Data;
SensorData node4Data;
uint8_t currentPage = 0;
uint32_t lastSeenNode2 = 0, lastSeenNode3 = 0, lastSeenNode4 = 0;
int8_t rssiNode2 = 0, rssiNode3 = 0, rssiNode4 = 0;
uint32_t lastTouchTime = 0, lastPageSwitch = 0;
bool isSleeping = false, isDimmed = false;

// Manual Control variables
uint8_t node2Mac[6];
// UI State
uint32_t lastDisplayUpdate = 0;
const uint32_t displayInterval = 1000; // Refresh display every 1 second automatically

// Node State
bool isNode2Paired = false;
uint8_t currentManualSpeed = 80;
uint32_t lastManualPressTime = 0;

// Cloud Publishing
uint32_t lastCloudPush = 0;

void sendControlCommand(uint8_t type, uint8_t speed) {
    if (!isNode2Paired) return;
    ControlCommand cmd;
    cmd.commandType = type;
    cmd.manualSpeed = speed;
    cmd.senderNode = 1;
    esp_now_send(node2Mac, (uint8_t*)&cmd, sizeof(cmd));
}

uint16_t getIAQColor(float val) {
    if (val < 100) return CLR_GOOD;
    if (val < 250) return CLR_WARN;
    return CLR_BAD;
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    Serial.printf("Received %d bytes from Node %d\n", len, ((SensorData*)incomingData)->senderNode);
    if (len == sizeof(SensorData)) {
        SensorData *newData = (SensorData *)incomingData;
        if (newData->senderNode == 2) {
            if (!esp_now_is_peer_exist(info->src_addr)) {
                memcpy(node2Mac, info->src_addr, 6);
                esp_now_peer_info_t peerInfo = {};
                memcpy(peerInfo.peer_addr, node2Mac, 6);
                peerInfo.channel = 0;
                peerInfo.encrypt = false;
                esp_now_add_peer(&peerInfo);
                isNode2Paired = true;
            }
            lastSeenNode2 = millis();
            rssiNode2 = info->rx_ctrl->rssi;
            node2Data = *newData;
        } else if (newData->senderNode == 3) {
            lastSeenNode3 = millis();
            rssiNode3 = info->rx_ctrl->rssi;
            node2Data.filterStaticP = newData->filterStaticP;
        } else if (newData->senderNode == 4) {
            lastSeenNode4 = millis();
            rssiNode4 = info->rx_ctrl->rssi;
            node4Data = *newData;
        }
        // updateDisplay() REMOVED from this interrupt/callback to prevent severe SPI bus collisions!
        // The display is now safely and smoothly updated by the 1-second background refresh in loop().
    }
}

void drawHeader() {
    uint16_t headerCol = CLR_HEADER;
    const char* alertMsg = "AIR QUALITY DASHBOARD";
    if (currentPage == 1) { headerCol = 0x3186; alertMsg = "SYSTEM STATUS"; }
    else if (currentPage == 2) { headerCol = 0x9300; alertMsg = "MANUAL FAN CONTROL"; }

    if (node2Data.filterAlert && node2Data.carbonAlert) { headerCol = CLR_BAD; alertMsg = "!! CLOGGED FILTERS !!"; }
    else if (node2Data.filterAlert) { headerCol = CLR_BAD; alertMsg = "!! REPLACE G4 FILTER !!"; }
    else if (node2Data.carbonAlert) { headerCol = CLR_WARN; alertMsg = "!! REPLACE CARBON FILTER !!"; }
    
    tft.fillRect(0, 0, 320, 35, headerCol);
    tft.setFont(&FreeSansBold9pt7b);
    tft.setTextColor(CLR_TEXT);
    tft.setCursor(10, 24); 
    tft.print(alertMsg);
}

void drawPage0Static() {
    tft.fillScreen(CLR_BG);
    drawHeader();
    
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(0xAD55);
    tft.setCursor(30, 50); tft.print("CABINET (INT)");
    tft.setCursor(185, 50); tft.print("ROOM (EXT)");
    tft.drawRoundRect(5, 60, 150, 48, 6, CLR_CARD);
    tft.drawRoundRect(165, 60, 150, 48, 6, CLR_CARD);
    tft.drawRoundRect(5, 120, 310, 100, 6, CLR_CARD);
    tft.setFont(NULL);
    tft.setCursor(12, 64); tft.print("VOCs");
    tft.setCursor(172, 64); tft.print("VOCs");
    tft.setCursor(12, 127); tft.print("DETAILED COMPARISON");
    tft.setCursor(205, 225); tft.print("SYSTEM STATUS >");
}

void drawPage1Static() {
    tft.fillScreen(CLR_BG);
    drawHeader();
    
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(0xAD55);
    tft.setCursor(10, 60); tft.print("NETWORK NODES");
    for(int i=0; i<3; i++) tft.drawRoundRect(5, 75 + (i*40), 310, 35, 4, CLR_CARD);
    tft.setFont(NULL);
    tft.setCursor(35, 87); tft.print("NODE 2 (Stack)");
    tft.setCursor(35, 127); tft.print("NODE 3 (Filter)");
    tft.setCursor(35, 167); tft.print("NODE 4 (Ambient)");
    tft.setCursor(195, 225); tft.print("MANUAL CONTROL >");
}

void drawPage2Static() {
    int16_t x1, y1; uint16_t w, h;
    tft.fillScreen(CLR_BG);
    drawHeader();
    
    tft.setFont(&FreeSans9pt7b);
    
    // Auto Button (Bottom Left: 5, 175, 150, 45)
    tft.fillRoundRect(5, 175, 150, 45, 6, CLR_GOOD);
    const char* txtAuto = "AUTO MODE";
    tft.getTextBounds(txtAuto, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(80 - (w/2), 197 - (y1 + h/2));
    tft.print(txtAuto);

    // Boost Button (Bottom Right: 165, 175, 150, 45)
    tft.fillRoundRect(165, 175, 150, 45, 6, CLR_BAD);
    const char* txtBoost = "BOOST MODE";
    tft.getTextBounds(txtBoost, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - (w/2), 197 - (y1 + h/2));
    tft.print(txtBoost);
    
    // Down Button (Left middle)
    tft.fillRoundRect(5, 100, 50, 50, 6, CLR_CARD);
    tft.setCursor(20, 131); tft.print("-");
    
    // Up Button (Right middle)
    tft.fillRoundRect(265, 100, 50, 50, 6, CLR_CARD);
    tft.setCursor(282, 131); tft.print("+");

    tft.setTextColor(0xAD55);
    tft.setCursor(135, 60); tft.print("SPEED");
    tft.setFont(NULL);
    tft.setCursor(225, 225); tft.print("DASHBOARD >");
}

void updatePage2() {
    tft.fillRect(70, 75, 180, 80, CLR_BG); // Clear center area
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(CLR_TEXT);
    char buf[16];
    int16_t x1, y1; uint16_t w, h;
    uint8_t displaySpeed = (millis() - lastManualPressTime < 3000) ? currentManualSpeed : node2Data.fanSpeed;
    sprintf(buf, "%d%%", displaySpeed);
    tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(160 - (w/2), 125); 
    tft.print(buf);
}

void updatePage0() {
    tft.setFont(&FreeSansBold18pt7b); 
    
    tft.fillRect(10, 72, 140, 32, CLR_BG);
    tft.fillRect(170, 72, 140, 32, CLR_BG);
    
    char buf[16];
    int16_t x1, y1;
    uint16_t w, h;
    int card_mid_y = 60 + 24; 
    
    // Internal VOC (Card center X = 80)
    sprintf(buf, "%.0f", node2Data.internalVOC);
    tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(80 - (w/2), card_mid_y - (y1 + h/2)); 
    tft.setTextColor(getIAQColor(node2Data.internalVOC));
    tft.print(buf);
    
    // External VOC (Card center X = 240)
    sprintf(buf, "%.0f", node4Data.externalVOC);
    tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(240 - (w/2), card_mid_y - (y1 + h/2));
    tft.setTextColor(getIAQColor(node4Data.externalVOC));
    tft.print(buf);
    
    // Detailed Comparison block
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(CLR_TEXT);
    tft.fillRect(15, 135, 290, 78, CLR_BG);
    
    float x_det = 80; 
    tft.setCursor(x_det, 150); tft.printf("PM2.5: %.1f | %.1f", (float)node2Data.particulate, (float)node4Data.externalParticulate);
    tft.setCursor(x_det, 170); tft.printf("PM10 : %.1f | %.1f", (float)node2Data.particulate10, (float)node4Data.externalParticulate10);
    tft.setCursor(x_det, 190); tft.printf("Temp : %.1f | %.1f", node2Data.internalTemp, node4Data.externalTemp);
    tft.setCursor(x_det, 210); tft.printf("Humid: %.1f | %.1f", node2Data.internalHum, node4Data.externalHum);
}

void updateDisplay() {
    if (currentPage == 0) {
        drawHeader(); // Live header refresh
        updatePage0();
    } else if (currentPage == 1) {
        tft.setFont(&FreeSans9pt7b);
        auto ds = [&](int y, uint32_t ls, int8_t rs, uint16_t cl) {
            bool al = (millis() - ls < 5000);
            tft.fillCircle(18, y + 17, 5, al ? cl : CLR_BAD);
            tft.setTextColor(al ? CLR_TEXT : CLR_BAD, CLR_BG);
            tft.fillRect(180, y + 5, 120, 25, CLR_BG);
            tft.setCursor(180, y + 23);
            if (al) tft.printf("%d dBm", rs); else tft.print("OFFLINE");
        };
        ds(75, lastSeenNode2, rssiNode2, CLR_GOOD);
        ds(115, lastSeenNode3, rssiNode3, CLR_ACCENT); 
        ds(155, lastSeenNode4, rssiNode4, CLR_PURPLE);
        
        tft.fillRect(10, 195, 300, 25, CLR_BG);
        tft.setCursor(10, 215); tft.setTextColor(0xAD55);
        tft.printf("Fan: %d%% | Plenum VOC: %.0f", node2Data.fanSpeed, node2Data.plenumVOC);
    } else if (currentPage == 2) {
        updatePage2();
    }
    
    static bool lastAlertState = false;
    bool currentAlertState = (node2Data.filterAlert || node2Data.carbonAlert);
    if (currentAlertState != lastAlertState) {
        if (currentPage == 0) drawPage0Static(); else drawPage1Static();
        lastAlertState = currentAlertState;
    }
}

// ============================================================
//  InfluxDB Cloud Push — Sends all sensor data as line protocol
//  Gracefully skips if WiFi is down or INFLUX_URL is empty.
// ============================================================
void pushToCloud() {
    // Guard: Skip if WiFi is disconnected
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Guard: Skip if InfluxDB is not configured (URL is empty)
    if (strlen(INFLUX_URL) == 0) return;

    HTTPClient http;
    String url = String(INFLUX_URL) + "/api/v2/write?org=" + INFLUX_ORG + "&bucket=" + INFLUX_BUCKET + "&precision=s";
    
    http.begin(url);
    http.addHeader("Authorization", String("Token ") + INFLUX_TOKEN);
    http.addHeader("Content-Type", "text/plain");
    http.setTimeout(2000); // 2s timeout to avoid blocking the UI

    // Build InfluxDB line protocol payload
    // Format: measurement field1=val1,field2=val2
    String payload = "cabinet ";
    payload += "int_temp=" + String(node2Data.internalTemp, 1);
    payload += ",int_hum=" + String(node2Data.internalHum, 1);
    payload += ",int_voc=" + String(node2Data.internalVOC, 0);
    payload += ",int_pressure=" + String(node2Data.internalStaticP, 1);
    payload += ",pm25=" + String(node2Data.particulate, 1);
    payload += ",pm10=" + String(node2Data.particulate10, 1);
    payload += ",plenum_voc=" + String(node2Data.plenumVOC, 0);
    payload += ",plenum_pressure=" + String(node2Data.plenumStaticP, 1);
    payload += ",filter_pressure=" + String(node2Data.filterStaticP, 1);
    payload += ",ext_temp=" + String(node4Data.externalTemp, 1);
    payload += ",ext_hum=" + String(node4Data.externalHum, 1);
    payload += ",ext_voc=" + String(node4Data.externalVOC, 0);
    payload += ",ext_pm25=" + String(node4Data.externalParticulate, 1);
    payload += ",ext_pm10=" + String(node4Data.externalParticulate10, 1);
    payload += ",fan_speed=" + String(node2Data.fanSpeed);
    payload += ",filter_alert=" + String(node2Data.filterAlert ? "true" : "false");
    payload += ",carbon_alert=" + String(node2Data.carbonAlert ? "true" : "false");

    int httpCode = http.POST(payload);
    
    if (httpCode == 204) {
        // 204 No Content = InfluxDB accepted the write
        static uint32_t lastSuccessLog = 0;
        if (millis() - lastSuccessLog > 60000) { // Log success once per minute
            Serial.println(">> Cloud: Data pushed OK.");
            lastSuccessLog = millis();
        }
    } else {
        Serial.printf(">> Cloud: Push failed (HTTP %d)\n", httpCode);
    }
    
    http.end();
}

void setup() {
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(CLR_BG);
    pinMode(TFT_LED, OUTPUT);
    analogWrite(TFT_LED, 255);
    touch.begin();

    // --- Wireless OTA: Connect WiFi & Init ESP-NOW ---
    setupWirelessOTA("cabinet-hub");

    // --- ESP-NOW Callbacks (registered AFTER wireless init) ---
    esp_now_register_recv_cb(onDataRecv);

    lastTouchTime = millis();
    currentPage = 0;
    drawPage0Static();
    updateDisplay();
    Serial.println("\n--- Hub Setup Complete (Wireless OTA). Loop Starting ---");
}

void loop() {
    // --- OTA: Check for incoming wireless updates ---
    ArduinoOTA.handle();

    // Re-sync local tracker to Node 2's actual aerodynamic speed grid
    if (millis() - lastManualPressTime > 3000) currentManualSpeed = node2Data.fanSpeed;

    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        long px = map(p.y, 200, 3800, 0, 320);
        long py = map(p.x, 200, 3800, 0, 240);
        bool handled = false;
        
        lastTouchTime = millis();
        if (isSleeping || isDimmed) {
            isSleeping = false; isDimmed = false;
            analogWrite(TFT_LED, 255);
            if (currentPage == 0) drawPage0Static(); else if (currentPage == 1) drawPage1Static(); else if (currentPage == 2) drawPage2Static();
            updateDisplay();
            delay(300);
            return;
        }
        if (millis() - lastPageSwitch > 600) {
            TS_Point p2 = touch.getPoint();
            // Touch mapping (Hardware Corrected)
            long px2 = map(p2.y, 200, 3800, 0, 320);
            long py2 = map(p2.x, 200, 3800, 0, 240);

            if (currentPage == 2) {
                if (px > 0 && px <= 160 && py > 165) {
                    sendControlCommand(0, node2Data.fanSpeed); // AUTO (tell it to resume from its current state if possible)
                    lastManualPressTime = millis();
                    handled = true;
                } else if (px > 160 && px <= 320 && py > 165) {
                    currentManualSpeed = 80;
                    sendControlCommand(1, currentManualSpeed); // HIGH (80%)
                    lastManualPressTime = millis();
                    handled = true;
                } else if (px > 240 && py < 165 && py > 80) {
                    currentManualSpeed = min(100, currentManualSpeed + 10);
                    sendControlCommand(1, currentManualSpeed); // UP
                    lastManualPressTime = millis();
                    handled = true;
                } else if (px < 80 && py < 165 && py > 80) {
                    if (currentManualSpeed >= 10) currentManualSpeed -= 10;
                    sendControlCommand(1, currentManualSpeed); // DOWN
                    lastManualPressTime = millis();
                    handled = true;
                }
                if (handled) delay(200); // UI Debounce for manual buttons
            }

            if (!handled) {
                currentPage = (currentPage + 1) % 3;
                if (currentPage == 0) drawPage0Static();
                else if (currentPage == 1) drawPage1Static();
                else if (currentPage == 2) drawPage2Static();
            }
            updateDisplay();
            lastPageSwitch = millis();
        }
    }
    uint32_t elapsed = millis() - lastTouchTime;
    if (elapsed > SLEEP_TIME && !isSleeping) {
        isSleeping = true; analogWrite(TFT_LED, 0);
    } else if (elapsed > DIM_TIME && !isDimmed) {
        isDimmed = true; analogWrite(TFT_LED, 25);
    }

    // Background Display Refresh (Updates numbers and clears alerts automatically)
    if (!isSleeping && (millis() - lastDisplayUpdate > displayInterval)) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }

    // --- Cloud: Push sensor data to InfluxDB ---
    if (millis() - lastCloudPush > CLOUD_PUSH_INTERVAL) {
        pushToCloud();
        lastCloudPush = millis();
    }

    delay(50); // Faster loop for more responsive UI
}
