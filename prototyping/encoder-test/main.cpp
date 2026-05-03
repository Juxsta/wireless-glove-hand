/**
 * AS5600 Encoder Test (VERIFIED WORKING)
 *
 * Reads absolute angle from AS5600 magnetic encoder via I2C.
 * Uses the AS5600 library (not SimpleFOC's MagneticSensorI2C).
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - AS5600 encoder on I2C (SDA=21, SCL=22)
 *
 * Original code by Matthew Men
 */

#include <Wire.h>
#include <AS5600.h>

AS5600 encoder;

void setup() {
  Serial.begin(115200);

  // Start I2C (SDA, SCL)
  Wire.begin(21, 22);

  if (!encoder.begin()) {
    Serial.println("AS5600 not found!");
    while (1);
  }

  Serial.println("AS5600 Ready");
}

void loop() {
  int raw = encoder.readAngle();     // 0 - 4095
  float angle = raw * 360.0 / 4096.0;

  Serial.print("Raw: ");
  Serial.print(raw);

  Serial.print("   Degrees: ");
  Serial.println(angle);

  delay(100);
}
