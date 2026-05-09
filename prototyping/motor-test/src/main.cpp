/**
 * Angle openloop — NO encoder. SimpleFOC sweeps the electrical field to
 * a commanded shaft angle. Rotor follows the field, no feedback.
 *
 * Pins: AH=25 AL=26 BH=27 BL=14 CH=12 CL=18
 *
 * Serial: <degrees> target angle. e.g. 30, 0, 90, -45, 360
 *         Z = rezero target at current commanded position
 */

#include <SimpleFOC.h>

BLDCDriver6PWM driver = BLDCDriver6PWM(25, 26, 27, 14, 12, 18);
BLDCMotor motor = BLDCMotor(7);

float target_rad = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(200);

  SimpleFOCDebug::enable(&Serial);

  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0;
  driver.voltage_limit        = 12.0;
  if (!driver.init()) {
    Serial.println("Driver init FAILED");
    while (1) delay(1000);
  }
  motor.linkDriver(&driver);

  motor.controller     = MotionControlType::angle_openloop;
  motor.voltage_limit  = 6.0;          // open-loop torque
  motor.velocity_limit = 20.0;         // rad/s slew toward target
  motor.init();

  Serial.println("Angle openloop ready. <deg>: target. Z: rezero.");
}

void loop() {
  motor.loopFOC();
  motor.move(target_rad);

  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        if (buf[0] == 'Z' || buf[0] == 'z') {
          // Reset target to current internal "shaft" — in openloop this is
          // just the integrator state. Effectively makes "here" the new 0.
          motor.shaft_angle = 0;
          target_rad = 0;
          Serial.println("Rezeroed");
        } else {
          float deg = buf.toFloat();
          target_rad = deg * PI / 180.0f;
          Serial.print("Target = ");
          Serial.print(deg);
          Serial.println(" deg");
        }
        buf = "";
      }
    } else if (c != ' ') {
      buf += c;
    }
  }
}
