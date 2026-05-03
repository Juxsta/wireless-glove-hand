/**
 * ESP-NOW Transmitter Test (Button Press)
 *
 * Sends a message when button is pressed. Simple validation
 * that ESP-NOW link works between two ESP32 boards.
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - Button on GPIO 14 (pull-up)
 *
 * Original code by Matthew Men
 */

#include <WiFi.h>
#include <esp_now.h>

#define BUTTON_PIN 14
uint8_t receptorAddress[] = {0x8C, 0xCE, 0x4E, 0xBA, 0x5E, 0x14};

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receptorAddress, 6);
  esp_now_add_peer(&peer);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    uint8_t msg = 1;
    esp_now_send(receptorAddress, &msg, sizeof(msg));
    Serial.println("Message sent");
    delay(200);
  }
}
