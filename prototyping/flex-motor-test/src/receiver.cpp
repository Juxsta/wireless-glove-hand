/**
 * RECEIVER — Motor side
 * Receives flex value over ESP-NOW, maps it to an open-loop velocity command,
 * drives the BLDC via SimpleFOC velocity_openloop.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - DRV8300 6-PWM driver, AH=5 AL=17 BH=16 BL=4 CH=2 CL=15  (proven from motor-test)
 *  - 2804 BLDC, 7 pole pairs
 *  - 12V bench supply
 *
 * Flex value (~0..4095) is mapped to velocity in rad/s.
 * Tune FLEX_MIN / FLEX_MAX after observing live values from your sensor on serial.
 */

#include <SimpleFOC.h>
#include <WiFi.h>
#include <esp_now.h>

// ---------- Driver / Motor (matches motor-test pin map) ----------
BLDCDriver6PWM driver = BLDCDriver6PWM(
  5,  17,
  16, 4,
  2,  15
);
BLDCMotor motor = BLDCMotor(7);

// ---------- Flex -> velocity mapping ----------
// Calibrate these to your sensor's resting / fully-bent ADC values.
const int   FLEX_MIN     = 1500;   // resting (straight finger)
const int   FLEX_MAX     = 3000;   // fully bent
const float VEL_MAX      = 20.0f;  // rad/s at full bend
const float VEL_DEADBAND = 1.0f;   // rad/s, ignore tiny commands

// ---------- ESP-NOW ----------
typedef struct struct_message {
  int flexValue;
} struct_message;

volatile int   g_lastFlex      = 0;
volatile bool  g_haveData      = false;
volatile uint32_t g_lastRxMs   = 0;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(struct_message)) return;
  struct_message m;
  memcpy(&m, data, sizeof(m));
  g_lastFlex   = m.flexValue;
  g_haveData   = true;
  g_lastRxMs   = millis();
}

float flexToVelocity(int flex) {
  if (flex < FLEX_MIN) flex = FLEX_MIN;
  if (flex > FLEX_MAX) flex = FLEX_MAX;
  float norm = (float)(flex - FLEX_MIN) / (float)(FLEX_MAX - FLEX_MIN);  // 0..1
  float v = norm * VEL_MAX;
  if (v < VEL_DEADBAND) v = 0.0f;
  return v;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  SimpleFOCDebug::enable(&Serial);

  // ----- Driver -----
  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0;
  driver.voltage_limit        = 12.0;
  if (!driver.init()) {
    Serial.println("Driver init FAILED — halting");
    while (1) delay(1000);
  }
  motor.linkDriver(&driver);

  motor.controller    = MotionControlType::velocity_openloop;
  motor.voltage_limit = 10.0;            // applied voltage in open loop — overcome stiction
  motor.init();
  Serial.println("Motor ready (velocity_openloop)");

  // ----- ESP-NOW -----
  WiFi.mode(WIFI_STA);
  Serial.print("RX MAC (paste into transmitter): ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed — halting");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Receiver ready");
}

void loop() {
  // Safety: if we haven't heard from the glove in 500ms, stop the motor.
  float target = 0.0f;
  if (g_haveData && (millis() - g_lastRxMs) < 500) {
    target = flexToVelocity(g_lastFlex);
  }

  motor.move(target);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    Serial.print("flex=");
    Serial.print(g_lastFlex);
    Serial.print("  vel=");
    Serial.println(target);
  }
}
