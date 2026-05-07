/**
 * TRANSMITTER — Glove side
 * Reads flex sensor on GPIO34, broadcasts value over ESP-NOW @ ~50 Hz.
 *
 * Before flashing:
 *   1. Flash the receiver first, copy its MAC from serial, paste into receiverMAC[] below.
 */

#include <WiFi.h>
#include <esp_now.h>

// >>> EDIT: paste receiver's MAC after flashing receiver once
uint8_t receiverMAC[] = {0xB0, 0xCB, 0xD8, 0x8B, 0x55, 0x04};

#define FLEX_PIN 34

typedef struct struct_message {
  int flexValue;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  // keep callback minimal — the loop print is enough
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(FLEX_PIN, INPUT);

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

  Serial.println("Transmitter ready");
}

void loop() {
  myData.flexValue = analogRead(FLEX_PIN);
  esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));

  Serial.print("flex=");
  Serial.println(myData.flexValue);

  delay(20);  // ~50 Hz
}
