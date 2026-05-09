/**
 * HAND — Production firmware (2-motor receiver, no encoder, no current sense)
 *
 * Listens for ESP-NOW glove angles and drives two BLDC motors via SimpleFOC
 * `angle_openloop`. Adaptive velocity_limit per motor scales with the rate
 * of glove change so quick gestures don't lag. Calibration values
 * (per-joint M0/M90 motor positions) persist in NVS.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - 2× DRV8300 6-PWM driver
 *      Motor 1 (joint 0):  AH=25 AL=26 BH=27 BL=14 CH=12 CL=18
 *      Motor 2 (joint 1):  AH=4  AL=16 BH=17 BL=19 CH=21 CL=23
 *  - 2× 2204 BLDC, 7 pole pairs, 12V supply
 *  - 20:1 reduction gearbox per joint
 *  - NO encoder, NO current sense (open-loop)
 *
 * Serial commands:
 *   T1<deg>  manual override of motor 1 (motor-frame degrees), for cal
 *   T2<deg>  manual override of motor 2
 *   X        exit manual override, follow glove again
 *   M0       capture current motor positions as the glove-0° mapping
 *            (call when both joints are physically at "finger straight")
 *   M90      capture as glove-90° mapping (joints at "fully bent")
 *   S        save calibration to NVS
 *   R        reset calibration to defaults
 *   P        print calibration + state
 */

#include <SimpleFOC.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <Preferences.h>

// ----- Joint count -----
constexpr int N_JOINTS = 2;

// ----- Drivers + motors -----
BLDCDriver6PWM driver0 = BLDCDriver6PWM(25, 26, 27, 14, 12, 18);
BLDCDriver6PWM driver1 = BLDCDriver6PWM(4,  16, 17, 19, 21, 23);
BLDCMotor      motor0  = BLDCMotor(7);
BLDCMotor      motor1  = BLDCMotor(7);
BLDCMotor*     motors [N_JOINTS] = { &motor0, &motor1 };
BLDCDriver6PWM* drivers[N_JOINTS] = { &driver0, &driver1 };

// ----- Constants -----
constexpr float    GEAR_RATIO    = 20.0f;
constexpr int      GLOVE_MIN_DEG = 0;
constexpr int      GLOVE_MAX_DEG = 90;
constexpr float    RX_ALPHA      = 0.4f;
constexpr float    VL_MIN_RAD_S  = 30.0f;
constexpr float    VL_MAX_RAD_S  = 250.0f;
constexpr float    VL_HEADROOM   = 2.0f;
constexpr uint32_t VL_PERIOD_MS  = 50;
constexpr float    VL_LPF_ALPHA  = 0.4f;
constexpr float    VOLTAGE_LIMIT = 6.0f;     // ~2.6A peak through 2.3Ω

// ----- Wire-format packet (MUST match glove transmitter) -----
typedef struct __attribute__((packed)) {
  int16_t angle[N_JOINTS];     // 0..90 deg, joint frame
} GloveMessage;

// ----- Calibration state (persisted in NVS) -----
struct CalState {
  float M0_motor_rad [N_JOINTS];
  float M90_motor_rad[N_JOINTS];
} cal = {
  { 0.0f, 0.0f },
  { 90.0f * GEAR_RATIO * (PI / 180.0f), 90.0f * GEAR_RATIO * (PI / 180.0f) },
};

// ----- Live state -----
volatile int16_t  g_lastAngle[N_JOINTS] = {0, 0};
volatile bool     g_haveData = false;
volatile uint32_t g_lastRxMs = 0;

float smooth_glove_deg[N_JOINTS] = {0.0f, 0.0f};
bool  rx_primed      [N_JOINTS] = {false, false};

bool  manualOverride [N_JOINTS] = {false, false};
float manualTargetRad[N_JOINTS] = {0.0f, 0.0f};

Preferences prefs;

// ----- Helpers -----
float gloveDegToMotorRad(int joint, float deg) {
  if (deg < GLOVE_MIN_DEG) deg = GLOVE_MIN_DEG;
  if (deg > GLOVE_MAX_DEG) deg = GLOVE_MAX_DEG;
  float t = deg / (float)GLOVE_MAX_DEG;
  return cal.M0_motor_rad[joint]
       + t * (cal.M90_motor_rad[joint] - cal.M0_motor_rad[joint]);
}

float smoothedGloveDeg(int joint) {
  int raw = g_lastAngle[joint];
  if (!rx_primed[joint]) {
    smooth_glove_deg[joint] = raw;
    rx_primed[joint] = true;
  } else {
    smooth_glove_deg[joint] =
      RX_ALPHA * raw + (1.0f - RX_ALPHA) * smooth_glove_deg[joint];
  }
  return smooth_glove_deg[joint];
}

float updateAdaptiveVelocityLimit(int joint, float smooth_glove) {
  static uint32_t last_ms[N_JOINTS]      = {0, 0};
  static float    last_smooth[N_JOINTS]  = {0.0f, 0.0f};
  static float    vlim_filt[N_JOINTS]    = {VL_MIN_RAD_S, VL_MIN_RAD_S};
  uint32_t now = millis();
  if (now - last_ms[joint] >= VL_PERIOD_MS) {
    float dt_s = (now - last_ms[joint]) / 1000.0f;
    float rate_joint_deg_s = fabsf(smooth_glove - last_smooth[joint]) / dt_s;
    float motor_rate_rad_s = rate_joint_deg_s * GEAR_RATIO * (PI / 180.0f);
    float target_vlim      = motor_rate_rad_s * VL_HEADROOM;
    if (target_vlim < VL_MIN_RAD_S) target_vlim = VL_MIN_RAD_S;
    if (target_vlim > VL_MAX_RAD_S) target_vlim = VL_MAX_RAD_S;
    vlim_filt[joint] = VL_LPF_ALPHA * target_vlim + (1.0f - VL_LPF_ALPHA) * vlim_filt[joint];
    last_ms[joint]     = now;
    last_smooth[joint] = smooth_glove;
  }
  return vlim_filt[joint];
}

// ----- ESP-NOW -----
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(GloveMessage)) return;
  GloveMessage m;
  memcpy(&m, data, sizeof(m));
  for (int j = 0; j < N_JOINTS; j++) g_lastAngle[j] = m.angle[j];
  g_haveData  = true;
  g_lastRxMs  = millis();
}

// ----- NVS persistence -----
void loadCal() {
  prefs.begin("hand", true);
  cal.M0_motor_rad[0]  = prefs.getFloat("m0_0",  cal.M0_motor_rad[0]);
  cal.M0_motor_rad[1]  = prefs.getFloat("m0_1",  cal.M0_motor_rad[1]);
  cal.M90_motor_rad[0] = prefs.getFloat("m90_0", cal.M90_motor_rad[0]);
  cal.M90_motor_rad[1] = prefs.getFloat("m90_1", cal.M90_motor_rad[1]);
  prefs.end();
}

void saveCal() {
  prefs.begin("hand", false);
  prefs.putFloat("m0_0",  cal.M0_motor_rad[0]);
  prefs.putFloat("m0_1",  cal.M0_motor_rad[1]);
  prefs.putFloat("m90_0", cal.M90_motor_rad[0]);
  prefs.putFloat("m90_1", cal.M90_motor_rad[1]);
  prefs.end();
}

void resetCal() {
  cal.M0_motor_rad[0]  = 0.0f;
  cal.M0_motor_rad[1]  = 0.0f;
  cal.M90_motor_rad[0] = 90.0f * GEAR_RATIO * (PI / 180.0f);
  cal.M90_motor_rad[1] = 90.0f * GEAR_RATIO * (PI / 180.0f);
}

void printCal() {
  Serial.printf("CAL: J0  M0=%.2f rad (%.0f°)  M90=%.2f rad (%.0f°)\n",
                cal.M0_motor_rad[0],  cal.M0_motor_rad[0]  * 180.0f / PI,
                cal.M90_motor_rad[0], cal.M90_motor_rad[0] * 180.0f / PI);
  Serial.printf("     J1  M0=%.2f rad (%.0f°)  M90=%.2f rad (%.0f°)\n",
                cal.M0_motor_rad[1],  cal.M0_motor_rad[1]  * 180.0f / PI,
                cal.M90_motor_rad[1], cal.M90_motor_rad[1] * 180.0f / PI);
}

// Compute the current "live" target_rad for a joint (manual override or gloved).
float currentTarget(int joint) {
  if (manualOverride[joint]) return manualTargetRad[joint];
  return gloveDegToMotorRad(joint, smooth_glove_deg[joint]);
}

// ----- Serial command parser -----
void handleSerial() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      String s = buf; s.toUpperCase(); buf = "";

      if (s.startsWith("T1") || s.startsWith("T2")) {
        int j = (s.charAt(1) == '1') ? 0 : 1;
        float deg = s.substring(2).toFloat();
        manualTargetRad[j] = deg * PI / 180.0f;
        manualOverride[j]  = true;
        Serial.printf("Manual override J%d: %.1f° (%.3f rad)\n",
                      j, deg, manualTargetRad[j]);
      } else if (s == "X") {
        manualOverride[0] = false;
        manualOverride[1] = false;
        Serial.println("Manual override OFF — both joints follow glove");
      } else if (s == "M0") {
        cal.M0_motor_rad[0] = currentTarget(0);
        cal.M0_motor_rad[1] = currentTarget(1);
        Serial.printf("CAL captured M0: J0=%.3f  J1=%.3f rad  (run 'S' to save)\n",
                      cal.M0_motor_rad[0], cal.M0_motor_rad[1]);
      } else if (s == "M90") {
        cal.M90_motor_rad[0] = currentTarget(0);
        cal.M90_motor_rad[1] = currentTarget(1);
        Serial.printf("CAL captured M90: J0=%.3f  J1=%.3f rad  (run 'S' to save)\n",
                      cal.M90_motor_rad[0], cal.M90_motor_rad[1]);
      } else if (s == "S") {
        saveCal();
        Serial.println("CAL saved to NVS");
      } else if (s == "R") {
        resetCal();
        Serial.println("CAL reset to defaults (run 'S' to persist)");
      } else if (s == "P") {
        printCal();
      } else {
        Serial.printf("unknown cmd: %s\n", s.c_str());
      }
    } else {
      buf += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  delay(100);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("\n=== HAND RX ===\nRX MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);

  // Init both motor stacks
  for (int j = 0; j < N_JOINTS; j++) {
    drivers[j]->pwm_frequency        = 25000;
    drivers[j]->voltage_power_supply = 12.0;
    drivers[j]->voltage_limit        = 12.0;
    if (!drivers[j]->init()) {
      Serial.printf("Driver %d init FAILED\n", j);
      while (1) delay(1000);
    }
    motors[j]->linkDriver(drivers[j]);
    motors[j]->controller     = MotionControlType::angle_openloop;
    motors[j]->voltage_limit  = VOLTAGE_LIMIT;
    motors[j]->velocity_limit = VL_MIN_RAD_S;
    motors[j]->init();
    Serial.printf("Motor %d ready\n", j);
  }

  loadCal();
  printCal();

  Serial.println("Ready. Cmds: T1<d>|T2<d> | X | M0 | M90 | S save | R reset | P print");
}

void loop() {
  handleSerial();

  for (int j = 0; j < N_JOINTS; j++) {
    float gloveDeg = smoothedGloveDeg(j);
    motors[j]->velocity_limit = updateAdaptiveVelocityLimit(j, gloveDeg);
    float target_rad;
    if (manualOverride[j]) target_rad = manualTargetRad[j];
    else                   target_rad = gloveDegToMotorRad(j, gloveDeg);
    motors[j]->loopFOC();
    motors[j]->move(target_rad);
  }

  // Status print 5 Hz
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  if (now - lastPrint > 200) {
    lastPrint = now;
    bool fresh = g_haveData && (now - g_lastRxMs < 500);
    Serial.printf("J0 g=%d s=%.1f tgt=%.2f vlim=%.0f manual=%d  |  "
                  "J1 g=%d s=%.1f tgt=%.2f vlim=%.0f manual=%d  |  link=%s\n",
                  g_lastAngle[0], smooth_glove_deg[0],
                  manualOverride[0] ? manualTargetRad[0] : gloveDegToMotorRad(0, smooth_glove_deg[0]),
                  motors[0]->velocity_limit, manualOverride[0],
                  g_lastAngle[1], smooth_glove_deg[1],
                  manualOverride[1] ? manualTargetRad[1] : gloveDegToMotorRad(1, smooth_glove_deg[1]),
                  motors[1]->velocity_limit, manualOverride[1],
                  fresh ? "OK" : "STALE");
  }
}
