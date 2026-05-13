#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// ESP-NOW Broadcast structure for sensor data exchange
struct SensorData {
    // Node 2 Data (Main Sensor Stack)
    float internalTemp;     // Celsius
    float internalHum;      // %
    float internalVOC;      // IAQ Index (0-500)
    float internalStaticP;  // Pascal
    float particulate;     // PM2.5 ug/m3
    float particulate10;   // PM10 ug/m3
    float plenumVOC;        // IAQ Index (0-500)
    float plenumStaticP;    // Pascal
    
    // Node 3 Data (Filter Monitor)
    float filterStaticP;    // Pascal (Pressure after G4 filters)

    // Node 4 Data (External Monitor)
    float externalTemp;     // Celsius
    float externalHum;      // %
    float externalVOC;      // IAQ Index (0-500)
    float externalParticulate; // PM2.5 ug/m3
    float externalParticulate10; // PM10 ug/m3
    float externalStaticP;  // Pascal (Atmospheric Room Ground Truth)

    // Control Status
    uint8_t fanSpeed;       // 0-100%
    bool filterAlert;       // G4 Filter replacement required
    bool carbonAlert;       // Carbon saturation detected
    
    // Origin Node ID string or Type to filter received data
    uint8_t senderNode;     // 2: Node2, 3: Node3, 4: Node4
    
    uint32_t timestamp;     // Uptime in ms
};

// ESP-NOW Command structure for Hub -> Node 2 overrides
struct ControlCommand {
    uint8_t commandType;    // 0: Auto Mode, 1: Manual Mode Override
    uint8_t manualSpeed;    // 0-100% target
    uint8_t senderNode;     // Expected: 1 (Hub)
};

#endif // PROTOCOL_H
