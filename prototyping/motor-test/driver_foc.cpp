/**
 * BLDC Motor FOC Control Test (DRV8300 + AS5600)
 *
 * Uses SimpleFOC with 6-PWM driver (high/low side per phase).
 * Accepts target angle via serial input.
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - DRV8300 custom motor driver PCB (6-PWM: high/low per phase)
 * - 2204 BLDC motor (7 pole pairs - NEEDS VERIFICATION)
 * - AS5600 encoder on I2C (SDA=21, SCL=22)
 *
 * Pin mapping (DRV8300 PCB):
 *   Phase A: high=25, low=26
 *   Phase B: high=27, low=14
 *   Phase C: high=12, low=13
 *
 * Original code by Matthew Men
 */

#include <SimpleFOC.h>
#include <Wire.h>

// ---------- Encoder ----------
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);

// ---------- 6PWM Driver ----------
BLDCDriver6PWM driver = BLDCDriver6PWM(
  25, 26,   // Phase A high / low
  27, 14,   // Phase B high / low
  12, 13    // Phase C high / low
);

// ---------- Motor ----------
// Change pole pairs to your motor value
BLDCMotor motor = BLDCMotor(7);

float target_angle = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Sensor init
  sensor.init();

  // Link sensor
  motor.linkSensor(&sensor);

  // Driver settings
  driver.voltage_power_supply = 10.0;   // your motor supply
  driver.voltage_limit = 3.0;           // start LOW
  driver.init();

  motor.linkDriver(&driver);

  // Control mode = angle position
  motor.controller = MotionControlType::angle;

  // PID defaults
  motor.PID_velocity.P = 0.2;
  motor.PID_velocity.I = 1;
  motor.PID_velocity.D = 0;

  motor.P_angle.P = 5;

  // Init motor
  motor.init();
  motor.initFOC();

  Serial.println("Enter target angle in degrees:");
}

void loop() {
  motor.loopFOC();

  motor.move(target_angle);

  // Serial input angle
  if (Serial.available()) {
    float deg = Serial.parseFloat();
    target_angle = deg * 3.14159 / 180.0;

    Serial.print("New Target: ");
    Serial.println(deg);
  }
}
