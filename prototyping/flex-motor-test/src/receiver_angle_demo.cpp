/**
 * RECEIVER (angle, DEMO) — no calibration UI. Hardcoded motor-side bounds.
 *
 * Boots, listens for ESP-NOW glove angles, drives motor via SimpleFOC
 * angle_openloop. No serial commands, no manual override.
 *
 * Edit M0_MOTOR_RAD / M90_MOTOR_RAD below from the calibration build's
 * 'M0' / 'M90' captures before flashing for a demo.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - DRV8300 6-PWM driver
 *      AH=25 AL=26 BH=27 BL=14 CH=12 CL=18  (CL was 13; GPIO 13 is dead)
 *  - 2204 BLDC, 7 pole pairs
 *  - 12 V bench supply
 *  - 20:1 reduction gearbox at the joint
 */

#include <SimpleFOC.h>
#include <WiFi.h>
#include <esp_now.h>

// ===== HARDCODED JOINT-BOUND CALIBRATION — edit after running cal build =====
// Motor target_rad that puts the joint at "finger straight" / "finger fully bent".
constexpr float M0_MOTOR_RAD  = 0.0f;
constexpr float M90_MOTOR_RAD = 90.0f * 20.0f * (PI / 180.0f);   // 31.4 rad ≈ pure 20:1

BLDCDriver6PWM driver = BLDCDriver6PWM(25, 26, 27, 14, 12, 18);
BLDCMotor motor = BLDCMotor(7);

constexpr int   GLOVE_MIN_DEG = 0;
constexpr int   GLOVE_MAX_DEG = 90;
constexpr float RX_ALPHA      = 0.4f;

// Adaptive velocity_limit (same scheme as cal build).
//   30 rad/s motor   ≈ 86°/s joint  (idle floor)
//   250 rad/s motor ≈ 716°/s joint  (snap motion ceiling, near motor max RPM)
constexpr float VL_MIN_RAD_S    = 30.0f;
constexpr float VL_MAX_RAD_S    = 250.0f;
constexpr float VL_HEADROOM     = 2.0f;
constexpr uint32_t VL_PERIOD_MS = 50;
constexpr float VL_LPF_ALPHA    = 0.4f;

float updateAdaptiveVelocityLimit(float smooth_glove) {
  static uint32_t last_ms       = 0;
  static float    last_smooth   = 0.0f;
  static float    vlim_filtered = VL_MIN_RAD_S;
  uint32_t now = millis();
  if (now - last_ms >= VL_PERIOD_MS) {
    float dt_s = (now - last_ms) / 1000.0f;
    float rate_joint_deg_s = fabsf(smooth_glove - last_smooth) / dt_s;
    float motor_rate_rad_s = rate_joint_deg_s * 20.0f * (PI / 180.0f);  // GEAR_RATIO=20
    float target_vlim      = motor_rate_rad_s * VL_HEADROOM;
    if (target_vlim < VL_MIN_RAD_S) target_vlim = VL_MIN_RAD_S;
    if (target_vlim > VL_MAX_RAD_S) target_vlim = VL_MAX_RAD_S;
    vlim_filtered = VL_LPF_ALPHA * target_vlim + (1.0f - VL_LPF_ALPHA) * vlim_filtered;
    last_ms     = now;
    last_smooth = smooth_glove;
  }
  return vlim_filtered;
}

typedef struct struct_message {
  int angle;          // joint-frame degrees, 0..90
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

float gloveDegToMotorRad(float deg) {
  if (deg < GLOVE_MIN_DEG) deg = GLOVE_MIN_DEG;
  if (deg > GLOVE_MAX_DEG) deg = GLOVE_MAX_DEG;
  float t = deg / (float)GLOVE_MAX_DEG;
  return M0_MOTOR_RAD + t * (M90_MOTOR_RAD - M0_MOTOR_RAD);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  SimpleFOCDebug::enable(&Serial);

  WiFi.mode(WIFI_STA);
  delay(100);
  Serial.print("RX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);

  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0;
  driver.voltage_limit        = 12.0;
  if (!driver.init()) {
    Serial.println("Driver init FAILED");
    while (1) delay(1000);
  }
  motor.linkDriver(&driver);

  motor.controller     = MotionControlType::angle_openloop;
  motor.voltage_limit  = 6.0;
  motor.velocity_limit = 20.0;
  motor.init();

  Serial.printf("Demo RX ready. M0=%.2f rad  M90=%.2f rad\n",
                M0_MOTOR_RAD, M90_MOTOR_RAD);
}

void loop() {
  // Light EMA on incoming glove angle
  static float smooth = 0.0f;
  static bool primed = false;
  int raw = g_lastAngle;
  if (!primed) { smooth = raw; primed = true; }
  else { smooth = RX_ALPHA * raw + (1.0f - RX_ALPHA) * smooth; }

  motor.velocity_limit = updateAdaptiveVelocityLimit(smooth);
  float target_rad = gloveDegToMotorRad(smooth);
  motor.loopFOC();
  motor.move(target_rad);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    bool fresh = g_haveData && (millis() - g_lastRxMs < 500);
    Serial.printf("glove=%d  smooth=%.1f  motor_tgt=%.2f  vlim=%.1f  link=%s\n",
                  g_lastAngle, smooth, target_rad,
                  motor.velocity_limit, fresh ? "OK" : "STALE");
  }
}
