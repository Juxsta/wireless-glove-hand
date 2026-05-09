# BLDC Motor Control — Findings

**Dates:** 2026-05-06 → 2026-05-07
**Author:** Eric Reyes
**Scope:** Bring up motor control for the 2204 BLDC + DRV8300 + (optional) AS5600 stack at each finger joint of the wireless-glove → arm pipeline.

> **TL;DR (decision adopted 2026-05-07):** the production firmware uses
> `SimpleFOC angle_openloop`, NO encoder, NO current sensing. A 20:1 gearbox
> at each joint absorbs slip. ESP-NOW links the glove flex sensor to the
> motor. See `prototyping/flex-motor-test/` for the production sketches.
> The closed-loop FOC investigation that occupies most of this document is
> kept as the trail-of-evidence that led to that decision.

## Goal

Verify whether the existing BLDC + custom DRV8300 driver + AS5600 encoder stack can deliver position-accurate joint control suitable for mirroring glove inputs on a robotic arm. Outcome decides whether to continue with BLDC or commit to commercial servos for the demo.

## Hardware Under Test

| Item | Detail |
|---|---|
| MCU | ESP32 DevKit v1 (Espressif) |
| Driver | Custom DRV8300 PCB, 6-PWM topology (high+low side per phase) |
| Motor | 2204 BLDC, 7 pole pairs (verified) |
| Encoder | AS5600 magnetic absolute, I²C (SDA=21, SCL=22) |
| Current sense | **None** |
| Power | 12 V bench supply |
| Driver pin map | AH=5, AL=17, BH=16, BL=4, CH=2, CL=15 |

## Software Stack (final working config)

- PlatformIO platform: `pioarduino/platform-espressif32@55.03.38-1` (arduino-esp32 v3.3.8 / ESP-IDF v5.5.4)
- SimpleFOC 2.4.0
- AS5600 access via SimpleFOC's `MagneticSensorI2C(AS5600_I2C)`

## Methodology

Three test programs in `prototyping/`:

1. **`closed-loop-test/`** — hand-rolled 6-step commutation + AS5600 polling. No FOC library. Used to validate the driver hardware path independently.
2. **`motor-test/`** — SimpleFOC-based, full FOC with `initFOC()` alignment. Primary platform for closed-loop work.
3. **`encoder-test/`** — AS5600-only sanity check.

Each test was driven over USB-serial: type a target angle (degrees), motor responds.

## What Works

| Capability | Status |
|---|---|
| AS5600 reads (magnet detect, raw angle, unwrap) | ✅ Reliable |
| DRV8300 6-PWM driving (hand-rolled commutation) | ✅ Motor spins, encoder confirms motion |
| SimpleFOC `initFOC()` alignment | ✅ PP check passes (7 pole pairs), sensor direction detected, electrical zero found |
| SimpleFOC `velocity_openloop` / `angle_openloop` | ✅ Smooth open-loop drive |
| **SimpleFOC closed-loop angle control** | ✅ **Functional** (after fix described below) |

## The Core Bug We Found

Initially every closed-loop mode (`angle`, `velocity`, `torque`) **locked the rotor in place** despite SimpleFOC reporting `Uq = 6 V` being commanded. Open-loop modes worked fine, isolating the issue to the encoder→FOC modulation path.

**Root cause:** `motor.initFOC()`'s automatic `sensor_direction` detection consistently returned the wrong sign on this driver+motor combination. With the wrong sign, the FOC modulator applies `Uq` voltage at an electrical angle that aligns with the rotor instead of leading it by 90° → net torque ≈ 0 → rotor twitches and locks.

**Fix:** Force `motor.sensor_direction = Direction::CW;` *before* `initFOC()`. SimpleFOC then re-aligns `zero_electric_angle` for the forced direction (recalibrates from 5.41 → 0.91 rad, completely different value) and the motor responds correctly. Inverting `sensor_direction` *after* `initFOC()` does NOT work — the offset must be re-derived for the new sign.

A secondary toolchain issue compounded this: arduino-esp32 v2.x + SimpleFOC 2.3.x has known compatibility issues for 6-PWM on ESP32 and was upgraded to v3.x + SimpleFOC 2.4.0.

## Closed-Loop Behavior Once Working

Position commands `T<deg>` over serial produce real motor motion:

| Test | Result |
|---|---|
| Boot rest | Rotor parks at last alignment position |
| Small command (e.g. T10, T30, T60 within one detent) | **Rotor does not move** at low voltage_limit |
| Large command (T90, T180, T-90) | Rotor breaks free, slews fast, overshoots, lands on a nearby pole detent |
| Steady-state error | **~50° (≈ one pole-pair angle, 360/7 = 51.4°)** |

## What's Limiting Accuracy

The steady-state error is a **physical limit** of voltage-mode FOC on a multi-pole motor without current sensing. The mechanism:

1. Rotor sits in a magnetic detent (cogging well). 7 pole pairs → 7 stable detents per mechanical revolution → one detent every **51.4° at the motor shaft**.
2. To exit a detent requires `Uq ≥ V_break` (empirically ~7-12 V on this motor, varies per boot alignment).
3. At `V_break`, applied torque is roughly constant, rotor accelerates with no proportional control of force.
4. By the time rotor reaches target, kinetic energy is too high — overshoots into next detent.
5. Reducing `voltage_limit` below `V_break` → rotor cannot leave its current detent → no motion at all.

PID tuning experiments confirmed this — neither lowering `P_angle`, raising `voltage_limit`, slowing `velocity_limit`, nor adjusting `PID_velocity` could produce smooth fine-grained position holding. There is **no `voltage_limit` value where the rotor reliably moves AND tracks accurately**.

## Effect of a 20:1 Gearbox (planned for the joint)

The system will drive each finger joint through a **20:1 reduction gearbox**. This changes the picture:

| Quantity | At motor shaft | At joint (×1/20) |
|---|---|---|
| Cogging detent spacing | 51.4° | **2.57°** |
| Worst-case overshoot (one detent past target) | 51.4° | 2.57° |
| Worst-case overshoot we observed (two detents) | ~100° | ~5° |

So **the gearbox brings the cogging-induced position resolution to ~2.5° at the joint** — fine for finger pose mirroring (humans don't visually distinguish much below 5° of finger flexion).

### Empirical verification

We modified `motor-test` to accept commands in joint-frame degrees and scale internally by `GEAR_RATIO = 20.0`. Test runs (from rest, T5° joint command):

| Run | Motor moved to | Joint position | |Joint error| |
|---|---|---|---|
| A | +48.0° (one detent) | +2.4° | 2.6° |
| B | -2.4° (no breakout) | -0.12° | 5.12° |
| C | -2.5° (no breakout) | -0.13° | 5.13° |

When motion occurred (run A), the **observed 2.6° joint error matches the theoretical 2.57° cogging quantum** within sensor precision. The 20:1 ratio works as expected.

### The unsolved variability problem

In ~50% of boots, the post-`initFOC` alignment lands the rotor on a detent that even 11.5 V `Uq` cannot break. The motor either:
- Doesn't move at all in response to commands, OR
- Moves once on the first command, then locks on the new detent indefinitely

The variability comes from `initFOC()` parking the rotor at "electrical zero," which corresponds to a different *physical* detent each boot depending on encoder magnet alignment and rotor inertia at startup. Some detents are escapable at 11.5 V, others aren't. The DRV8300 board has no current sensing, so we can't differentiate "stalled" from "at target" — the controller just saturates `Uq` and waits.

This is acceptable for **continuous tracking** (a glove streaming target angles at 50 Hz keeps the integrator wound up enough to break detents on every cycle) but **fails for static "go to angle X" commands** unless the motor was already moving.

## What Would Fix It

Three paths, in order of effort:

1. **Add current sensing to the DRV8300 board.** Replace voltage-mode `Uq` command with current-mode (true torque) command. SimpleFOC supports this directly via `motor.torque_controller = TorqueControlType::foc_current` once the ADC channels are wired. Effort: hardware redesign + reflow. Probably 1-2 weeks.
2. **Implement trajectory generation in firmware.** Instead of step-target commands, the firmware ramps target through a smooth velocity profile so the rotor never builds up free-flight momentum. ~50 lines of code. Effort: 1-2 days. Will probably get to ~10° accuracy but is not a substitute for proper torque control.
3. **Switch motor.** A low-cogging gimbal motor (e.g. 4108, BGM4108) has much weaker detents and may give acceptable results with voltage-mode FOC. Effort: order + integrate.

## Decision-Relevant Summary

- The **wireless sensing + control pipeline (glove → ESP-NOW → control firmware)** is the project's core innovation; this is independent of motor choice.
- BLDC closed-loop control is **demonstrably functional** but **not accurate enough for finger-joint mirroring** without further hardware investment.
- **Servos provide guaranteed-good position accuracy out of the box** with no firmware-level torque control to develop. They are the lower-risk path to a working demo on the May 12 timeline.

## Files Produced This Session

- `prototyping/motor-test/src/main.cpp` — working closed-loop config (forced `Direction::CW`, voltage_limit=9V, P_angle=2)
- `prototyping/motor-test/platformio.ini` — pioarduino 55.03.38-1, SimpleFOC 2.3.4+
- `prototyping/closed-loop-test/` — hand-rolled 6-step + AS5600 unwrap; useful as a "no-library" baseline

---

# Update — 2026-05-07

After another full session of trying to make closed-loop FOC reliable on this rig, we **abandoned the closed-loop approach** in favor of `angle_openloop` + the 20:1 gearbox. The closed-loop "cogging" diagnosis from 2026-05-06 was ultimately a *symptom*, not a root cause. The actual reasons closed-loop kept failing turned out to be a stack of hardware and software faults that compounded.

## The faults we discovered (in the order that mattered)

### 1. GPIO 13 on the ESP32 was dead
Single biggest issue, discovered late via scope inspection. With `CL` (phase C low-side) wired to GPIO 13, **only 2 of 3 phases were ever switching**. Every test from May 6 — closed-loop, open-loop, hand-rolled — was running on a 2-phase motor that physically cannot make a complete electrical revolution. This invalidated all our earlier tuning.

**Fix:** rewired CL from GPIO 13 → **GPIO 18**. After this, even the basic open-loop spin started working.

### 2. AS5600 encoder was unreliable
Multiple intermittent failures during the session:
- I2C cable bursts of `ESP_ERR_INVALID_STATE` errors then quiet zeros
- Magnet got knocked off the rotor at one point
- Even when reading, often returned exact `0.0°` (sensor present on bus but reading garbage)

For closed-loop FOC, the encoder feeds directly into the **Park transform**:
```
electrical_angle = pole_pairs × shaft_angle - zero_electric_angle
```
A bad `shaft_angle` makes `electrical_angle` wrong, which means **`Uq` is applied at the wrong axis** (mix of q-axis "torque" and d-axis "flux"). Anywhere the axis is wrong, electrical power becomes resistive heating instead of mechanical work — exactly matching the "motor stuck and getting hot" behavior we kept fighting.

### 3. Pole pair count was empirically ambiguous
SimpleFOC's `initFOC()` PP-check gave inconsistent estimates (`14.68` with software PP=7; `9.97` with software PP=14). Spec sheet says 7 (12N14P). Likely due to test conditions (cogging, partial settling between test points) skewing the estimator. We never got a confident empirical confirmation.

### 4. Hall current sensors weren't actually reading phase current
A diagnostic firmware that drove each phase pair individually and read all three Hall ADCs showed **<25 mA of differential signal** during 1.6 A test injections. The Hall sensors' mounting or wiring isn't where actual phase current flows. Without working current sense, FOC's automatic CS-polarity alignment failed nondeterministically — explaining why `MOT:Success: 2` happened only once across many boots.

## Why open-loop sidesteps all of it

| Failure mode | velocity_openloop | angle_openloop | closed-loop FOC |
|---|---|---|---|
| Bad encoder I2C reads | doesn't care | doesn't care | **fatal** |
| Magnet displaced | doesn't care | doesn't care | **fatal** |
| Wrong PP | minor inefficiency | minor inefficiency | **fatal** (Park transform misaligned) |
| Wrong sensor_direction | doesn't care | doesn't care | **fatal** (torque applied opposite to motion) |
| Wrong zero_electric_angle | doesn't care | doesn't care | **fatal** (Uq on flux axis = heat) |
| CS polarity wrong | doesn't care | doesn't care | **fatal** (init fails) |
| One phase pin dead | **fatal** | **fatal** | **fatal** |

In open-loop modes SimpleFOC just synthesizes a rotating magnetic field at a commanded rate. The rotor's permanent magnets follow the field by physics. There is **no Park transform**, no electrical-angle estimation, no feedback loop — therefore none of the closed-loop failure modes apply.

The price: we don't *measure* the rotor. `motor.shaft_angle` in the firmware is the **commanded** position (an integrator state), not measured. If the rotor slips a pole-pair (51° at the motor), we never know. **The 20:1 gearbox absorbs this**: a one-pole-pair slip is `51°/20 = 2.5°` at the joint — below human visual perception for a finger-pose mimic.

## Production architecture (committed 2026-05-07)

Lives in `prototyping/flex-motor-test/`. Two PIO envs each for the glove and motor side:

| Env | File | Purpose |
|---|---|---|
| `transmitter` | `transmitter.cpp` | Glove side, **calibration UI** (`C0` flat, `C90` bent, `P` print) |
| `transmitter_demo` | `transmitter_demo.cpp` | Glove side, hardcoded calibration constants, no UI — for demo |
| `receiver_angle` | `receiver_angle.cpp` | Motor side, **calibration UI** (`T<deg>` manual, `M0`/`M90` capture, `X` exit) |
| `receiver_angle_demo` | `receiver_angle_demo.cpp` | Motor side, hardcoded joint bounds, no UI — for demo |

Pipeline:
1. **Glove ESP32** reads flex sensor (analog GPIO 34), low-pass filters it (EMA α=0.2, ~80ms TC).
2. Maps raw → joint angle `0..90°` via two-point linear calibration (`CAL_RAW_AT_0`, `CAL_RAW_AT_90`).
3. Sends joint angle over **ESP-NOW** at 50 Hz to receiver.
4. **Motor ESP32** receives angle, applies a second EMA (α=0.4) for additional smoothing.
5. Maps glove angle to motor `target_rad` via interpolation between captured `M0_motor_rad` / `M90_motor_rad` (motor-side calibration of the joint's mechanical bounds).
6. Drives motor in `MotionControlType::angle_openloop`. Motor pin map: `AH=25 AL=26 BH=27 BL=14 CH=12 CL=18`.

### Adaptive `velocity_limit`
Static `velocity_limit = 20 rad/s` worked for slow tracking but lagged on quick gestures. We added an adaptive scheme:
```
desired_vlim = (rate_of_glove_change × GEAR_RATIO) × HEADROOM
motor.velocity_limit = clamp(desired_vlim, 30 rad/s, 250 rad/s)
```
- `HEADROOM = 2.0×` (so we can catch up after any phase lag)
- Updated every 50 ms, with a further LPF (α=0.4) on the vlim value itself
- Floor 30 rad/s ≈ 86°/s joint (idle, low-noise)
- Ceiling 250 rad/s ≈ 716°/s joint (snap motion, near 12V/KV=220 mechanical max)

### Calibration workflow (cal builds)
On the glove (TX): hold finger flat → `C0`, fully bent → `C90`, then `P` to verify.
On the motor (RX), with mechanical assembly attached:
- `T<deg>` to manually drive motor, find motor angle that puts joint in "finger straight" position → `M0`
- Repeat for "finger fully bent" position → `M90`
- `X` to exit manual override; system tracks glove from then on.

For the demo build: bake the captured constants into the `_demo.cpp` files and reflash.

## Lessons (for the report and future-us)

1. **Don't trust an encoder that's "responding"** without verifying the readings actually change. AS5600 happily ACKs on I2C without a magnet present and returns 0.
2. **Verify driver outputs at the gate pins, not just at the firmware command layer.** A dead GPIO is invisible to all software-level diagnostics.
3. **Open-loop drag-the-rotor-with-the-field isn't a hack** — it's the appropriate control mode when (a) the load is light, (b) absolute position can drift a few degrees, and (c) feedback infrastructure is too brittle to be the foundation of the demo. The 20:1 gearbox effectively turns the BLDC + open-loop combo into a stepper-with-microstepping in this application.
4. **Symptoms ≠ causes.** The "cogging detents" framing on May 6 was wrong; we were really seeing FOC misalignment producing no torque (just heat). On a properly-aligned closed-loop, even this motor's cogging is escapable at modest Uq.
