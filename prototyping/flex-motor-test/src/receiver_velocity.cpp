/**
 * RECEIVER (velocity) — glove angle -> motor velocity (rad/s), open loop.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - DRV8300 6-PWM driver, AH=25 AL=26 BH=27 BL=14 CH=12 CL=13
 *  - 2204 260KV gimbal BLDC, 7 pole pairs
 *  - 12 V bench supply (gimbal motor — keep voltage modest)
 *
 * The glove sends an int angle in degrees (0..90). We map that to a velocity
 * command in rad/s and drive the motor in velocity_openloop.
 */

#include <SimpleFOC.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>

BLDCDriver6PWM driver = BLDCDriver6PWM(25, 26, 27, 14, 12, 13);
BLDCMotor motor = BLDCMotor(7);

// Glove angle (deg) -> motor velocity (rad/s)
const int   GLOVE_MIN_DEG = 0;
const int   GLOVE_MAX_DEG = 90;
const float VEL_MAX       = 15.0f;   // rad/s at full bend (gimbal motor — modest)
const float VEL_DEADBAND  = 0.5f;    // rad/s, ignore tiny commands

// Packet must match transmitter exactly.
typedef struct struct_message {
  int angle;
} struct_message;

volatile int      g_lastAngle = 0;
volatile bool     g_haveData  = false;
volatile uint32_t g_lastRxMs  = 0;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(struct_message)) return;
  struct_message m;
  memcpy(&m, data, sizeof(m));
  g_lastAngle = m.angle;
  g_haveData  = true;
  g_lastRxMs  = millis();
}

float gloveDegToVelocity(int deg) {
  if (deg < GLOVE_MIN_DEG) deg = GLOVE_MIN_DEG;
  if (deg > GLOVE_MAX_DEG) deg = GLOVE_MAX_DEG;
  float norm = (float)(deg - GLOVE_MIN_DEG) / (float)(GLOVE_MAX_DEG - GLOVE_MIN_DEG);
  float v = norm * VEL_MAX;
  if (v < VEL_DEADBAND) v = 0.0f;
  return v;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  SimpleFOCDebug::enable(&Serial);

  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0f;
  driver.voltage_limit        = 12.0f;
  if (!driver.init()) {
    Serial.println("Driver init FAILED — halting");
    while (1) delay(1000);
  }
  motor.linkDriver(&driver);

  motor.controller    = MotionControlType::velocity_openloop;
  motor.voltage_limit = 6.0f;       // gimbal motor: lower applied voltage
  motor.init();
  Serial.println("Motor ready (velocity_openloop)");

  WiFi.mode(WIFI_STA);
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[32];
  snprintf(macStr, sizeof(macStr),
           "{0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("RX MAC: ");
  Serial.println(macStr);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed — halting");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Receiver ready");
}

void loop() {
  // Safety: stop if no packet for 500 ms.
  bool fresh = g_haveData && (millis() - g_lastRxMs) < 500;
  float target_vel = fresh ? gloveDegToVelocity(g_lastAngle) : 0.0f;

  motor.loopFOC();
  motor.move(target_vel);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    Serial.print("glove_deg="); Serial.print(g_lastAngle);
    Serial.print("  vel=");     Serial.print(target_vel, 2);
    Serial.print("  fresh=");   Serial.println(fresh ? 1 : 0);
  }
}
