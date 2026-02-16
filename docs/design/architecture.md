---



date: 2026-02-05
author: Eric Reyes
status: Approved
---

# Architecture Decision Document
## Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**Version:** 1.0  
**Author:** Eric Reyes  
**Date:** February 5, 2026  
**Status:** ✅ Approved for PoC Implementation

---

## 1. System Context

### 1.1 PoC Scope (Single Finger Demo)

For proof of concept, we're building a **single finger** (index finger) with 4 joints to validate the core FOC approach before scaling to full hand.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CONTROL GLOVE (PoC: 1 Finger)                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │ Flex Sensors│───▶│   ESP32     │───▶│  BLE Radio  │─────────┐   │
│  │  (4 total)  │    │  (ADC+App)  │    │ (Transmit)  │         │   │
│  └─────────────┘    └─────────────┘    └─────────────┘         │   │
└─────────────────────────────────────────────────────────────────│───┘
                                                                  │
                              Bluetooth LE                        │
                              (~30Hz data)                        │
                                                                  │
┌─────────────────────────────────────────────────────────────────│───┐
│                    ROBOTIC HAND (PoC: 1 Finger)                 │   │
│                                                                 ▼   │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │  BLE Radio  │───▶│   ESP32     │───▶│ FOC Control │             │
│  │  (Receive)  │    │   (Main)    │    │   Loop      │             │
│  └─────────────┘    └─────────────┘    └──────┬──────┘             │
│                                               │                     │
│     ┌─────────────┬─────────────┬─────────────┼─────────────┐      │
│     ▼             ▼             ▼             ▼             │      │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐           │      │
│  │DRV8302│    │DRV8302│    │DRV8302│    │DRV8302│           │      │
│  │  #1  │     │  #2  │     │  #3  │     │  #4  │           │      │
│  └──┬───┘     └──┬───┘     └──┬───┘     └──┬───┘           │      │
│     │            │            │            │               │      │
│  ┌──┴───┐     ┌──┴───┐     ┌──┴───┐     ┌──┴───┐          │      │
│  │ BLDC │     │ BLDC │     │ BLDC │     │ BLDC │          │      │
│  │ MCP  │     │ PIP  │     │ DIP  │     │ TIP  │          │      │
│  └──────┘     └──────┘     └──────┘     └──────┘          │      │
│                                                                     │
│  ┌─────────────┐                                                   │
│  │   Battery   │ 2S LiPo (7.4V) for PoC                           │
│  └─────────────┘                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Data Flow

```
Flex Sensor → ADC (12-bit) → Angle Calculation → BLE Packet
    │                                                │
    │ 50Hz sample                                    │ 30Hz transmit
    ▼                                                ▼
[0-4095] → [0-90°] ────────────────────────▶ [4 bytes]
                                                     │
                                                     │ BLE notify
                                                     ▼
                                            BLE Receive → Unpack
                                                     │
                                                     │ 1kHz control loop
                                                     ▼
                                            Position Command → FOC
                                                     │
                                                     ▼
                                            Motor Position (AS5600)
```

---

## 2. Architectural Decisions (All Finalized)

### ADR-001: MCU Platform

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **ESP32-WROOM-32** (DevKit v1) |
| **Rationale** | Dual-core (FOC + BLE), built-in BLE, Arduino/PlatformIO support, $4 cost |
| **Quantity** | 2 (one glove, one hand) |

---

### ADR-002: FOC Library

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **SimpleFOC v2.3.x** |
| **Rationale** | Arduino-compatible, extensive documentation, active community, proven on ESP32 |
| **Control Mode** | Position control (angle mode) — no torque sensing needed for PoC |
| **Loop Rate** | Target 1kHz per motor, may reduce to 500Hz for 4 motors |

**Implementation Notes:**
- Use `motor.controller = MotionControlType::angle`
- PID tuning per motor: start with P=20, I=0, D=0.5
- Sensor: Magnetic encoder (AS5600) via I2C

---

### ADR-003: Motor Driver

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **DRV8302 Module** (budget choice) |
| **Rationale** | $10-12/ea vs $25 for SimpleFOC Shield; proven with SimpleFOC |
| **Quantity** | 4 for PoC (1 per joint) |
| **Specs** | 6-45V input, 15A peak, 3-phase BLDC |

**Wiring:**
- INHA/B/C → ESP32 PWM pins (6 pins per driver, 24 total for 4 motors)
- ENABLE → ESP32 GPIO
- Current sense pins optional for PoC (not using torque control)

**Pin Allocation (4 motors):**
| Motor | PWM A | PWM B | PWM C | Enable |
|-------|-------|-------|-------|--------|
| MCP   | 25    | 26    | 27    | 14     |
| PIP   | 16    | 17    | 18    | 15     |
| DIP   | 19    | 21    | 22    | 23     |
| TIP   | 32    | 33    | 13    | 12     |

---

### ADR-004: BLDC Motor Selection

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **Generic 2804 Gimbal Motor** (AliExpress) |
| **Specs** | 28mm diameter, 100-140Kv, ~0.08 Nm torque |
| **Price** | ~$8/ea (vs $20 for iPower branded) |
| **Quantity** | 5 (4 + 1 spare for testing) |
| **Supplier** | AliExpress (2-3 week lead time) — ORDER IMMEDIATELY |

**Rationale:** For PoC, quality variability is acceptable. If motion is jerky, upgrade to iPower GM2804 for final demo.

**Alternative (faster delivery):** Amazon generic 2804 at $12-15/ea if schedule is tight.

---

### ADR-005: Encoder Type

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **AS5600 Magnetic Encoder** |
| **Interface** | I2C (with TCA9548A multiplexer for 4 encoders) |
| **Resolution** | 12-bit (4096 positions/rev) — sufficient for ±1° accuracy |
| **Price** | ~$2-3/ea |
| **Quantity** | 5 (4 + 1 spare) |

**I2C Architecture:**
```
ESP32 I2C (GPIO 21 SDA, GPIO 22 SCL)
    │
    └── TCA9548A Multiplexer (addr 0x70)
            ├── Channel 0 → AS5600 (MCP joint)
            ├── Channel 1 → AS5600 (PIP joint)
            ├── Channel 2 → AS5600 (DIP joint)
            └── Channel 3 → AS5600 (TIP joint)
```

**Magnet:** 6x3mm diametric magnet epoxied to motor shaft.

---

### ADR-006: Flex Sensor Configuration (PoC)

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | **2 flex sensors for PoC** (MCP + PIP joints) |
| **Sensor** | Spectra Symbol 4.5" flex sensor |
| **Quantity** | 3 (2 + 1 spare) |
| **Price** | ~$8/ea |

**Rationale:** Full 4-sensors-per-finger is expensive ($32/finger). For PoC demo, 2 sensors showing clear bend tracking is sufficient. DIP/TIP can mirror PIP proportionally.

**Circuit:**
```
3.3V ─── Flex Sensor ───┬─── ADC Pin (GPIO 34, 35)
                        │
                       10kΩ
                        │
                       GND
```

**Mapping:**
- MCP sensor → controls MCP + DIP motors (coupled)
- PIP sensor → controls PIP + TIP motors (coupled)

---

### ADR-007: BLE Protocol Design

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Service UUID** | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` |
| **Characteristic UUID** | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |
| **MTU** | 23 bytes (default, no negotiation needed) |

**Packet Format (PoC - 6 bytes):**
```
Byte 0: Sequence number (0-255, for loss detection)
Byte 1: MCP angle (0-180 mapped to 0-255)
Byte 2: PIP angle (0-180 mapped to 0-255)
Byte 3: DIP angle (derived from PIP, or 0xFF = use coupling)
Byte 4: TIP angle (derived from PIP, or 0xFF = use coupling)
Byte 5: Checksum (XOR of bytes 0-4)
```

**Connection Parameters:**
- Connection interval: 15ms (minimum reliable)
- Notification rate: ~30Hz actual
- Reconnect timeout: 5 seconds

---

### ADR-008: Power Architecture (PoC)

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |

**Glove (Transmitter):**
- USB power from laptop for PoC development
- Future: 1S LiPo (3.7V 500mAh)

**Hand (Receiver + Motors):**
- **2S LiPo (7.4V 1000mAh)** for PoC
- Step-down to 5V for ESP32 (use AMS1117-5.0 or buck converter)
- Motors run directly from 7.4V (DRV8302 supports 6-45V)
- Estimated runtime: ~30-45 minutes under light load

**Why 2S instead of 3S:**
- Cheaper battery
- Lower risk of motor overheating
- Sufficient for demo torque
- Upgrade to 3S for full hand if needed

---

## 3. PoC Bill of Materials

| Component | Qty | Unit Cost | Total | Source | Lead Time |
|-----------|-----|-----------|-------|--------|-----------|
| ESP32 DevKit v1 | 2 | $4 | $8 | Amazon | 2 days |
| DRV8302 Module | 4 | $12 | $48 | Amazon/AliExpress | 3-14 days |
| 2804 BLDC Motor | 5 | $8 | $40 | AliExpress | 14-21 days |
| AS5600 Encoder | 5 | $3 | $15 | Amazon | 2 days |
| TCA9548A I2C Mux | 1 | $3 | $3 | Amazon | 2 days |
| Flex Sensor 4.5" | 3 | $8 | $24 | Amazon/SparkFun | 2-5 days |
| 6x3mm Diametric Magnet | 10 | $0.50 | $5 | Amazon | 2 days |
| 2S LiPo 1000mAh | 1 | $15 | $15 | Amazon | 2 days |
| AMS1117-5.0 Regulator | 2 | $1 | $2 | Amazon | 2 days |
| Prototype PCB/Wires | - | - | $15 | - | - |
| 3D Print Filament | 0.5kg | $12 | $12 | Available | - |
| **TOTAL** | | | **~$187** | | |

**Budget Status:** ✅ Under $200 target for PoC

---

## 4. Risk Analysis (PoC)

| Risk | Prob | Impact | Mitigation | Owner |
|------|------|--------|------------|-------|
| Motor quality variance | Med | Med | Order extra, test before assembly | Antonio |
| I2C encoder interference | Low | High | Shield wires, test early | Matthew |
| FOC tuning difficulty | Med | Med | Use SimpleFOC examples, community help | Group |
| 3D print tolerances | Med | Low | Iterate design, test fit | Antonio |
| BLE latency spikes | Low | Med | Implement interpolation buffer | Matthew |
| Motor lead time (AliExpress) | High | High | **Order TODAY**, Amazon backup | Eric |

---

## 5. Directory Structure

```
wireless-glove-hand/
├── docs/
│   └── bmad/
│       ├── product-brief.md      ✅
│       ├── prd.md                ✅
│       ├── architecture.md       ✅ (this file)
│       └── epics.md              ✅
├── firmware/
│   ├── glove/
│   │   ├── platformio.ini
│   │   └── src/
│   │       ├── main.cpp          # Sensor reading + BLE transmit
│   │       ├── flex_sensor.h     # Sensor calibration
│   │       └── ble_transmit.h    # BLE GATT server
│   └── hand/
│       ├── platformio.ini
│       └── src/
│           ├── main.cpp          # BLE receive + FOC control
│           ├── foc_controller.h  # SimpleFOC wrapper
│           ├── ble_receive.h     # BLE GATT client
│           └── encoder_mux.h     # I2C multiplexer for AS5600
├── hardware/
│   ├── cad/                      # Fusion 360 / STEP files
│   └── schematics/               # KiCad or Fritzing
└── tests/
    ├── motor_test/               # Single motor FOC validation
    ├── encoder_test/             # AS5600 + mux test
    └── ble_latency_test/         # Round-trip timing
```

---

## 6. Next Steps

### Immediate (Today)
- [x] Finalize architecture decisions ← **DONE**
- [ ] Order motors from AliExpress (longest lead time)
- [ ] Order DRV8302, AS5600, TCA9548A from Amazon

### Week 1 (Feb 5-12)
- [ ] Set up PlatformIO project structure
- [ ] Single motor + SimpleFOC + AS5600 test (use borrowed/available motor)
- [ ] BLE connection test between two ESP32s

### Week 2 (Feb 12-19)
- [ ] Motors arrive → full 4-motor test
- [ ] Flex sensor circuit + calibration
- [ ] Initial CAD for single finger

### Week 3-4 (Feb 19 - Mar 4)
- [ ] Print and assemble finger
- [ ] End-to-end integration
- [ ] Demo-ready PoC

---

## Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Project Lead | Eric Reyes | 2026-02-05 | ✅ |
| Hardware Lead | Antonio Rojas | | |
| Firmware Lead | Matthew Men | | |
| Advisor | Junaid Anwar | | |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-05 | Eric Reyes | Initial architecture draft |
| 1.0 | 2026-02-05 | Eric Reyes | Finalized all decisions for PoC, added BOM |
