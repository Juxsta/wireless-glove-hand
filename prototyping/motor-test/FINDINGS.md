# BLDC Closed-Loop Control — Findings

**Date:** 2026-05-06
**Author:** Eric Reyes
**Scope:** Bring up encoder-feedback closed-loop control for the 2204 BLDC motor on the DRV8300 driver, as part of the wireless glove → robotic arm pipeline.

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
