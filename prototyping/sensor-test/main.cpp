/**
 * Finger Joint Angle Tracker (Original Prototype)
 * 
 * Hardware: ESP32 + 3x MLX90395KLW (SPI) + ESP-NOW TX
 * Authors: Matthew Men & Antonio Rojas
 * Source:  EE198B Midterm Report — "Wireless Transmission of Sensing Data"
 *
 * Note: This is the original prototype code as written in the midterm report.
 *       The production version lives in firmware/glove/src/main.cpp
 */

#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

// -------------------------------------------------------------
// PIN DEFINITIONS
// -------------------------------------------------------------
// SPI bus (use ESP32 default VSPI pins, or change as needed)
#define PIN_SCK   18
#define PIN_MISO  19
#define PIN_MOSI  23

// Chip-select pins — one per sensor
#define CS_SENSOR_0  5   // Proximal joint  (knuckle)
#define CS_SENSOR_1  17  // Middle joint
#define CS_SENSOR_2  16  // Distal joint    (fingertip)

// -------------------------------------------------------------
// ESP-NOW CONFIG
// Replace with the actual MAC address of the receiver ESP32
// -------------------------------------------------------------
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// -------------------------------------------------------------
// MLX90395 SPI SETTINGS
// Max SPI clock for MLX90395 is 10 MHz
// -------------------------------------------------------------
#define SPI_FREQ     1000000  // 1 MHz (conservative; raise to 10 MHz once working)
#define SPI_MODE     SPI_MODE3
SPISettings mlxSettings(SPI_FREQ, MSBFIRST, SPI_MODE);

// -------------------------------------------------------------
// MLX90395 COMMAND BYTES
// -------------------------------------------------------------
#define CMD_START_BURST   0x1E  // Start burst mode: X, Y, Z, T
#define CMD_READ_MEAS     0x4E  // Read measurement (X,Y,Z,T)
#define CMD_EXIT          0x80  // Exit mode / reset communication
#define CMD_NOP           0x00

// Axis selection bits for the READ command
#define AXIS_XYZ  0x0E  // X=bit1, Y=bit2, Z=bit3

// Response status mask
#define STATUS_DRDY  0x01  // Data Ready bit in status byte

// -------------------------------------------------------------
// DATA STRUCTURES
// -------------------------------------------------------------
struct RawMag {
  int16_t x, y, z;
  uint16_t t;       // temperature (optional)
  bool valid;
};

// Packet sent via ESP-NOW to the robot ESP32
struct AnglePacket {
  float angle[3];   // degrees, one per joint
  uint32_t timestamp_ms;
};

// -------------------------------------------------------------
// GLOBALS
// -------------------------------------------------------------
const uint8_t CS_PINS[3] = {CS_SENSOR_0, CS_SENSOR_1, CS_SENSOR_2};
AnglePacket packet;
esp_now_peer_info_t peerInfo;

// Baseline (zero-position) magnetic readings captured at startup
float baseline_angle[3] = {0.0f, 0.0f, 0.0f};

// -------------------------------------------------------------
// FORWARD DECLARATIONS
// -------------------------------------------------------------
bool     mlx_sendCommand(uint8_t csPin, uint8_t cmd);
RawMag   mlx_readMeasurement(uint8_t csPin);
bool     mlx_startBurst(uint8_t csPin);
float    calculateAngle(RawMag &raw);
void     calibrateBaselines();
void     onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

// =============================================================
// SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Finger Tracker Booting ===");

  // --- SPI ---
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);  // SCLK, MISO, MOSI
  for (int i = 0; i < 3; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);        // deselect all
  }
  delay(10);

  // --- Init each MLX90395 ---
  for (int i = 0; i < 3; i++) {
    Serial.printf("Initialising sensor %d ... ", i);
    if (mlx_startBurst(CS_PINS[i])) {
      Serial.println("OK");
    } else {
      Serial.println("FAILED – check wiring/CS pin");
    }
    delay(5);
  }

  // --- ESP-NOW ---
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onDataSent);

  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer!");
  }

  // --- Calibrate baselines (hold finger straight during boot) ---
  Serial.println("Hold finger STRAIGHT for calibration...");
  delay(2000);
  calibrateBaselines();
  Serial.println("Calibration done. Starting loop.");
}

// =============================================================
// MAIN LOOP
// =============================================================
void loop() {
  for (int i = 0; i < 3; i++) {
    RawMag raw = mlx_readMeasurement(CS_PINS[i]);

    if (raw.valid) {
      float angle = calculateAngle(raw) - baseline_angle[i];
      packet.angle[i] = angle;

      Serial.printf("Joint %d | X:%6d Y:%6d Z:%6d | Angle: %6.1f deg\n",
                    i, raw.x, raw.y, raw.z, angle);
    } else {
      Serial.printf("Joint %d | NO DATA\n", i);
      packet.angle[i] = -999.0f;  // sentinel for invalid
    }
  }

  packet.timestamp_ms = millis();

  // Send over ESP-NOW
  esp_err_t result = esp_now_send(receiverMAC,
                                  (uint8_t *)&packet,
                                  sizeof(packet));
  if (result != ESP_OK) {
    Serial.println("ESP-NOW send error");
  }

  delay(20);  // ~50 Hz update rate — adjust as needed
}

// =============================================================
// MLX90395 DRIVER FUNCTIONS
// =============================================================

// Send a single command byte, return true if status looks valid
bool mlx_sendCommand(uint8_t csPin, uint8_t cmd) {
  SPI.beginTransaction(mlxSettings);
  digitalWrite(csPin, LOW);
  delayMicroseconds(1);
  uint8_t status = SPI.transfer(cmd);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
  return (status != 0xFF);
}

// Start continuous burst mode (X, Y, Z)
bool mlx_startBurst(uint8_t csPin) {
  mlx_sendCommand(csPin, CMD_EXIT);
  delay(2);
  return mlx_sendCommand(csPin, CMD_START_BURST | AXIS_XYZ);
}

// Read a completed measurement from the sensor
RawMag mlx_readMeasurement(uint8_t csPin) {
  RawMag result = {0, 0, 0, 0, false};

  SPI.beginTransaction(mlxSettings);
  digitalWrite(csPin, LOW);
  delayMicroseconds(1);

  uint8_t status = SPI.transfer(CMD_READ_MEAS | AXIS_XYZ);

  if (!(status & STATUS_DRDY)) {
    digitalWrite(csPin, HIGH);
    SPI.endTransaction();
    return result;
  }

  uint8_t xh = SPI.transfer(CMD_NOP);
  uint8_t xl = SPI.transfer(CMD_NOP);
  uint8_t yh = SPI.transfer(CMD_NOP);
  uint8_t yl = SPI.transfer(CMD_NOP);
  uint8_t zh = SPI.transfer(CMD_NOP);
  uint8_t zl = SPI.transfer(CMD_NOP);

  digitalWrite(csPin, HIGH);
  SPI.endTransaction();

  result.x     = (int16_t)((xh << 8) | xl);
  result.y     = (int16_t)((yh << 8) | yl);
  result.z     = (int16_t)((zh << 8) | zl);
  result.valid = true;
  return result;
}

// =============================================================
// ANGLE CALCULATION
// =============================================================
float calculateAngle(RawMag &raw) {
  float radians = atan2f((float)raw.y, (float)raw.x);
  float degrees = radians * (180.0f / M_PI);
  return degrees;
}

// =============================================================
// CALIBRATION
// =============================================================
void calibrateBaselines() {
  const int samples = 20;
  float sum[3] = {0, 0, 0};

  for (int s = 0; s < samples; s++) {
    for (int i = 0; i < 3; i++) {
      RawMag raw = mlx_readMeasurement(CS_PINS[i]);
      if (raw.valid) {
        sum[i] += calculateAngle(raw);
      }
    }
    delay(10);
  }

  for (int i = 0; i < 3; i++) {
    baseline_angle[i] = sum[i] / samples;
    Serial.printf("Baseline joint %d: %.2f deg\n", i, baseline_angle[i]);
  }
}

// =============================================================
// ESP-NOW CALLBACK
// =============================================================
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Uncomment for verbose TX confirmation:
  // Serial.printf("TX to %02X:%02X:%02X:%02X:%02X:%02X  %s\n",
  //   mac_addr[0], mac_addr[1], mac_addr[2],
  //   mac_addr[3], mac_addr[4], mac_addr[5],
  //   status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
