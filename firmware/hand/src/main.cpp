/**
 * Wireless Glove Interface - Hand Firmware (Receiver + Motor Control)
 * 
 * Receives joint angles via ESP-NOW and controls 2204 BLDC motors with FOC.
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - 4x DRV8300 custom motor driver PCBs (designed by Raul)
 * - 4x 2204 BLDC motors (sourced by Matthew)
 * - 4x AS5600 encoders via TCA9548A I2C mux
 * 
 * Protocol: ESP-NOW (from glove controller)
 * 
 * Motor driver PCB designed by Raul Hernandez-Solis (DRV8300 + ACS37042)
 * Motors sourced and tested by Matthew Men
 * FOC integration and firmware by Eric Reyes
 */

#include <Arduino.h>
#include <SimpleFOC.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

// =============================================================================
// Configuration
// =============================================================================

// TODO: Verify pole pair count for 2204 motors from Matthew/datasheet
// Common values: 7, 11, 12, 14
#define MOTOR_POLE_PAIRS 7  // *** NEEDS VERIFICATION ***

// I2C Multiplexer (for 4x AS5600 encoders)
#define TCA9548A_ADDR 0x70
#define I2C_SDA 21
#define I2C_SCL 22

// Motor pin definitions
// NOTE: These are placeholders for DRV8302 - WILL CHANGE for DRV8300 PCB
// TODO: Update pin mappings once Raul provides DRV8300 PCB schematic

// Motor 0 - MCP Joint
#define M0_PWM_A 25
#define M0_PWM_B 26
#define M0_PWM_C 27
#define M0_EN    14

// Motor 1 - PIP Joint
#define M1_PWM_A 16
#define M1_PWM_B 17
#define M1_PWM_C 18
#define M1_EN    15

// Motor 2 - DIP Joint
#define M2_PWM_A 19
#define M2_PWM_B 21
#define M2_PWM_C 22
#define M2_EN    23

// Motor 3 - TIP Joint
#define M3_PWM_A 32
#define M3_PWM_B 33
#define M3_PWM_C 13
#define M3_EN    12

// TODO: Add ACS37042 current sense pins (on DRV8300 PCB, future feature)
// Will enable torque control mode in SimpleFOC

// Number of motors for PoC (start with 1, scale to 4)
#define NUM_MOTORS 1  // Increase when ready for full finger

// FOC control frequency
#define FOC_LOOP_HZ 1000  // Target 1kHz

// =============================================================================
// ESP-NOW Packet Structure (must match glove firmware)
// =============================================================================

struct __attribute__((packed)) GlovePacket {
    uint8_t sequence;      // Sequence number (0-255, wraps)
    int16_t mcp_angle;     // MCP angle in degrees * 10
    int16_t pip_angle;     // PIP angle in degrees * 10
    int16_t dip_angle;     // DIP angle in degrees * 10
    uint8_t reserved;      // Reserved
    uint8_t checksum;      // XOR checksum
};

// Received data (volatile - accessed in ISR)
volatile GlovePacket latestPacket = {0};
volatile bool newPacketAvailable = false;
volatile uint8_t lastSequence = 255;
volatile uint32_t packetsReceived = 0;
volatile uint32_t packetsLost = 0;

// =============================================================================
// Motor & Encoder Objects
// =============================================================================

// Motor 0 (MCP joint) - single motor for PoC
BLDCMotor motor0 = BLDCMotor(MOTOR_POLE_PAIRS);
BLDCDriver3PWM driver0 = BLDCDriver3PWM(M0_PWM_A, M0_PWM_B, M0_PWM_C, M0_EN);
MagneticSensorI2C sensor0 = MagneticSensorI2C(AS5600_I2C);

// TODO: Uncomment when scaling to 4 motors
// BLDCMotor motor1 = BLDCMotor(MOTOR_POLE_PAIRS);
// BLDCDriver3PWM driver1 = BLDCDriver3PWM(M1_PWM_A, M1_PWM_B, M1_PWM_C, M1_EN);
// MagneticSensorI2C sensor1 = MagneticSensorI2C(AS5600_I2C);

// ... motor2, motor3 ...

// =============================================================================
// I2C Multiplexer
// =============================================================================

void selectI2CChannel(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(TCA9548A_ADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

// =============================================================================
// ESP-NOW Callbacks
// =============================================================================

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (len != sizeof(GlovePacket)) {
        Serial.printf("ESP-NOW: Wrong packet size (%d bytes)\n", len);
        return;
    }
    
    GlovePacket packet;
    memcpy(&packet, data, sizeof(GlovePacket));
    
    // Verify checksum
    uint8_t checksum = 0;
    uint8_t* pdata = (uint8_t*)&packet;
    for (int i = 0; i < sizeof(GlovePacket) - 1; i++) {
        checksum ^= pdata[i];
    }
    
    if (checksum != packet.checksum) {
        Serial.println("ESP-NOW: Checksum error");
        return;
    }
    
    // Track packet loss
    if (lastSequence != 255) {
        uint8_t expected = (lastSequence + 1) % 256;
        if (packet.sequence != expected) {
            int lost = (packet.sequence - expected + 256) % 256;
            packetsLost += lost;
            Serial.printf("ESP-NOW: Lost %d packets (seq %d -> %d)\n", 
                lost, lastSequence, packet.sequence);
        }
    }
    
    lastSequence = packet.sequence;
    packetsReceived++;
    
    // Store packet for main loop
    latestPacket = packet;
    newPacketAvailable = true;
}

// =============================================================================
// Motor Control
// =============================================================================

void initMotor() {
    Serial.println("Initializing motor system...");
    
    // Initialize I2C for encoders
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);  // 400kHz I2C
    
    // Select encoder channel 0 (MCP joint)
    selectI2CChannel(0);
    delay(10);
    
    // Initialize encoder
    sensor0.init();
    motor0.linkSensor(&sensor0);
    Serial.println("Encoder 0 initialized");
    
    // Initialize driver
    driver0.voltage_power_supply = 7.4;  // 2S LiPo
    driver0.init();
    motor0.linkDriver(&driver0);
    Serial.println("Driver 0 initialized");
    
    // FOC configuration
    motor0.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor0.controller = MotionControlType::angle;
    
    // PID tuning (start conservative, tune later)
    motor0.PID_velocity.P = 0.2;
    motor0.PID_velocity.I = 10;
    motor0.PID_velocity.D = 0;
    motor0.PID_velocity.output_ramp = 1000;
    
    motor0.P_angle.P = 20;
    motor0.velocity_limit = 20;  // rad/s
    motor0.voltage_limit = 6;    // V (safe for tuning)
    
    // Initialize motor
    Serial.println("Initializing motor FOC...");
    motor0.init();
    motor0.initFOC();
    
    Serial.println("Motor 0 ready!");
    
    // TODO: Initialize motors 1-3 when scaling up
}

float degreesToRadians(int16_t degrees_times_10) {
    // Convert degrees*10 to radians
    // Example: 450 (45.0°) -> 0.785 rad
    float degrees = degrees_times_10 / 10.0f;
    return degrees * PI / 180.0f;
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Wireless Glove - Hand Controller ===");
    Serial.println("Based on 2204 BLDC motors + DRV8300 PCB + SimpleFOC");
    Serial.println("EE198B Senior Project - SJSU 2026\n");
    
    Serial.printf("Motor pole pairs: %d (VERIFY THIS!)\n", MOTOR_POLE_PAIRS);
    Serial.println("DRV8300 PCB pin mappings: PLACEHOLDER (update from Raul)\n");
    
    // Initialize motor system
    initMotor();
    
    // Initialize ESP-NOW
    WiFi.mode(WIFI_STA);
    Serial.print("Hand MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.println("Configure this MAC in glove firmware (RECEIVER_MAC)\n");
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_recv_cb(onDataRecv);
    
    Serial.println("=== Ready to receive ===");
    Serial.println("Waiting for glove controller packets...\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    static unsigned long lastDebug = 0;
    static unsigned long lastFOC = 0;
    
    // High-frequency FOC loop (aim for 1kHz)
    unsigned long now = micros();
    if (now - lastFOC >= 1000) {  // 1ms = 1kHz
        motor0.loopFOC();
        lastFOC = now;
    }
    
    // Apply new target angle when received
    if (newPacketAvailable) {
        noInterrupts();
        GlovePacket packet = latestPacket;
        newPacketAvailable = false;
        interrupts();
        
        // Convert MCP angle to radians
        float targetAngle = degreesToRadians(packet.mcp_angle);
        
        // Clamp to safe range (0-90° = 0-PI/2 rad)
        targetAngle = constrain(targetAngle, 0, PI / 2);
        
        // Update motor target
        motor0.move(targetAngle);
        
        // Debug output every ~1 second
        if (millis() - lastDebug >= 1000) {
            float currentAngle = sensor0.getAngle();
            float voltage = motor0.voltage.q;
            
            Serial.printf("[%3d] Target: %.2f rad (%.1f°) | Current: %.2f rad (%.1f°) | V: %.2fV | Pkts: %lu (lost: %lu)\n",
                packet.sequence,
                targetAngle, targetAngle * 180 / PI,
                currentAngle, currentAngle * 180 / PI,
                voltage,
                packetsReceived, packetsLost);
            
            lastDebug = millis();
        }
    } else {
        // Continue moving to last target
        motor0.move();
    }
    
    // Print status if no packets received in 5 seconds
    static unsigned long lastPacketTime = 0;
    if (newPacketAvailable) {
        lastPacketTime = millis();
    } else if (millis() - lastPacketTime > 5000 && packetsReceived == 0) {
        static unsigned long lastWarning = 0;
        if (millis() - lastWarning > 5000) {
            Serial.println("WARNING: No packets received. Check:");
            Serial.println("  1. Glove controller is powered on");
            Serial.println("  2. RECEIVER_MAC in glove firmware matches this MAC");
            Serial.println("  3. Both devices on same WiFi channel");
            lastWarning = millis();
        }
    }
}
