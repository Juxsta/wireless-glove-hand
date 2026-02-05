/**
 * Wireless Glove Interface - Hand Firmware (Receiver + Motor Control)
 * 
 * Receives joint angles via BLE and controls BLDC motors with FOC.
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - 4x DRV8302 motor drivers
 * - 4x 2804 BLDC motors
 * - 4x AS5600 encoders via TCA9548A I2C mux
 * 
 * BLE Service: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
 * BLE Characteristic: beb5483e-36e1-4688-b7f5-ea07361b26a8
 */

#include <Arduino.h>
#include <SimpleFOC.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// =============================================================================
// Configuration
// =============================================================================

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// I2C Multiplexer
#define TCA9548A_ADDR 0x70
#define I2C_SDA 21
#define I2C_SCL 22

// Motor pin definitions (per architecture doc)
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
#define M2_PWM_B 21  // Note: shared with I2C SDA - may need adjustment
#define M2_PWM_C 22  // Note: shared with I2C SCL - may need adjustment
#define M2_EN    23

// Motor 3 - TIP Joint
#define M3_PWM_A 32
#define M3_PWM_B 33
#define M3_PWM_C 13
#define M3_EN    12

// For PoC: Start with single motor test
#define NUM_MOTORS 1  // Increase to 4 when ready

// =============================================================================
// Motor & Encoder Objects
// =============================================================================

// Single motor for initial testing
BLDCMotor motor0 = BLDCMotor(7);  // 7 pole pairs (typical for 2804)
BLDCDriver3PWM driver0 = BLDCDriver3PWM(M0_PWM_A, M0_PWM_B, M0_PWM_C, M0_EN);

// Magnetic angle sensor (AS5600)
MagneticSensorI2C sensor0 = MagneticSensorI2C(AS5600_I2C);

// =============================================================================
// BLE State
// =============================================================================

static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEClient* pClient;
static bool connected = false;
static bool doConnect = false;
static BLEAdvertisedDevice* targetDevice;

// Received joint angles (0-255 mapped from 0-180°)
volatile uint8_t targetAngles[4] = {0, 0, 0, 0};
volatile uint8_t lastSequence = 0;
volatile bool newData = false;

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
// BLE Callbacks
// =============================================================================

static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) 
{
    if (length >= 6) {
        // Verify checksum
        uint8_t checksum = pData[0] ^ pData[1] ^ pData[2] ^ pData[3] ^ pData[4];
        if (checksum == pData[5]) {
            lastSequence = pData[0];
            targetAngles[0] = pData[1];  // MCP
            targetAngles[1] = pData[2];  // PIP
            targetAngles[2] = pData[3];  // DIP
            targetAngles[3] = pData[4];  // TIP
            newData = true;
        }
    }
}

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        connected = true;
        Serial.println("Connected to glove");
    }

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("Disconnected from glove");
    }
};

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && 
            advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) 
        {
            Serial.println("Found glove controller!");
            BLEDevice::getScan()->stop();
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
        }
    }
};

bool connectToServer() {
    Serial.print("Connecting to ");
    Serial.println(targetDevice->getAddress().toString().c_str());
    
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    pClient->connect(targetDevice);
    
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find service");
        pClient->disconnect();
        return false;
    }
    
    pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic");
        pClient->disconnect();
        return false;
    }
    
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }
    
    return true;
}

// =============================================================================
// Motor Control
// =============================================================================

float angleFromByte(uint8_t value) {
    // Convert 0-255 to 0-PI/2 radians (0-90°)
    return (value / 255.0f) * (PI / 2.0f);
}

void initMotor() {
    // Initialize I2C for encoder
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Select encoder channel (0 for first motor)
    selectI2CChannel(0);
    
    // Initialize encoder
    sensor0.init();
    motor0.linkSensor(&sensor0);
    
    // Initialize driver
    driver0.voltage_power_supply = 7.4;  // 2S LiPo
    driver0.init();
    motor0.linkDriver(&driver0);
    
    // FOC configuration
    motor0.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor0.controller = MotionControlType::angle;
    
    // PID tuning (start conservative)
    motor0.PID_velocity.P = 0.2;
    motor0.PID_velocity.I = 10;
    motor0.PID_velocity.D = 0;
    motor0.P_angle.P = 20;
    motor0.velocity_limit = 20;  // rad/s
    
    // Initialize motor
    motor0.init();
    motor0.initFOC();
    
    Serial.println("Motor initialized");
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Wireless Glove - Hand Controller ===");
    
    // Initialize motor (single motor for PoC)
    initMotor();
    
    // Initialize BLE
    BLEDevice::init("");
    
    // Start scanning for glove
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
    
    Serial.println("Scanning for glove controller...");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // Handle BLE connection
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("Connected to glove controller");
        } else {
            Serial.println("Failed to connect, retrying...");
        }
        doConnect = false;
    }
    
    // Restart scan if disconnected
    if (!connected) {
        static unsigned long lastScan = 0;
        if (millis() - lastScan > 5000) {
            BLEDevice::getScan()->start(5, false);
            lastScan = millis();
        }
    }
    
    // Update motor control (must run frequently for FOC)
    motor0.loopFOC();
    
    // Apply new target angle if received
    if (newData) {
        float target = angleFromByte(targetAngles[0]);  // MCP for motor 0
        motor0.move(target);
        newData = false;
        
        // Debug output
        static int debugCounter = 0;
        if (++debugCounter >= 100) {
            Serial.printf("Target: %.2f rad, Current: %.2f rad\n", 
                target, sensor0.getAngle());
            debugCounter = 0;
        }
    } else {
        // Continue moving to last target
        motor0.move();
    }
}
