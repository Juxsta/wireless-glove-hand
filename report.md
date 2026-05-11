# Engineering Report — Wireless Glove → Robotic Hand

**Project:** EE198A/B Senior Project, San Jose State University, Spring 2026
**Authors:** Antonio Rojas, Raul Hernandez-Solis, Matthew Men, Eric Reyes
**Advisor:** Dr. Birsen Sirkeci
**Report compiled:** 2026-05-11 (for the May symposium)

---

## 1. Summary

A wearable glove with two flex sensors per finger streams joint angles over ESP-NOW at 50 Hz to an ESP32 hand controller. The hand drives 2204 BLDC motors through Raul-designed DRV8300 6-PWM gate drivers and **20:1 cycloidal-drive reduction gearboxes**, one per joint. Motor control runs SimpleFOC's `angle_openloop` mode — no encoder, no current sensing in the loop. Calibration values persist in NVS so the system boots ready to use.

The motor-control architecture that the team eventually shipped is a deliberate down-scoping from the original closed-loop FOC plan. The story of why is the bulk of this report.

---

## 2. What was originally planned

| Subsystem | Original plan |
|---|---|
| Glove sensors | MLX90395 Hall-effect, 3 per finger (MCP, PIP, DIP), SPI |
| Wireless link | ESP-NOW or BLE — under team discussion |
| Hand motor control | Closed-loop FOC with AS5600 magnetic encoder per joint |
| Motor driver | DRV8300 custom PCB with inline current sensing (ACS37042) |
| Mechanical | 3D-printed PLA/PETG hand; 2204 BLDC at each joint, direct-drive |
| Position accuracy goal | ±5° at each joint |

---

## 3. What changed during the project

### 3.1 Sensor pivot: MLX90395 → flex sensors (April 2026)
The original Hall-effect-magnet approach required precise magnet placement, SPI bus discipline across three CS lines per finger, and per-axis calibration to convert magnetic vector to joint angle via `atan2()`. The team determined this was too complex to land reliably in the remaining time, and switched to commodity resistive flex sensors with a simple two-point linear calibration. **Trade-off:** lower theoretical resolution, but vastly more robust and standard signal-conditioning. Net positive for project risk.

### 3.2 Gearbox change: direct-drive → 20:1 cycloidal reduction
Direct-driving a finger joint with a 2204 BLDC at typical voltages (12 V, ~2 A) produces ~0.03 N·m of torque — borderline for a 3D-printed finger with even modest static friction. A 20:1 reduction gives ~0.6 N·m at the joint, which is comfortably above stall load. The team selected a 3D-printed cycloidal drive (lower backlash than planetary at this size, and tractable to print). **Crucial downstream consequence**: this 20:1 ratio is what made the eventual open-loop control choice viable.

### 3.3 Encoder removed for the symposium
This is the largest architectural change. Originally each joint would have run closed-loop FOC with an AS5600 magnetic rotary encoder providing rotor position to the controller. After extensive debugging (Section 4 below) and a stack of compounding faults, the team concluded that getting closed-loop FOC reliable on this hardware would consume more time than remained before the symposium. Open-loop position control was adopted as a deliberate trade.

**What we give up:**
- True closed-loop disturbance rejection
- Absolute position knowledge
- Self-correcting drift

**What absorbs the loss:**
- The 20:1 cycloidal reduction makes a one-pole-pair motor slip (51° at the motor) into a ~2.5° error at the joint — below human visual perception for finger pose mimicry
- The glove streams continuously at 50 Hz, so the open-loop integrator never sits still long enough to accumulate large errors before the next command arrives
- The system supports a fast (~30 s) recalibration if the joints visibly drift

---

## 4. The closed-loop FOC investigation

Two full work sessions (2026-05-06 and 2026-05-07) were spent attempting to make the encoder + FOC stack work. The chronological story, with the cause finally identified at the end:

### 4.1 Symptoms observed
1. Motors stuck on cogging detents at modest Uq, then runaway at higher Uq
2. Motor windings getting **very hot** while not moving
3. `motor.initFOC()` succeeded inconsistently across power cycles — sometimes "MOT:Ready", sometimes "CS: Err too low current" or "PP check fail"
4. When motion did happen, the rotor would race past the commanded target by many turns
5. Empirical pole-pair check returned different values across runs (estimated 9.97 and 14.68 for a motor specced at 7)

### 4.2 What we *thought* was happening (and was wrong)
For most of the investigation we attributed the symptoms to **cogging torque** — the 2204's 7-pole-pair magnetic detents being too strong for voltage-mode FOC to escape with the available bus voltage (12 V). We made these attempts to push through it:

- Bumped `voltage_limit` from 3 V → 12 V (full bus)
- Tried PWM frequencies from 20 kHz to 50 kHz
- Pinned platform / library versions (arduino-esp32 v2 → v3, SimpleFOC 2.3.2 → 2.4.0)
- Toggled `Direction::CW` ↔ `Direction::CCW` to chase sign issues
- Added per-axis current sense alignment with `motor.torque_controller = foc_current`
- Bumped supply voltage to 19 V and current limit to 1.3 A
- Switched motor types twice in the BOM

None of these *fixed* the problem; some marginally changed which detents the motor parked on and how hot it got.

### 4.3 What was *actually* happening
On the second day, an oscilloscope on the gate driver inputs revealed that **GPIO 13 on the ESP32** — wired to phase C's low-side gate input — was not switching. The pin had failed silently. With one of six gate signals dead, only two of the motor's three phases could ever carry current; the motor was physically incapable of completing a full electrical revolution. Moving the wire from GPIO 13 to GPIO 18 was the first single change that allowed even basic open-loop spin to work consistently.

That was the headline fault, but several others were also present:

| Fault | Severity | Found |
|---|---|---|
| **GPIO 13 dead** — phase C low-side never switched | Catastrophic — every test poisoned | Day 2 |
| AS5600 I²C cable intermittent (loose wire → bursts of `ESP_ERR_INVALID_STATE`, then constant 0 reads) | Catastrophic for closed-loop — encoder fed bogus data to Park transform | Day 2 |
| Encoder magnet fell off rotor mid-session | Sudden loss of feedback | Day 2 |
| Pole-pair count empirically inconsistent (estimator returned 9.97 or 14.68 depending on test conditions) | High — FOC modulation needs exact PP | Both days |
| Hall current sensors weren't actually wrapping the phase wires | High — fail-fast condition for `initFOC` CS alignment | Day 2 |
| `initFOC()` sensor-direction / zero-electric-angle calibration non-deterministic across boots | High | Both days |

### 4.4 What this meant for the closed-loop math
FOC works by computing an electrical angle (rotor's instantaneous electrical position) and applying voltage on the q-axis — perpendicular to the rotor magnet — to produce torque. The compute is

```
electrical_angle = pole_pairs × shaft_angle − zero_electric_angle
```

When `shaft_angle` is corrupt (encoder garbage), or `pole_pairs` is wrong, or `zero_electric_angle` is wrong, the synthesized voltage no longer points at the q-axis. It rotates onto the **d-axis** instead — the flux axis — which produces magnetic flux but **no torque**:

| `θ_err` (axis error) | Torque produced | Flux produced (= heat) |
|---:|---:|---:|
| 0° | 100% | 0% |
| 30° | 87% | 50% |
| 60° | 50% | 87% |
| **90°** | **0%** | **100%** |
| 180° | -100% (reverse!) | 0% |

This explains every symptom we saw. The motor sat in detents not because cogging was inescapable but because **the applied voltage was producing flux instead of torque** — all energy went into resistive heating of the windings. At 9 V across the motor's 2.3 Ω phase resistance, that's `9² / 2.3 ≈ 35 W` of pure heat — hence the "burn your finger" surface temperatures.

The "cogging" diagnosis was a symptom-level explanation. The actual root cause was alignment failure stacked on top of a dead GPIO.

### 4.5 Why open-loop sidesteps every one of these
The failure surface of open-loop is much smaller. There is no Park transform, no electrical-angle estimation, no PP-dependent math. SimpleFOC simply rotates the stator's magnetic field at a commanded rate; the rotor's permanent magnet follows by physics.

| Fault | velocity_openloop | angle_openloop | closed-loop FOC |
|---|---|---|---|
| Bad encoder reads | doesn't care | doesn't care | **fatal** (axis misalignment → heat) |
| Magnet displaced | doesn't care | doesn't care | **fatal** |
| Wrong pole-pair count | minor inefficiency | minor inefficiency | **fatal** (axis ratio wrong) |
| Wrong sensor_direction | doesn't care | doesn't care | **fatal** (reverse torque) |
| Wrong zero_electric_angle | doesn't care | doesn't care | **fatal** (torque → flux) |
| Current sense miswired | doesn't care | doesn't care | **fatal** (init fails) |
| One phase pin dead | **fatal** | **fatal** | **fatal** |

Open-loop only needs all three phases to be switching. With the GPIO 13 → 18 wiring fix, that condition is met, and the motor moves smoothly.

---

## 5. The production architecture (what is being demoed)

### 5.1 Pipeline
```
Flex sensor (16-sample avg)
  → EMA (α = 0.2, ~80 ms TC)
  → 2-point linear cal (per joint, persisted in NVS)
  → joint angle (0..90°)
  → ESP-NOW unicast, 50 Hz, 2-element int16 packet
                ───────────────────────────────────
                       receiver
  → EMA on incoming angle (α = 0.4)
  → linear interp from glove [0..90] to motor target_rad [M0..M90]
  → adaptive velocity_limit
  → SimpleFOC angle_openloop
  → DRV8300 gates → 2204 BLDC
  → 20:1 cycloidal drive
  → joint motion
```

### 5.2 Adaptive `velocity_limit`
Fixed `velocity_limit` produced two failure modes: low values felt sluggish on quick gestures, high values caused motor-rotor desync on small fine motions. The receiver computes the joint's instantaneous angular rate from the smoothed glove angle, scales it by the gear ratio, applies a 2× headroom multiplier, and clamps to [30, 250] rad/s motor frame. A light low-pass filter on the resulting `velocity_limit` value itself prevents jitter from the noisy derivative.

| Joint motion | Computed motor vlim | Joint slew |
|---|---|---|
| < 1°/s (idle / sensor noise) | 30 rad/s (floor) | ~86°/s |
| 100°/s (slow finger curl) | ~70 rad/s | ~200°/s |
| 300°/s (normal motion) | ~210 rad/s | ~600°/s |
| > 360°/s (snap motion) | 250 rad/s (cap) | ~720°/s |

### 5.3 Calibration & persistence
Both the glove and the hand store their calibration values (per-channel for the glove, per-joint for the hand) in the ESP32's NVS via the Arduino `Preferences` library. The same firmware acts as both the calibration and the demo build — calibration commands are always live, but absent any input the device just streams or follows the saved values.

### 5.4 Pinouts
| Function | GPIO |
|---|---|
| Glove flex 1 (proximal joint) | 34 (ADC1, input-only) |
| Glove flex 2 (distal joint) | 35 (ADC1, input-only) |
| Hand motor 1 AH / AL / BH / BL / CH / CL | 25, 26, 27, 14, 12, 18 |
| Hand motor 2 AH / AL / BH / BL / CH / CL | 4, 16, 17, 19, 21, 23 |

GPIO 13 is **not** used on the hand — it failed on the specific ESP32 board the team has been developing with. The bring-up procedure for any future board should scope each gate pin during a known-good test pattern before assuming all six work.

---

## 6. Known limitations of the shipped system

1. **Positional drift over time.** With no absolute feedback, errors that occur (pole-pair slip during direction reversal, mechanical stiction transient, etc.) accumulate. The 20:1 ratio makes each event small (~2.5° at the joint) but they can stack across many gestures. A re-zero takes about 30 seconds and is documented in the README.

2. **No active disturbance rejection.** If a finger of the robotic hand is physically resisted, the commanded position will keep advancing while the rotor stalls. The mechanism may quietly desync.

3. **Backlash from the cycloidal drive.** Even an open-loop-tracked rotor has backlash on direction reversal at the joint. Empirically <1° but not measured rigorously.

4. **Calibration is per-finger, two-point linear.** Real flex-sensor response is mildly non-linear; mid-range angles can be off by a few degrees from the user's actual joint angle. Acceptable for visual mimicry; not acceptable for, e.g., teleoperation requiring force feedback.

5. **One ESP32 board has a dead GPIO 13.** Captured here so future readers wiring a fresh board don't avoid GPIO 13 on principle — it's specific to that unit.

---

## 7. What we would do differently / next steps

### Near term (to fix on the current hardware)
- Replace the AS5600 I²C cable with a proper shielded JST connector. The intermittent reads were the worst single contributor to closed-loop failures.
- Confirm the AS5600 magnet is mechanically captured to the rotor in a way that survives bench handling. We lost the magnet once during a session.
- Verify gate signals on all six DRV8300 inputs with an oscilloscope as part of bring-up, not as a debugging step late in the process.

### Architectural improvements (a future iteration)
- **Current sensing wired correctly through the phase wires** (the Hall-effect sensors on the existing PCB didn't actually wrap the phase conductors — we verified this with a per-phase injection test). Working current sense would let `motor.torque_controller = TorqueControlType::foc_current`, giving proper torque control rather than voltage-mode.
- **Encoder validation built into firmware.** A boot-time self-test that verifies the encoder is reading sensible values before initFOC runs would have caught the magnet-loose case immediately.
- **A simpler / cheaper position sensor.** A precision potentiometer at the cycloidal-drive output side (post-gearbox) would give absolute joint position with no concerns about magnetic coupling, electrical noise, or I²C reliability — and avoid the entire pole-pair / electrical-angle alignment problem because the FOC could run open-loop on the motor while position is closed on the joint side.

---

## 8. Code layout
- `firmware/glove/` — production transmitter (two-flex, NVS calibration)
- `firmware/hand/` — production receiver (two-motor, NVS calibration, adaptive slew)
- `prototyping/` — kept as historical artifacts; not part of the symposium build

---

## 9. Lessons (for the report and for future-us)

1. **Verify hardware before tuning software.** Half the debugging time was spent tuning a control loop whose underlying gate signals were never reaching the motor. A 60-second scope check on each gate input at the start would have changed the trajectory of the project.

2. **An encoder that responds on the bus is not an encoder that's working.** AS5600 cheerfully ACKs without a magnet present and returns 0 angle. Always check for actual rotation by hand-spinning the rotor and watching the reported angle move.

3. **In closed-loop FOC, *worse* than no encoder is a wrong encoder.** With no feedback, the worst case is uncontrolled drift. With wrong feedback fed into a Park transform, applied voltage can land entirely on the d-axis and the motor becomes a resistive heater. That's how we kept burning fingers.

4. **Open-loop drag-the-rotor-with-the-field is not a hack** when (a) the load is light, (b) gearbox reduction absorbs slip, and (c) the alternative is fragility. For this application it was the right call.

5. **Reduction ratios are the great equalizer.** A 20:1 cycloidal drive converts a 51° motor pole-pair slip into 2.5° at the joint — that's the difference between "obviously broken" and "below human perception." Mechanical design choices upstream can rescue control choices downstream.

---

## Appendix A: Diagnostic Data

The following are verbatim serial transcripts captured during the 2026-05-06 / 2026-05-07 debug sessions. They are the concrete evidence behind the conclusions in Section 4.

### A.1 Hall current-sense diagnostic

To verify whether the on-board Hall current sensors actually responded to phase current, the hand-rolled `closed-loop-test/` firmware was modified to drive each of the six commutation phase-pairs in sequence at 29 % duty (`duty = 76/255`) for 80 ms each, then read all three Hall ADCs and convert to amps using the spec'd 200 mV/A sensitivity:

```
=== CURRENT-SENSE DIAGNOSTIC ===
Zero bias counts: A=1771  B=1769  C=1896  (V: 1.427 1.425 1.528)
Duty=76/255 (29%), expected steady-state I ~ 1.6A

Step    rawA  rawB  rawC    Ia(A)   Ib(A)   Ic(A)
A->B    1770  1767  1896    -0.00   -0.01   +0.00
A->C    1770  1768  1897    -0.00   -0.00   +0.00
B->C    1773  1767  1902    +0.01   -0.01   +0.02
B->A    1771  1768  1896    +0.00   -0.00   +0.00
C->A    1772  1766  1896    +0.00   -0.01   +0.00
C->B    1773  1768  1896    +0.01   -0.00   +0.00
```

The motor was confirmed energized during this run (independently driven into rotation in open-loop). The Hall outputs do not register the expected ~1.6 A flow on **any** channel — variations of 1–6 ADC counts (≈ 5–25 mA at 200 mV/A) are at the noise floor. **The Hall sensors are not physically wrapping the phase conductors** — likely mounted at a position in the PCB that doesn't carry the phase return current. This is the direct cause of the repeated `CS: Err too low current, rise voltage!` alignment failures from `motor.initFOC()`.

### A.2 AS5600 encoder behavior

**Normal boot, magnet present (early in the session):**
```
AS5600 connected. magnet detected=1 tooWeak=1 tooStrong=0 rawAngle=3954
```
(The `tooWeak=1` flag is a quirk of the AS5600 Arduino library's reporting of the AGC bit and is not a functional warning when other reads are stable.)

Open-loop spin (with motor energized via F mode at 50 % duty) produced raw values cycling continuously through the full 0..4095 range:
```
raw=1420 → 3156 → 831 → 2695 → 237 → 1972 → 1677 → 3442 → ...
```
demonstrating the encoder was operational and tracking rotation.

**Later in the same session, after rewiring:**
```
=== boot/initFOC ===
sensor_raw=0.0 deg
sensor_raw=0.0 deg
...
[3723][E][esp32-hal-i2c-ng.c:372] i2cWriteReadNonStop(): i2c_master_transmit_receive failed: [259] ESP_ERR_INVALID_STATE
[3735][E][Wire.cpp:520] requestFrom(): i2cWriteReadNonStop returned Error 259
[3742][E][esp32-hal-i2c-ng.c:372] i2cWriteReadNonStop(): ... failed: [259] ESP_ERR_INVALID_STATE
... (bursts of identical errors continue) ...
sensor_raw=0.0 deg          # back to silent zero after bus errors clear
```
This is the signature of a marginally seated I²C cable: bursts of bus-fault errors followed by the slave (AS5600) responding to its address but returning zero data. With manual rotation of the rotor during this period the reported `sensor_raw` did not change — the encoder was effectively dead from the firmware's perspective.

### A.3 SimpleFOC `initFOC()` alignment outcomes

The same firmware was rebooted multiple times without code changes between attempts. Observed outcomes:

```
# Run 1 — auto-detection
MOT:Align sensor.
MOT:sensor dir: CCW
WARN-MOT:PP check: fail - est. pp: 14.68
Skip dir calib.
MOT:Zero elec. angle: 4.83
MOT:Align current sense.
CS: Err too low current, rise voltage!
ERR-MOT:Align error!
ERR-MOT:Init FOC fail
initFOC = 0

# Run 2 — same config, different boot
MOT:Align sensor.
MOT:Skip dir calib.
MOT:Zero elec. angle: 5.41
MOT:Align current sense.
CS: Switch A-C
CS: Switch B-C
MOT:Success: 2
MOT:Ready.
initFOC = 1

# Run 3 — PP forced to 14
WARN-MOT:PP check: fail - est. pp: 9.97
ERR-MOT:Init FOC fail

# Run 4 — direction forced to CCW
MOT:Align current sense.
CS: Inv A
CS: Err align B
ERR-MOT:Align error!
ERR-MOT:Init FOC fail
```

The PP-check empirically returned `14.68` and `9.97` for the same physical motor depending on conditions. Spec sheet for the 2804 says 12N14P → 7 pole pairs. The estimator is sensitive to whether the rotor settles cleanly at each electrical test point — cogging interferes.

### A.4 Motor runaway under closed-loop FOC

After one of the successful `initFOC = 1` runs, with `MotionControlType::angle`, the position controller commanded `Uq=-12V` (saturated) continuously while the rotor ran away in the positive direction:

```
target=0.0 deg  shaft=343.4 deg     vel=4.01 rad/s    Uq=-12.00
target=0.0 deg  shaft=343.4 deg     vel=2.17 rad/s    Uq=-12.00
... rotor at ~343° with Uq sat ...
                shaft=8099.2 deg    vel=158.15        Uq=-12.00
                shaft=11726.5 deg   vel=160.26        Uq=-12.00
                shaft=15360.2 deg   vel=159.61        Uq=-12.00
                shaft=156625.5 deg  vel=138.94        Uq=-12.00
```

`shaft_angle` accumulated from 0° to over 156,000° (≈430 mechanical revolutions) at a steady ~155 rad/s motor speed. The controller could not catch up because applying `Uq` at the wrong electrical axis (consequence of bad encoder readings) was producing motion in the same direction as the error growth, not against it. This is the closed-loop sign-error failure mode described in Section 4.4.

### A.5 ESP-NOW link characterization

After flashing the production hand firmware (post-rewire), the link came up immediately:

```
=== HAND RX ===
RX MAC: B0:CB:D8:8B:55:04
Motor 0 ready
Motor 1 ready
CAL: J0  M0=0.00 rad (0°)  M90=31.42 rad (1800°)
     J1  M0=0.00 rad (0°)  M90=31.42 rad (1800°)
Ready. Cmds: T1<d>|T2<d> | X | M0 | M90 | S save | R reset | P print
J0 g=9  s=9.0  tgt=3.14 vlim=15 manual=0  |  J1 g=0  s=0.0  tgt=0.00 vlim=15 manual=0  |  link=OK
J0 g=21 s=21.0 tgt=7.33 vlim=15 manual=0  |  J1 g=16 s=16.0 tgt=5.59 vlim=15 manual=0  |  link=OK
J0 g=21 s=21.0 tgt=7.33 vlim=15 manual=0  |  J1 g=33 s=33.0 tgt=11.52 vlim=67 manual=0  |  link=OK
J0 g=14 s=14.0 tgt=4.89 vlim=27 manual=0  |  J1 g=52 s=52.0 tgt=18.15 vlim=54 manual=0  |  link=OK
```

The transmitter was paired (MAC hardcoded), update rate 50 Hz, `link=OK` reported whenever a packet arrived in the last 500 ms. The `vlim` column shows the adaptive velocity limit responding to gesture speed: idle joints sit at the floor of 15 rad/s; J1's brief jump from 16° → 33° → 52° pushed vlim transiently to 67 rad/s before settling back as the motion completed.

### A.6 GPIO 13 (CL pin) failure

Before the GPIO 13 → GPIO 18 rewire, with the raw PWM force-test firmware (deliberately driving GPIO 25, 27, **and 13** at 50 % duty), oscilloscope verification (user-confirmed) showed PWM present on GPIO 25 and GPIO 27 but **no output** on GPIO 13. The pin was responding to `ledcAttach()` in software (returned non-zero success), and `ledcWrite(13, 128)` was being executed by the loop, but no signal appeared at the pin. After rewiring CL to GPIO 18 the open-loop spin-test produced clean rotation immediately and consistently — confirming GPIO 13 was the dead pin and not any of the other 5 driver inputs. This was the largest single fault in the closed-loop debug history.
