/**
 * TRANSMITTER — Glove side. Reads flex sensor, calibrates, sends joint
 * angle (0..90°) over ESP-NOW @ ~50 Hz to the motor receiver.
 *
 * Calibration:
 *   1. On boot, default mapping (cal0=1500, cal90=2500) is a placeholder
 *      and will be wildly wrong until calibrated.
 *   2. Type 'C0' over serial while finger is FLAT (extended) — captures
 *      the raw ADC reading at 0°.
 *   3. Type 'C90' over serial while finger is FULLY BENT — captures the
 *      raw ADC reading at 90°.
 *   4. Calibration values live in RAM only — re-do after each power cycle.
 *
 * Other serial commands:
 *   P     print current calibration + a fresh angle reading
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - Flex sensor voltage-divider on GPIO 34 (ADC1_CH6, input-only)
 *
 * Before flashing:
 *  - Flash the receiver first, read its MAC from serial,
 *    paste it into receiverMAC[] below.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// EDIT: paste receiver MAC after flashing receiver once.
uint8_t receiverMAC[] = {0xB0, 0xCB, 0xD8, 0x8B, 0x55, 0x04};

#define FLEX_PIN     34
#define SAMPLE_AVG   16     // averaged samples per reading

// Default calibration — overwrite via C0 / C90 over serial.
int cal_raw_at_0  = 1500;
int cal_raw_at_90 = 2500;

typedef struct struct_message {
  int angle;            // 0..90 degrees, joint frame
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

int readFlexAvg() {
  long sum = 0;
  for (int i = 0; i < SAMPLE_AVG; i++) sum += analogRead(FLEX_PIN);
  return sum / SAMPLE_AVG;
}

int rawToAngleDeg(int raw) {
  // Linear interp between cal_raw_at_0 and cal_raw_at_90.
  // Works whether sensor's resistance increases OR decreases with bend
  // (sign of `span` handles inversion automatically).
  int span = cal_raw_at_90 - cal_raw_at_0;
  if (span == 0) return 0;          // un-calibrated: avoid div0
  long deg = (long)(raw - cal_raw_at_0) * 90 / span;
  if (deg < 0)  deg = 0;
  if (deg > 90) deg = 90;
  return (int)deg;
}

// EMA smoothing on the raw ADC reading. Alpha = 0.2 → ~80 ms time constant
// at 50 Hz sample rate, which is faster than human finger motion but slow
// enough to suppress ±1 LSB noise.
constexpr float SMOOTH_ALPHA = 0.2f;
float    smoothed_raw   = 0.0f;
bool     smoothing_primed = false;

int smoothedAngleDeg() {
  int raw = readFlexAvg();
  if (!smoothing_primed) {
    smoothed_raw = raw;
    smoothing_primed = true;
  } else {
    smoothed_raw = SMOOTH_ALPHA * raw + (1.0f - SMOOTH_ALPHA) * smoothed_raw;
  }
  return rawToAngleDeg((int)smoothed_raw);
}

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(FLEX_PIN, INPUT);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  Serial.print("TX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) delay(1000);
  }
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Add peer failed");
    while (1) delay(1000);
  }

  Serial.println("Transmitter ready.");
  Serial.println("Calibrate: 'C0' (finger flat) then 'C90' (finger fully bent).");
  Serial.println("'P' prints current calibration + reading.");
}

void handleSerial() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      buf.toUpperCase();
      if (buf == "C0") {
        cal_raw_at_0 = readFlexAvg();
        Serial.printf("CAL: raw_at_0  = %d\n", cal_raw_at_0);
      } else if (buf == "C90") {
        cal_raw_at_90 = readFlexAvg();
        Serial.printf("CAL: raw_at_90 = %d\n", cal_raw_at_90);
      } else if (buf == "P") {
        int raw = readFlexAvg();
        Serial.printf("raw=%d  cal0=%d  cal90=%d  angle=%d deg\n",
                      raw, cal_raw_at_0, cal_raw_at_90, rawToAngleDeg(raw));
      } else {
        Serial.printf("unknown cmd: %s\n", buf.c_str());
      }
      buf = "";
    } else {
      buf += c;
    }
  }
}

void loop() {
  handleSerial();

  int angle = smoothedAngleDeg();

  myData.angle = angle;
  esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.printf("raw=%d  smooth=%d  angle=%d\n",
                  (int)analogRead(FLEX_PIN), (int)smoothed_raw, angle);
  }

  delay(20);   // ~50 Hz send rate
}
