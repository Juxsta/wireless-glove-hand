/**
 * RECEIVER (angle, OPEN-LOOP) — glove angle → motor angle via SimpleFOC angle_openloop.
 *
 * No encoder, no current sensing. Glove sends a joint-frame angle in degrees,
 * we multiply by the 20:1 gearbox ratio and command the motor.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - DRV8300 6-PWM driver
 *      AH=25 AL=26 BH=27 BL=14 CH=12 CL=18   (CL was 13 originally; GPIO 13
 *      was non-functional on this board — moved to 18)
 *  - 2204 BLDC, 7 pole pairs
 *  - 12 V bench supply
 *  - 20:1 reduction gearbox at the joint
 */

#include <SimpleFOC.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>

BLDCDriver6PWM driver = BLDCDriver6PWM(25, 26, 27, 14, 12, 18);
BLDCMotor motor = BLDCMotor(7);

const float GEAR_RATIO    = 20.0f;
const int   GLOVE_MIN_DEG = 0;
const int   GLOVE_MAX_DEG = 90;

// Joint-side calibration. M0_motor_rad = motor target_rad that puts the
// joint in its "finger straight" pose; M90 the same for "finger fully bent".
// Default = pure 20:1 gear scaling — overwrite via 'M0' / 'M90' commands once
// the mechanical assembly is attached.
float M0_motor_rad  = 0.0f;
float M90_motor_rad = 90.0f * GEAR_RATIO * (PI / 180.0f);

// Manual motor override (for calibration). When manualOverride is true,
// we ignore the glove and drive to manualTargetRad. Set via 'T<deg>'.
bool  manualOverride = false;
float manualTargetRad = 0.0f;

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

// Map glove degrees to the calibrated motor target_rad via linear interp
// between M0 and M90 captures.
float gloveDegToMotorRad(int deg) {
  if (deg < GLOVE_MIN_DEG) deg = GLOVE_MIN_DEG;
  if (deg > GLOVE_MAX_DEG) deg = GLOVE_MAX_DEG;
  float t = (float)deg / (float)GLOVE_MAX_DEG;            // 0..1
  return M0_motor_rad + t * (M90_motor_rad - M0_motor_rad);
}

// EMA smoothing on the received glove angle. Alpha = 0.4 — light smoothing
// since the transmitter already filters noise.
constexpr float RX_ALPHA = 0.4f;
float smooth_glove_deg = 0.0f;
bool  rx_primed = false;

float smoothedGloveDeg() {
  int raw = g_lastAngle;
  if (!rx_primed) { smooth_glove_deg = raw; rx_primed = true; }
  else { smooth_glove_deg = RX_ALPHA * raw + (1.0f - RX_ALPHA) * smooth_glove_deg; }
  return smooth_glove_deg;
}

// ===== Adaptive velocity_limit =====
// Slew rate scales with how fast the glove is changing.
// Motor frame numbers; divide by GEAR_RATIO=20 for joint-frame speed.
//   30 rad/s motor   = 1.5 rad/s joint  =  86°/s   (idle floor)
//   250 rad/s motor = 12.5 rad/s joint  = 716°/s   (near 12V/KV=220 mechanical max)
constexpr float VL_MIN_RAD_S    = 30.0f;
constexpr float VL_MAX_RAD_S    = 250.0f;
constexpr float VL_HEADROOM     = 2.0f;       // more aggressive catch-up
constexpr uint32_t VL_PERIOD_MS = 50;
constexpr float VL_LPF_ALPHA    = 0.4f;

float updateAdaptiveVelocityLimit(float smooth_glove) {
  static uint32_t last_ms       = 0;
  static float    last_smooth   = 0.0f;
  static float    vlim_filtered = VL_MIN_RAD_S;

  uint32_t now = millis();
  if (now - last_ms >= VL_PERIOD_MS) {
    float dt_s = (now - last_ms) / 1000.0f;
    float rate_joint_deg_s   = fabsf(smooth_glove - last_smooth) / dt_s;       // joint frame
    float motor_rate_rad_s   = rate_joint_deg_s * GEAR_RATIO * (PI / 180.0f);  // motor frame
    float target_vlim        = motor_rate_rad_s * VL_HEADROOM;
    if (target_vlim < VL_MIN_RAD_S) target_vlim = VL_MIN_RAD_S;
    if (target_vlim > VL_MAX_RAD_S) target_vlim = VL_MAX_RAD_S;
    vlim_filtered = VL_LPF_ALPHA * target_vlim + (1.0f - VL_LPF_ALPHA) * vlim_filtered;
    last_ms     = now;
    last_smooth = smooth_glove;
  }
  return vlim_filtered;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  SimpleFOCDebug::enable(&Serial);

  // ----- ESP-NOW -----
  WiFi.mode(WIFI_STA);
  delay(100);   // give WiFi a moment to actually come up before reading MAC
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("RX MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);

  // ----- Driver -----
  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0;
  driver.voltage_limit        = 12.0;
  if (!driver.init()) {
    Serial.println("Driver init FAILED");
    while (1) delay(1000);
  }
  motor.linkDriver(&driver);

  // ----- Open-loop angle control -----
  motor.controller     = MotionControlType::angle_openloop;
  motor.voltage_limit  = 6.0;       // open-loop torque
  motor.velocity_limit = 20.0;      // rad/s slew toward target (motor frame)
  motor.init();

  Serial.println("Receiver ready.");
  Serial.println("Cmds:");
  Serial.println("  T<deg>  manual motor target (motor-frame degrees) for calibration");
  Serial.println("  X       exit manual override, follow glove again");
  Serial.println("  M0      capture current target as glove-0deg motor position");
  Serial.println("  M90     capture current target as glove-90deg motor position");
  Serial.println("  P       print calibration");
}

void handleSerial() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      String s = buf; s.toUpperCase(); buf = "";

      if (s == "M0") {
        M0_motor_rad = manualOverride ? manualTargetRad
                                       : gloveDegToMotorRad(smoothedGloveDeg());
        Serial.printf("CAL: M0  = %.3f rad (%.1f deg motor)\n",
                      M0_motor_rad, M0_motor_rad * 180.0f / PI);
      } else if (s == "M90") {
        M90_motor_rad = manualOverride ? manualTargetRad
                                        : gloveDegToMotorRad(smoothedGloveDeg());
        Serial.printf("CAL: M90 = %.3f rad (%.1f deg motor)\n",
                      M90_motor_rad, M90_motor_rad * 180.0f / PI);
      } else if (s == "X") {
        manualOverride = false;
        Serial.println("Manual override OFF — following glove");
      } else if (s == "P") {
        Serial.printf("M0=%.3f rad (%.1f deg)  M90=%.3f rad (%.1f deg)  manual=%d\n",
                      M0_motor_rad, M0_motor_rad * 180.0f / PI,
                      M90_motor_rad, M90_motor_rad * 180.0f / PI,
                      manualOverride);
      } else if (s.startsWith("T")) {
        float deg = s.substring(1).toFloat();
        manualTargetRad = deg * PI / 180.0f;
        manualOverride  = true;
        Serial.printf("Manual override ON: target = %.1f deg motor (%.3f rad)\n",
                      deg, manualTargetRad);
      } else {
        Serial.printf("unknown cmd: %s\n", s.c_str());
      }
    } else {
      buf += c;
    }
  }
}

void loop() {
  handleSerial();

  float target_rad;
  float gloveDeg = smoothedGloveDeg();   // always update smoothing & rate tracking
  motor.velocity_limit = updateAdaptiveVelocityLimit(gloveDeg);

  if (manualOverride) {
    target_rad = manualTargetRad;
  } else {
    target_rad = gloveDegToMotorRad((int)(gloveDeg + 0.5f));
  }

  motor.loopFOC();
  motor.move(target_rad);

  // Status print 10 Hz
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    bool fresh = g_haveData && (millis() - g_lastRxMs < 500);
    Serial.print("glove_deg=");      Serial.print(g_lastAngle);
    Serial.print("  smooth=");       Serial.print(smooth_glove_deg, 1);
    Serial.print("  motor_tgt_rad="); Serial.print(target_rad, 2);
    Serial.print("  vlim=");         Serial.print(motor.velocity_limit, 1);
    Serial.print("  manual=");       Serial.print(manualOverride);
    Serial.print("  link=");         Serial.println(fresh ? "OK" : "STALE");
  }
}
