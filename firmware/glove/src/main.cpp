/**
 * Wireless Glove Interface - Glove Firmware (Transmitter)
 * 
 * Reads MLX90395 Hall-effect magnetic sensors via SPI and transmits
 * joint angles via ESP-NOW to robotic hand.
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - 3x MLX90395 sensors (MCP, PIP, DIP joints)
 * - SPI bus: SCK=18, MISO=19, MOSI=23
 * - Chip selects: CS0=5, CS1=17, CS2=16
 * 
 * Based on Antonio's implementation (EE198B Midterm Report)
 * Cleaned up by Eric for production firmware
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include "mlx90395.h"

// =============================================================================
// Configuration
// =============================================================================

// Receiver MAC address (hand controller ESP32)
// TODO: Replace with actual MAC address of hand controller
// Run WiFi.macAddress() on hand controller to get its MAC
#define RECEIVER_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// MLX90395 chip select pins
#define CS_MCP  5   // Metacarpophalangeal (knuckle)
#define CS_PIP  17  // Proximal interphalangeal
#define CS_DIP  16  // Distal interphalangeal

// Transmission rate
#define TX_INTERVAL_MS 20  // ~50 Hz

// Calibration button (optional)
#define CALIB_BUTTON_PIN 0  // BOOT button on DevKit

// =============================================================================
// Global Objects
// =============================================================================

MLX90395 sensorMCP(CS_MCP);
MLX90395 sensorPIP(CS_PIP);
MLX90395 sensorDIP(CS_DIP);

Preferences prefs;

// Peer info for ESP-NOW
uint8_t receiverAddress[] = RECEIVER_MAC;
esp_now_peer_info_t peerInfo;

// Packet state
uint8_t sequenceNumber = 0;
bool espnowReady = false;

// =============================================================================
// ESP-NOW Packet Structure (10 bytes)
// =============================================================================

struct __attribute__((packed)) GlovePacket {
    uint8_t sequence;      // Sequence number (0-255, wraps)
    int16_t mcp_angle;     // MCP angle in degrees * 10 (e.g., 450 = 45.0°)
    int16_t pip_angle;     // PIP angle in degrees * 10
    int16_t dip_angle;     // DIP angle in degrees * 10
    uint8_t reserved;      // Future: TIP angle or status flags
    uint8_t checksum;      // XOR of all previous bytes
};

// =============================================================================
// ESP-NOW Callbacks
// =============================================================================

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Optional: Track send failures for debugging
    if (status != ESP_NOW_SEND_SUCCESS) {
        static int failCount = 0;
        if (++failCount % 100 == 0) {
            Serial.printf("ESP-NOW: %d send failures\n", failCount);
        }
    }
}

// =============================================================================
// Sensor Functions
// =============================================================================

void initSensors() {
    Serial.println("Initializing MLX90395 sensors...");
    
    sensorMCP.init();
    sensorPIP.init();
    sensorDIP.init();
    
    delay(50);
    
    // Start burst mode on all sensors
    sensorMCP.startBurst();
    sensorPIP.startBurst();
    sensorDIP.startBurst();
    
    Serial.println("Sensors initialized and in burst mode");
}

void loadCalibration() {
    prefs.begin("glove-cal", true);  // Read-only
    
    // Load MCP calibration
    if (prefs.isKey("mcp_x")) {
        int16_t x = prefs.getShort("mcp_x", 0);
        int16_t y = prefs.getShort("mcp_y", 0);
        int16_t z = prefs.getShort("mcp_z", 0);
        sensorMCP.setCalibration(x, y, z);
        Serial.printf("Loaded MCP calibration: X=%d, Y=%d, Z=%d\n", x, y, z);
    }
    
    // Load PIP calibration
    if (prefs.isKey("pip_x")) {
        int16_t x = prefs.getShort("pip_x", 0);
        int16_t y = prefs.getShort("pip_y", 0);
        int16_t z = prefs.getShort("pip_z", 0);
        sensorPIP.setCalibration(x, y, z);
        Serial.printf("Loaded PIP calibration: X=%d, Y=%d, Z=%d\n", x, y, z);
    }
    
    // Load DIP calibration
    if (prefs.isKey("dip_x")) {
        int16_t x = prefs.getShort("dip_x", 0);
        int16_t y = prefs.getShort("dip_y", 0);
        int16_t z = prefs.getShort("dip_z", 0);
        sensorDIP.setCalibration(x, y, z);
        Serial.printf("Loaded DIP calibration: X=%d, Y=%d, Z=%d\n", x, y, z);
    }
    
    prefs.end();
}

void saveCalibration() {
    prefs.begin("glove-cal", false);  // Read-write
    
    // Save MCP
    MagCalibration cal = sensorMCP.getCalibration();
    prefs.putShort("mcp_x", cal.x_offset);
    prefs.putShort("mcp_y", cal.y_offset);
    prefs.putShort("mcp_z", cal.z_offset);
    
    // Save PIP
    cal = sensorPIP.getCalibration();
    prefs.putShort("pip_x", cal.x_offset);
    prefs.putShort("pip_y", cal.y_offset);
    prefs.putShort("pip_z", cal.z_offset);
    
    // Save DIP
    cal = sensorDIP.getCalibration();
    prefs.putShort("dip_x", cal.x_offset);
    prefs.putShort("dip_y", cal.y_offset);
    prefs.putShort("dip_z", cal.z_offset);
    
    prefs.end();
    Serial.println("Calibration saved to flash");
}

void runCalibration() {
    Serial.println("\n=== CALIBRATION MODE ===");
    Serial.println("Hold hand FLAT and press ENTER...");
    while (!Serial.available()) delay(100);
    while (Serial.available()) Serial.read();  // Clear buffer
    
    // Calibrate all sensors (20 samples each)
    sensorMCP.calibrate(20);
    sensorPIP.calibrate(20);
    sensorDIP.calibrate(20);
    
    // Save to flash
    saveCalibration();
    
    Serial.println("=== CALIBRATION COMPLETE ===\n");
}

// =============================================================================
// ESP-NOW Functions
// =============================================================================

void initESPNow() {
    // Set device as a WiFi Station
    WiFi.mode(WIFI_STA);
    
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    // Register send callback
    esp_now_register_send_cb(onDataSent);
    
    // Register peer (hand controller)
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;  // Use current WiFi channel
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
    
    espnowReady = true;
    Serial.println("ESP-NOW initialized");
}

void sendPacket(float mcp_deg, float pip_deg, float dip_deg) {
    if (!espnowReady) return;
    
    GlovePacket packet;
    packet.sequence = sequenceNumber++;
    packet.mcp_angle = (int16_t)(mcp_deg * 10.0f);  // Store as degrees * 10
    packet.pip_angle = (int16_t)(pip_deg * 10.0f);
    packet.dip_angle = (int16_t)(dip_deg * 10.0f);
    packet.reserved = 0;
    
    // Calculate checksum (XOR of all bytes except checksum itself)
    uint8_t* data = (uint8_t*)&packet;
    uint8_t checksum = 0;
    for (int i = 0; i < sizeof(GlovePacket) - 1; i++) {
        checksum ^= data[i];
    }
    packet.checksum = checksum;
    
    // Send via ESP-NOW
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&packet, sizeof(packet));
    
    if (result != ESP_OK) {
        Serial.println("Error sending packet");
    }
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);  // Allow serial to initialize
    
    Serial.println("\n=== Wireless Glove - Transmitter ===");
    Serial.println("Based on MLX90395 Hall-effect sensors + ESP-NOW");
    Serial.println("EE198B Senior Project - SJSU 2026\n");
    
    // Initialize calibration button
    pinMode(CALIB_BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize sensors
    initSensors();
    
    // Load calibration from flash (if available)
    loadCalibration();
    
    // Initialize ESP-NOW
    initESPNow();
    
    Serial.println("\n=== Ready to transmit ===");
    Serial.println("Press 'c' in serial monitor for calibration mode");
    Serial.println("Press BOOT button for quick calibration\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    static unsigned long lastTx = 0;
    static int debugCounter = 0;
    
    // Check for calibration trigger (serial or button)
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c' || c == 'C') {
            runCalibration();
        }
    }
    
    if (digitalRead(CALIB_BUTTON_PIN) == LOW) {
        delay(50);  // Debounce
        if (digitalRead(CALIB_BUTTON_PIN) == LOW) {
            runCalibration();
            while (digitalRead(CALIB_BUTTON_PIN) == LOW) delay(10);  // Wait for release
        }
    }
    
    // Read sensors and transmit at fixed rate
    if (millis() - lastTx >= TX_INTERVAL_MS) {
        // Read all sensors
        RawMag magMCP = sensorMCP.readMeasurement();
        RawMag magPIP = sensorPIP.readMeasurement();
        RawMag magDIP = sensorDIP.readMeasurement();
        
        if (magMCP.valid && magPIP.valid && magDIP.valid) {
            // Calculate angles (with calibration applied)
            float angleMCP = sensorMCP.calculateAngle(magMCP);
            float anglePIP = sensorPIP.calculateAngle(magPIP);
            float angleDIP = sensorDIP.calculateAngle(magDIP);
            
            // Send packet
            sendPacket(angleMCP, anglePIP, angleDIP);
            
            // Debug output (every ~1 second)
            if (++debugCounter >= 50) {
                Serial.printf("TX [%3d]: MCP=%.1f° PIP=%.1f° DIP=%.1f°\n",
                    sequenceNumber - 1, angleMCP, anglePIP, angleDIP);
                debugCounter = 0;
            }
        } else {
            Serial.println("Warning: Invalid sensor reading");
        }
        
        lastTx = millis();
    }
}
