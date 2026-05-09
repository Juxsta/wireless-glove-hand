/**
 * GLOVE — Production firmware (2 flex sensors → ESP-NOW → hand)
 *
 * Reads two flex sensors (one finger, two joint segments). Per-channel
 * EMA smoothing. Per-channel two-point linear calibration. ESP-NOW sends
 * a fixed-format packet @ 50 Hz to the hand receiver.
 *
 * Calibration values persist in NVS (Preferences). After a one-time
 * calibration (`C0` flat, `C90` bent, `S` save) the firmware boots
 * ready to use — no separate "demo" build.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - Flex sensor 1 (proximal joint of finger) on GPIO 34
 *  - Flex sensor 2 (distal joint of finger)   on GPIO 35
 *
 * Before flashing:
 *  - Set RECEIVER_MAC[] from the hand controller's serial output
 *
 * Serial commands:
 *   C0    capture both flex readings as joint-0° (hold finger flat)
 *   C90   capture both flex readings as joint-90° (hold finger fully bent)
 *   S     save calibration to NVS (persists across reboots)
 *   R     reset calibration to defaults
 *   P     print current calibration + live readings
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <Preferences.h>

// ----- Hardware -----
constexpr uint8_t FLEX_PIN[2] = {34, 35};
constexpr int     SAMPLE_AVG  = 16;

// ----- ESP-NOW receiver -----
// Update from the hand controller's serial print at boot.
uint8_t RECEIVER_MAC[6] = {0xB0, 0xCB, 0xD8, 0x8B, 0x55, 0x04};

// ----- Smoothing / packet rate -----
constexpr float    SMOOTH_ALPHA  = 0.2f;     // EMA on raw ADC
constexpr uint32_t TX_PERIOD_MS  = 20;       // 50 Hz

// ----- Wire-format packet shared with the hand receiver -----
typedef struct __attribute__((packed)) {
  int16_t angle[2];     // joint angles 0..90 deg, joint frame
} GloveMessage;

// ----- State -----
struct CalState {
  int raw_at_0[2];
  int raw_at_90[2];
} cal = {
  {660, 660},        // sensible-ish default; user re-cals at boot
  {180, 180},
};

float smoothed_raw[2] = {0.0f, 0.0f};
bool  smoothing_primed[2] = {false, false};

GloveMessage tx_msg = {};
esp_now_peer_info_t peerInfo = {};
Preferences prefs;

// ----- Helpers -----
int readFlexAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < SAMPLE_AVG; i++) sum += analogRead(pin);
  return sum / SAMPLE_AVG;
}

int rawToAngleDeg(int raw, int cal0, int cal90) {
  int span = cal90 - cal0;
  if (span == 0) return 0;
  long deg = (long)(raw - cal0) * 90 / span;
  if (deg < 0)  deg = 0;
  if (deg > 90) deg = 90;
  return (int)deg;
}

int updatedAngleFor(int idx) {
  int raw = readFlexAvg(FLEX_PIN[idx]);
  if (!smoothing_primed[idx]) {
    smoothed_raw[idx] = raw;
    smoothing_primed[idx] = true;
  } else {
    smoothed_raw[idx] = SMOOTH_ALPHA * raw + (1.0f - SMOOTH_ALPHA) * smoothed_raw[idx];
  }
  return rawToAngleDeg((int)smoothed_raw[idx], cal.raw_at_0[idx], cal.raw_at_90[idx]);
}

// ----- NVS persistence -----
void loadCal() {
  prefs.begin("glove", true);
  cal.raw_at_0[0]  = prefs.getInt("c0_a",  cal.raw_at_0[0]);
  cal.raw_at_0[1]  = prefs.getInt("c0_b",  cal.raw_at_0[1]);
  cal.raw_at_90[0] = prefs.getInt("c90_a", cal.raw_at_90[0]);
  cal.raw_at_90[1] = prefs.getInt("c90_b", cal.raw_at_90[1]);
  prefs.end();
}

void saveCal() {
  prefs.begin("glove", false);
  prefs.putInt("c0_a",  cal.raw_at_0[0]);
  prefs.putInt("c0_b",  cal.raw_at_0[1]);
  prefs.putInt("c90_a", cal.raw_at_90[0]);
  prefs.putInt("c90_b", cal.raw_at_90[1]);
  prefs.end();
}

void resetCal() {
  cal.raw_at_0[0]  = 660;
  cal.raw_at_0[1]  = 660;
  cal.raw_at_90[0] = 180;
  cal.raw_at_90[1] = 180;
}

void printCal() {
  int r0 = readFlexAvg(FLEX_PIN[0]);
  int r1 = readFlexAvg(FLEX_PIN[1]);
  int a0 = rawToAngleDeg(r0, cal.raw_at_0[0], cal.raw_at_90[0]);
  int a1 = rawToAngleDeg(r1, cal.raw_at_0[1], cal.raw_at_90[1]);
  Serial.printf("CAL: J0[%d..%d]→raw%d→%d°  J1[%d..%d]→raw%d→%d°\n",
                cal.raw_at_0[0], cal.raw_at_90[0], r0, a0,
                cal.raw_at_0[1], cal.raw_at_90[1], r1, a1);
}

// ----- ESP-NOW -----
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

// ----- Serial command parser -----
void handleSerial() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      buf.toUpperCase();
      String s = buf; buf = "";

      if (s == "C0") {
        cal.raw_at_0[0] = readFlexAvg(FLEX_PIN[0]);
        cal.raw_at_0[1] = readFlexAvg(FLEX_PIN[1]);
        Serial.printf("CAL captured: 0° → J0=%d  J1=%d  (run 'S' to save)\n",
                      cal.raw_at_0[0], cal.raw_at_0[1]);
      } else if (s == "C90") {
        cal.raw_at_90[0] = readFlexAvg(FLEX_PIN[0]);
        cal.raw_at_90[1] = readFlexAvg(FLEX_PIN[1]);
        Serial.printf("CAL captured: 90° → J0=%d  J1=%d  (run 'S' to save)\n",
                      cal.raw_at_90[0], cal.raw_at_90[1]);
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

  pinMode(FLEX_PIN[0], INPUT);
  pinMode(FLEX_PIN[1], INPUT);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  delay(100);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("\n=== GLOVE TX ===\nTX MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  loadCal();
  printCal();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, RECEIVER_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW add_peer failed");
    while (1) delay(1000);
  }

  Serial.println("Ready. Cmds: C0 | C90 | S save | R reset | P print");
}

void loop() {
  handleSerial();

  static uint32_t last_tx = 0;
  uint32_t now = millis();
  if (now - last_tx >= TX_PERIOD_MS) {
    last_tx = now;
    int a0 = updatedAngleFor(0);
    int a1 = updatedAngleFor(1);
    tx_msg.angle[0] = a0;
    tx_msg.angle[1] = a1;
    esp_now_send(RECEIVER_MAC, (uint8_t *)&tx_msg, sizeof(tx_msg));

    static uint32_t last_print = 0;
    if (now - last_print > 200) {
      last_print = now;
      Serial.printf("J0 raw=%d a=%d°  J1 raw=%d a=%d°\n",
                    (int)smoothed_raw[0], a0, (int)smoothed_raw[1], a1);
    }
  }
}
