/**
 * Closed-Loop Test: 6-Step Commutation + AS5600 Encoder Feedback
 *
 * Builds on the verified open-loop 6-step commutation by adding AS5600
 * encoder reads. Lets you drive manually (F/B + duty) or set a target
 * angle and have the firmware step toward it using encoder feedback.
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - 3-phase driver: AH=5  AL=17  BH=16  BL=4  CH=2  CL=15
 * - AS5600 encoder on I2C: SDA=21, SCL=22
 *
 * Serial commands (one per line):
 *   F            - drive CW (open loop)
 *   B            - drive CCW (open loop)
 *   S            - stop (hold position, no commutation)
 *   <0-100>      - set duty %
 *   T<deg>       - set target angle in degrees, closed-loop step toward it
 *                  e.g.  T90    T-45    T180
 */

#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>

// ---------- Pins ----------
#define AH 5
#define AL 17
#define BH 16
#define BL 4
#define CH 2
#define CL 15

#define PWM_FREQ      40000
#define PWM_RES       8
#define STEP_DELAY_US 5000
#define DEADTIME_US   5

// LEDC channels for each high-side pin (Arduino-ESP32 core 2.x API)
#define CH_AH 0
#define CH_BH 1
#define CH_CH 2

// Closed-loop tuning
#define ANGLE_TOL_DEG   2.0f    // acceptable error band
#define ENC_PRINT_MS    100     // status print interval

enum Mode { MODE_STOP, MODE_OPEN_FWD, MODE_OPEN_REV, MODE_TARGET };

AS5600 encoder;

uint8_t duty       = 128;       // 0-255
Mode    mode       = MODE_STOP;
float   target_deg = 0.0f;      // unwrapped target (can exceed 360)
float   angle_deg  = 0.0f;      // unwrapped measured angle
float   last_raw_deg = 0.0f;    // last wrapped reading 0..360
bool    have_last  = false;

uint32_t last_print_ms = 0;

// ---------- Low-level commutation ----------
void allOff() {
  ledcWrite(AH, 0);
  ledcWrite(BH, 0);
  ledcWrite(CH, 0);
  digitalWrite(AL, LOW);
  digitalWrite(BL, LOW);
  digitalWrite(CL, LOW);
}

void setPhase(int highPin, int lowPin) {
  ledcWrite(highPin, duty);
  digitalWrite(lowPin, HIGH);
}

// One commutation step. step in 0..5. CW order; reverse iterates backward.
void commutate(int step) {
  allOff();
  delayMicroseconds(DEADTIME_US);
  switch (step) {
    case 0: setPhase(AH, BL); break;
    case 1: setPhase(AH, CL); break;
    case 2: setPhase(BH, CL); break;
    case 3: setPhase(BH, AL); break;
    case 4: setPhase(CH, AL); break;
    case 5: setPhase(CH, BL); break;
  }
}

// ---------- Encoder ----------
// Read the AS5600 and update unwrapped angle_deg by accumulating deltas.
void updateEncoder() {
  int raw = encoder.readAngle();           // 0..4095
  float wrapped = raw * (360.0f / 4096.0f);

  if (!have_last) {
    last_raw_deg = wrapped;
    have_last = true;
    return;
  }

  float d = wrapped - last_raw_deg;
  if (d >  180.0f) d -= 360.0f;            // wrap-around correction
  if (d < -180.0f) d += 360.0f;
  angle_deg   += d;
  last_raw_deg = wrapped;
}

// ---------- Serial parsing ----------
void handleSerial() {
  static String input = "";

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (input.length() == 0) continue;

      char first = input[0];
      if (first == 'F' || first == 'f') {
        mode = MODE_OPEN_FWD;
        Serial.println("Mode: open-loop CW");
      } else if (first == 'B' || first == 'b') {
        mode = MODE_OPEN_REV;
        Serial.println("Mode: open-loop CCW");
      } else if (first == 'S' || first == 's') {
        mode = MODE_STOP;
        allOff();
        Serial.println("Mode: stop");
      } else if (first == 'T' || first == 't') {
        target_deg = input.substring(1).toFloat();
        mode = MODE_TARGET;
        Serial.print("Mode: target = ");
        Serial.print(target_deg);
        Serial.println(" deg");
      } else {
        // Numeric -> duty %
        int val = input.toInt();
        if (val >= 0 && val <= 100) {
          duty = map(val, 0, 100, 0, 255);
          Serial.print("Duty: ");
          Serial.print(val);
          Serial.println("%");
        }
      }
      input = "";
    } else if (c != ' ') {
      input += c;
    }
  }
}

// ---------- Status print ----------
void maybePrintStatus() {
  uint32_t now = millis();
  if (now - last_print_ms < ENC_PRINT_MS) return;
  last_print_ms = now;

  Serial.print("angle=");
  Serial.print(angle_deg, 2);
  Serial.print("  target=");
  Serial.print(target_deg, 2);
  Serial.print("  err=");
  Serial.print(target_deg - angle_deg, 2);
  Serial.print("  duty=");
  Serial.print(duty);
  Serial.print("  raw=");
  Serial.println(encoder.readAngle());
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(AL, OUTPUT);
  pinMode(BL, OUTPUT);
  pinMode(CL, OUTPUT);

  ledcAttach(AH, PWM_FREQ, PWM_RES);
  ledcAttach(BH, PWM_FREQ, PWM_RES);
  ledcAttach(CH, PWM_FREQ, PWM_RES);

  allOff();

  Wire.begin(21, 22);
  if (!encoder.begin()) {
    Serial.println("AS5600 not found! Check wiring.");
    while (1) delay(1000);
  }

  // Magnet diagnostics
  Serial.print("AS5600 connected. magnet detected=");
  Serial.print(encoder.detectMagnet());
  Serial.print(" tooWeak=");
  Serial.print(encoder.magnetTooWeak());
  Serial.print(" tooStrong=");
  Serial.print(encoder.magnetTooStrong());
  Serial.print(" rawAngle=");
  Serial.println(encoder.readAngle());

  // Prime encoder
  updateEncoder();
  angle_deg = 0.0f;             // zero at startup

  Serial.println("Closed-loop test ready.");
  Serial.println("Cmds: F | B | S | <0-100> duty | T<deg> target");
}

// ---------- Loop ----------
void loop() {
  handleSerial();
  updateEncoder();
  maybePrintStatus();

  static int step_idx = 0;

  switch (mode) {
    case MODE_STOP:
      allOff();
      delay(1);
      break;

    case MODE_OPEN_FWD:
      commutate(step_idx);
      delayMicroseconds(STEP_DELAY_US);
      step_idx = (step_idx + 1) % 6;
      break;

    case MODE_OPEN_REV:
      commutate(step_idx);
      delayMicroseconds(STEP_DELAY_US);
      step_idx = (step_idx + 5) % 6;   // -1 mod 6
      break;

    case MODE_TARGET: {
      float err = target_deg - angle_deg;
      if (fabsf(err) <= ANGLE_TOL_DEG) {
        allOff();                       // hold
        delay(1);
      } else {
        bool fwd = (err > 0);
        commutate(step_idx);
        delayMicroseconds(STEP_DELAY_US);
        step_idx = fwd ? (step_idx + 1) % 6
                       : (step_idx + 5) % 6;
      }
      break;
    }
  }
}
