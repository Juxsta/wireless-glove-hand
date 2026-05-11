# Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**EE198A/B Senior Project — San Jose State University**
**Department of Electrical Engineering**
**Spring 2026**

## Team

| Name | Student ID | Role |
|------|-----------|------|
| Antonio Rojas | 014974063 | Robotic hand hardware / 3D printing |
| Raul Hernandez-Solis | 016319693 | Control glove hardware / motor driver PCB design |
| Matthew Men | 016601806 | Wireless communication / motor sourcing |
| Eric Reyes | 011351067 | 3D printing / CAD / firmware |

**Advisor:** Dr. Birsen Sirkeci

## Abstract

A wearable glove with **flex sensors** at each finger joint wirelessly commands a 3D-printed robotic hand whose joints are driven by 2204 BLDC motors through custom DRV8300 drivers and **cycloidal-drive reduction gearboxes (20:1)**. ESP-NOW links the two sides at 50 Hz, end-to-end.

The motor control runs **SimpleFOC's `angle_openloop` mode** — no encoder, no current sensing in the loop. This was a deliberate trade chosen for symposium reliability after the team's encoder-based closed-loop FOC stack proved too fragile to land in time. The 20:1 cycloidal reduction absorbs any pole-pair slip into a small, sub-perceptible angular error at the joint. The trade-off is that there is no absolute position feedback, so the joints drift slowly over time and need periodic re-zeroing if they accumulate error.

## System Architecture

```
┌──────────────────────────┐   ESP-NOW (50 Hz)   ┌──────────────────────────────┐
│        Control Glove      │  ────────────────▶  │        Robotic Hand          │
│                           │   2 angles/packet   │                              │
│  • 2 flex sensors per     │                     │  • 2204 BLDC motors          │
│    finger (proximal +     │                     │    (SimpleFOC, open-loop     │
│    distal joint segments) │                     │    angle control)            │
│  • Per-channel EMA filter │                     │  • DRV8300 custom drivers    │
│  • 2-point linear cal     │                     │  • Cycloidal drive 20:1      │
│    (C0/C90), persisted    │                     │  • Per-joint adaptive slew   │
│    in NVS                 │                     │  • Per-joint M0/M90 cal,     │
│  • ESP32 DevKit v1        │                     │    persisted in NVS          │
│                           │                     │  • ESP32 DevKit v1           │
└──────────────────────────┘                     └──────────────────────────────┘
```

### Data flow
```
Flex (ADC) → 16-sample avg → EMA → 2-point cal → angle [0..90°]
                                                      │
                                        ESP-NOW unicast @ 50 Hz
                                                      │
                                                      ▼
                              RX → EMA → adaptive velocity_limit → linear interp
                                  (motor-frame target_rad between M0 and M90)
                                                      │
                                                      ▼
                            SimpleFOC angle_openloop → rotating field
                                                      │
                                              20:1 cycloidal drive
                                                      │
                                                      ▼
                                                 Joint motion
```

## Specifications

| Parameter | Value | Notes |
|---|---|---|
| Glove → joint latency | ~20–40 ms | One ESP-NOW hop + adaptive slew |
| Joint angle quantization | ~2.5° | Pole-pair slip / 20:1 gear ratio |
| Joint accuracy (relative) | within slip + cal error | Drifts slowly without encoder |
| Update rate (TX & RX) | 50 Hz | 20 ms period |
| Joint motion ceiling | ~7.5 rad/s (~430°/s) | Adaptive vlim cap, motor-frame 250 rad/s |
| Joint motion floor | ~1.5 rad/s (~86°/s) | Adaptive vlim floor, motor-frame 30 rad/s |
| Continuous runtime | > 30 min | Bench supply or 2S LiPo |
| Wireless range | > 2 m | Practical desktop |

## Project Structure

```
wireless-glove-hand/
├── docs/                       # Project documentation
│   ├── proposal/               # EE198A proposal, Gantt
│   ├── design/                 # Architecture, ADRs
│   ├── hardware/               # Schematics, BOM, CAD
│   └── references.md
├── firmware/                   # PlatformIO — production firmware
│   ├── glove/                  # 2-flex-sensor TX, NVS-persisted cal
│   └── hand/                   # 2-motor RX, NVS-persisted cal
├── prototyping/                # Stand-alone test sketches (historical)
│   ├── motor-test/             # SimpleFOC closed-loop investigation
│   ├── closed-loop-test/       # Hand-rolled 6-step commutation
│   ├── flex-motor-test/        # Single-joint glove-to-motor pipeline
│   ├── encoder-test/           # AS5600 read sanity check
│   ├── sensor-test/            # MLX90395 prototype (pivoted away from)
│   └── esp-now-test/           # Link bring-up
├── report.md                   # Consolidated engineering report
└── README.md
```

## Hardware

### Glove (transmitter)
- ESP32 DevKit v1
- 2× flex sensors per finger (proximal + distal joint segments)
- Pinout: flex inputs on GPIO 34, 35 (ADC1, input-only)

### Hand (receiver)
- ESP32 DevKit v1
- 2× 2204 BLDC motors, 7 pole pairs, 12 V supply
- 2× DRV8300 custom 6-PWM gate driver PCBs (Raul's design)
- 2× **cycloidal-drive gearboxes, 20:1 reduction** between motor and joint
- Pinouts:
  - Motor 1: AH=25 AL=26 BH=27 BL=14 CH=12 CL=18
  - Motor 2: AH=4  AL=16 BH=17 BL=19 CH=21 CL=23
- *No encoder, no current sensing in the active control loop* — see `report.md`

### Why no encoder?
Closed-loop FOC was extensively attempted (see `report.md` for the full investigation). The combination of an unreliable AS5600 I²C link, ambiguous pole-pair measurements, a non-deterministic SimpleFOC alignment routine, and one dead ESP32 GPIO produced FOC misalignment symptoms (motor stuck, very hot windings) that were repeatedly misdiagnosed as motor cogging. With no time to re-architect the encoder hardware before the symposium, the team adopted `angle_openloop` motor control. The cycloidal drive's 20:1 ratio makes individual pole-pair slips (~51° motor) invisible at the joint (~2.5°). The cost is slow positional drift over time; periodic re-zeroing fixes it.

## Software

- PlatformIO platform: `pioarduino/platform-espressif32@55.03.38-1` (arduino-esp32 v3.3.8 / ESP-IDF v5.5.4)
- Library: SimpleFOC 2.4.0 (hand side)
- Calibration persisted via the Arduino `Preferences` (ESP32 NVS) library

## Bring-up

### Building & flashing

```bash
# Glove (transmitter)
cd firmware/glove
pio run -t upload --upload-port /dev/cu.usbserial-XXXX

# Hand (receiver)
cd firmware/hand
pio run -t upload --upload-port /dev/cu.usbserial-YYYY
```

The hand's MAC prints at boot. Paste it into `RECEIVER_MAC[]` at the top of `firmware/glove/src/main.cpp` once.

### Calibration

The same firmware runs in both "calibration" and "demo" modes — calibration commands are always live, and values persist in NVS so the system boots ready to use after a one-time setup.

**Glove**, in its serial monitor:
1. Hold the finger flat → `C0` (captures both flex sensors as 0°)
2. Bend the finger fully → `C90` (captures both as 90°)
3. `S` to save to NVS

**Hand**, with the mechanical assembly attached, in its serial monitor:
1. `T1<deg>` to manually drive motor 1, `T2<deg>` for motor 2 — adjust until both joints are physically at "finger straight" pose
2. `M0` (captures both motor positions as the "joint 0°" mapping)
3. Repeat at "fully bent" → `M90`
4. `X` to leave manual override
5. `S` to save to NVS

Once saved, both ESP32s boot ready to mirror the user's finger motion.

## Calibration & drift management for demos

- A fresh power-on reads the saved calibration; no UI interaction needed.
- After a long run the open-loop motor commanded position can drift from the physical reality. If it visibly desyncs, send `R` on the hand then re-do the `M0`/`M90` workflow (~30 seconds), or simply power-cycle and physically re-park the mechanism.
- The drift floor is set by the cycloidal drive's backlash + the motor's pole-pair slip on direction reversals (~2.5° per slip event, accumulates only if multiple slips happen in the same direction without correction).

## References

See [`docs/references.md`](docs/references.md) for full bibliography. The full engineering investigation that led to the current architecture is in [`report.md`](report.md).

## License

Academic project — San Jose State University, Department of Electrical Engineering, EE198A/B.
