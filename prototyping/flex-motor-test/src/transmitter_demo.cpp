/**
 * TRANSMITTER (DEMO) — Glove side, NO calibration UI.
 *
 * Hardcoded calibration constants at the top. Edit them after running the
 * calibration version of the firmware once. No serial commands, no
 * interactive setup — boots and streams angles immediately.
 *
 * Hardware:
 *  - ESP32 DevKit v1
 *  - Flex sensor on GPIO 34
 *
 * Before flashing:
 *  - Update CAL_RAW_AT_0 / CAL_RAW_AT_90 from the calibration build
 *  - Update receiverMAC[] from the receiver's serial output
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// ===== HARDCODED CALIBRATION — edit after running calibration build =====
// Note inverted span (660 → 180): this sensor's resistance decreases with
// bend. The linear interp handles either direction automatically.
constexpr int CAL_RAW_AT_0  = 660;      // ADC reading when finger is FLAT
constexpr int CAL_RAW_AT_90 = 180;      // ADC reading when finger is FULLY BENT

uint8_t receiverMAC[] = {0xB0, 0xCB, 0xD8, 0x8B, 0x55, 0x04};

#define FLEX_PIN     34
#define SAMPLE_AVG   16

constexpr float SMOOTH_ALPHA = 0.2f;     // EMA on raw ADC, ~80ms TC at 50Hz

typedef struct struct_message {
  int angle;          // 0..90 deg, joint frame
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;
float smoothed_raw   = 0.0f;
bool  smoothing_primed = false;

int readFlexAvg() {
  long sum = 0;
  for (int i = 0; i < SAMPLE_AVG; i++) sum += analogRead(FLEX_PIN);
  return sum / SAMPLE_AVG;
}

int rawToAngleDeg(int raw) {
  constexpr int span = CAL_RAW_AT_90 - CAL_RAW_AT_0;
  static_assert(span != 0, "CAL_RAW_AT_0 and CAL_RAW_AT_90 must differ");
  long deg = (long)(raw - CAL_RAW_AT_0) * 90 / span;
  if (deg < 0)  deg = 0;
  if (deg > 90) deg = 90;
  return (int)deg;
}

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(FLEX_PIN, INPUT);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  delay(100);
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

  Serial.printf("Demo TX ready. cal0=%d  cal90=%d\n", CAL_RAW_AT_0, CAL_RAW_AT_90);
}

void loop() {
  int raw = readFlexAvg();
  if (!smoothing_primed) { smoothed_raw = raw; smoothing_primed = true; }
  else { smoothed_raw = SMOOTH_ALPHA * raw + (1.0f - SMOOTH_ALPHA) * smoothed_raw; }
  int angle = rawToAngleDeg((int)smoothed_raw);

  myData.angle = angle;
  esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.printf("raw=%d  smooth=%d  angle=%d\n",
                  raw, (int)smoothed_raw, angle);
  }

  delay(20);   // ~50 Hz
}
