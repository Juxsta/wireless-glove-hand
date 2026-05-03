/**
 * 6-Step BLDC Commutation Test (No FOC, raw drive)
 *
 * Simple open-loop commutation to verify motor spins.
 * Useful as a first hardware validation before FOC.
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - DRV8300 custom motor driver PCB
 * - 2204 BLDC motor
 *
 * Pin mapping (alternate - verify against PCB):
 *   AH=33, AL=25, BH=26, BL=27, CH=14, CL=12
 *
 * NOTE: Pin mapping differs from driver_foc.cpp. Confirm which
 * matches the actual DRV8300 PCB with Raul.
 *
 * Original code by Matthew Men
 */

// -------- Pin mapping (EDIT THESE) --------
#define AH  33
#define AL  25
#define BH  26
#define BL  27
#define CH  14
#define CL  12

// step delay controls speed (lower = faster)
int stepDelay = 5;

// commutation step (0-5)
int stepIndex = 0;

void setup() {
  Serial.begin(115200);

  pinMode(AH, OUTPUT);
  pinMode(AL, OUTPUT);
  pinMode(BH, OUTPUT);
  pinMode(BL, OUTPUT);
  pinMode(CH, OUTPUT);
  pinMode(CL, OUTPUT);

  disableAll();
  Serial.println("Starting 6-step BLDC commutation...");
}

void loop() {
  commutate(stepIndex);

  stepIndex++;
  if (stepIndex > 5) stepIndex = 0;

  delay(stepDelay);
}

// ---------------- Core commutation table ----------------
void commutate(int step) {
  disableAll(); // avoid shoot-through

  switch (step) {
    case 0:  // A+ B-
      digitalWrite(AH, HIGH);
      digitalWrite(BL, HIGH);
      break;
    case 1:  // A+ C-
      digitalWrite(AH, HIGH);
      digitalWrite(CL, HIGH);
      break;
    case 2:  // B+ C-
      digitalWrite(BH, HIGH);
      digitalWrite(CL, HIGH);
      break;
    case 3:  // B+ A-
      digitalWrite(BH, HIGH);
      digitalWrite(AL, HIGH);
      break;
    case 4:  // C+ A-
      digitalWrite(CH, HIGH);
      digitalWrite(AL, HIGH);
      break;
    case 5:  // C+ B-
      digitalWrite(CH, HIGH);
      digitalWrite(BL, HIGH);
      break;
  }
}

// ---------------- Safety helper ----------------
void disableAll() {
  digitalWrite(AH, LOW);
  digitalWrite(AL, LOW);
  digitalWrite(BH, LOW);
  digitalWrite(BL, LOW);
  digitalWrite(CH, LOW);
  digitalWrite(CL, LOW);
}
