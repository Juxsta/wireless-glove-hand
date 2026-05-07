/**
 * Minimal motor spin test — open-loop velocity, no FOC alignment, no encoder loop.
 *
 * The motor should reliably spin in response to a commanded velocity (rad/s).
 * If this works, the driver + motor + power path is healthy.
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - DRV8300 6-PWM driver, AH=5 AL=17 BH=16 BL=4 CH=2 CL=15
 * - 2804 BLDC, 7 pole pairs
 * - 12V (or up to 19V) bench supply
 *
 * Serial commands (one per line):
 *   <number>   target velocity in rad/s (e.g. 5, -5, 10, 0)
 *   S          stop
 */

#include <SimpleFOC.h>

BLDCDriver6PWM driver = BLDCDriver6PWM(
  5,  17,
  16, 4,
  2,  15
);

BLDCMotor motor = BLDCMotor(7);

float target_velocity = 0.0f;     // rad/s (electrical-angle ramp rate × pole_pairs)

void setup() {
  Serial.begin(115200);
  delay(200);

  SimpleFOCDebug::enable(&Serial);        // print driver/motor init state to serial

  driver.pwm_frequency        = 25000;
  driver.voltage_power_supply = 12.0;
  driver.voltage_limit        = 12.0;     // allow full driver swing
  // dead_zone NOT set — let SimpleFOC use its default; core 3.x MCPWM also applies hw dead-time
  if (!driver.init()) {
    Serial.println("Driver init FAILED — halting");
    while (1) delay(1000);
  }
  Serial.println("Driver init OK");
  motor.linkDriver(&driver);

  motor.controller    = MotionControlType::velocity_openloop;
  motor.voltage_limit = 10.0;             // open-loop applied voltage — must overcome stiction

  motor.init();
  Serial.println("Motor init done");

  Serial.println("Open-loop spin test ready.");
  Serial.println("Cmds: <rad/s>  e.g. 5   |   S to stop");
}

void loop() {
  motor.move(target_velocity);            // velocity_openloop just sweeps electrical angle

  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        if (buf[0] == 'S' || buf[0] == 's') {
          target_velocity = 0;
          Serial.println("Stop");
        } else {
          target_velocity = buf.toFloat();
          Serial.print("Velocity = ");
          Serial.print(target_velocity);
          Serial.println(" rad/s");
        }
        buf = "";
      }
    } else if (c != ' ') {
      buf += c;
    }
  }
}
