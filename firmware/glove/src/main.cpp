/**
 * Wireless Glove Interface - Glove Firmware (Transmitter)
 * 
 * Reads flex sensors and transmits joint angles via BLE.
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - 2x Flex sensors on GPIO 34, 35
 * 
 * BLE Service: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
 * BLE Characteristic: beb5483e-36e1-4688-b7f5-ea07361b26a8
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =============================================================================
// Configuration
// =============================================================================

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Flex sensor pins (ADC1 channels - safe with BLE)
#define FLEX_MCP_PIN 34
#define FLEX_PIP_PIN 35

// Transmission rate
#define TX_INTERVAL_MS 33  // ~30 Hz

// =============================================================================
// Calibration Data (stored in flash)
// =============================================================================

struct CalibrationData {
    uint16_t mcp_min = 1000;
    uint16_t mcp_max = 3000;
    uint16_t pip_min = 1000;
    uint16_t pip_max = 3000;
};

CalibrationData cal;

// =============================================================================
// BLE State
// =============================================================================

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t sequenceNumber = 0;

// =============================================================================
// BLE Callbacks
// =============================================================================

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
    }
};

// =============================================================================
// Sensor Functions
// =============================================================================

uint8_t readFlexAngle(int pin, uint16_t minVal, uint16_t maxVal) {
    uint16_t raw = analogRead(pin);
    
    // Constrain to calibration range
    raw = constrain(raw, minVal, maxVal);
    
    // Map to 0-180 degrees, then to 0-255 for transmission
    float angle = map(raw, minVal, maxVal, 0, 180);
    return (uint8_t)((angle / 180.0f) * 255.0f);
}

void calibrateSensors() {
    Serial.println("=== CALIBRATION MODE ===");
    Serial.println("Hold hand FLAT and press any key...");
    while (!Serial.available()) delay(100);
    Serial.read();
    
    cal.mcp_min = analogRead(FLEX_MCP_PIN);
    cal.pip_min = analogRead(FLEX_PIP_PIN);
    Serial.printf("Flat: MCP=%d, PIP=%d\n", cal.mcp_min, cal.pip_min);
    
    Serial.println("Make a FIST and press any key...");
    while (!Serial.available()) delay(100);
    Serial.read();
    
    cal.mcp_max = analogRead(FLEX_MCP_PIN);
    cal.pip_max = analogRead(FLEX_PIP_PIN);
    Serial.printf("Fist: MCP=%d, PIP=%d\n", cal.mcp_max, cal.pip_max);
    
    Serial.println("Calibration complete!");
    // TODO: Save to EEPROM/Preferences
}

// =============================================================================
// Packet Building
// =============================================================================

void buildPacket(uint8_t* packet) {
    uint8_t mcp = readFlexAngle(FLEX_MCP_PIN, cal.mcp_min, cal.mcp_max);
    uint8_t pip = readFlexAngle(FLEX_PIP_PIN, cal.pip_min, cal.pip_max);
    
    // Derive DIP and TIP from PIP (coupled motion for PoC)
    uint8_t dip = pip;  // Same as PIP
    uint8_t tip = pip;  // Same as PIP
    
    packet[0] = sequenceNumber++;
    packet[1] = mcp;
    packet[2] = pip;
    packet[3] = dip;
    packet[4] = tip;
    packet[5] = packet[0] ^ packet[1] ^ packet[2] ^ packet[3] ^ packet[4];  // Checksum
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Wireless Glove - Transmitter ===");
    
    // Initialize ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // Full 0-3.3V range
    
    // Initialize BLE
    BLEDevice::init("GloveController");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    
    pService->start();
    
    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // For iPhone compatibility
    BLEDevice::startAdvertising();
    
    Serial.println("BLE advertising started. Waiting for connection...");
    Serial.println("Press 'c' for calibration mode");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    static unsigned long lastTx = 0;
    
    // Check for calibration command
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c' || c == 'C') {
            calibrateSensors();
        }
    }
    
    // Transmit at fixed rate
    if (deviceConnected && (millis() - lastTx >= TX_INTERVAL_MS)) {
        uint8_t packet[6];
        buildPacket(packet);
        
        pCharacteristic->setValue(packet, 6);
        pCharacteristic->notify();
        
        // Debug output (reduce rate for production)
        static int debugCounter = 0;
        if (++debugCounter >= 30) {  // Every ~1 second
            Serial.printf("TX: seq=%d MCP=%d PIP=%d\n", 
                packet[0], packet[1], packet[2]);
            debugCounter = 0;
        }
        
        lastTx = millis();
    }
    
    // Handle reconnection
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // Give BLE stack time to reset
        pServer->startAdvertising();
        Serial.println("Restarted advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}
